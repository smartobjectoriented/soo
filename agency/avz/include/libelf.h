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

#ifndef __LIBELF_H__
#define __LIBELF_H__

#define ELF_LITTLE_ENDIAN

#undef ELFSIZE
#include "elfstructs.h"


/* ------------------------------------------------------------------------ */

typedef union {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
} elf_ehdr;

typedef union {
    Elf32_Phdr e32;
    Elf64_Phdr e64;
} elf_phdr;

typedef union {
    Elf32_Shdr e32;
    Elf64_Shdr e64;
} elf_shdr;

typedef union {
    Elf32_Sym e32;
    Elf64_Sym e64;
} elf_sym;

typedef union {
    Elf32_Rel e32;
    Elf64_Rel e64;
} elf_rel;

typedef union {
    Elf32_Rela e32;
    Elf64_Rela e64;
} elf_rela;

typedef union {
    Elf32_Note e32;
    Elf64_Note e64;
} elf_note;

struct elf_binary {
    /* elf binary */
    const char *image_base;
    size_t size;
    char class;
    char data;

    const elf_ehdr *ehdr;
    const char *sec_strtab;
    const elf_shdr *sym_tab;
    const char *sym_strtab;

    /* loaded to */
    char *dest_base;
    uint64_t pstart;
    uint64_t pend;
    uint64_t reloc_offset;

    uint64_t bsd_symtab_pstart;
    uint64_t bsd_symtab_pend;

    int verbose;
};

/* ------------------------------------------------------------------------ */
/* accessing elf header fields                                              */

#define NATIVE_ELFDATA ELFDATA2LSB

#define elf_32bit(elf) (ELFCLASS32 == (elf)->class)
#define elf_64bit(elf) (ELFCLASS64 == (elf)->class)
#define elf_msb(elf)   (ELFDATA2MSB == (elf)->data)
#define elf_lsb(elf)   (ELFDATA2LSB == (elf)->data)
#define elf_swap(elf)  (NATIVE_ELFDATA != (elf)->data)

#define elf_uval(elf, str, elem)                                        \
    ((ELFCLASS64 == (elf)->class)                                       \
     ? elf_access_unsigned((elf), (str),                                \
                           offsetof(typeof(*(str)),e64.elem),           \
                           sizeof((str)->e64.elem))                     \
     : elf_access_unsigned((elf), (str),                                \
                           offsetof(typeof(*(str)),e32.elem),           \
                           sizeof((str)->e32.elem)))

#define elf_sval(elf, str, elem)                                        \
    ((ELFCLASS64 == (elf)->class)                                       \
     ? elf_access_signed((elf), (str),                                  \
                         offsetof(typeof(*(str)),e64.elem),             \
                         sizeof((str)->e64.elem))                       \
     : elf_access_signed((elf), (str),                                  \
                         offsetof(typeof(*(str)),e32.elem),             \
                         sizeof((str)->e32.elem)))

#define elf_size(elf, str)                              \
    ((ELFCLASS64 == (elf)->class)                       \
     ? sizeof((str)->e64) : sizeof((str)->e32))

uint64_t elf_access_unsigned(struct elf_binary *elf, const void *ptr,
                             uint64_t offset, size_t size);
int64_t elf_access_signed(struct elf_binary *elf, const void *ptr,
                          uint64_t offset, size_t size);

uint64_t elf_round_up(struct elf_binary *elf, uint64_t addr);

/* ------------------------------------------------------------------------ */
/* xc_libelf_tools.c                                                        */

int elf_shdr_count(struct elf_binary *elf);
int elf_phdr_count(struct elf_binary *elf);

const elf_shdr *elf_shdr_by_name(struct elf_binary *elf, const char *name);
const elf_shdr *elf_shdr_by_index(struct elf_binary *elf, int index);
const elf_phdr *elf_phdr_by_index(struct elf_binary *elf, int index);

const char *elf_section_name(struct elf_binary *elf, const elf_shdr * shdr);
const void *elf_section_start(struct elf_binary *elf, const elf_shdr * shdr);
const void *elf_section_end(struct elf_binary *elf, const elf_shdr * shdr);

const void *elf_segment_start(struct elf_binary *elf, const elf_phdr * phdr);
const void *elf_segment_end(struct elf_binary *elf, const elf_phdr * phdr);

const elf_sym *elf_sym_by_name(struct elf_binary *elf, const char *symbol);
const elf_sym *elf_sym_by_index(struct elf_binary *elf, int index);

const char *elf_note_name(struct elf_binary *elf, const elf_note * note);
const void *elf_note_desc(struct elf_binary *elf, const elf_note * note);
uint64_t elf_note_numeric(struct elf_binary *elf, const elf_note * note);
const elf_note *elf_note_next(struct elf_binary *elf, const elf_note * note);

int elf_is_elfbinary(const void *image);
int elf_phdr_is_loadable(struct elf_binary *elf, const elf_phdr * phdr);

/* ------------------------------------------------------------------------ */
/* xc_libelf_loader.c                                                       */

int elf_init(struct elf_binary *elf, const char *image, size_t size);

void elf_set_verbose(struct elf_binary *elf);

void elf_parse_binary(struct elf_binary *elf);
void elf_load_binary(struct elf_binary *elf);

void *elf_get_ptr(struct elf_binary *elf, unsigned long addr);
uint64_t elf_lookup_addr(struct elf_binary *elf, const char *symbol);

void elf_parse_bsdsyms(struct elf_binary *elf, uint64_t pstart); /* private */

/* ------------------------------------------------------------------------ */
/* xc_libelf_relocate.c                                                     */

int elf_reloc(struct elf_binary *elf);

#endif /* __LIBELF_H__ */
