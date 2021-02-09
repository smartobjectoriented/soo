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
@ GNU C89 (Linaro GCC 6.4-2018.05) version 6.4.1 20180425 [linaro-6.4-2018.05 revision 7b15d0869c096fe39603ad63dc19ab7cf035eb70] (arm-linux-gnueabihf)
@	compiled by GNU C version 4.8.4, GMP version 6.1.0, MPFR version 3.1.4, MPC version 1.0.3, isl version none
@ GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
@ options passed:  -nostdinc -I include -I . -I include -I .
@ -I ./lib/libfdt -I arch/arm/include/ -I arch/arm/so3virt/include/
@ -imultilib . -imultiarch arm-linux-gnueabihf
@ -iprefix /opt/toolchain/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf/bin/../lib/gcc/arm-linux-gnueabihf/6.4.1/
@ -isysroot /opt/toolchain/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf/bin/../arm-linux-gnueabihf/libc
@ -D __KERNEL__ -D __SO3__ -U arm -D BITS_PER_LONG=32 -D KBUILD_STR(s)=#s
@ -D KBUILD_BASENAME=KBUILD_STR(asm_offsets)
@ -include include/generated/autoconf.h
@ -isystem /opt/toolchain/gcc-linaro-6.4.1-2018.05-x86_64_arm-linux-gnueabihf/bin/../lib/gcc/arm-linux-gnueabihf/6.4.1/include
@ -MD arch/arm/.asm-offsets.s.d arch/arm/asm-offsets.c -mabi=aapcs-linux
@ -mlittle-endian -mabi=aapcs-linux -mno-thumb-interwork -mfpu=vfp -marm
@ -march=armv7-a -mtune=cortex-a9 -mfloat-abi=hard -mtls-dialect=gnu
@ -auxbase-strip arch/arm/asm-offsets.s -g -Wall -Wundef
@ -Wstrict-prototypes -Wno-trigraphs -Werror=implicit-function-declaration
@ -Wno-format-security -Wno-frame-address -Wframe-larger-than=1024
@ -Wno-unused-but-set-variable -Wunused-const-variable=0
@ -Wdeclaration-after-statement -Wno-pointer-sign -Werror=implicit-int
@ -Werror=strict-prototypes -Werror=date-time
@ -Werror=incompatible-pointer-types -Werror=designated-init -std=gnu90
@ -fno-builtin -ffreestanding -fno-strict-aliasing -fno-common -fno-PIE
@ -fno-dwarf2-cfi-asm -fno-ipa-sra -funwind-tables
@ -fno-delete-null-pointer-checks -fno-stack-protector -fomit-frame-pointer
@ -fno-var-tracking-assignments -fno-strict-overflow
@ -fno-merge-all-constants -fmerge-constants -fstack-check=no
@ -fconserve-stack -fno-function-sections -fno-data-sections -fverbose-asm
@ --param allow-store-data-races=0
@ options enabled:  -faggressive-loop-optimizations -fauto-inc-dec
@ -fchkp-check-incomplete-type -fchkp-check-read -fchkp-check-write
@ -fchkp-instrument-calls -fchkp-narrow-bounds -fchkp-optimize
@ -fchkp-store-bounds -fchkp-use-static-bounds
@ -fchkp-use-static-const-bounds -fchkp-use-wrappers -fearly-inlining
@ -feliminate-unused-debug-types -ffunction-cse -fgcse-lm -fgnu-runtime
@ -fgnu-unique -fident -finline-atomics -fira-hoist-pressure
@ -fira-share-save-slots -fira-share-spill-slots -fivopts
@ -fkeep-static-consts -fleading-underscore -flifetime-dse
@ -flto-odr-type-merging -fmath-errno -fmerge-constants
@ -fmerge-debug-strings -fomit-frame-pointer -fpeephole -fplt
@ -fprefetch-loop-arrays -freg-struct-return
@ -fsched-critical-path-heuristic -fsched-dep-count-heuristic
@ -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
@ -fsched-rank-heuristic -fsched-spec -fsched-spec-insn-heuristic
@ -fsched-stalled-insns-dep -fsemantic-interposition -fshow-column
@ -fsigned-zeros -fsplit-ivs-in-unroller -fssa-backprop -fstdarg-opt
@ -fstrict-volatile-bitfields -fsync-libcalls -ftrapping-math -ftree-cselim
@ -ftree-forwprop -ftree-loop-if-convert -ftree-loop-im -ftree-loop-ivcanon
@ -ftree-loop-optimize -ftree-parallelize-loops= -ftree-phiprop
@ -ftree-reassoc -ftree-scev-cprop -funit-at-a-time -funwind-tables
@ -fverbose-asm -fzero-initialized-in-bss -marm -mglibc -mlittle-endian
@ -mpic-data-is-text-relative -msched-prolog -munaligned-access
@ -mvectorize-with-neon-quad

	.text
.Ltext0:
	.align	2
	.global	main
	.syntax unified
	.arm
	.fpu vfp
	.type	main, %function
main:
	.fnstart
.LFB45:
	.file 1 "arch/arm/asm-offsets.c"
	.loc 1 51 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
	.loc 1 52 0
	.syntax divided
@ 52 "arch/arm/asm-offsets.c" 1
	
->
@ 0 "" 2
	.loc 1 54 0
@ 54 "arch/arm/asm-offsets.c" 1
	
->OFFSET_TCB_CPU_REGS #132 offsetof(tcb_t, cpu_regs)	@
@ 0 "" 2
	.loc 1 56 0
@ 56 "arch/arm/asm-offsets.c" 1
	
->
@ 0 "" 2
	.loc 1 58 0
@ 58 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R0 #0 offsetof(cpu_regs_t, r0)	@
@ 0 "" 2
	.loc 1 59 0
@ 59 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R1 #4 offsetof(cpu_regs_t, r1)	@
@ 0 "" 2
	.loc 1 60 0
@ 60 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R2 #8 offsetof(cpu_regs_t, r2)	@
@ 0 "" 2
	.loc 1 61 0
@ 61 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R3 #12 offsetof(cpu_regs_t, r3)	@
@ 0 "" 2
	.loc 1 62 0
@ 62 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R4 #16 offsetof(cpu_regs_t, r4)	@
@ 0 "" 2
	.loc 1 63 0
@ 63 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R5 #20 offsetof(cpu_regs_t, r5)	@
@ 0 "" 2
	.loc 1 64 0
@ 64 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R6 #24 offsetof(cpu_regs_t, r6)	@
@ 0 "" 2
	.loc 1 65 0
@ 65 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R7 #28 offsetof(cpu_regs_t, r7)	@
@ 0 "" 2
	.loc 1 66 0
@ 66 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R8 #32 offsetof(cpu_regs_t, r8)	@
@ 0 "" 2
	.loc 1 67 0
@ 67 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R9 #36 offsetof(cpu_regs_t, r9)	@
@ 0 "" 2
	.loc 1 68 0
@ 68 "arch/arm/asm-offsets.c" 1
	
->OFFSET_R10 #40 offsetof(cpu_regs_t, r10)	@
@ 0 "" 2
	.loc 1 69 0
@ 69 "arch/arm/asm-offsets.c" 1
	
->OFFSET_FP #44 offsetof(cpu_regs_t, fp)	@
@ 0 "" 2
	.loc 1 70 0
@ 70 "arch/arm/asm-offsets.c" 1
	
->OFFSET_IP #48 offsetof(cpu_regs_t, ip)	@
@ 0 "" 2
	.loc 1 71 0
@ 71 "arch/arm/asm-offsets.c" 1
	
->OFFSET_SP #52 offsetof(cpu_regs_t, sp)	@
@ 0 "" 2
	.loc 1 72 0
@ 72 "arch/arm/asm-offsets.c" 1
	
->OFFSET_LR #56 offsetof(cpu_regs_t, lr)	@
@ 0 "" 2
	.loc 1 73 0
@ 73 "arch/arm/asm-offsets.c" 1
	
->OFFSET_PC #60 offsetof(cpu_regs_t, pc)	@
@ 0 "" 2
	.loc 1 74 0
@ 74 "arch/arm/asm-offsets.c" 1
	
->OFFSET_PSR #64 offsetof(cpu_regs_t, psr)	@
@ 0 "" 2
	.loc 1 75 0
@ 75 "arch/arm/asm-offsets.c" 1
	
->OFFSET_SP_USR #68 offsetof(cpu_regs_t, sp_usr)	@
@ 0 "" 2
	.loc 1 76 0
@ 76 "arch/arm/asm-offsets.c" 1
	
->OFFSET_LR_USR #72 offsetof(cpu_regs_t, lr_usr)	@
@ 0 "" 2
	.loc 1 78 0
@ 78 "arch/arm/asm-offsets.c" 1
	
->
@ 0 "" 2
	.loc 1 80 0
@ 80 "arch/arm/asm-offsets.c" 1
	
->OFFSET_SYS_SIGNUM #0 offsetof(__sigaction_t, signum)	@
@ 0 "" 2
	.loc 1 81 0
@ 81 "arch/arm/asm-offsets.c" 1
	
->OFFSET_SYS_SA #4 offsetof(__sigaction_t, sa)	@
@ 0 "" 2
	.loc 1 83 0
@ 83 "arch/arm/asm-offsets.c" 1
	
->
@ 0 "" 2
	.loc 1 85 0
@ 85 "arch/arm/asm-offsets.c" 1
	
->OFFSET_SA_HANDLER #0 offsetof(sigaction_t, sa_handler)	@
@ 0 "" 2
	.loc 1 86 0
@ 86 "arch/arm/asm-offsets.c" 1
	
->OFFSET_SA_RESTORER #8 offsetof(sigaction_t, sa_restorer)	@
@ 0 "" 2
	.loc 1 88 0
	.arm
	.syntax unified
	mov	r3, #0	@ _1,
	.loc 1 89 0
	mov	r0, r3	@, <retval>
	bx	lr	@
.LFE45:
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
	.4byte	.LFB45
	.4byte	.LFE45-.LFB45
	.align	2
.LEFDE0:
	.text
.Letext0:
	.file 2 "arch/arm/include/asm/types.h"
	.file 3 "include/types.h"
	.file 4 "include/linker.h"
	.file 5 "include/common.h"
	.file 6 "arch/arm/include/asm/processor.h"
	.file 7 "include/list.h"
	.file 8 "arch/arm/include/asm/memory.h"
	.file 9 "include/schedule.h"
	.file 10 "include/thread.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x3b1
	.2byte	0x4
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF54
	.byte	0x1
	.4byte	.LASF55
	.4byte	.LASF56
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	.Ldebug_line0
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF1
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF2
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF3
	.uleb128 0x3
	.byte	0x4
	.byte	0x5
	.ascii	"int\000"
	.uleb128 0x4
	.4byte	.LASF6
	.byte	0x2
	.byte	0x26
	.4byte	0x53
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF4
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF5
	.uleb128 0x4
	.4byte	.LASF7
	.byte	0x2
	.byte	0x29
	.4byte	0x6c
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF8
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF9
	.uleb128 0x4
	.4byte	.LASF10
	.byte	0x3
	.byte	0x3f
	.4byte	0x48
	.uleb128 0x5
	.ascii	"u64\000"
	.byte	0x3
	.byte	0x43
	.4byte	0x61
	.uleb128 0x4
	.4byte	.LASF11
	.byte	0x3
	.byte	0xbb
	.4byte	0x33
	.uleb128 0x6
	.4byte	.LASF12
	.byte	0x4
	.byte	0x1b
	.4byte	0x73
	.uleb128 0x6
	.4byte	.LASF13
	.byte	0x4
	.byte	0x1b
	.4byte	0x73
	.uleb128 0x6
	.4byte	.LASF14
	.byte	0x4
	.byte	0x1b
	.4byte	0x73
	.uleb128 0x6
	.4byte	.LASF15
	.byte	0x4
	.byte	0x1b
	.4byte	0x73
	.uleb128 0x7
	.byte	0x4
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF16
	.uleb128 0x6
	.4byte	.LASF17
	.byte	0x5
	.byte	0x1c
	.4byte	0x7a
	.uleb128 0x8
	.byte	0x4
	.4byte	0x53
	.byte	0x5
	.byte	0x5d
	.4byte	0x100
	.uleb128 0x9
	.4byte	.LASF18
	.byte	0
	.uleb128 0x9
	.4byte	.LASF19
	.byte	0x1
	.uleb128 0x9
	.4byte	.LASF20
	.byte	0x2
	.uleb128 0x9
	.4byte	.LASF21
	.byte	0x3
	.byte	0
	.uleb128 0x4
	.4byte	.LASF22
	.byte	0x5
	.byte	0x5f
	.4byte	0xdb
	.uleb128 0x6
	.4byte	.LASF23
	.byte	0x5
	.byte	0x60
	.4byte	0x100
	.uleb128 0x6
	.4byte	.LASF24
	.byte	0x5
	.byte	0x65
	.4byte	0x7a
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF25
	.uleb128 0x6
	.4byte	.LASF26
	.byte	0x6
	.byte	0xbd
	.4byte	0x90
	.uleb128 0xa
	.4byte	.LASF31
	.byte	0x50
	.byte	0x6
	.byte	0xc5
	.4byte	0x221
	.uleb128 0xb
	.ascii	"r0\000"
	.byte	0x6
	.byte	0xc6
	.4byte	0x48
	.byte	0
	.uleb128 0xb
	.ascii	"r1\000"
	.byte	0x6
	.byte	0xc7
	.4byte	0x48
	.byte	0x4
	.uleb128 0xb
	.ascii	"r2\000"
	.byte	0x6
	.byte	0xc8
	.4byte	0x48
	.byte	0x8
	.uleb128 0xb
	.ascii	"r3\000"
	.byte	0x6
	.byte	0xc9
	.4byte	0x48
	.byte	0xc
	.uleb128 0xb
	.ascii	"r4\000"
	.byte	0x6
	.byte	0xca
	.4byte	0x48
	.byte	0x10
	.uleb128 0xb
	.ascii	"r5\000"
	.byte	0x6
	.byte	0xcb
	.4byte	0x48
	.byte	0x14
	.uleb128 0xb
	.ascii	"r6\000"
	.byte	0x6
	.byte	0xcc
	.4byte	0x48
	.byte	0x18
	.uleb128 0xb
	.ascii	"r7\000"
	.byte	0x6
	.byte	0xcd
	.4byte	0x48
	.byte	0x1c
	.uleb128 0xb
	.ascii	"r8\000"
	.byte	0x6
	.byte	0xce
	.4byte	0x48
	.byte	0x20
	.uleb128 0xb
	.ascii	"r9\000"
	.byte	0x6
	.byte	0xcf
	.4byte	0x48
	.byte	0x24
	.uleb128 0xb
	.ascii	"r10\000"
	.byte	0x6
	.byte	0xd0
	.4byte	0x48
	.byte	0x28
	.uleb128 0xb
	.ascii	"fp\000"
	.byte	0x6
	.byte	0xd1
	.4byte	0x48
	.byte	0x2c
	.uleb128 0xb
	.ascii	"ip\000"
	.byte	0x6
	.byte	0xd2
	.4byte	0x48
	.byte	0x30
	.uleb128 0xb
	.ascii	"sp\000"
	.byte	0x6
	.byte	0xd3
	.4byte	0x48
	.byte	0x34
	.uleb128 0xb
	.ascii	"lr\000"
	.byte	0x6
	.byte	0xd4
	.4byte	0x48
	.byte	0x38
	.uleb128 0xb
	.ascii	"pc\000"
	.byte	0x6
	.byte	0xd5
	.4byte	0x48
	.byte	0x3c
	.uleb128 0xb
	.ascii	"psr\000"
	.byte	0x6
	.byte	0xd6
	.4byte	0x48
	.byte	0x40
	.uleb128 0xc
	.4byte	.LASF27
	.byte	0x6
	.byte	0xd7
	.4byte	0x48
	.byte	0x44
	.uleb128 0xc
	.4byte	.LASF28
	.byte	0x6
	.byte	0xd8
	.4byte	0x48
	.byte	0x48
	.uleb128 0xc
	.4byte	.LASF29
	.byte	0x6
	.byte	0xd9
	.4byte	0x48
	.byte	0x4c
	.byte	0
	.uleb128 0x4
	.4byte	.LASF30
	.byte	0x6
	.byte	0xda
	.4byte	0x133
	.uleb128 0xa
	.4byte	.LASF32
	.byte	0x8
	.byte	0x7
	.byte	0x31
	.4byte	0x251
	.uleb128 0xc
	.4byte	.LASF33
	.byte	0x7
	.byte	0x32
	.4byte	0x251
	.byte	0
	.uleb128 0xc
	.4byte	.LASF34
	.byte	0x7
	.byte	0x32
	.4byte	0x251
	.byte	0x4
	.byte	0
	.uleb128 0xd
	.byte	0x4
	.4byte	0x22c
	.uleb128 0x6
	.4byte	.LASF35
	.byte	0x8
	.byte	0x2a
	.4byte	0x7a
	.uleb128 0x6
	.4byte	.LASF36
	.byte	0x8
	.byte	0x34
	.4byte	0x26d
	.uleb128 0xd
	.byte	0x4
	.4byte	0x7a
	.uleb128 0x6
	.4byte	.LASF37
	.byte	0x9
	.byte	0x1c
	.4byte	0x85
	.uleb128 0x6
	.4byte	.LASF38
	.byte	0x9
	.byte	0x1d
	.4byte	0x85
	.uleb128 0xe
	.ascii	"tcb\000"
	.byte	0xd4
	.byte	0xa
	.byte	0x36
	.4byte	0x332
	.uleb128 0xb
	.ascii	"tid\000"
	.byte	0xa
	.byte	0x37
	.4byte	0x41
	.byte	0
	.uleb128 0xc
	.4byte	.LASF39
	.byte	0xa
	.byte	0x38
	.4byte	0x374
	.byte	0x4
	.uleb128 0xc
	.4byte	.LASF40
	.byte	0xa
	.byte	0x3d
	.4byte	0x7a
	.byte	0x54
	.uleb128 0xc
	.4byte	.LASF41
	.byte	0xa
	.byte	0x40
	.4byte	0x393
	.byte	0x58
	.uleb128 0xc
	.4byte	.LASF42
	.byte	0xa
	.byte	0x41
	.4byte	0xc7
	.byte	0x5c
	.uleb128 0xc
	.4byte	.LASF43
	.byte	0xa
	.byte	0x43
	.4byte	0x34e
	.byte	0x60
	.uleb128 0xc
	.4byte	.LASF44
	.byte	0xa
	.byte	0x44
	.4byte	0x41
	.byte	0x64
	.uleb128 0xb
	.ascii	"pcb\000"
	.byte	0xa
	.byte	0x47
	.4byte	0x399
	.byte	0x68
	.uleb128 0xc
	.4byte	.LASF45
	.byte	0xa
	.byte	0x48
	.4byte	0x41
	.byte	0x6c
	.uleb128 0xc
	.4byte	.LASF46
	.byte	0xa
	.byte	0x4a
	.4byte	0x41
	.byte	0x70
	.uleb128 0xc
	.4byte	.LASF47
	.byte	0xa
	.byte	0x4c
	.4byte	0x22c
	.byte	0x74
	.uleb128 0xc
	.4byte	.LASF48
	.byte	0xa
	.byte	0x4f
	.4byte	0x22c
	.byte	0x7c
	.uleb128 0xc
	.4byte	.LASF31
	.byte	0xa
	.byte	0x51
	.4byte	0x221
	.byte	0x84
	.byte	0
	.uleb128 0x6
	.4byte	.LASF49
	.byte	0x9
	.byte	0x21
	.4byte	0x33d
	.uleb128 0xd
	.byte	0x4
	.4byte	0x289
	.uleb128 0x6
	.4byte	.LASF50
	.byte	0x9
	.byte	0x36
	.4byte	0x33d
	.uleb128 0x4
	.4byte	.LASF51
	.byte	0xa
	.byte	0x29
	.4byte	0x53
	.uleb128 0x6
	.4byte	.LASF52
	.byte	0xa
	.byte	0x2b
	.4byte	0x53
	.uleb128 0x4
	.4byte	.LASF53
	.byte	0xa
	.byte	0x30
	.4byte	0x36f
	.uleb128 0xf
	.ascii	"pcb\000"
	.uleb128 0x10
	.4byte	0xc9
	.4byte	0x384
	.uleb128 0x11
	.4byte	0x121
	.byte	0x4f
	.byte	0
	.uleb128 0x12
	.4byte	0x41
	.4byte	0x393
	.uleb128 0x13
	.4byte	0xc7
	.byte	0
	.uleb128 0xd
	.byte	0x4
	.4byte	0x384
	.uleb128 0xd
	.byte	0x4
	.4byte	0x364
	.uleb128 0x14
	.4byte	.LASF57
	.byte	0x1
	.byte	0x32
	.4byte	0x41
	.4byte	.LFB45
	.4byte	.LFE45-.LFB45
	.uleb128 0x1
	.byte	0x9c
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
	.uleb128 0x4
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x4
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xb
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
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x13
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x12
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
	.uleb128 0x13
	.uleb128 0x5
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x14
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
.LASF51:
	.ascii	"thread_t\000"
.LASF36:
	.ascii	"__sys_l1pgtable\000"
.LASF14:
	.ascii	"__initcall_driver_postcore\000"
.LASF54:
	.ascii	"GNU C89 6.4.1 20180425 [linaro-6.4-2018.05 revision"
	.ascii	" 7b15d0869c096fe39603ad63dc19ab7cf035eb70] -mabi=aa"
	.ascii	"pcs-linux -mlittle-endian -mabi=aapcs-linux -mno-th"
	.ascii	"umb-interwork -mfpu=vfp -marm -march=armv7-a -mtune"
	.ascii	"=cortex-a9 -mfloat-abi=hard -mtls-dialect=gnu -g -s"
	.ascii	"td=gnu90 -fno-builtin -ffreestanding -fno-strict-al"
	.ascii	"iasing -fno-common -fno-PIE -fno-dwarf2-cfi-asm -fn"
	.ascii	"o-ipa-sra -funwind-tables -fno-delete-null-pointer-"
	.ascii	"checks -fno-stack-protector -fomit-frame-pointer -f"
	.ascii	"no-var-tracking-assignments -fno-strict-overflow -f"
	.ascii	"no-merge-all-constants -fmerge-constants -fstack-ch"
	.ascii	"eck=no -fconserve-stack -fno-function-sections -fno"
	.ascii	"-data-sections --param allow-store-data-races=0\000"
.LASF17:
	.ascii	"__end\000"
.LASF32:
	.ascii	"list_head\000"
.LASF7:
	.ascii	"__u64\000"
.LASF46:
	.ascii	"exit_status\000"
.LASF23:
	.ascii	"boot_stage\000"
.LASF43:
	.ascii	"state\000"
.LASF3:
	.ascii	"short int\000"
.LASF25:
	.ascii	"sizetype\000"
.LASF50:
	.ascii	"current_thread\000"
.LASF57:
	.ascii	"main\000"
.LASF18:
	.ascii	"BOOT_STAGE_INIT\000"
.LASF12:
	.ascii	"__initcall_driver_core\000"
.LASF22:
	.ascii	"boot_stage_t\000"
.LASF6:
	.ascii	"__u32\000"
.LASF45:
	.ascii	"pcb_stack_slotID\000"
.LASF11:
	.ascii	"bool\000"
.LASF28:
	.ascii	"lr_usr\000"
.LASF5:
	.ascii	"long long int\000"
.LASF13:
	.ascii	"__initcall_driver_core_end\000"
.LASF52:
	.ascii	"__stack_top\000"
.LASF39:
	.ascii	"name\000"
.LASF53:
	.ascii	"pcb_t\000"
.LASF30:
	.ascii	"cpu_regs_t\000"
.LASF2:
	.ascii	"unsigned char\000"
.LASF1:
	.ascii	"signed char\000"
.LASF8:
	.ascii	"long long unsigned int\000"
.LASF10:
	.ascii	"uint32_t\000"
.LASF4:
	.ascii	"unsigned int\000"
.LASF35:
	.ascii	"avz_guest_phys_offset\000"
.LASF55:
	.ascii	"arch/arm/asm-offsets.c\000"
.LASF0:
	.ascii	"short unsigned int\000"
.LASF15:
	.ascii	"__initcall_driver_postcore_end\000"
.LASF16:
	.ascii	"char\000"
.LASF47:
	.ascii	"list\000"
.LASF38:
	.ascii	"jiffies_ref\000"
.LASF29:
	.ascii	"padding\000"
.LASF42:
	.ascii	"th_arg\000"
.LASF44:
	.ascii	"stack_slotID\000"
.LASF9:
	.ascii	"long unsigned int\000"
.LASF24:
	.ascii	"origin_cpu\000"
.LASF49:
	.ascii	"tcb_idle\000"
.LASF37:
	.ascii	"jiffies\000"
.LASF19:
	.ascii	"BOOT_STAGE_IRQ_INIT\000"
.LASF56:
	.ascii	"/home/david/Projects/SOO/me-align/ME/SOO.outdoor/so"
	.ascii	"3\000"
.LASF26:
	.ascii	"in_upcall_progress\000"
.LASF31:
	.ascii	"cpu_regs\000"
.LASF41:
	.ascii	"th_fn\000"
.LASF27:
	.ascii	"sp_usr\000"
.LASF20:
	.ascii	"BOOT_STAGE_SCHED\000"
.LASF34:
	.ascii	"prev\000"
.LASF48:
	.ascii	"joinQueue\000"
.LASF21:
	.ascii	"BOOT_STAGE_IRQ_ENABLE\000"
.LASF40:
	.ascii	"prio\000"
.LASF33:
	.ascii	"next\000"
	.ident	"GCC: (Linaro GCC 6.4-2018.05) 6.4.1 20180425 [linaro-6.4-2018.05 revision 7b15d0869c096fe39603ad63dc19ab7cf035eb70]"
	.section	.note.GNU-stack,"",%progbits
