/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/console.h>
#include <linux/irqreturn.h>
#include <linux/kthread.h>

#include <soo/evtchn.h>
#include <soo/uapi/soo.h>

#include <opencn/dev/vlog.h>
#include <opencn/backend/vlog.h>
#include <opencn/logfile.h>

#include <asm/delay.h>

static vlog_back_ring_t ring;
static int ring_irq;

static struct {
	vlog_sring_t *sring;
	uint32_t evtchn;
	uint32_t domid;
} args_init_gate;

DECLARE_COMPLETION(cons_sync);

/* Deferring the processing of lprintk().
 * We prefer using a completion to avoid slow display (like a VGA on a PC)
 * which can potential lead to RT throttling activation due to the fact
 * that the threaded IRQ as always a priority higher than normal threads.
 */
int vlog_deferring(void *data) {

	RING_IDX i, rp;
	vlog_request_t *ring_req;

	while (true) {

		wait_for_completion(&cons_sync);

		rp = ring.sring->req_prod;
		mb();

		for (i = ring.sring->req_cons; i != rp; i++) {

			ring_req = RING_GET_REQUEST(&ring, i);

			/* Display the received string */
			if (logfile_enabled())
				logfile_write(ring_req->line);
			else
				printk(ring_req->line);

		}

		ring.sring->req_cons = i;
	}

	return 0;
}

irqreturn_t vlog_interrupt(int irq, void *dev_id) {

	/* Prefer to defer the processing since lprintk and vga display may re-enable interrupts along the path. */

	complete(&cons_sync);

	return IRQ_HANDLED;
}

void vlog_do_flush(void) {
	do_sync_dom(AGENCY_RT_CPU, DC_VLOG_FLUSH);
}

void vlogback_setup_sring(domid_t domid, vlog_sring_t *sring, uint32_t evtchn) {

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&ring, sring, VLOG_RING_SIZE);

	ring_irq = bind_interdomain_evtchn_to_virqhandler(domid, evtchn, vlog_interrupt, NULL, 0, VLOG_NAME "-backend", NULL);
	BUG_ON(ring_irq < 0);

}

/*
 * Free the ring and unbind evtchn.
 */
void vlogback_free_sring(void) {

	/* Prepare to empty all buffers */
	BACK_RING_INIT(&ring, ring.sring, VLOG_RING_SIZE);

	unbind_from_virqhandler(ring_irq, NULL);
}

/*
 * Called by the RT domain to propagate args to the backend in the non-RT domain.
 */
void probe_vlogback(vlog_sring_t *sring, uint32_t evtchn) {
	BUG_ON(smp_processor_id() != AGENCY_RT_CPU);

	args_init_gate.evtchn = evtchn;
	args_init_gate.sring = sring;
	args_init_gate.domid = smp_processor_id();

	/* Propagate the probe operation on CPU #0 */
	do_sync_dom(AGENCY_CPU0, DC_VLOG_INIT);
}

/*
 * Called by the dc_event on CPU #0
 */
void vlogback_init_gate(dc_event_t dc_event) {
	BUG_ON(smp_processor_id() != AGENCY_CPU0);

	vlogback_setup_sring(args_init_gate.domid, args_init_gate.sring, args_init_gate.evtchn);

	tell_dc_stable(DC_VLOG_INIT);
}

int vlogback_init(void) {
	register_dc_event_callback(DC_VLOG_INIT, vlogback_init_gate);

	kthread_run(vlog_deferring, NULL, "vlog_deferring");

	return 0;
}

device_initcall(vlogback_init);

