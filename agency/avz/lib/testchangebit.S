#include <asm/linkage.h>
#include <asm/processor.h>

.text

ENTRY(_test_and_change_bit_be)
		eor	r0, r0, #0x18		@ big endian byte ordering
ENTRY(_test_and_change_bit_le)
		add	r1, r1, r0, lsr #3
		and	r3, r0, #7
		mov	r0, #1
		save_and_disable_irqs ip
		ldrb	r2, [r1]
		tst	r2, r0, lsl r3
		eor	r2, r2, r0, lsl r3
		strb	r2, [r1]
		restore_irqs ip
		moveq	r0, #0
		RETINSTR(mov,pc,lr)


