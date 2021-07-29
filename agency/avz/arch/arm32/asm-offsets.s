	.arch armv7-a
	.eabi_attribute 28, 1	@ Tag_ABI_VFP_args
	.eabi_attribute 20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute 21, 1	@ Tag_ABI_FP_exceptions
	.eabi_attribute 23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute 24, 1	@ Tag_ABI_align8_needed
	.eabi_attribute 25, 1	@ Tag_ABI_align8_preserved
	.eabi_attribute 26, 2	@ Tag_ABI_enum_size
	.eabi_attribute 30, 6	@ Tag_ABI_optimization_goals
	.eabi_attribute 34, 1	@ Tag_CPU_unaligned_access
	.eabi_attribute 18, 4	@ Tag_ABI_PCS_wchar_t
	.file	"asm-offsets.c"
@ GNU C89 (GNU Toolchain for the A-profile Architecture 9.2-2019.12 (arm-9.10)) version 9.2.1 20191025 (arm-none-linux-gnueabihf)
@	compiled by GNU C version 4.8.1, GMP version 4.3.2, MPFR version 3.1.6, MPC version 1.0.3, isl version isl-0.15-1-g835ea3a-GMP

@ GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
@ options passed:  -nostdinc -I include -I . -I include -I .
@ -I ./lib/libfdt -I arch/arm32/include/ -I arch/arm32/rpi4/include/
@ -iprefix /opt/toolchains/arm-none-linux-gnueabihf_9.2.1/bin/../lib/gcc/arm-none-linux-gnueabihf/9.2.1/
@ -isysroot /opt/toolchains/arm-none-linux-gnueabihf_9.2.1/bin/../arm-none-linux-gnueabihf/libc
@ -D __KERNEL__ -D __AVZ__ -U arm -D BITS_PER_LONG=32 -D KBUILD_STR(s)=#s
@ -D KBUILD_BASENAME=KBUILD_STR(asm_offsets)
@ -include include/generated/autoconf.h
@ -isystem /opt/toolchains/arm-none-linux-gnueabihf_9.2.1/bin/../lib/gcc/arm-none-linux-gnueabihf/9.2.1/include
@ -MD arch/arm32/.asm-offsets.s.d arch/arm32/asm-offsets.c -mlittle-endian
@ -mabi=aapcs-linux -mabi=aapcs-linux -mno-thumb-interwork -mfpu=vfp -marm
@ -mfloat-abi=hard -mtls-dialect=gnu -march=armv7-a+fp
@ -auxbase-strip arch/arm32/asm-offsets.s -g -Wall -Wundef
@ -Wstrict-prototypes -Wno-trigraphs -Werror=implicit-function-declaration
@ -Wno-format-security -Wno-frame-address -Wframe-larger-than=1024
@ -Wno-unused-but-set-variable -Wunused-const-variable=0
@ -Wdeclaration-after-statement -Wno-pointer-sign -Werror=implicit-int
@ -Werror=strict-prototypes -Werror=date-time
@ -Werror=incompatible-pointer-types -Werror=designated-init -std=gnu90
@ -fno-builtin -ffreestanding -fno-strict-aliasing -fno-common -fno-PIE
@ -fno-dwarf2-cfi-asm -fno-ipa-sra -fno-delete-null-pointer-checks
@ -fno-stack-protector -fomit-frame-pointer -fno-var-tracking-assignments
@ -fno-strict-overflow -fno-merge-all-constants -fmerge-constants
@ -fstack-check=no -fconserve-stack -fno-function-sections
@ -fno-data-sections -funwind-tables -fverbose-asm
@ --param allow-store-data-races=0
@ options enabled:  -faggressive-loop-optimizations -fassume-phsa
@ -fauto-inc-dec -fearly-inlining -feliminate-unused-debug-types
@ -ffp-int-builtin-inexact -ffunction-cse -fgcse-lm -fgnu-runtime
@ -fgnu-unique -fident -finline-atomics -fipa-stack-alignment
@ -fira-hoist-pressure -fira-share-save-slots -fira-share-spill-slots
@ -fivopts -fkeep-static-consts -fleading-underscore -flifetime-dse
@ -flto-odr-type-merging -fmath-errno -fmerge-constants
@ -fmerge-debug-strings -fomit-frame-pointer -fpeephole -fplt
@ -fprefetch-loop-arrays -freg-struct-return
@ -fsched-critical-path-heuristic -fsched-dep-count-heuristic
@ -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
@ -fsched-rank-heuristic -fsched-spec -fsched-spec-insn-heuristic
@ -fsched-stalled-insns-dep -fsemantic-interposition -fshow-column
@ -fshrink-wrap-separate -fsigned-zeros -fsplit-ivs-in-unroller
@ -fssa-backprop -fstdarg-opt -fstrict-volatile-bitfields -fsync-libcalls
@ -ftrapping-math -ftree-cselim -ftree-forwprop -ftree-loop-if-convert
@ -ftree-loop-im -ftree-loop-ivcanon -ftree-loop-optimize
@ -ftree-parallelize-loops= -ftree-phiprop -ftree-reassoc -ftree-scev-cprop
@ -funit-at-a-time -funwind-tables -fverbose-asm -fwrapv -fwrapv-pointer
@ -fzero-initialized-in-bss -marm -mbe32 -mglibc -mlittle-endian
@ -mpic-data-is-text-relative -msched-prolog -munaligned-access
@ -mvectorize-with-neon-quad

	.text
.Ltext0:
	.section	.rodata.str1.4,"aMS",%progbits,1
	.align	2
.LC0:
	.ascii	"!! __bad_xchg called! Failure...\012\000"
	.text
	.align	2
	.arch armv7-a
	.syntax unified
	.arm
	.fpu vfp
	.type	__bad_xchg, %function
__bad_xchg:
	.fnstart
.LFB21:
	.file 1 "arch/arm32/include/asm/atomic.h"
	.loc 1 155 54
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 0, uses_anonymous_args = 0
	str	lr, [sp, #-4]!	@,
	.save {lr}
.LCFI0:
	.pad #12
	sub	sp, sp, #12	@,,
.LCFI1:
	str	r0, [sp, #4]	@ ptr, ptr
	str	r1, [sp]	@ size, size
@ arch/arm32/include/asm/atomic.h:156: 	printk("!! __bad_xchg called! Failure...\n");
	.loc 1 156 2
	movw	r0, #:lower16:.LC0	@,
	movt	r0, #:upper16:.LC0	@,
	bl	printk		@
.L2:
@ arch/arm32/include/asm/atomic.h:157: 	while (1);
	.loc 1 157 8 discriminator 1
	b	.L2		@
.LFE21:
	.fnend
	.size	__bad_xchg, .-__bad_xchg
	.align	2
	.global	main
	.syntax unified
	.arm
	.fpu vfp
	.type	main, %function
main:
	.fnstart
.LFB86:
	.file 2 "arch/arm32/asm-offsets.c"
	.loc 2 29 1
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
@ arch/arm32/asm-offsets.c:31: 	DEFINE(OFFSET_SHARED_INFO, offsetof(struct domain, shared_info));
	.loc 2 31 2
	.syntax divided
@ 31 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_SHARED_INFO #392 offsetof(struct domain, shared_info)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:32: 	DEFINE(OFFSET_DOMAIN_ID, offsetof(struct domain, domain_id));
	.loc 2 32 2
@ 32 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_DOMAIN_ID #0 offsetof(struct domain, domain_id)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:34: 	DEFINE(OFFSET_EVTCHN_UPCALL_PENDING, offsetof(struct shared_info, evtchn_upcall_pending));
	.loc 2 34 2
@ 34 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_EVTCHN_UPCALL_PENDING #0 offsetof(struct shared_info, evtchn_upcall_pending)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:36: 	DEFINE(OFFSET_HYPERVISOR_CALLBACK,  offsetof(struct domain, event_callback));
	.loc 2 36 2
@ 36 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_HYPERVISOR_CALLBACK #88 offsetof(struct domain, event_callback)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:37: 	DEFINE(OFFSET_DOMCALL_CALLBACK, offsetof(struct domain, domcall));
	.loc 2 37 2
@ 37 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_DOMCALL_CALLBACK #92 offsetof(struct domain, domcall)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:38: 	DEFINE(OFFSET_G_SP,		 offsetof(struct domain, g_sp));
	.loc 2 38 2
@ 38 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_G_SP #84 offsetof(struct domain, g_sp)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:40: 	DEFINE(OFFSET_CPU_REGS,		offsetof(struct domain, cpu_regs));
	.loc 2 40 2
@ 40 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_CPU_REGS #4 offsetof(struct domain, cpu_regs)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:42: 	BLANK();
	.loc 2 42 2
@ 42 "arch/arm32/asm-offsets.c" 1
	
->	
@ 0 "" 2
@ arch/arm32/asm-offsets.c:44: 	DEFINE(OFFSET_R0,		offsetof(cpu_regs_t, r0));
	.loc 2 44 2
@ 44 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R0 #0 offsetof(cpu_regs_t, r0)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:45: 	DEFINE(OFFSET_R1,		offsetof(cpu_regs_t, r1));
	.loc 2 45 2
@ 45 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R1 #4 offsetof(cpu_regs_t, r1)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:46: 	DEFINE(OFFSET_R2,		offsetof(cpu_regs_t, r2));
	.loc 2 46 2
@ 46 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R2 #8 offsetof(cpu_regs_t, r2)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:47: 	DEFINE(OFFSET_R3,		offsetof(cpu_regs_t, r3));
	.loc 2 47 2
@ 47 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R3 #12 offsetof(cpu_regs_t, r3)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:48: 	DEFINE(OFFSET_R4,		offsetof(cpu_regs_t, r4));
	.loc 2 48 2
@ 48 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R4 #16 offsetof(cpu_regs_t, r4)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:49: 	DEFINE(OFFSET_R5,		offsetof(cpu_regs_t, r5));
	.loc 2 49 2
@ 49 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R5 #20 offsetof(cpu_regs_t, r5)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:50: 	DEFINE(OFFSET_R6,		offsetof(cpu_regs_t, r6));
	.loc 2 50 2
@ 50 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R6 #24 offsetof(cpu_regs_t, r6)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:51: 	DEFINE(OFFSET_R7,		offsetof(cpu_regs_t, r7));
	.loc 2 51 2
@ 51 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R7 #28 offsetof(cpu_regs_t, r7)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:52: 	DEFINE(OFFSET_R8,		offsetof(cpu_regs_t, r8));
	.loc 2 52 2
@ 52 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R8 #32 offsetof(cpu_regs_t, r8)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:53: 	DEFINE(OFFSET_R9,		offsetof(cpu_regs_t, r9));
	.loc 2 53 2
@ 53 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R9 #36 offsetof(cpu_regs_t, r9)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:54: 	DEFINE(OFFSET_R10,		offsetof(cpu_regs_t, r10));
	.loc 2 54 2
@ 54 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_R10 #40 offsetof(cpu_regs_t, r10)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:55: 	DEFINE(OFFSET_FP,		offsetof(cpu_regs_t, fp));
	.loc 2 55 2
@ 55 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_FP #44 offsetof(cpu_regs_t, fp)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:56: 	DEFINE(OFFSET_IP,		offsetof(cpu_regs_t, ip));
	.loc 2 56 2
@ 56 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_IP #48 offsetof(cpu_regs_t, ip)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:57: 	DEFINE(OFFSET_SP,		offsetof(cpu_regs_t, sp));
	.loc 2 57 2
@ 57 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_SP #52 offsetof(cpu_regs_t, sp)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:58: 	DEFINE(OFFSET_LR,		offsetof(cpu_regs_t, lr));
	.loc 2 58 2
@ 58 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_LR #56 offsetof(cpu_regs_t, lr)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:59: 	DEFINE(OFFSET_PC,		offsetof(cpu_regs_t, pc));
	.loc 2 59 2
@ 59 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_PC #60 offsetof(cpu_regs_t, pc)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:60: 	DEFINE(OFFSET_PSR,		offsetof(cpu_regs_t, psr));
	.loc 2 60 2
@ 60 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_PSR #64 offsetof(cpu_regs_t, psr)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:61: 	DEFINE(OFFSET_SP_USR,		offsetof(cpu_regs_t, sp_usr));
	.loc 2 61 2
@ 61 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_SP_USR #68 offsetof(cpu_regs_t, sp_usr)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:62: 	DEFINE(OFFSET_LR_USR,		offsetof(cpu_regs_t, lr_usr));
	.loc 2 62 2
@ 62 "arch/arm32/asm-offsets.c" 1
	
->OFFSET_LR_USR #72 offsetof(cpu_regs_t, lr_usr)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:64: 	DEFINE(S_FRAME_SIZE,		sizeof(cpu_regs_t));
	.loc 2 64 2
@ 64 "arch/arm32/asm-offsets.c" 1
	
->S_FRAME_SIZE #80 sizeof(cpu_regs_t)	@
@ 0 "" 2
@ arch/arm32/asm-offsets.c:66: 	return 0;
	.loc 2 66 9
	.arm
	.syntax unified
	mov	r3, #0	@ _1,
@ arch/arm32/asm-offsets.c:67: }
	.loc 2 67 1
	mov	r0, r3	@, <retval>
	bx	lr	@
.LFE86:
	.fnend
	.size	main, .-main
	.section	.debug_frame,"",%progbits
.Lframe0:
	.4byte	.LECIE0-.LSCIE0
.LSCIE0:
	.4byte	0xffffffff
	.byte	0x3
	.ascii	"\000"
	.uleb128 0x1
	.sleb128 -4
	.uleb128 0xe
	.byte	0xc
	.uleb128 0xd
	.uleb128 0
	.align	2
.LECIE0:
.LSFDE0:
	.4byte	.LEFDE0-.LASFDE0
.LASFDE0:
	.4byte	.Lframe0
	.4byte	.LFB21
	.4byte	.LFE21-.LFB21
	.byte	0x4
	.4byte	.LCFI0-.LFB21
	.byte	0xe
	.uleb128 0x4
	.byte	0x8e
	.uleb128 0x1
	.byte	0x4
	.4byte	.LCFI1-.LCFI0
	.byte	0xe
	.uleb128 0x10
	.align	2
.LEFDE0:
.LSFDE2:
	.4byte	.LEFDE2-.LASFDE2
.LASFDE2:
	.4byte	.Lframe0
	.4byte	.LFB86
	.4byte	.LFE86-.LFB86
	.align	2
.LEFDE2:
	.text
.Letext0:
	.file 3 "include/types.h"
	.file 4 "arch/arm32/include/asm/processor.h"
	.file 5 "include/config.h"
	.file 6 "include/linker.h"
	.file 7 "include/soo/uapi/console.h"
	.file 8 "include/common.h"
	.file 9 "arch/arm32/include/asm/spinlock.h"
	.file 10 "include/spinlock.h"
	.file 11 "include/smp.h"
	.file 12 "include/list.h"
	.file 13 "include/soo/uapi/soo.h"
	.file 14 "include/soo/uapi/avz.h"
	.file 15 "arch/arm32/include/asm/percpu.h"
	.file 16 "include/device/irq.h"
	.file 17 "include/domain.h"
	.file 18 "include/time.h"
	.file 19 "include/timer.h"
	.file 20 "arch/arm32/include/asm/mmu.h"
	.file 21 "include/memory.h"
	.file 22 "arch/arm32/include/asm/vfp.h"
	.file 23 "include/sched.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x115a
	.2byte	0x4
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF246
	.byte	0x1
	.4byte	.LASF247
	.4byte	.LASF248
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	.Ldebug_line0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF1
	.uleb128 0x3
	.4byte	0x2c
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.ascii	"int\000"
	.uleb128 0x3
	.4byte	0x38
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF2
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF3
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF4
	.uleb128 0x5
	.4byte	.LASF6
	.byte	0x3
	.byte	0x3d
	.byte	0x17
	.4byte	0x4b
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF5
	.uleb128 0x5
	.4byte	.LASF7
	.byte	0x3
	.byte	0x40
	.byte	0x18
	.4byte	0x44
	.uleb128 0x5
	.4byte	.LASF8
	.byte	0x3
	.byte	0x43
	.byte	0x16
	.4byte	0x2c
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF9
	.uleb128 0x5
	.4byte	.LASF10
	.byte	0x3
	.byte	0x46
	.byte	0x1c
	.4byte	0x97
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF11
	.uleb128 0x5
	.4byte	.LASF12
	.byte	0x3
	.byte	0x49
	.byte	0x10
	.4byte	0x59
	.uleb128 0x6
	.ascii	"u8\000"
	.byte	0x3
	.byte	0x4a
	.byte	0x10
	.4byte	0x59
	.uleb128 0x5
	.4byte	.LASF13
	.byte	0x3
	.byte	0x4c
	.byte	0x11
	.4byte	0x6c
	.uleb128 0x6
	.ascii	"u16\000"
	.byte	0x3
	.byte	0x4d
	.byte	0x11
	.4byte	0x6c
	.uleb128 0x5
	.4byte	.LASF14
	.byte	0x3
	.byte	0x55
	.byte	0x11
	.4byte	0x78
	.uleb128 0x6
	.ascii	"u32\000"
	.byte	0x3
	.byte	0x56
	.byte	0x11
	.4byte	0x78
	.uleb128 0x5
	.4byte	.LASF15
	.byte	0x3
	.byte	0x58
	.byte	0x11
	.4byte	0x8b
	.uleb128 0x6
	.ascii	"u64\000"
	.byte	0x3
	.byte	0x59
	.byte	0x11
	.4byte	0x8b
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.4byte	.LASF16
	.uleb128 0x5
	.4byte	.LASF17
	.byte	0x3
	.byte	0xd7
	.byte	0x17
	.4byte	0x4b
	.uleb128 0x3
	.4byte	0x104
	.uleb128 0x5
	.4byte	.LASF18
	.byte	0x3
	.byte	0xd8
	.byte	0x17
	.4byte	0x4b
	.uleb128 0x5
	.4byte	.LASF19
	.byte	0x3
	.byte	0xe1
	.byte	0x17
	.4byte	0x25
	.uleb128 0x7
	.4byte	0x121
	.4byte	0x138
	.uleb128 0x8
	.byte	0
	.uleb128 0x9
	.4byte	.LASF24
	.byte	0x5
	.byte	0x4e
	.byte	0xf
	.4byte	0x12d
	.uleb128 0xa
	.4byte	.LASF44
	.byte	0x50
	.byte	0x4
	.2byte	0x15d
	.byte	0x10
	.4byte	0x25c
	.uleb128 0xb
	.ascii	"r0\000"
	.byte	0x4
	.2byte	0x15e
	.byte	0xa
	.4byte	0x78
	.byte	0
	.uleb128 0xb
	.ascii	"r1\000"
	.byte	0x4
	.2byte	0x15f
	.byte	0xa
	.4byte	0x78
	.byte	0x4
	.uleb128 0xb
	.ascii	"r2\000"
	.byte	0x4
	.2byte	0x160
	.byte	0xa
	.4byte	0x78
	.byte	0x8
	.uleb128 0xb
	.ascii	"r3\000"
	.byte	0x4
	.2byte	0x161
	.byte	0xa
	.4byte	0x78
	.byte	0xc
	.uleb128 0xb
	.ascii	"r4\000"
	.byte	0x4
	.2byte	0x162
	.byte	0xa
	.4byte	0x78
	.byte	0x10
	.uleb128 0xb
	.ascii	"r5\000"
	.byte	0x4
	.2byte	0x163
	.byte	0xa
	.4byte	0x78
	.byte	0x14
	.uleb128 0xb
	.ascii	"r6\000"
	.byte	0x4
	.2byte	0x164
	.byte	0xa
	.4byte	0x78
	.byte	0x18
	.uleb128 0xb
	.ascii	"r7\000"
	.byte	0x4
	.2byte	0x165
	.byte	0xa
	.4byte	0x78
	.byte	0x1c
	.uleb128 0xb
	.ascii	"r8\000"
	.byte	0x4
	.2byte	0x166
	.byte	0xa
	.4byte	0x78
	.byte	0x20
	.uleb128 0xb
	.ascii	"r9\000"
	.byte	0x4
	.2byte	0x167
	.byte	0xa
	.4byte	0x78
	.byte	0x24
	.uleb128 0xb
	.ascii	"r10\000"
	.byte	0x4
	.2byte	0x168
	.byte	0xa
	.4byte	0x78
	.byte	0x28
	.uleb128 0xb
	.ascii	"fp\000"
	.byte	0x4
	.2byte	0x169
	.byte	0xa
	.4byte	0x78
	.byte	0x2c
	.uleb128 0xb
	.ascii	"ip\000"
	.byte	0x4
	.2byte	0x16a
	.byte	0xa
	.4byte	0x78
	.byte	0x30
	.uleb128 0xb
	.ascii	"sp\000"
	.byte	0x4
	.2byte	0x16b
	.byte	0xa
	.4byte	0x78
	.byte	0x34
	.uleb128 0xb
	.ascii	"lr\000"
	.byte	0x4
	.2byte	0x16c
	.byte	0xa
	.4byte	0x78
	.byte	0x38
	.uleb128 0xb
	.ascii	"pc\000"
	.byte	0x4
	.2byte	0x16d
	.byte	0xa
	.4byte	0x78
	.byte	0x3c
	.uleb128 0xb
	.ascii	"psr\000"
	.byte	0x4
	.2byte	0x16e
	.byte	0xa
	.4byte	0x78
	.byte	0x40
	.uleb128 0xc
	.4byte	.LASF20
	.byte	0x4
	.2byte	0x16f
	.byte	0x8
	.4byte	0x78
	.byte	0x44
	.uleb128 0xc
	.4byte	.LASF21
	.byte	0x4
	.2byte	0x170
	.byte	0xa
	.4byte	0x78
	.byte	0x48
	.uleb128 0xc
	.4byte	.LASF22
	.byte	0x4
	.2byte	0x171
	.byte	0xa
	.4byte	0x78
	.byte	0x4c
	.byte	0
	.uleb128 0xd
	.4byte	.LASF23
	.byte	0x4
	.2byte	0x172
	.byte	0x3
	.4byte	0x144
	.uleb128 0x9
	.4byte	.LASF25
	.byte	0x6
	.byte	0x1b
	.byte	0x16
	.4byte	0x25
	.uleb128 0x9
	.4byte	.LASF26
	.byte	0x6
	.byte	0x1b
	.byte	0x2e
	.4byte	0x25
	.uleb128 0x9
	.4byte	.LASF27
	.byte	0x6
	.byte	0x1b
	.byte	0x4a
	.4byte	0x25
	.uleb128 0x9
	.4byte	.LASF28
	.byte	0x6
	.byte	0x1b
	.byte	0x66
	.4byte	0x25
	.uleb128 0xe
	.byte	0x4
	.uleb128 0xf
	.byte	0x4
	.4byte	0x2a8
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF29
	.uleb128 0x10
	.4byte	0x2a1
	.uleb128 0x11
	.4byte	0x2b8
	.uleb128 0x12
	.4byte	0x2a1
	.byte	0
	.uleb128 0x9
	.4byte	.LASF30
	.byte	0x7
	.byte	0x26
	.byte	0xf
	.4byte	0x2c4
	.uleb128 0xf
	.byte	0x4
	.4byte	0x2ad
	.uleb128 0x9
	.4byte	.LASF31
	.byte	0x8
	.byte	0x1e
	.byte	0xf
	.4byte	0x121
	.uleb128 0x13
	.byte	0x7
	.byte	0x4
	.4byte	0x2c
	.byte	0x8
	.byte	0x5f
	.byte	0xe
	.4byte	0x303
	.uleb128 0x14
	.4byte	.LASF32
	.byte	0
	.uleb128 0x14
	.4byte	.LASF33
	.byte	0x1
	.uleb128 0x14
	.4byte	.LASF34
	.byte	0x2
	.uleb128 0x14
	.4byte	.LASF35
	.byte	0x3
	.uleb128 0x14
	.4byte	.LASF36
	.byte	0x4
	.byte	0
	.uleb128 0x5
	.4byte	.LASF37
	.byte	0x8
	.byte	0x61
	.byte	0x3
	.4byte	0x2d6
	.uleb128 0x9
	.4byte	.LASF38
	.byte	0x8
	.byte	0x62
	.byte	0x15
	.4byte	0x303
	.uleb128 0x9
	.4byte	.LASF39
	.byte	0x8
	.byte	0x67
	.byte	0x11
	.4byte	0xcd
	.uleb128 0x15
	.byte	0x4
	.byte	0x1
	.byte	0x13
	.byte	0x9
	.4byte	0x33e
	.uleb128 0x16
	.4byte	.LASF40
	.byte	0x1
	.byte	0x13
	.byte	0x1f
	.4byte	0x3f
	.byte	0
	.byte	0
	.uleb128 0x5
	.4byte	.LASF41
	.byte	0x1
	.byte	0x13
	.byte	0x2a
	.4byte	0x327
	.uleb128 0x15
	.byte	0x4
	.byte	0x9
	.byte	0x18
	.byte	0x9
	.4byte	0x361
	.uleb128 0x16
	.4byte	.LASF42
	.byte	0x9
	.byte	0x19
	.byte	0x18
	.4byte	0x33
	.byte	0
	.byte	0
	.uleb128 0x5
	.4byte	.LASF43
	.byte	0x9
	.byte	0x1a
	.byte	0x3
	.4byte	0x34a
	.uleb128 0x17
	.4byte	.LASF45
	.byte	0x4
	.byte	0xa
	.byte	0xb
	.byte	0x8
	.4byte	0x388
	.uleb128 0x16
	.4byte	.LASF46
	.byte	0xa
	.byte	0xc
	.byte	0x9
	.4byte	0x38
	.byte	0
	.byte	0
	.uleb128 0x15
	.byte	0xc
	.byte	0xa
	.byte	0x1b
	.byte	0x9
	.4byte	0x3c6
	.uleb128 0x18
	.ascii	"raw\000"
	.byte	0xa
	.byte	0x1c
	.byte	0x14
	.4byte	0x361
	.byte	0
	.uleb128 0x16
	.4byte	.LASF47
	.byte	0xa
	.byte	0x1d
	.byte	0x9
	.4byte	0xc1
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF48
	.byte	0xa
	.byte	0x1e
	.byte	0x9
	.4byte	0xc1
	.byte	0x6
	.uleb128 0x16
	.4byte	.LASF49
	.byte	0xa
	.byte	0x1f
	.byte	0x17
	.4byte	0x36d
	.byte	0x8
	.byte	0
	.uleb128 0x5
	.4byte	.LASF50
	.byte	0xa
	.byte	0x20
	.byte	0x3
	.4byte	0x388
	.uleb128 0x9
	.4byte	.LASF51
	.byte	0xb
	.byte	0x1d
	.byte	0x15
	.4byte	0x3f
	.uleb128 0x17
	.4byte	.LASF52
	.byte	0x8
	.byte	0xc
	.byte	0x1c
	.byte	0x8
	.4byte	0x406
	.uleb128 0x16
	.4byte	.LASF53
	.byte	0xc
	.byte	0x1d
	.byte	0x17
	.4byte	0x406
	.byte	0
	.uleb128 0x16
	.4byte	.LASF54
	.byte	0xc
	.byte	0x1d
	.byte	0x1e
	.4byte	0x406
	.byte	0x4
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0x3de
	.uleb128 0x5
	.4byte	.LASF55
	.byte	0xd
	.byte	0x38
	.byte	0x12
	.4byte	0xb5
	.uleb128 0x7
	.4byte	0x33e
	.4byte	0x428
	.uleb128 0x19
	.4byte	0x2c
	.byte	0x7
	.byte	0
	.uleb128 0x9
	.4byte	.LASF56
	.byte	0xd
	.byte	0x51
	.byte	0x11
	.4byte	0x418
	.uleb128 0x9
	.4byte	.LASF57
	.byte	0xd
	.byte	0x52
	.byte	0x11
	.4byte	0x418
	.uleb128 0x13
	.byte	0x7
	.byte	0x4
	.4byte	0x2c
	.byte	0xd
	.byte	0x98
	.byte	0xe
	.4byte	0x485
	.uleb128 0x14
	.4byte	.LASF58
	.byte	0
	.uleb128 0x14
	.4byte	.LASF59
	.byte	0x1
	.uleb128 0x14
	.4byte	.LASF60
	.byte	0x2
	.uleb128 0x14
	.4byte	.LASF61
	.byte	0x3
	.uleb128 0x14
	.4byte	.LASF62
	.byte	0x4
	.uleb128 0x14
	.4byte	.LASF63
	.byte	0x5
	.uleb128 0x14
	.4byte	.LASF64
	.byte	0x6
	.uleb128 0x14
	.4byte	.LASF65
	.byte	0x7
	.uleb128 0x14
	.4byte	.LASF66
	.byte	0x8
	.byte	0
	.uleb128 0x5
	.4byte	.LASF67
	.byte	0xd
	.byte	0xa2
	.byte	0x3
	.4byte	0x440
	.uleb128 0x15
	.byte	0x18
	.byte	0xd
	.byte	0xb7
	.byte	0x9
	.4byte	0x4b4
	.uleb128 0x18
	.ascii	"id\000"
	.byte	0xd
	.byte	0xbd
	.byte	0x10
	.4byte	0x4b4
	.byte	0
	.uleb128 0x16
	.4byte	.LASF68
	.byte	0xd
	.byte	0xbf
	.byte	0x13
	.4byte	0x3de
	.byte	0x10
	.byte	0
	.uleb128 0x7
	.4byte	0x4b
	.4byte	0x4c4
	.uleb128 0x19
	.4byte	0x2c
	.byte	0xf
	.byte	0
	.uleb128 0x5
	.4byte	.LASF69
	.byte	0xd
	.byte	0xc1
	.byte	0x3
	.4byte	0x491
	.uleb128 0x15
	.byte	0x18
	.byte	0xd
	.byte	0xc6
	.byte	0x9
	.4byte	0x4e7
	.uleb128 0x16
	.4byte	.LASF70
	.byte	0xd
	.byte	0xc7
	.byte	0xe
	.4byte	0x4c4
	.byte	0
	.byte	0
	.uleb128 0x5
	.4byte	.LASF71
	.byte	0xd
	.byte	0xc8
	.byte	0x3
	.4byte	0x4d0
	.uleb128 0x15
	.byte	0x11
	.byte	0xd
	.byte	0xe7
	.byte	0x9
	.4byte	0x517
	.uleb128 0x16
	.4byte	.LASF72
	.byte	0xd
	.byte	0xea
	.byte	0x8
	.4byte	0x104
	.byte	0
	.uleb128 0x16
	.4byte	.LASF73
	.byte	0xd
	.byte	0xec
	.byte	0x10
	.4byte	0x4b4
	.byte	0x1
	.byte	0
	.uleb128 0x5
	.4byte	.LASF74
	.byte	0xd
	.byte	0xed
	.byte	0x3
	.4byte	0x4f3
	.uleb128 0x15
	.byte	0x34
	.byte	0xd
	.byte	0xf7
	.byte	0x9
	.4byte	0x57b
	.uleb128 0x16
	.4byte	.LASF75
	.byte	0xd
	.byte	0xf8
	.byte	0xd
	.4byte	0x485
	.byte	0
	.uleb128 0x16
	.4byte	.LASF76
	.byte	0xd
	.byte	0xfa
	.byte	0xf
	.4byte	0x2c
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF77
	.byte	0xd
	.byte	0xfb
	.byte	0xf
	.4byte	0x2c
	.byte	0x8
	.uleb128 0x18
	.ascii	"pfn\000"
	.byte	0xd
	.byte	0xfc
	.byte	0xf
	.4byte	0x2c
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF78
	.byte	0xd
	.byte	0xfe
	.byte	0x10
	.4byte	0x4b4
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF79
	.byte	0xd
	.byte	0xff
	.byte	0xa
	.4byte	0x517
	.byte	0x20
	.byte	0
	.uleb128 0xd
	.4byte	.LASF80
	.byte	0xd
	.2byte	0x100
	.byte	0x3
	.4byte	0x523
	.uleb128 0x1a
	.byte	0x34
	.byte	0xd
	.2byte	0x145
	.byte	0x2
	.4byte	0x5ac
	.uleb128 0x1b
	.4byte	.LASF81
	.byte	0xd
	.2byte	0x146
	.byte	0x11
	.4byte	0x4e7
	.uleb128 0x1c
	.ascii	"ME\000"
	.byte	0xd
	.2byte	0x147
	.byte	0xd
	.4byte	0x57b
	.byte	0
	.uleb128 0x1d
	.byte	0x34
	.byte	0xd
	.2byte	0x144
	.byte	0x9
	.4byte	0x5c3
	.uleb128 0xb
	.ascii	"u\000"
	.byte	0xd
	.2byte	0x148
	.byte	0x4
	.4byte	0x588
	.byte	0
	.byte	0
	.uleb128 0xd
	.4byte	.LASF82
	.byte	0xd
	.2byte	0x149
	.byte	0x3
	.4byte	0x5ac
	.uleb128 0x1e
	.4byte	.LASF83
	.byte	0xd
	.2byte	0x1c7
	.byte	0x16
	.4byte	0x110
	.uleb128 0x1e
	.4byte	.LASF84
	.byte	0xd
	.2byte	0x1c8
	.byte	0x16
	.4byte	0x110
	.uleb128 0x1f
	.4byte	.LASF241
	.uleb128 0x1e
	.4byte	.LASF85
	.byte	0xd
	.2byte	0x2a3
	.byte	0x19
	.4byte	0x5ea
	.uleb128 0x1e
	.4byte	.LASF86
	.byte	0xd
	.2byte	0x2a4
	.byte	0x19
	.4byte	0x5ea
	.uleb128 0x17
	.4byte	.LASF87
	.byte	0xf0
	.byte	0xe
	.byte	0x4f
	.byte	0x8
	.4byte	0x6a6
	.uleb128 0x16
	.4byte	.LASF88
	.byte	0xe
	.byte	0x51
	.byte	0xa
	.4byte	0x9e
	.byte	0
	.uleb128 0x16
	.4byte	.LASF89
	.byte	0xe
	.byte	0x5c
	.byte	0xb
	.4byte	0xcd
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF90
	.byte	0xe
	.byte	0x5d
	.byte	0xb
	.4byte	0xe5
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF91
	.byte	0xe
	.byte	0x5e
	.byte	0xb
	.4byte	0xe5
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF92
	.byte	0xe
	.byte	0x67
	.byte	0x10
	.4byte	0x6b6
	.byte	0x18
	.uleb128 0x16
	.4byte	.LASF93
	.byte	0xe
	.byte	0x69
	.byte	0xb
	.4byte	0x33e
	.byte	0x98
	.uleb128 0x16
	.4byte	.LASF94
	.byte	0xe
	.byte	0x6c
	.byte	0xb
	.4byte	0xe5
	.byte	0xa0
	.uleb128 0x16
	.4byte	.LASF95
	.byte	0xe
	.byte	0x6d
	.byte	0xb
	.4byte	0xe5
	.byte	0xa8
	.uleb128 0x16
	.4byte	.LASF96
	.byte	0xe
	.byte	0x70
	.byte	0xd
	.4byte	0x5c3
	.byte	0xb0
	.uleb128 0x16
	.4byte	.LASF97
	.byte	0xe
	.byte	0x72
	.byte	0x16
	.4byte	0x6bb
	.byte	0xe4
	.uleb128 0x16
	.4byte	.LASF98
	.byte	0xe
	.byte	0x75
	.byte	0x8
	.4byte	0x299
	.byte	0xe8
	.byte	0
	.uleb128 0x7
	.4byte	0x110
	.4byte	0x6b6
	.uleb128 0x19
	.4byte	0x2c
	.byte	0x7f
	.byte	0
	.uleb128 0x3
	.4byte	0x6a6
	.uleb128 0xf
	.byte	0x4
	.4byte	0x609
	.uleb128 0x5
	.4byte	.LASF99
	.byte	0xe
	.byte	0x78
	.byte	0x1c
	.4byte	0x609
	.uleb128 0x3
	.4byte	0x6c1
	.uleb128 0x9
	.4byte	.LASF100
	.byte	0xe
	.byte	0x7a
	.byte	0x20
	.4byte	0x6de
	.uleb128 0xf
	.byte	0x4
	.4byte	0x6cd
	.uleb128 0x17
	.4byte	.LASF101
	.byte	0x2c
	.byte	0xe
	.byte	0x87
	.byte	0x8
	.4byte	0x781
	.uleb128 0x16
	.4byte	.LASF102
	.byte	0xe
	.byte	0x89
	.byte	0x9
	.4byte	0x38
	.byte	0
	.uleb128 0x16
	.4byte	.LASF103
	.byte	0xe
	.byte	0x8b
	.byte	0x13
	.4byte	0x25
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF87
	.byte	0xe
	.byte	0x8d
	.byte	0x14
	.4byte	0x781
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF104
	.byte	0xe
	.byte	0x8f
	.byte	0x13
	.4byte	0x25
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF105
	.byte	0xe
	.byte	0x90
	.byte	0x13
	.4byte	0x25
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF106
	.byte	0xe
	.byte	0x93
	.byte	0xc
	.4byte	0x2c4
	.byte	0x14
	.uleb128 0x16
	.4byte	.LASF107
	.byte	0xe
	.byte	0x95
	.byte	0x13
	.4byte	0x25
	.byte	0x18
	.uleb128 0x16
	.4byte	.LASF108
	.byte	0xe
	.byte	0x97
	.byte	0x13
	.4byte	0x25
	.byte	0x1c
	.uleb128 0x16
	.4byte	.LASF109
	.byte	0xe
	.byte	0x98
	.byte	0x13
	.4byte	0x25
	.byte	0x20
	.uleb128 0x16
	.4byte	.LASF110
	.byte	0xe
	.byte	0x9a
	.byte	0x13
	.4byte	0x25
	.byte	0x24
	.uleb128 0x16
	.4byte	.LASF111
	.byte	0xe
	.byte	0x9c
	.byte	0x13
	.4byte	0x25
	.byte	0x28
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0x6c1
	.uleb128 0x5
	.4byte	.LASF112
	.byte	0xe
	.byte	0x9f
	.byte	0x1b
	.4byte	0x6e4
	.uleb128 0x9
	.4byte	.LASF113
	.byte	0xe
	.byte	0xa1
	.byte	0x16
	.4byte	0x79f
	.uleb128 0xf
	.byte	0x4
	.4byte	0x787
	.uleb128 0x7
	.4byte	0x25
	.4byte	0x7b5
	.uleb128 0x19
	.4byte	0x2c
	.byte	0x3
	.byte	0
	.uleb128 0x9
	.4byte	.LASF114
	.byte	0xf
	.byte	0x28
	.byte	0x16
	.4byte	0x7a5
	.uleb128 0x9
	.4byte	.LASF115
	.byte	0x10
	.byte	0x7f
	.byte	0x16
	.4byte	0x110
	.uleb128 0x5
	.4byte	.LASF116
	.byte	0x10
	.byte	0x96
	.byte	0xd
	.4byte	0x38
	.uleb128 0xf
	.byte	0x4
	.4byte	0x7df
	.uleb128 0x11
	.4byte	0x7ea
	.uleb128 0x12
	.4byte	0x2c
	.byte	0
	.uleb128 0x5
	.4byte	.LASF117
	.byte	0x10
	.byte	0x98
	.byte	0x10
	.4byte	0x7f6
	.uleb128 0xf
	.byte	0x4
	.4byte	0x7fc
	.uleb128 0x11
	.4byte	0x80c
	.uleb128 0x12
	.4byte	0x2c
	.uleb128 0x12
	.4byte	0x80c
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0x812
	.uleb128 0x17
	.4byte	.LASF118
	.byte	0x40
	.byte	0x10
	.byte	0xc4
	.byte	0x8
	.4byte	0x8c9
	.uleb128 0x16
	.4byte	.LASF119
	.byte	0x10
	.byte	0xc5
	.byte	0xa
	.4byte	0xb3d
	.byte	0
	.uleb128 0x16
	.4byte	.LASF120
	.byte	0x10
	.byte	0xc6
	.byte	0x11
	.4byte	0x7ea
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF121
	.byte	0x10
	.byte	0xc7
	.byte	0x13
	.4byte	0xb43
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF122
	.byte	0x10
	.byte	0xc8
	.byte	0x15
	.4byte	0xb49
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF123
	.byte	0x10
	.byte	0xc9
	.byte	0x10
	.4byte	0x2c
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF124
	.byte	0x10
	.byte	0xca
	.byte	0x10
	.4byte	0x2c
	.byte	0x14
	.uleb128 0x16
	.4byte	.LASF42
	.byte	0x10
	.byte	0xcb
	.byte	0xe
	.4byte	0x3c6
	.byte	0x18
	.uleb128 0x16
	.4byte	.LASF125
	.byte	0x10
	.byte	0xcc
	.byte	0xa
	.4byte	0x299
	.byte	0x24
	.uleb128 0x16
	.4byte	.LASF126
	.byte	0x10
	.byte	0xcd
	.byte	0xa
	.4byte	0x299
	.byte	0x28
	.uleb128 0x16
	.4byte	.LASF127
	.byte	0x10
	.byte	0xce
	.byte	0x1a
	.4byte	0x2c
	.byte	0x2c
	.uleb128 0x16
	.4byte	.LASF128
	.byte	0x10
	.byte	0xd1
	.byte	0x13
	.4byte	0x3de
	.byte	0x30
	.uleb128 0x16
	.4byte	.LASF129
	.byte	0x10
	.byte	0xd4
	.byte	0xf
	.4byte	0x2c
	.byte	0x38
	.uleb128 0x16
	.4byte	.LASF130
	.byte	0x10
	.byte	0xd5
	.byte	0x8
	.4byte	0x299
	.byte	0x3c
	.byte	0
	.uleb128 0x17
	.4byte	.LASF131
	.byte	0x10
	.byte	0x10
	.byte	0x9a
	.byte	0x10
	.4byte	0x90b
	.uleb128 0x16
	.4byte	.LASF120
	.byte	0x10
	.byte	0x9b
	.byte	0x12
	.4byte	0x91f
	.byte	0
	.uleb128 0x16
	.4byte	.LASF132
	.byte	0x10
	.byte	0x9c
	.byte	0x10
	.4byte	0x29b
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF133
	.byte	0x10
	.byte	0x9d
	.byte	0xb
	.4byte	0x299
	.byte	0x8
	.uleb128 0x18
	.ascii	"irq\000"
	.byte	0x10
	.byte	0x9e
	.byte	0x6
	.4byte	0x38
	.byte	0xc
	.byte	0
	.uleb128 0x20
	.4byte	0x7cd
	.4byte	0x91f
	.uleb128 0x12
	.4byte	0x38
	.uleb128 0x12
	.4byte	0x299
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0x90b
	.uleb128 0x5
	.4byte	.LASF134
	.byte	0x10
	.byte	0x9f
	.byte	0x3
	.4byte	0x8c9
	.uleb128 0x17
	.4byte	.LASF135
	.byte	0x24
	.byte	0x10
	.byte	0xa2
	.byte	0x8
	.4byte	0x9b4
	.uleb128 0x18
	.ascii	"ack\000"
	.byte	0x10
	.byte	0xa8
	.byte	0x9
	.4byte	0x7d9
	.byte	0
	.uleb128 0x16
	.4byte	.LASF136
	.byte	0x10
	.byte	0xac
	.byte	0x9
	.4byte	0x7d9
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF137
	.byte	0x10
	.byte	0xb0
	.byte	0x9
	.4byte	0x7d9
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF132
	.byte	0x10
	.byte	0xb2
	.byte	0x13
	.4byte	0x29b
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF138
	.byte	0x10
	.byte	0xb3
	.byte	0xd
	.4byte	0x7d9
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF139
	.byte	0x10
	.byte	0xb5
	.byte	0x14
	.4byte	0x7d9
	.byte	0x14
	.uleb128 0x16
	.4byte	.LASF140
	.byte	0x10
	.byte	0xb6
	.byte	0x14
	.4byte	0x7d9
	.byte	0x18
	.uleb128 0x16
	.4byte	.LASF141
	.byte	0x10
	.byte	0xb8
	.byte	0x14
	.4byte	0x7d9
	.byte	0x1c
	.uleb128 0x18
	.ascii	"eoi\000"
	.byte	0x10
	.byte	0xb9
	.byte	0x14
	.4byte	0x7d9
	.byte	0x20
	.byte	0
	.uleb128 0x21
	.4byte	.LASF142
	.2byte	0xa10
	.byte	0x11
	.byte	0x2e
	.byte	0x8
	.4byte	0xb37
	.uleb128 0x16
	.4byte	.LASF143
	.byte	0x11
	.byte	0x30
	.byte	0xa
	.4byte	0x40c
	.byte	0
	.uleb128 0x16
	.4byte	.LASF44
	.byte	0x11
	.byte	0x33
	.byte	0xd
	.4byte	0x25c
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF144
	.byte	0x11
	.byte	0x34
	.byte	0xb
	.4byte	0x121
	.byte	0x54
	.uleb128 0x16
	.4byte	.LASF145
	.byte	0x11
	.byte	0x36
	.byte	0x9
	.4byte	0x121
	.byte	0x58
	.uleb128 0x16
	.4byte	.LASF146
	.byte	0x11
	.byte	0x37
	.byte	0x9
	.4byte	0x121
	.byte	0x5c
	.uleb128 0x18
	.ascii	"vfp\000"
	.byte	0x11
	.byte	0x39
	.byte	0x13
	.4byte	0xf84
	.byte	0x60
	.uleb128 0x22
	.4byte	.LASF147
	.byte	0x11
	.byte	0x3c
	.byte	0xe
	.4byte	0xf14
	.2byte	0x170
	.uleb128 0x22
	.4byte	.LASF87
	.byte	0x11
	.byte	0x3e
	.byte	0x11
	.4byte	0x781
	.2byte	0x188
	.uleb128 0x22
	.4byte	.LASF148
	.byte	0x11
	.byte	0x40
	.byte	0xd
	.4byte	0x3c6
	.2byte	0x18c
	.uleb128 0x22
	.4byte	.LASF149
	.byte	0x11
	.byte	0x42
	.byte	0xf
	.4byte	0x2c
	.2byte	0x198
	.uleb128 0x22
	.4byte	.LASF150
	.byte	0x11
	.byte	0x43
	.byte	0xf
	.4byte	0x2c
	.2byte	0x19c
	.uleb128 0x22
	.4byte	.LASF151
	.byte	0x11
	.byte	0x46
	.byte	0x10
	.4byte	0x10a0
	.2byte	0x1a0
	.uleb128 0x22
	.4byte	.LASF152
	.byte	0x11
	.byte	0x47
	.byte	0xd
	.4byte	0x3c6
	.2byte	0x9a0
	.uleb128 0x22
	.4byte	.LASF153
	.byte	0x11
	.byte	0x4a
	.byte	0x39
	.4byte	0x107f
	.2byte	0x9ac
	.uleb128 0x22
	.4byte	.LASF154
	.byte	0x11
	.byte	0x4d
	.byte	0x9
	.4byte	0x115
	.2byte	0x9b0
	.uleb128 0x22
	.4byte	.LASF155
	.byte	0x11
	.byte	0x4f
	.byte	0x6
	.4byte	0x38
	.2byte	0x9b4
	.uleb128 0x22
	.4byte	.LASF156
	.byte	0x11
	.byte	0x51
	.byte	0x7
	.4byte	0x104
	.2byte	0x9b8
	.uleb128 0x22
	.4byte	.LASF157
	.byte	0x11
	.byte	0x52
	.byte	0xf
	.4byte	0xe2e
	.2byte	0x9c0
	.uleb128 0x22
	.4byte	.LASF158
	.byte	0x11
	.byte	0x54
	.byte	0x14
	.4byte	0x10b5
	.2byte	0x9e0
	.uleb128 0x22
	.4byte	.LASF159
	.byte	0x11
	.byte	0x56
	.byte	0x6
	.4byte	0x38
	.2byte	0x9e4
	.uleb128 0x22
	.4byte	.LASF160
	.byte	0x11
	.byte	0x59
	.byte	0x9
	.4byte	0x115
	.2byte	0x9e8
	.uleb128 0x22
	.4byte	.LASF161
	.byte	0x11
	.byte	0x5b
	.byte	0x10
	.4byte	0x25
	.2byte	0x9ec
	.uleb128 0x22
	.4byte	.LASF162
	.byte	0x11
	.byte	0x5c
	.byte	0xb
	.4byte	0x33e
	.2byte	0x9f0
	.uleb128 0x22
	.4byte	.LASF163
	.byte	0x11
	.byte	0x5f
	.byte	0x6
	.4byte	0x10bb
	.2byte	0x9f4
	.uleb128 0x22
	.4byte	.LASF164
	.byte	0x11
	.byte	0x60
	.byte	0xd
	.4byte	0x3c6
	.2byte	0x9f8
	.uleb128 0x22
	.4byte	.LASF165
	.byte	0x11
	.byte	0x62
	.byte	0x10
	.4byte	0x25
	.2byte	0xa04
	.uleb128 0x22
	.4byte	.LASF166
	.byte	0x11
	.byte	0x63
	.byte	0x10
	.4byte	0x25
	.2byte	0xa08
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0x9b4
	.uleb128 0xf
	.byte	0x4
	.4byte	0x2a1
	.uleb128 0xf
	.byte	0x4
	.4byte	0x931
	.uleb128 0xf
	.byte	0x4
	.4byte	0x8c9
	.uleb128 0x7
	.4byte	0x812
	.4byte	0xb60
	.uleb128 0x23
	.4byte	0x2c
	.2byte	0x3fb
	.byte	0
	.uleb128 0x9
	.4byte	.LASF167
	.byte	0x10
	.byte	0xd9
	.byte	0x17
	.4byte	0xb4f
	.uleb128 0x15
	.byte	0x14
	.byte	0x10
	.byte	0xde
	.byte	0x9
	.4byte	0xbb7
	.uleb128 0x16
	.4byte	.LASF168
	.byte	0x10
	.byte	0xe0
	.byte	0xc
	.4byte	0x7d9
	.byte	0
	.uleb128 0x16
	.4byte	.LASF169
	.byte	0x10
	.byte	0xe1
	.byte	0xc
	.4byte	0x7d9
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF170
	.byte	0x10
	.byte	0xe2
	.byte	0xc
	.4byte	0x7d9
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF171
	.byte	0x10
	.byte	0xe3
	.byte	0xc
	.4byte	0x7d9
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF172
	.byte	0x10
	.byte	0xe5
	.byte	0xc
	.4byte	0xbc8
	.byte	0x10
	.byte	0
	.uleb128 0x11
	.4byte	0xbc2
	.uleb128 0x12
	.4byte	0xbc2
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0x25c
	.uleb128 0xf
	.byte	0x4
	.4byte	0xbb7
	.uleb128 0x5
	.4byte	.LASF173
	.byte	0x10
	.byte	0xe7
	.byte	0x3
	.4byte	0xb6c
	.uleb128 0x9
	.4byte	.LASF174
	.byte	0x10
	.byte	0xe9
	.byte	0x12
	.4byte	0xbce
	.uleb128 0x1e
	.4byte	.LASF175
	.byte	0x10
	.2byte	0x110
	.byte	0x1
	.4byte	0x3c6
	.uleb128 0x5
	.4byte	.LASF176
	.byte	0x12
	.byte	0x19
	.byte	0xd
	.4byte	0xf1
	.uleb128 0x17
	.4byte	.LASF177
	.byte	0x38
	.byte	0x12
	.byte	0x3b
	.byte	0x8
	.4byte	0xc9c
	.uleb128 0x16
	.4byte	.LASF132
	.byte	0x12
	.byte	0x3f
	.byte	0x8
	.4byte	0xb3d
	.byte	0
	.uleb128 0x16
	.4byte	.LASF178
	.byte	0x12
	.byte	0x41
	.byte	0x8
	.4byte	0x299
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF179
	.byte	0x12
	.byte	0x42
	.byte	0x8
	.4byte	0x299
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF180
	.byte	0x12
	.byte	0x44
	.byte	0xf
	.4byte	0x2c
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF181
	.byte	0x12
	.byte	0x46
	.byte	0xc
	.4byte	0xca1
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF136
	.byte	0x12
	.byte	0x47
	.byte	0xa
	.4byte	0xbf3
	.byte	0x18
	.uleb128 0x16
	.4byte	.LASF182
	.byte	0x12
	.byte	0x48
	.byte	0x6
	.4byte	0xd9
	.byte	0x20
	.uleb128 0x16
	.4byte	.LASF183
	.byte	0x12
	.byte	0x49
	.byte	0x6
	.4byte	0xd9
	.byte	0x24
	.uleb128 0x16
	.4byte	.LASF184
	.byte	0x12
	.byte	0x4a
	.byte	0x6
	.4byte	0xd9
	.byte	0x28
	.uleb128 0x16
	.4byte	.LASF123
	.byte	0x12
	.byte	0x4b
	.byte	0x10
	.4byte	0x25
	.byte	0x2c
	.uleb128 0x16
	.4byte	.LASF185
	.byte	0x12
	.byte	0x52
	.byte	0xa
	.4byte	0xbf3
	.byte	0x30
	.byte	0
	.uleb128 0x24
	.4byte	0xbf3
	.uleb128 0xf
	.byte	0x4
	.4byte	0xc9c
	.uleb128 0x25
	.4byte	.LASF249
	.byte	0x7
	.byte	0x4
	.4byte	0x2c
	.byte	0x12
	.byte	0x89
	.byte	0x6
	.4byte	0xcd2
	.uleb128 0x14
	.4byte	.LASF186
	.byte	0
	.uleb128 0x14
	.4byte	.LASF187
	.byte	0x1
	.uleb128 0x14
	.4byte	.LASF188
	.byte	0x2
	.uleb128 0x14
	.4byte	.LASF189
	.byte	0x3
	.byte	0
	.uleb128 0x17
	.4byte	.LASF190
	.byte	0x58
	.byte	0x12
	.byte	0xa8
	.byte	0x8
	.4byte	0xdbd
	.uleb128 0x16
	.4byte	.LASF132
	.byte	0x12
	.byte	0xa9
	.byte	0xe
	.4byte	0x29b
	.byte	0
	.uleb128 0x16
	.4byte	.LASF191
	.byte	0x12
	.byte	0xaa
	.byte	0xf
	.4byte	0x2c
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF178
	.byte	0x12
	.byte	0xac
	.byte	0x8
	.4byte	0x299
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF192
	.byte	0x12
	.byte	0xae
	.byte	0x6
	.4byte	0xd9
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF180
	.byte	0x12
	.byte	0xaf
	.byte	0xf
	.4byte	0x2c
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF193
	.byte	0x12
	.byte	0xb0
	.byte	0xf
	.4byte	0x2c
	.byte	0x14
	.uleb128 0x16
	.4byte	.LASF182
	.byte	0x12
	.byte	0xb2
	.byte	0x6
	.4byte	0xd9
	.byte	0x18
	.uleb128 0x16
	.4byte	.LASF184
	.byte	0x12
	.byte	0xb3
	.byte	0x6
	.4byte	0x38
	.byte	0x1c
	.uleb128 0x16
	.4byte	.LASF194
	.byte	0x12
	.byte	0xb5
	.byte	0x6
	.4byte	0xf1
	.byte	0x20
	.uleb128 0x16
	.4byte	.LASF195
	.byte	0x12
	.byte	0xb6
	.byte	0x6
	.4byte	0xf1
	.byte	0x28
	.uleb128 0x16
	.4byte	.LASF196
	.byte	0x12
	.byte	0xb8
	.byte	0x10
	.4byte	0x25
	.byte	0x30
	.uleb128 0x16
	.4byte	.LASF197
	.byte	0x12
	.byte	0xb9
	.byte	0x10
	.4byte	0x25
	.byte	0x34
	.uleb128 0x16
	.4byte	.LASF198
	.byte	0x12
	.byte	0xbb
	.byte	0xe
	.4byte	0x925
	.byte	0x38
	.uleb128 0x16
	.4byte	.LASF199
	.byte	0x12
	.byte	0xbd
	.byte	0x8
	.4byte	0xdd7
	.byte	0x48
	.uleb128 0x16
	.4byte	.LASF200
	.byte	0x12
	.byte	0xbe
	.byte	0x9
	.4byte	0xded
	.byte	0x4c
	.uleb128 0x16
	.4byte	.LASF201
	.byte	0x12
	.byte	0xbf
	.byte	0x9
	.4byte	0xdfe
	.byte	0x50
	.uleb128 0x16
	.4byte	.LASF202
	.byte	0x12
	.byte	0xc1
	.byte	0x18
	.4byte	0xca7
	.byte	0x54
	.byte	0
	.uleb128 0x20
	.4byte	0x38
	.4byte	0xdd1
	.uleb128 0x12
	.4byte	0x25
	.uleb128 0x12
	.4byte	0xdd1
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0xcd2
	.uleb128 0xf
	.byte	0x4
	.4byte	0xdbd
	.uleb128 0x11
	.4byte	0xded
	.uleb128 0x12
	.4byte	0xca7
	.uleb128 0x12
	.4byte	0xdd1
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0xddd
	.uleb128 0x11
	.4byte	0xdfe
	.uleb128 0x12
	.4byte	0xdd1
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0xdf3
	.uleb128 0x9
	.4byte	.LASF203
	.byte	0x12
	.byte	0xc8
	.byte	0x16
	.4byte	0x25
	.uleb128 0x9
	.4byte	.LASF204
	.byte	0x12
	.byte	0xda
	.byte	0x1c
	.4byte	0xe1c
	.uleb128 0xf
	.byte	0x4
	.4byte	0xbff
	.uleb128 0x9
	.4byte	.LASF205
	.byte	0x12
	.byte	0xea
	.byte	0x23
	.4byte	0xdd1
	.uleb128 0x17
	.4byte	.LASF206
	.byte	0x20
	.byte	0x13
	.byte	0x10
	.byte	0x8
	.4byte	0xea4
	.uleb128 0x16
	.4byte	.LASF207
	.byte	0x13
	.byte	0x12
	.byte	0x9
	.4byte	0xf1
	.byte	0
	.uleb128 0x16
	.4byte	.LASF208
	.byte	0x13
	.byte	0x15
	.byte	0x13
	.4byte	0xea4
	.byte	0x8
	.uleb128 0x16
	.4byte	.LASF209
	.byte	0x13
	.byte	0x18
	.byte	0xc
	.4byte	0xeb5
	.byte	0xc
	.uleb128 0x16
	.4byte	.LASF210
	.byte	0x13
	.byte	0x1b
	.byte	0x13
	.4byte	0xea4
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF211
	.byte	0x13
	.byte	0x1c
	.byte	0x9
	.4byte	0x38
	.byte	0x14
	.uleb128 0x16
	.4byte	.LASF126
	.byte	0x13
	.byte	0x1e
	.byte	0xb
	.4byte	0x299
	.byte	0x18
	.uleb128 0x18
	.ascii	"cpu\000"
	.byte	0x13
	.byte	0x21
	.byte	0xe
	.4byte	0xb5
	.byte	0x1c
	.uleb128 0x16
	.4byte	.LASF124
	.byte	0x13
	.byte	0x28
	.byte	0xd
	.4byte	0x9e
	.byte	0x1e
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0xe2e
	.uleb128 0x11
	.4byte	0xeb5
	.uleb128 0x12
	.4byte	0x299
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0xeaa
	.uleb128 0x9
	.4byte	.LASF212
	.byte	0x13
	.byte	0x63
	.byte	0x1
	.4byte	0xf1
	.uleb128 0x9
	.4byte	.LASF213
	.byte	0x13
	.byte	0x64
	.byte	0x1
	.4byte	0xf1
	.uleb128 0x15
	.byte	0x18
	.byte	0x14
	.byte	0xcd
	.byte	0x9
	.4byte	0xf04
	.uleb128 0x16
	.4byte	.LASF214
	.byte	0x14
	.byte	0xce
	.byte	0xb
	.4byte	0xf04
	.byte	0
	.uleb128 0x16
	.4byte	.LASF215
	.byte	0x14
	.byte	0xcf
	.byte	0xb
	.4byte	0xcd
	.byte	0x10
	.uleb128 0x16
	.4byte	.LASF216
	.byte	0x14
	.byte	0xd0
	.byte	0xb
	.4byte	0xcd
	.byte	0x14
	.byte	0
	.uleb128 0x7
	.4byte	0xcd
	.4byte	0xf14
	.uleb128 0x19
	.4byte	0x2c
	.byte	0x3
	.byte	0
	.uleb128 0x5
	.4byte	.LASF217
	.byte	0x14
	.byte	0xd1
	.byte	0x3
	.4byte	0xed3
	.uleb128 0x9
	.4byte	.LASF218
	.byte	0x14
	.byte	0xe3
	.byte	0x12
	.4byte	0xf2c
	.uleb128 0xf
	.byte	0x4
	.4byte	0xcd
	.uleb128 0x9
	.4byte	.LASF219
	.byte	0x14
	.byte	0xe4
	.byte	0x12
	.4byte	0xf2c
	.uleb128 0x9
	.4byte	.LASF220
	.byte	0x14
	.byte	0xe5
	.byte	0x16
	.4byte	0x25
	.uleb128 0x9
	.4byte	.LASF221
	.byte	0x15
	.byte	0x20
	.byte	0x19
	.4byte	0x3de
	.uleb128 0x7
	.4byte	0x38
	.4byte	0xf61
	.uleb128 0x8
	.byte	0
	.uleb128 0x9
	.4byte	.LASF222
	.byte	0x15
	.byte	0x31
	.byte	0xc
	.4byte	0xf56
	.uleb128 0x7
	.4byte	0xb37
	.4byte	0xf78
	.uleb128 0x8
	.byte	0
	.uleb128 0x9
	.4byte	.LASF223
	.byte	0x15
	.byte	0x33
	.byte	0x17
	.4byte	0xf6d
	.uleb128 0x21
	.4byte	.LASF224
	.2byte	0x110
	.byte	0x16
	.byte	0x1d
	.byte	0x8
	.4byte	0xfe5
	.uleb128 0x16
	.4byte	.LASF225
	.byte	0x16
	.byte	0x1e
	.byte	0xc
	.4byte	0xfe5
	.byte	0
	.uleb128 0x16
	.4byte	.LASF226
	.byte	0x16
	.byte	0x1f
	.byte	0xb
	.4byte	0xfe5
	.byte	0x80
	.uleb128 0x22
	.4byte	.LASF227
	.byte	0x16
	.byte	0x20
	.byte	0xb
	.4byte	0xcd
	.2byte	0x100
	.uleb128 0x22
	.4byte	.LASF228
	.byte	0x16
	.byte	0x21
	.byte	0xb
	.4byte	0xcd
	.2byte	0x104
	.uleb128 0x22
	.4byte	.LASF229
	.byte	0x16
	.byte	0x24
	.byte	0xb
	.4byte	0xcd
	.2byte	0x108
	.uleb128 0x22
	.4byte	.LASF230
	.byte	0x16
	.byte	0x25
	.byte	0xb
	.4byte	0xcd
	.2byte	0x10c
	.byte	0
	.uleb128 0x7
	.4byte	0xe5
	.4byte	0xff5
	.uleb128 0x19
	.4byte	0x2c
	.byte	0xf
	.byte	0
	.uleb128 0x15
	.byte	0x2
	.byte	0x11
	.byte	0x21
	.byte	0x2
	.4byte	0x100c
	.uleb128 0x16
	.4byte	.LASF231
	.byte	0x11
	.byte	0x22
	.byte	0xb
	.4byte	0x40c
	.byte	0
	.byte	0
	.uleb128 0x15
	.byte	0x8
	.byte	0x11
	.byte	0x25
	.byte	0x2
	.4byte	0x1030
	.uleb128 0x16
	.4byte	.LASF232
	.byte	0x11
	.byte	0x26
	.byte	0x7
	.4byte	0xc1
	.byte	0
	.uleb128 0x16
	.4byte	.LASF233
	.byte	0x11
	.byte	0x27
	.byte	0x12
	.4byte	0xb37
	.byte	0x4
	.byte	0
	.uleb128 0x17
	.4byte	.LASF151
	.byte	0x10
	.byte	0x11
	.byte	0x1b
	.byte	0x8
	.4byte	0x107f
	.uleb128 0x16
	.4byte	.LASF75
	.byte	0x11
	.byte	0x1d
	.byte	0x6
	.4byte	0xaa
	.byte	0
	.uleb128 0x16
	.4byte	.LASF234
	.byte	0x11
	.byte	0x1f
	.byte	0x7
	.4byte	0x104
	.byte	0x1
	.uleb128 0x16
	.4byte	.LASF235
	.byte	0x11
	.byte	0x23
	.byte	0x4
	.4byte	0xff5
	.byte	0x2
	.uleb128 0x16
	.4byte	.LASF236
	.byte	0x11
	.byte	0x28
	.byte	0x4
	.4byte	0x100c
	.byte	0x4
	.uleb128 0x16
	.4byte	.LASF237
	.byte	0x11
	.byte	0x2a
	.byte	0x6
	.4byte	0xc1
	.byte	0xc
	.byte	0
	.uleb128 0x13
	.byte	0x7
	.byte	0x4
	.4byte	0x2c
	.byte	0x11
	.byte	0x4a
	.byte	0x7
	.4byte	0x10a0
	.uleb128 0x14
	.4byte	.LASF238
	.byte	0
	.uleb128 0x14
	.4byte	.LASF239
	.byte	0x1
	.uleb128 0x14
	.4byte	.LASF240
	.byte	0x2
	.byte	0
	.uleb128 0x7
	.4byte	0x1030
	.4byte	0x10b0
	.uleb128 0x19
	.4byte	0x2c
	.byte	0x7f
	.byte	0
	.uleb128 0x1f
	.4byte	.LASF242
	.uleb128 0xf
	.byte	0x4
	.4byte	0x10b0
	.uleb128 0x7
	.4byte	0xc1
	.4byte	0x10cb
	.uleb128 0x19
	.4byte	0x2c
	.byte	0x1
	.byte	0
	.uleb128 0x9
	.4byte	.LASF243
	.byte	0x11
	.byte	0x6a
	.byte	0x17
	.4byte	0xb37
	.uleb128 0x7
	.4byte	0xb37
	.4byte	0x10e7
	.uleb128 0x19
	.4byte	0x2c
	.byte	0x6
	.byte	0
	.uleb128 0x9
	.4byte	.LASF244
	.byte	0x11
	.byte	0x6b
	.byte	0x17
	.4byte	0x10d7
	.uleb128 0x9
	.4byte	.LASF81
	.byte	0x17
	.byte	0x34
	.byte	0x17
	.4byte	0xb37
	.uleb128 0x9
	.4byte	.LASF245
	.byte	0x17
	.byte	0x37
	.byte	0x1
	.4byte	0xb37
	.uleb128 0x26
	.4byte	.LASF250
	.byte	0x2
	.byte	0x1c
	.byte	0x5
	.4byte	0x38
	.4byte	.LFB86
	.4byte	.LFE86-.LFB86
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0x27
	.4byte	.LASF251
	.byte	0x1
	.byte	0x9b
	.byte	0xd
	.4byte	.LFB21
	.4byte	.LFE21-.LFB21
	.uleb128 0x1
	.byte	0x9c
	.4byte	0x1156
	.uleb128 0x28
	.ascii	"ptr\000"
	.byte	0x1
	.byte	0x9b
	.byte	0x27
	.4byte	0x1156
	.uleb128 0x2
	.byte	0x91
	.sleb128 -12
	.uleb128 0x29
	.4byte	.LASF77
	.byte	0x1
	.byte	0x9b
	.byte	0x30
	.4byte	0x38
	.uleb128 0x2
	.byte	0x91
	.sleb128 -16
	.byte	0
	.uleb128 0xf
	.byte	0x4
	.4byte	0x115c
	.uleb128 0x2a
	.byte	0
	.section	.debug_abbrev,"",%progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x15
	.byte	0x1
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x5
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x4
	.byte	0x1
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x17
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1d
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1e
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x1f
	.uleb128 0x13
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x20
	.uleb128 0x15
	.byte	0x1
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x21
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0x5
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x22
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x23
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x24
	.uleb128 0x15
	.byte	0
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x25
	.uleb128 0x4
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x26
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2117
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x27
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2116
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x28
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x29
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x18
	.byte	0
	.byte	0
	.uleb128 0x2a
	.uleb128 0x35
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_aranges,"",%progbits
	.4byte	0x1c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x4
	.byte	0
	.2byte	0
	.2byte	0
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	0
	.4byte	0
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.section	.debug_str,"MS",%progbits,1
.LASF38:
	.ascii	"boot_stage\000"
.LASF179:
	.ascii	"vaddr\000"
.LASF46:
	.ascii	"irq_safe\000"
.LASF165:
	.ascii	"vstartinfo_start\000"
.LASF31:
	.ascii	"__end\000"
.LASF249:
	.ascii	"clock_event_mode\000"
.LASF43:
	.ascii	"raw_spinlock_t\000"
.LASF130:
	.ascii	"reg_base\000"
.LASF55:
	.ascii	"domid_t\000"
.LASF60:
	.ascii	"ME_state_living\000"
.LASF123:
	.ascii	"flags\000"
.LASF135:
	.ascii	"irqchip\000"
.LASF182:
	.ascii	"mult\000"
.LASF74:
	.ascii	"spad_t\000"
.LASF129:
	.ascii	"irq_base\000"
.LASF246:
	.ascii	"GNU C89 9.2.1 20191025 -mlittle-endian -mabi=aapcs-"
	.ascii	"linux -mabi=aapcs-linux -mno-thumb-interwork -mfpu="
	.ascii	"vfp -marm -mfloat-abi=hard -mtls-dialect=gnu -march"
	.ascii	"=armv7-a+fp -g -std=gnu90 -fno-builtin -ffreestandi"
	.ascii	"ng -fno-strict-aliasing -fno-common -fno-PIE -fno-d"
	.ascii	"warf2-cfi-asm -fno-ipa-sra -fno-delete-null-pointer"
	.ascii	"-checks -fno-stack-protector -fomit-frame-pointer -"
	.ascii	"fno-var-tracking-assignments -fno-strict-overflow -"
	.ascii	"fno-merge-all-constants -fmerge-constants -fstack-c"
	.ascii	"heck=no -fconserve-stack -fno-function-sections -fn"
	.ascii	"o-data-sections -funwind-tables --param allow-store"
	.ascii	"-data-races=0\000"
.LASF1:
	.ascii	"unsigned int\000"
.LASF229:
	.ascii	"fpinst\000"
.LASF53:
	.ascii	"next\000"
.LASF89:
	.ascii	"version\000"
.LASF58:
	.ascii	"ME_state_booting\000"
.LASF118:
	.ascii	"irqdesc\000"
.LASF220:
	.ascii	"l2pt_phys_start\000"
.LASF116:
	.ascii	"irqreturn_t\000"
.LASF186:
	.ascii	"CLOCK_EVT_MODE_UNUSED\000"
.LASF70:
	.ascii	"agencyUID\000"
.LASF144:
	.ascii	"g_sp\000"
.LASF152:
	.ascii	"event_lock\000"
.LASF205:
	.ascii	"system_timer_clockevent\000"
.LASF120:
	.ascii	"handler\000"
.LASF36:
	.ascii	"BOOT_STAGE_COMPLETED\000"
.LASF105:
	.ascii	"fdt_paddr\000"
.LASF59:
	.ascii	"ME_state_preparing\000"
.LASF177:
	.ascii	"clocksource\000"
.LASF192:
	.ascii	"timer_nr\000"
.LASF153:
	.ascii	"is_dying\000"
.LASF54:
	.ascii	"prev\000"
.LASF195:
	.ascii	"min_delta_ns\000"
.LASF21:
	.ascii	"lr_usr\000"
.LASF91:
	.ascii	"tsc_prev\000"
.LASF160:
	.ascii	"is_running\000"
.LASF134:
	.ascii	"irqaction_t\000"
.LASF56:
	.ascii	"dc_outgoing_domID\000"
.LASF26:
	.ascii	"__initcall_driver_core_end\000"
.LASF65:
	.ascii	"ME_state_terminated\000"
.LASF34:
	.ascii	"BOOT_STAGE_SCHED\000"
.LASF14:
	.ascii	"uint32_t\000"
.LASF204:
	.ascii	"system_timer_clocksource\000"
.LASF98:
	.ascii	"logbool_ht\000"
.LASF172:
	.ascii	"irq_handle\000"
.LASF215:
	.ascii	"pgtable_paddr\000"
.LASF41:
	.ascii	"atomic_t\000"
.LASF184:
	.ascii	"shift\000"
.LASF78:
	.ascii	"spid\000"
.LASF32:
	.ascii	"BOOT_STAGE_INIT\000"
.LASF178:
	.ascii	"base\000"
.LASF240:
	.ascii	"DOMDYING_dead\000"
.LASF102:
	.ascii	"domID\000"
.LASF211:
	.ascii	"timer_cpu\000"
.LASF64:
	.ascii	"ME_state_killed\000"
.LASF228:
	.ascii	"fpscr\000"
.LASF11:
	.ascii	"long long unsigned int\000"
.LASF196:
	.ascii	"min_delta_ticks\000"
.LASF223:
	.ascii	"idle_domain\000"
.LASF203:
	.ascii	"loops_per_jiffy\000"
.LASF188:
	.ascii	"CLOCK_EVT_MODE_PERIODIC\000"
.LASF44:
	.ascii	"cpu_regs\000"
.LASF139:
	.ascii	"enable\000"
.LASF141:
	.ascii	"mask_ack\000"
.LASF48:
	.ascii	"recurse_cnt\000"
.LASF84:
	.ascii	"__cobalt_ready\000"
.LASF210:
	.ascii	"sibling\000"
.LASF117:
	.ascii	"irq_handler_t\000"
.LASF149:
	.ascii	"tot_pages\000"
.LASF197:
	.ascii	"max_delta_ticks\000"
.LASF40:
	.ascii	"counter\000"
.LASF148:
	.ascii	"domain_lock\000"
.LASF183:
	.ascii	"mult_orig\000"
.LASF7:
	.ascii	"__u16\000"
.LASF145:
	.ascii	"event_callback\000"
.LASF18:
	.ascii	"bool_t\000"
.LASF202:
	.ascii	"mode\000"
.LASF227:
	.ascii	"fpexc\000"
.LASF127:
	.ascii	"irq_count\000"
.LASF90:
	.ascii	"tsc_timestamp\000"
.LASF109:
	.ascii	"dom_phys_offset\000"
.LASF111:
	.ascii	"logbool_ht_set_addr\000"
.LASF230:
	.ascii	"fpinst2\000"
.LASF28:
	.ascii	"__initcall_driver_postcore_end\000"
.LASF171:
	.ascii	"irq_unmask\000"
.LASF76:
	.ascii	"slotID\000"
.LASF114:
	.ascii	"__per_cpu_offset\000"
.LASF242:
	.ascii	"scheduler\000"
.LASF189:
	.ascii	"CLOCK_EVT_MODE_ONESHOT\000"
.LASF106:
	.ascii	"printch\000"
.LASF244:
	.ascii	"domains\000"
.LASF185:
	.ascii	"cycle_last\000"
.LASF29:
	.ascii	"char\000"
.LASF180:
	.ascii	"rate\000"
.LASF170:
	.ascii	"irq_mask\000"
.LASF143:
	.ascii	"domain_id\000"
.LASF217:
	.ascii	"addrspace_t\000"
.LASF47:
	.ascii	"recurse_cpu\000"
.LASF126:
	.ascii	"data\000"
.LASF62:
	.ascii	"ME_state_migrating\000"
.LASF12:
	.ascii	"uint8_t\000"
.LASF8:
	.ascii	"__u32\000"
.LASF124:
	.ascii	"status\000"
.LASF187:
	.ascii	"CLOCK_EVT_MODE_SHUTDOWN\000"
.LASF236:
	.ascii	"interdomain\000"
.LASF110:
	.ascii	"pt_vaddr\000"
.LASF80:
	.ascii	"ME_desc_t\000"
.LASF206:
	.ascii	"timer\000"
.LASF104:
	.ascii	"hypercall_addr\000"
.LASF193:
	.ascii	"prescale\000"
.LASF9:
	.ascii	"long long int\000"
.LASF216:
	.ascii	"pgtable_vaddr\000"
.LASF190:
	.ascii	"clock_event_device\000"
.LASF42:
	.ascii	"lock\000"
.LASF72:
	.ascii	"valid\000"
.LASF22:
	.ascii	"padding\000"
.LASF248:
	.ascii	"/home/reds/soo/agency/avz\000"
.LASF150:
	.ascii	"max_pages\000"
.LASF35:
	.ascii	"BOOT_STAGE_IRQ_ENABLE\000"
.LASF17:
	.ascii	"bool\000"
.LASF86:
	.ascii	"injection_sem\000"
.LASF23:
	.ascii	"cpu_regs_t\000"
.LASF221:
	.ascii	"io_maplist\000"
.LASF155:
	.ascii	"processor\000"
.LASF39:
	.ascii	"origin_cpu\000"
.LASF87:
	.ascii	"shared_info\000"
.LASF77:
	.ascii	"size\000"
.LASF33:
	.ascii	"BOOT_STAGE_IRQ_INIT\000"
.LASF83:
	.ascii	"__xenomai_ready_to_go\000"
.LASF99:
	.ascii	"shared_info_t\000"
.LASF136:
	.ascii	"mask\000"
.LASF233:
	.ascii	"remote_dom\000"
.LASF251:
	.ascii	"__bad_xchg\000"
.LASF199:
	.ascii	"set_next_event\000"
.LASF151:
	.ascii	"evtchn\000"
.LASF232:
	.ascii	"remote_evtchn\000"
.LASF69:
	.ascii	"agencyUID_t\000"
.LASF191:
	.ascii	"features\000"
.LASF166:
	.ascii	"domain_stack\000"
.LASF247:
	.ascii	"arch/arm32/asm-offsets.c\000"
.LASF133:
	.ascii	"dev_id\000"
.LASF73:
	.ascii	"caps\000"
.LASF61:
	.ascii	"ME_state_suspended\000"
.LASF213:
	.ascii	"per_cpu__timer_deadline_end\000"
.LASF13:
	.ascii	"uint16_t\000"
.LASF24:
	.ascii	"pseudo_usr_mode\000"
.LASF71:
	.ascii	"agency_desc_t\000"
.LASF82:
	.ascii	"dom_desc_t\000"
.LASF138:
	.ascii	"startup\000"
.LASF10:
	.ascii	"__u64\000"
.LASF222:
	.ascii	"__irq_safe\000"
.LASF156:
	.ascii	"need_periodic_timer\000"
.LASF50:
	.ascii	"spinlock_t\000"
.LASF19:
	.ascii	"addr_t\000"
.LASF5:
	.ascii	"short int\000"
.LASF25:
	.ascii	"__initcall_driver_core\000"
.LASF16:
	.ascii	"long int\000"
.LASF201:
	.ascii	"event_handler\000"
.LASF112:
	.ascii	"start_info_t\000"
.LASF30:
	.ascii	"__printch\000"
.LASF159:
	.ascii	"runstate\000"
.LASF79:
	.ascii	"spad\000"
.LASF131:
	.ascii	"irqaction\000"
.LASF128:
	.ascii	"bound_domains\000"
.LASF107:
	.ascii	"store_mfn\000"
.LASF108:
	.ascii	"nr_pt_frames\000"
.LASF168:
	.ascii	"irq_enable\000"
.LASF113:
	.ascii	"avz_start_info\000"
.LASF231:
	.ascii	"remote_domid\000"
.LASF122:
	.ascii	"action\000"
.LASF15:
	.ascii	"uint64_t\000"
.LASF97:
	.ascii	"subdomain_shared_info\000"
.LASF115:
	.ascii	"__in_interrupt\000"
.LASF161:
	.ascii	"pause_flags\000"
.LASF214:
	.ascii	"ttbr0\000"
.LASF218:
	.ascii	"__sys_l1pgtable\000"
.LASF142:
	.ascii	"domain\000"
.LASF67:
	.ascii	"ME_state_t\000"
.LASF101:
	.ascii	"start_info\000"
.LASF132:
	.ascii	"name\000"
.LASF241:
	.ascii	"semaphore\000"
.LASF140:
	.ascii	"disable\000"
.LASF49:
	.ascii	"debug\000"
.LASF125:
	.ascii	"chipdata\000"
.LASF169:
	.ascii	"irq_disable\000"
.LASF237:
	.ascii	"virq\000"
.LASF164:
	.ascii	"virq_lock\000"
.LASF234:
	.ascii	"can_notify\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF6:
	.ascii	"__u8\000"
.LASF68:
	.ascii	"list\000"
.LASF239:
	.ascii	"DOMDYING_dying\000"
.LASF163:
	.ascii	"virq_to_evtchn\000"
.LASF198:
	.ascii	"__irqaction\000"
.LASF93:
	.ascii	"dc_event\000"
.LASF167:
	.ascii	"irq_desc\000"
.LASF200:
	.ascii	"set_mode\000"
.LASF37:
	.ascii	"boot_stage_t\000"
.LASF226:
	.ascii	"fpregs2\000"
.LASF176:
	.ascii	"cycle_t\000"
.LASF119:
	.ascii	"type\000"
.LASF100:
	.ascii	"HYPERVISOR_shared_info\000"
.LASF219:
	.ascii	"l2pt_current_base\000"
.LASF3:
	.ascii	"unsigned char\000"
.LASF57:
	.ascii	"dc_incoming_domID\000"
.LASF209:
	.ascii	"function\000"
.LASF88:
	.ascii	"evtchn_upcall_pending\000"
.LASF158:
	.ascii	"sched\000"
.LASF238:
	.ascii	"DOMDYING_alive\000"
.LASF194:
	.ascii	"max_delta_ns\000"
.LASF52:
	.ascii	"list_head\000"
.LASF75:
	.ascii	"state\000"
.LASF81:
	.ascii	"agency\000"
.LASF85:
	.ascii	"usr_feedback\000"
.LASF162:
	.ascii	"pause_count\000"
.LASF27:
	.ascii	"__initcall_driver_postcore\000"
.LASF94:
	.ascii	"clocksource_ref\000"
.LASF235:
	.ascii	"unbound\000"
.LASF147:
	.ascii	"addrspace\000"
.LASF4:
	.ascii	"signed char\000"
.LASF146:
	.ascii	"domcall\000"
.LASF2:
	.ascii	"short unsigned int\000"
.LASF45:
	.ascii	"lock_debug\000"
.LASF154:
	.ascii	"is_paused_by_controller\000"
.LASF250:
	.ascii	"main\000"
.LASF243:
	.ascii	"agency_rt_domain\000"
.LASF51:
	.ascii	"pen_release\000"
.LASF121:
	.ascii	"chip\000"
.LASF208:
	.ascii	"list_next\000"
.LASF207:
	.ascii	"expires\000"
.LASF245:
	.ascii	"per_cpu__current_domain\000"
.LASF103:
	.ascii	"nr_pages\000"
.LASF212:
	.ascii	"per_cpu__timer_deadline_start\000"
.LASF225:
	.ascii	"fpregs1\000"
.LASF175:
	.ascii	"per_cpu__intc_lock\000"
.LASF173:
	.ascii	"irq_ops_t\000"
.LASF20:
	.ascii	"sp_usr\000"
.LASF137:
	.ascii	"unmask\000"
.LASF224:
	.ascii	"vfp_state\000"
.LASF63:
	.ascii	"ME_state_dormant\000"
.LASF66:
	.ascii	"ME_state_dead\000"
.LASF92:
	.ascii	"evtchn_pending\000"
.LASF181:
	.ascii	"read\000"
.LASF95:
	.ascii	"clocksource_base\000"
.LASF157:
	.ascii	"oneshot_timer\000"
.LASF174:
	.ascii	"irq_ops\000"
.LASF96:
	.ascii	"dom_desc\000"
	.ident	"GCC: (GNU Toolchain for the A-profile Architecture 9.2-2019.12 (arm-9.10)) 9.2.1 20191025"
	.section	.note.GNU-stack,"",%progbits
