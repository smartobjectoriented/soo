/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_IRQ_H
#define __ASM_IRQ_H

#ifndef __ASSEMBLER__

#include <asm-generic/irq.h>

struct pt_regs;

/* SOO.tech */

#define	NR_PIRQS	256
#define PIRQ_BASE       0

#define VIRQ_BASE       (PIRQ_BASE + NR_PIRQS)
#define NR_VIRQS        256

/* Max number of possible IPIs */
#define NR_IPIS		16


static inline int nr_legacy_irqs(void)
{
	return 0;
}

#endif /* !__ASSEMBLER__ */
#endif
