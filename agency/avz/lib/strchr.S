#include <asm/linkage.h>
#include <asm/processor.h>

		.text
		.align	5
ENTRY(strchr)
		and	r1, r1, #0xff
1:		ldrb	r2, [r0], #1
		teq	r2, r1
		teqne	r2, #0
		bne	1b
		teq	r2, r1
		movne	r0, #0
		subeq	r0, r0, #1
		RETINSTR(mov,pc,lr)
