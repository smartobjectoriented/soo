/*
 * Linker script for the avz hypervisor 
 */

OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)
ENTRY(_start)

/* The sizes of the stacks used by the application. */
 
/*
 * SVC stack is used for kernel activities *and* for each newly created thread.
 * Indeed a user thread (running in the user space) will also involve kernel activities
 * with syscalls and other.
 */
SVC_STACK_SIZE = 32*1024;  /* Initial system stack */
HEAP_SIZE = 8 * 1024 * 1024;    /* 8 MB dedicated to kernel heap */

/*
 * The system (1st-level) page table is located at 0xc000'4000 and occupies 16 KB (4096 32-bit entries)
 */ 
 
SECTIONS
{
   
     . = ENTRY_ADDR + 0x8000;    
       
    __vectors_start = .;

    .vectors.text :
    {
        *(.vectors.text); 
    }
    
  	__vectors_end = .;
  
    . = ALIGN(16384);
    
    .head.text :
    {
        *(.head.text);
    }

    .text :
    {
        *(.text*) 
    }

    . = ALIGN(4096);
    .data  : 
    { 
        *(.data*) 
    }

    . = ALIGN(4096);
 
    .bss : 
    {
       __bss_start = .;
        
        *(.bss) 
        
        . = ALIGN(4096);
        
    	__per_cpu_start = .;
    	
    	*(.bss.percpu)
    
     	. = ALIGN(128);
    	*(.bss.percpu.read_mostly)
    
     	. = ALIGN(128);
    	
    	__per_cpu_data_end = .;
       
     	. = ALIGN(128);
     	
     	__per_cpu_end = .;
   
    	__bss_end = .;  
    }
   
    . = ALIGN(4096);
 
    .heap :
    {
      __heap_base_addr = . ;

      . += HEAP_SIZE;
    }
    . = ALIGN(4096);

    __end = .;
}
