


ifneq ($(CONFIG_ARM64VT),y)
obj-y += head.o 
obj-y += hyp-stub.o
endif

obj-y += fault.o backtrace.o 
obj-y += backtrace.o backtrace_asm.o
obj-y += setup.o cache_v8.o cache.o context.o domain.o
obj-y += hypercalls.o
obj-y += smccc-call.o
obj-y += migration.o

obj-$(CONFIG_ARM64VT) += exception_arm64vt.o head_arm64vt.o mmu_arm64vt.o traps_arm64vt.o 

ifneq ($(CONFIG_ARM64VT),y)
obj-y += exception.o mmu.o traps.o
endif

obj-y += lib/

ifneq ($(CONFIG_ARM64VT),y)
obj-y += $(TARGET)/
endif
