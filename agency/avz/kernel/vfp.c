/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <avz/types.h>
#include <avz/sched.h>

#include <asm/vfp.h>
#include <asm/cpregs.h>


/*
 * Save the VFP context related to the guest
 */
void vfp_save_state(struct vcpu *v)
{
  v->arch.vfp.fpexc = READ_CP32(FPEXC);

  WRITE_CP32(v->arch.vfp.fpexc | FPEXC_EN, FPEXC);

  v->arch.vfp.fpscr = READ_CP32(FPSCR);

  if ( v->arch.vfp.fpexc & FPEXC_EX ) /* Check for sub-architecture */
  {
    v->arch.vfp.fpinst = READ_CP32(FPINST);

    if ( v->arch.vfp.fpexc & FPEXC_FP2V )
      v->arch.vfp.fpinst2 = READ_CP32(FPINST2);
    /* Disable FPEXC_EX */
    WRITE_CP32((v->arch.vfp.fpexc | FPEXC_EN) & ~FPEXC_EX, FPEXC);
  }

  /* Save {d0-d15} */
  asm volatile("stc p11, cr0, [%1], #32*4"
               : "=Q" (*v->arch.vfp.fpregs1) : "r" (v->arch.vfp.fpregs1));

  /* 32 x 64 bits registers? */
  if ( (READ_CP32(MVFR0) & MVFR0_A_SIMD_MASK) == 2 )
  {
    /* Save {d16-d31} */
    asm volatile("stcl p11, cr0, [%1], #32*4"
                 : "=Q" (*v->arch.vfp.fpregs2) : "r" (v->arch.vfp.fpregs2));
  }

  WRITE_CP32(v->arch.vfp.fpexc & ~(FPEXC_EN), FPEXC);
}

void vfp_restore_state(struct vcpu *v)
{
	WRITE_CP32(READ_CP32(FPEXC) | FPEXC_EN, FPEXC);

	/* Restore {d0-d15} */
	asm volatile("ldc p11, cr0, [%1], #32*4" : : "Q" (*v->arch.vfp.fpregs1), "r" (v->arch.vfp.fpregs1));

	/* 32 x 64 bits registers? */
	if ( (READ_CP32(MVFR0) & MVFR0_A_SIMD_MASK) == 2 ) /* 32 x 64 bits registers */
		/* Restore {d16-d31} */
		asm volatile("ldcl p11, cr0, [%1], #32*4" : : "Q" (*v->arch.vfp.fpregs2), "r" (v->arch.vfp.fpregs2));

	if (v->arch.vfp.fpexc & FPEXC_EX)
	{
		WRITE_CP32(v->arch.vfp.fpinst, FPINST);
		if (v->arch.vfp.fpexc & FPEXC_FP2V)
			WRITE_CP32(v->arch.vfp.fpinst2, FPINST2);
	}

	WRITE_CP32(v->arch.vfp.fpscr, FPSCR);

	WRITE_CP32(v->arch.vfp.fpexc, FPEXC);
}


static inline unsigned int get_copro_access(void)
{
	unsigned int val;
	asm("mrc p15, 0, %0, c1, c0, 2 @ get copro access"
			: "=r" (val) : : "cc");
	return val;
}

static inline void set_copro_access(unsigned int val)
{
	asm volatile("mcr p15, 0, %0, c1, c0, 2 @ set copro access"
			: : "r" (val) : "cc");
	isb();
}

void vfp_enable(void)
{
	u32 access;


	access = get_copro_access();

	/*
	 * Enable full access to VFP (cp10 and cp11)
	 */
	set_copro_access(access | CPACC_FULL(10) | CPACC_FULL(11));
}


