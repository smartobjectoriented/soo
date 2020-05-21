/*
 * Copyright (C) 2014-2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
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


#include <avz/init.h>
#include <avz/config.h>
#include <avz/percpu.h>
#include <avz/mm.h>
#include <avz/kernel.h>

#include <asm/mach-types.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/current.h>
#include <asm/config.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/domain.h>
#include <asm/mmu.h>

#include <asm/system.h>
#include <asm/memslot.h>

#include <avz/sched.h>

extern pde_t swapper_pg_dir[__L2_PAGETABLE_ENTRIES];
extern void *vectors_page;

#ifdef CONFIG_CPU_CP15_MMU
unsigned long __init __clear_cr(unsigned long mask)
{
	cr_alignment = cr_alignment & ~mask;
	return cr_alignment;
}
#endif

/*
 * empty_zero_page is a special page that is used for
 * zero-initialized data and COW.
 */
struct page *empty_zero_page;

/* May be used by arch-specific code */
struct meminfo meminfo;

/*
 * The pmd table for the upper-most set of pages.
 */
pde_t *top_pmd;


extern void arch_map_io(void);
extern void cpu_flush_tlb_all(void);

#define CPOLICY_UNCACHED	0
#define CPOLICY_BUFFERED	1
#define CPOLICY_WRITETHROUGH	2
#define CPOLICY_WRITEBACK	3
#define CPOLICY_WRITEALLOC	4

static unsigned int cachepolicy __initdata = CPOLICY_WRITEBACK;
static unsigned int ecc_mask __initdata = 0;
pgprot_t pgprot_user;
pgprot_t pgprot_kernel;

pgprot_t protection_map[16] = {
   __P000, __P001, __P010, __P011, __P100, __P101, __P110, __P111,
   __S000, __S001, __S010, __S011, __S100, __S101, __S110, __S111
};

#define PROT_PTE_DEVICE		L_PTE_PRESENT|L_PTE_YOUNG|L_PTE_DIRTY|L_PTE_XN
#define PROT_SECT_DEVICE	PMD_TYPE_SECT|PMD_SECT_AP_WRITE

static struct mem_type mem_types[] = {
			[MT_DEVICE] = {		  /* Strongly ordered / ARMv6 shared device */
				.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_SHARED | L_PTE_SHARED,
				.prot_l1	= PMD_TYPE_TABLE,
				.prot_sect	= PROT_SECT_DEVICE | PMD_SECT_S,
				.domain		= DOMAIN_IO,
			},
			[MT_DEVICE_NONSHARED] = { /* ARMv6 non-shared device */
				.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_NONSHARED,
				.prot_l1	= PMD_TYPE_TABLE,
				.prot_sect	= PROT_SECT_DEVICE,
				.domain		= DOMAIN_IO,
			},
			[MT_DEVICE_CACHED] = {	  /* ioremap_cached */
				.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_CACHED,
				.prot_l1	= PMD_TYPE_TABLE,
				.prot_sect	= PROT_SECT_DEVICE | PMD_SECT_WB,
				.domain		= DOMAIN_IO,
			},
			[MT_DEVICE_WC] = {	/* ioremap_wc */
				.prot_pte	= PROT_PTE_DEVICE | L_PTE_MT_DEV_WC,
				.prot_l1	= PMD_TYPE_TABLE,
				.prot_sect	= PROT_SECT_DEVICE,
				.domain		= DOMAIN_IO,
			},
			[MT_UNCACHED] = {
				.prot_pte	= PROT_PTE_DEVICE,
				.prot_l1	= PMD_TYPE_TABLE,
				.prot_sect	= PMD_TYPE_SECT | PMD_SECT_XN,
				.domain		= DOMAIN_IO,
			},
			[MT_CACHECLEAN] = {
				.prot_sect = PMD_TYPE_SECT | PMD_SECT_XN,
				.domain    = DOMAIN_KERNEL,
			},
		#ifndef CONFIG_ARM_LPAE
			[MT_MINICLEAN] = {
				.prot_sect = PMD_TYPE_SECT | PMD_SECT_XN | PMD_SECT_MINICACHE,
				.domain    = DOMAIN_KERNEL,
			},
		#endif
			[MT_LOW_VECTORS] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
						L_PTE_RDONLY,
				.prot_l1   = PMD_TYPE_TABLE,
				.domain    = DOMAIN_USER,
			},
			[MT_HIGH_VECTORS] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_USER | L_PTE_SHARED | L_PTE_RDONLY,
				.prot_l1   = PMD_TYPE_TABLE,
				.domain    = DOMAIN_USER,
			},
			[MT_MEMORY_RWX] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_SHARED,
				.prot_l1   = PMD_TYPE_TABLE,
				.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_S,
				.domain    = DOMAIN_KERNEL,
			},
			[MT_MEMORY_RW] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_XN | L_PTE_SHARED,
				.prot_l1   = PMD_TYPE_TABLE,
				.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_S,
				.domain    = DOMAIN_KERNEL,
			},
			[MT_ROM] = {
				.prot_sect = PMD_TYPE_SECT,
				.domain    = DOMAIN_KERNEL,
			},
			[MT_MEMORY_RWX_NONCACHED] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
						L_PTE_MT_BUFFERABLE | L_PTE_SHARED, 
				.prot_l1   = PMD_TYPE_TABLE,
				.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_S, 
				.domain    = DOMAIN_KERNEL,
			},
			[MT_MEMORY_RW_DTCM] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
						L_PTE_XN,
				.prot_l1   = PMD_TYPE_TABLE,
				.prot_sect = PMD_TYPE_SECT | PMD_SECT_XN,
				.domain    = DOMAIN_KERNEL,
			},
			[MT_MEMORY_RWX_ITCM] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY,
				.prot_l1   = PMD_TYPE_TABLE,
				.domain    = DOMAIN_KERNEL,
			},
			[MT_MEMORY_RW_SO] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
						L_PTE_MT_UNCACHED | L_PTE_XN,
				.prot_l1   = PMD_TYPE_TABLE,
				.prot_sect = PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_S |
						PMD_SECT_UNCACHED | PMD_SECT_XN,
				.domain    = DOMAIN_KERNEL,
			},
			[MT_MEMORY_DMA_READY] = {
				.prot_pte  = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY |
						L_PTE_XN,
				.prot_l1   = PMD_TYPE_TABLE,
				.domain    = DOMAIN_KERNEL,
			},
};

const struct mem_type *get_mem_type(unsigned int type)
{
	return type < ARRAY_SIZE(mem_types) ? &mem_types[type] : NULL;
}

struct cachepolicy {
	const char	policy[16];
	unsigned int	cr_mask;
	unsigned int	pmd;
	unsigned int	pte;
};

static struct cachepolicy cache_policies[] __initdata = {
	{
		.policy		= "uncached",
		.cr_mask	= CR_W|CR_C,
		.pmd		= PMD_SECT_UNCACHED,
		.pte		= L_PTE_MT_UNCACHED,
	}, {
		.policy		= "buffered",
		.cr_mask	= CR_C,
		.pmd		= PMD_SECT_BUFFERED,
		.pte		= L_PTE_MT_BUFFERABLE,
	}, {
		.policy		= "writethrough",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WT,
		.pte		= L_PTE_MT_WRITETHROUGH,
	}, {
		.policy		= "writeback",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WB,
		.pte		= L_PTE_MT_WRITEBACK,
	}, {
		.policy		= "writealloc",
		.cr_mask	= 0,
		.pmd		= PMD_SECT_WBWA,
		.pte		= L_PTE_MT_WRITEALLOC,
	}
};

static unsigned long initial_pmd_value __initdata = 0;

/*
 * Initialise the cache_policy variable with the initial state specified
 * via the "pmd" value.  This is used to ensure that on ARMv6 and later,
 * the C code sets the page tables up with the same policy as the head
 * assembly code, which avoids an illegal state where the TLBs can get
 * confused.  See comments in early_cachepolicy() for more information.
 */
void __init init_default_cache_policy(unsigned long pmd)
{
	int i;

	initial_pmd_value = pmd;

	pmd &= PMD_SECT_TEX(1) | PMD_SECT_BUFFERABLE | PMD_SECT_CACHEABLE;

	for (i = 0; i < ARRAY_SIZE(cache_policies); i++)
		if (cache_policies[i].pmd == pmd) {
			cachepolicy = i;
			break;
		}

	if (i == ARRAY_SIZE(cache_policies))
		printk("ERROR: could not find cache policy\n");
}

void adjust_cr(unsigned long mask, unsigned long set)
{
	unsigned long flags;

	mask &= ~CR_A;

	set &= mask;

	local_irq_save(flags);

	cr_no_alignment = (cr_no_alignment & ~mask) | set;
	cr_alignment = (cr_alignment & ~mask) | set;

	set_cr((get_cr() & ~mask) | set);

	local_irq_restore(flags);
}

/*
 * Adjust the PMD section entries according to the CPU in use.
 */
static void __init build_mem_type_table(void)
{
	struct cachepolicy *cp;
	unsigned int cr = get_cr();
	pteval_t user_pgprot, kern_pgprot, vecs_pgprot;
	int cpu_arch = cpu_architecture();
	int i;

	if (is_smp()) {
		if (cachepolicy != CPOLICY_WRITEALLOC) {
			printk("Forcing write-allocate cache policy for SMP\n");
			cachepolicy = CPOLICY_WRITEALLOC;
		}
		if (!(initial_pmd_value & PMD_SECT_S)) {
			printk("Forcing shared mappings for SMP\n");
			initial_pmd_value |= PMD_SECT_S;
		}
	}

	/*
	 * Mark the device areas according to the CPU/architecture.
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6 && (cr & CR_XP)) {

		if (cpu_arch >= CPU_ARCH_ARMv7 && (cr & CR_TRE)) {
			/*
			 * For ARMv7 with TEX remapping,
			 * - shared device is SXCB=1100
			 * - nonshared device is SXCB=0100
			 * - write combine device mem is SXCB=0001
			 * (Uncached Normal memory)
			 */
			mem_types[MT_DEVICE].prot_sect |= PMD_SECT_TEX(1);
			mem_types[MT_DEVICE_NONSHARED].prot_sect |= PMD_SECT_TEX(1);
			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_BUFFERABLE;
		} else {
			/*
			 * For ARMv6 and ARMv7 without TEX remapping,
			 * - shared device is TEXCB=00001
			 * - nonshared device is TEXCB=01000
			 * - write combine device mem is TEXCB=00100
			 * (Uncached Normal in ARMv6 parlance).
			 */
			mem_types[MT_DEVICE].prot_sect |= PMD_SECT_BUFFERED;
			mem_types[MT_DEVICE_NONSHARED].prot_sect |= PMD_SECT_TEX(2);
			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_TEX(1);
		}
	} else {
		/*
		 * On others, write combining is "Uncached/Buffered"
		 */
		mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_BUFFERABLE;
	}

	/*
	 * Now deal with the memory-type mappings
	 */
	cp = &cache_policies[cachepolicy];
	vecs_pgprot = kern_pgprot = user_pgprot = cp->pte;

	/*
	 * We don't use domains on ARMv6 (since this causes problems with
	 * v6/v7 kernels), so we must use a separate memory type for user
	 * r/o, kernel r/w to map the vectors page.
	 */
#ifndef CONFIG_ARM_LPAE
	if (cpu_arch == CPU_ARCH_ARMv6)
		vecs_pgprot |= L_PTE_MT_VECTORS;
#endif

	/*
	 * ARMv6 and above have extended page tables.
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6 && (cr & CR_XP)) {
#ifndef CONFIG_ARM_LPAE
		/*
		 * Mark cache clean areas and XIP ROM read only
		 * from SVC mode and no access from userspace.
		 */
		mem_types[MT_ROM].prot_sect |= PMD_SECT_APX|PMD_SECT_AP_WRITE;
		mem_types[MT_MINICLEAN].prot_sect |= PMD_SECT_APX|PMD_SECT_AP_WRITE;
		mem_types[MT_CACHECLEAN].prot_sect |= PMD_SECT_APX|PMD_SECT_AP_WRITE;
#endif

		/*
		 * If the initial page tables were created with the S bit
		 * set, then we need to do the same here for the same
		 * reasons given in early_cachepolicy().
		 */
		if (initial_pmd_value & PMD_SECT_S) {
			user_pgprot |= L_PTE_SHARED;
			kern_pgprot |= L_PTE_SHARED;
			vecs_pgprot |= L_PTE_SHARED;

			mem_types[MT_DEVICE_WC].prot_sect |= PMD_SECT_S;
			mem_types[MT_DEVICE_WC].prot_pte |= L_PTE_SHARED;
			mem_types[MT_DEVICE_CACHED].prot_sect |= PMD_SECT_S;
			mem_types[MT_DEVICE_CACHED].prot_pte |= L_PTE_SHARED;
			mem_types[MT_MEMORY_RWX].prot_sect |= PMD_SECT_S;
			mem_types[MT_MEMORY_RWX].prot_pte |= L_PTE_SHARED;
			mem_types[MT_MEMORY_RW].prot_sect |= PMD_SECT_S;
			mem_types[MT_MEMORY_RW].prot_pte |= L_PTE_SHARED;
			mem_types[MT_MEMORY_DMA_READY].prot_pte |= L_PTE_SHARED;
			mem_types[MT_MEMORY_RWX_NONCACHED].prot_sect |= PMD_SECT_S;
			mem_types[MT_MEMORY_RWX_NONCACHED].prot_pte |= L_PTE_SHARED;
		}
	}

	/*
	 * Non-cacheable Normal - intended for memory areas that must
	 * not cause dirty cache line writebacks when used
	 */
	if (cpu_arch >= CPU_ARCH_ARMv6) {
		if (cpu_arch >= CPU_ARCH_ARMv7 && (cr & CR_TRE)) {
			/* Non-cacheable Normal is XCB = 001 */
			mem_types[MT_MEMORY_RWX_NONCACHED].prot_sect |=
				PMD_SECT_BUFFERED;
		} else {
			/* For both ARMv6 and non-TEX-remapping ARMv7 */
			mem_types[MT_MEMORY_RWX_NONCACHED].prot_sect |=
				PMD_SECT_TEX(1);
		}
	} else {
		mem_types[MT_MEMORY_RWX_NONCACHED].prot_sect |= PMD_SECT_BUFFERABLE;
	}

	for (i = 0; i < 16; i++) {
		pteval_t v = pgprot_val(protection_map[i]);
		protection_map[i] = __pgprot(v | user_pgprot);
	}

	mem_types[MT_LOW_VECTORS].prot_pte |= vecs_pgprot;
	mem_types[MT_HIGH_VECTORS].prot_pte |= vecs_pgprot;

	pgprot_user   = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG | user_pgprot);
	pgprot_kernel = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY | kern_pgprot);


	mem_types[MT_LOW_VECTORS].prot_l1 |= ecc_mask;
	mem_types[MT_HIGH_VECTORS].prot_l1 |= ecc_mask;
	mem_types[MT_MEMORY_RWX].prot_sect |= ecc_mask | cp->pmd;
	mem_types[MT_MEMORY_RWX].prot_pte |= kern_pgprot;
	mem_types[MT_MEMORY_RW].prot_sect |= ecc_mask | cp->pmd;
	mem_types[MT_MEMORY_RW].prot_pte |= kern_pgprot;
	mem_types[MT_MEMORY_DMA_READY].prot_pte |= kern_pgprot;
	mem_types[MT_MEMORY_RWX_NONCACHED].prot_sect |= ecc_mask;
	mem_types[MT_ROM].prot_sect |= cp->pmd;

	switch (cp->pmd) {
	case PMD_SECT_WT:
		mem_types[MT_CACHECLEAN].prot_sect |= PMD_SECT_WT;
		break;
	case PMD_SECT_WB:
	case PMD_SECT_WBWA:
		mem_types[MT_CACHECLEAN].prot_sect |= PMD_SECT_WB;
		break;
	}
	printk("Memory policy: %sData cache %s\n", ecc_mask ? "ECC enabled, " : "", cp->policy);

	for (i = 0; i < ARRAY_SIZE(mem_types); i++) {
		struct mem_type *t = &mem_types[i];
		if (t->prot_l1)
			t->prot_l1 |= PMD_DOMAIN(t->domain);
		if (t->prot_sect)
			t->prot_sect |= PMD_DOMAIN(t->domain);
	}
}

static void __init alloc_init_pte(pmd_t *pmd, unsigned long addr,
				  unsigned long end, unsigned long pfn,
				  const struct mem_type *type)
{
	pte_t *pte;

	if (pmd_none(*pmd)) {
		pte = get_l2_pgtable();

		memset(pte, 0, PAGE_SIZE);

		__pmd_populate(pmd, __pa(pte) | type->prot_l1);
	}

	pte = pte_offset_kernel((pde_t *)pmd, addr);

	do {
		set_pte_ext(pte, pfn_pte(pfn, __pgprot(type->prot_pte)), 0);
		pfn++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
}

static void __init alloc_init_section(pde_t *pde, unsigned long addr,
				      unsigned long end, unsigned long phys,
				      const struct mem_type *type)
{
	pmd_t *pmd = (pmd_t *) pmd_offset(pde);

	/*
	 * Try a section mapping - end, addr and phys must all be aligned
	 * to a section boundary.  Note that PMDs refer to the individual
	 * L1 entries, whereas PGDs refer to a group of L1 entries making
	 * up one logical pointer to an L2 table.
	 */

	if (((addr | end | phys) & ~SECTION_MASK) == 0) {
		pmd_t *p = pmd;

		do {
			*pmd = __pmd(phys | type->prot_sect);

			phys += SECTION_SIZE;
		} while (pmd++, addr += SECTION_SIZE, addr != end);

		flush_pmd_entry((pde_t *) p);
	} else {
		/*
		 * No need to loop; pte's aren't interested in the
		 * individual L1 entries.
		 */

		alloc_init_pte(pmd, addr, end, __phys_to_pfn(phys), type);
	}
}

/*
 * Create the page directory entries and any necessary
 * page tables for the mapping specified by `md'.  We
 * are able to cope here with varying sizes and address
 * offsets, and we take full advantage of sections and
 * supersections.
 */
void create_mapping(struct map_desc *md, pde_t *pgd)
{
	unsigned long phys, addr, length, end;
	const struct mem_type *type;
	pde_t *pde;

	type = &mem_types[md->type];

	addr = md->virtual & PAGE_MASK;
	phys = (unsigned long)__pfn_to_phys(md->pfn);
	length = PAGE_ALIGN(md->length + (md->virtual & ~PAGE_MASK));

	if (type->prot_l1 == 0 && ((addr | phys | length) & ~SECTION_MASK)) {
		printk(KERN_WARNING "BUG: map for 0x%08lx at 0x%08lx can not "
		       "be mapped using pages, ignoring.\n",
		       __pfn_to_phys(md->pfn), addr);
		return;
	}

	if (pgd == NULL)
	  pde = pde_offset_k(addr);
	else
	  pde = pgd_offset_priv(pgd, addr);

	end = addr + length;

	do {
		unsigned long next = pgd_addr_end(addr, end);

		alloc_init_section(pde, addr, next, phys, type);

		phys += next - addr;
		addr = next;
	} while (pde++, addr != end);
}


/*
 * Set up device the mappings.  Since we clear out the page tables for all
 * mappings above VMALLOC_END, we will remove any debug device mappings.
 * This means you have to be careful how you debug this function, or any
 * called function.  This means you can't use any function or debugging
 * method which may touch any device, otherwise the kernel _will_ crash.
 */
static void __init devicemaps_init(void)
{

	unsigned long addr;
	pde_t *pde;
	struct map_desc map;

	/*
	 * Allocate the vector page early.
	 */
	vectors_page = alloc_heap_page();
	BUG_ON(!vectors_page);

	/* Be careful NOT to erase our hypervisor (0xFF000000) */

	for (addr = VMALLOC_END; addr < HYPERVISOR_VIRT_START; addr += (1 << PGD_SHIFT)) {
		pde = (pde_t *) (swapper_pg_dir + pde_index(addr));
		pmd_clear(pde);
	}

	/* Clear the vector region */
	addr = VECTORS_BASE;
	pde = (pde_t *) (swapper_pg_dir + pde_index(addr));
	pmd_clear(pde);

	/*
	 * Create a mapping for the machine vectors at the high-vectors
	 * location (0xffff0000).  If we aren't using high-vectors, also
	 * create a mapping at the low-vectors virtual address.
	 */
	map.pfn = __phys_to_pfn(virt_to_phys(vectors_page));
	map.virtual = VECTORS_BASE;
	map.length = PAGE_SIZE;
	map.type = MT_HIGH_VECTORS;
	create_mapping(&map, NULL);

	flush_all();

}

extern unsigned long io_map_current_base;

void bootmem_init(void)
{
	struct map_desc map;

	/* Readjust PTEs with correct bits according to the CPU type */
	map.pfn = __phys_to_pfn(PHYS_OFFSET);
	map.virtual = HYPERVISOR_VIRT_START;
	map.length = io_map_current_base - HYPERVISOR_VIRT_START;
	map.type = MT_MEMORY_RWX;

	create_mapping(&map, NULL);

	/* Linux-like linear mapping of the whole RAM */

	map.pfn     = __phys_to_pfn(CONFIG_RAM_BASE);
	map.virtual = L_PAGE_OFFSET;

	map.length  = ((CONFIG_RAM_SIZE > KERNEL_LINEAR_MAX_SIZE) ? KERNEL_LINEAR_MAX_SIZE : CONFIG_RAM_SIZE);

	map.type    = MT_MEMORY_RWX;

	create_mapping(&map, NULL);

	flush_all();
}

/*
 * Create the architecture specific mappings
 */
void __init iotable_init(struct map_desc *io_desc, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
	  create_mapping(io_desc + i, NULL);
}

extern unsigned long io_map_current_base;

void __iomem *ioremap_pages(unsigned long phys_addr, unsigned int size, unsigned int mtype) {
  struct map_desc map;
  int i;
  struct vcpu temp_vcpu;
  unsigned int offset;

  /* Make sure the virtual address will be correctly aligned (either section or page aligned). */

  io_map_current_base = ALIGN_UP(io_map_current_base, ((size < SZ_1M) ? PAGE_SIZE : SZ_1M));

  /* Preserve a possible offset */
  offset = phys_addr & 0xfff;

  map.pfn = phys_to_pfn(phys_addr);
  map.virtual = (unsigned long) io_map_current_base;
  map.length = size;
  map.type = mtype;

  create_mapping(&map, NULL);

  /* Of course ;-) we have to update all page tables... */
  for (i = 0; i < MAX_ME_DOMAINS; i++)
	  if (domains[i]) {

		  /* Need to swap page table to reach the right phys addr */
		  save_ptbase(&temp_vcpu);
		  write_ptbase(domains[i]->vcpu[0]);

		  create_mapping(&map, (pde_t *) domains[i]->vcpu[0]->arch.guest_vtable);

		  write_ptbase(&temp_vcpu);
	  }

  io_map_current_base += size;

  flush_all();
  return (void __iomem *) (map.virtual + offset);

}

void __init paging_init(struct machine_desc *mdesc)
{

	build_mem_type_table();

	bootmem_init();

	devicemaps_init();

	/*
	 * Ask the machine support to map in the statically mapped devices.
	 */

	if (mdesc->map_io)
		mdesc->map_io();

	/*
	 * Finally flush the caches and tlb to ensure that we're in a
	 * consistent state wrt the writebuffer.  This also ensures that
	 * any write-allocated cache lines in the vector page are written
	 * back.  After this point, we can start to touch devices again.
	 */
	flush_all();
}

void dump_pgtable(unsigned int *l1pgtable) {

	int i, j;
	unsigned int *l1pte, *l2pte;

	printk("           ***** Page table dump *****\n");

	for (i = 0; i < L1_PAGETABLE_ENTRIES; i++) {
		l1pte = l1pgtable + i;
		if (*l1pte) {
			if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_SECT)
				printk(" - L1 pte@%p (idx %d) mapping %x is section type  content: %x\n", l1pgtable+i, i, i << L1_PAGETABLE_SHIFT, *l1pte);
			else
				printk(" - L1 pte@%p (idx %d) is coarse type   content: %x\n", l1pgtable+i, i, *l1pte);

			if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_PT) {
				for (j = 0; j < 256; j++) {
					l2pte = ((unsigned int *) __va(*l1pte & L1DESC_L2PT_BASE_ADDR_MASK)) + j;
					if (*l2pte)
						printk("      - L2 pte@%p (i2=%x) mapping %x  content: %x\n", l2pte, j, (i << 20) | (j << 12), *l2pte);
				}
			}
		}
	}
}

