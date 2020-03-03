/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <avz/percpu.h>

#include <asm/mach/arch.h>
#include <asm/memory.h>

#include <avz/types.h>
#include <avz/sched.h>

#include <soo/uapi/arch-arm.h>

/* Use marker if you need to separate the values later */

#define DEFINE(sym, val) \
        asm volatile("\n->" #sym " %0 " #val : : "i" (val))

#define BLANK() asm volatile("\n->" : : )

int main(void)
{
  DEFINE(OFFSET_USER_REGS,	offsetof(struct vcpu_guest_context, user_regs));
  DEFINE(OFFSET_ARCH_VCPU,		offsetof(struct vcpu, arch));
  DEFINE(OFFSET_GUEST_CONTEXT,		offsetof(struct arch_vcpu, guest_context));

  DEFINE(OFFSET_VCPU_INFO,            offsetof(struct vcpu, vcpu_info));
  DEFINE(OFFSET_VCPU_DOMAIN,          offsetof(struct vcpu, domain));
  DEFINE(OFFSET_DOMAIN_ID,            offsetof(struct domain, domain_id));
  DEFINE(OFFSET_EVTCHN_UPCALL_PENDING,        offsetof(struct vcpu_info, evtchn_upcall_pending));
  DEFINE(OFFSET_HYPERVISOR_CALLBACK,  offsetof(struct vcpu_guest_context, event_callback));
  DEFINE(OFFSET_DOMCALL_CALLBACK, offsetof(struct vcpu_guest_context, domcall));
  DEFINE(OFFSET_PREP_SWITCH_DOMAIN_CALLBACK, offsetof(struct vcpu_guest_context, prep_switch_domain_callback));

  DEFINE(S_R0,			offsetof(struct cpu_user_regs, r0));
  DEFINE(S_R1,			offsetof(struct cpu_user_regs, r1));
  DEFINE(S_R2,			offsetof(struct cpu_user_regs, r2));
  DEFINE(S_R3,			offsetof(struct cpu_user_regs, r3));
  DEFINE(S_R4,			offsetof(struct cpu_user_regs, r4));
  DEFINE(S_R5,			offsetof(struct cpu_user_regs, r5));
  DEFINE(S_R6,			offsetof(struct cpu_user_regs, r6));
  DEFINE(S_R7,			offsetof(struct cpu_user_regs, r7));
  DEFINE(S_R8,			offsetof(struct cpu_user_regs, r8));
  DEFINE(S_R9,			offsetof(struct cpu_user_regs, r9));
  DEFINE(S_R10,			offsetof(struct cpu_user_regs, r10));
  DEFINE(S_FP,			offsetof(struct cpu_user_regs, r11));
  DEFINE(S_IP,			offsetof(struct cpu_user_regs, r12));
  DEFINE(S_SP,			offsetof(struct cpu_user_regs, r13));
  DEFINE(S_LR,			offsetof(struct cpu_user_regs, r14));
  DEFINE(S_PC,			offsetof(struct cpu_user_regs, r15));
  DEFINE(S_PSR,			offsetof(struct cpu_user_regs, psr));
  DEFINE(S_OLD_R0,		offsetof(struct cpu_user_regs, ctx));
  DEFINE(S_FRAME_SIZE,		sizeof(struct cpu_user_regs));

  BLANK();

  DEFINE(PAGE_SZ,	       	PAGE_SIZE);
  BLANK();
  DEFINE(SIZEOF_MACHINE_DESC,	sizeof(struct machine_desc));
  DEFINE(MACHINFO_TYPE,		offsetof(struct machine_desc, nr));
  DEFINE(MACHINFO_NAME,		offsetof(struct machine_desc, name));
  BLANK();
  DEFINE(PROC_INFO_SZ,		sizeof(struct proc_info_list));
  DEFINE(PROCINFO_INITFUNC,	offsetof(struct proc_info_list, __cpu_flush));
  DEFINE(PROCINFO_MM_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_mm_mmu_flags));
  DEFINE(PROCINFO_IO_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_io_mmu_flags));

  BLANK();

  DEFINE(OFFSET_R0,			offsetof(struct cpu_user_regs, r0));
  DEFINE(OFFSET_R1,			offsetof(struct cpu_user_regs, r1));
  DEFINE(OFFSET_R2,			offsetof(struct cpu_user_regs, r2));
  DEFINE(OFFSET_R3,			offsetof(struct cpu_user_regs, r3));
  DEFINE(OFFSET_R4,			offsetof(struct cpu_user_regs, r4));
  DEFINE(OFFSET_R5,			offsetof(struct cpu_user_regs, r5));
  DEFINE(OFFSET_R6,			offsetof(struct cpu_user_regs, r6));
  DEFINE(OFFSET_R7,			offsetof(struct cpu_user_regs, r7));
  DEFINE(OFFSET_R8,			offsetof(struct cpu_user_regs, r8));
  DEFINE(OFFSET_R9,			offsetof(struct cpu_user_regs, r9));
  DEFINE(OFFSET_R10,			offsetof(struct cpu_user_regs, r10));
  DEFINE(OFFSET_R11,			offsetof(struct cpu_user_regs, r11));
  DEFINE(OFFSET_R12,			offsetof(struct cpu_user_regs, r12));
  DEFINE(OFFSET_R13,			offsetof(struct cpu_user_regs, r13));
  DEFINE(OFFSET_R14,			offsetof(struct cpu_user_regs, r14));
  DEFINE(OFFSET_R15,			offsetof(struct cpu_user_regs, r15));
  DEFINE(OFFSET_PSR,			offsetof(struct cpu_user_regs, psr));
  DEFINE(OFFSET_CTX,			offsetof(struct cpu_user_regs, ctx));

  BLANK();

  DEFINE(OFFSET_SYS_REGS,		 offsetof(struct vcpu_guest_context, sys_regs));
  DEFINE(OFFSET_VPSR,			 offsetof(struct cpu_sys_regs, vpsr));
  DEFINE(OFFSET_VKSP,			 offsetof(struct cpu_sys_regs, vksp));
  DEFINE(OFFSET_VUSP,			 offsetof(struct cpu_sys_regs, vusp));
  DEFINE(OFFSET_VDACR,			 offsetof(struct cpu_sys_regs, vdacr));
  DEFINE(OFFSET_GUEST_DACR,		 offsetof(struct cpu_sys_regs, guest_dacr));
  DEFINE(OFFSET_GUEST_TLS,		 offsetof(struct cpu_sys_regs, guest_tls));
  DEFINE(OFFSET_GUEST_TLS_RW,	 offsetof(struct cpu_sys_regs, guest_tls_rw));
  DEFINE(OFFSET_GUEST_CONTEXTID, offsetof(struct cpu_sys_regs, guest_context_id));
  DEFINE(OFFSET_GUEST_TTBR0,     offsetof(struct cpu_sys_regs, guest_ttbr0));
  DEFINE(OFFSET_GUEST_TTBR1,     offsetof(struct cpu_sys_regs, guest_ttbr1));
  DEFINE(OFFSET_GUEST_TTBCR,     offsetof(struct cpu_sys_regs, guest_ttbcr));
  DEFINE(OFFSET_GUEST_PER_CPU,	 offsetof(struct cpu_sys_regs, guest_per_cpu));

  DEFINE(OFFSET_VFSR,			 offsetof(struct cpu_sys_regs, vfsr));
  DEFINE(OFFSET_VFAR, 			 offsetof(struct cpu_sys_regs, vfar));
  DEFINE(OFFSET_VCP0,			 offsetof(struct cpu_sys_regs, vcp0));
  DEFINE(OFFSET_VCP1,			 offsetof(struct cpu_sys_regs, vcp1));

  DEFINE(OFFSET_VCPU,			offsetof(struct cpu_info, cur_vcpu));

  return 0;
}

