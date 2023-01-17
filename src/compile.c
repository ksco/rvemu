#include "rvemu.h"

#define BINBUF_CAP 64 * 1024

static u8 elfbuf[BINBUF_CAP] = {0};

u8 *machine_compile(machine_t *m, str_t source) {
    int saved_stdout = dup(STDOUT_FILENO);
    int outp[2];

    if (pipe(outp) != 0) fatal("cannot make a pipe");
    dup2(outp[1], STDOUT_FILENO);
    close(outp[1]);

    FILE *f;
    f = popen("clang -O3 -c -xc -o /dev/stdout -", "w");
    if (f == NULL) fatal("cannot compile program");
    fwrite(source, 1, str_len(source), f);
    pclose(f);
    fflush(stdout);

    read(outp[0], elfbuf, BINBUF_CAP);
    dup2(saved_stdout, STDOUT_FILENO);

    elf64_ehdr_t *ehdr = (elf64_ehdr_t *)elfbuf;

    /**
     * for some instructions, clang will generate a corresponding .rodata section.
     * this means we need to write a mini-linker that puts the .rodata section into
     * memory, takes its actual address, and uses symbols and relocations to apply
     * it back to the corresponding location in the .text section.
     */

    i64 text_idx = 0, symtab_idx = 0, rela_idx = 0, rodata_idx = 0;
    {
        u64 shstr_shoff = ehdr->e_shoff + ehdr->e_shstrndx * sizeof(elf64_shdr_t);
        elf64_shdr_t *shstr_shdr = (elf64_shdr_t *)(elfbuf + shstr_shoff);
        assert(ehdr->e_shnum != 0);

        for (i64 idx = 0; idx < ehdr->e_shnum; idx++) {
            u64 shoff = ehdr->e_shoff + idx * sizeof(elf64_shdr_t);
            elf64_shdr_t *shdr = (elf64_shdr_t *)(elfbuf + shoff);
            char *str = (char *)(elfbuf + shstr_shdr->sh_offset + shdr->sh_name);
            if (strcmp(str, ".text") == 0) text_idx = idx;
            if (strcmp(str, ".rela.text") == 0) rela_idx = idx;
            if (strncmp(str, ".rodata.", strlen(".rodata.")) == 0) rodata_idx = idx;
            if (strcmp(str, ".symtab") == 0) symtab_idx = idx;
        }
    }

    assert(text_idx != 0 && symtab_idx != 0);

    u64 text_shoff = ehdr->e_shoff + text_idx * sizeof(elf64_shdr_t);
    elf64_shdr_t *text_shdr = (elf64_shdr_t *)(elfbuf + text_shoff);

    if (rela_idx == 0 || rodata_idx == 0) {
        return cache_add(m->cache, m->state.pc, elfbuf + text_shdr->sh_offset,
                         text_shdr->sh_size, text_shdr->sh_addralign);
    }

    u64 text_addr = 0;
    {
        u64 shoff = ehdr->e_shoff + rodata_idx * sizeof(elf64_shdr_t);
        elf64_shdr_t *shdr = (elf64_shdr_t *)(elfbuf + shoff);
        cache_add(m->cache, m->state.pc, elfbuf + shdr->sh_offset,
                  shdr->sh_size, shdr->sh_addralign);
        text_addr = (u64)cache_add(m->cache, m->state.pc, elfbuf + text_shdr->sh_offset,
                                   text_shdr->sh_size, text_shdr->sh_addralign);
    }

    // apply relocations to .text section.
    {
        u64 shoff = ehdr->e_shoff + rela_idx * sizeof(elf64_shdr_t);
        elf64_shdr_t *shdr = (elf64_shdr_t *)(elfbuf + shoff);
        i64 rels = shdr->sh_size / sizeof(elf64_rela_t);

        u64 symtab_shoff = ehdr->e_shoff + symtab_idx * sizeof(elf64_shdr_t);
        elf64_shdr_t *symtab_shdr = (elf64_shdr_t *)(elfbuf + symtab_shoff);

        for (i64 idx = 0; idx < rels; idx++) {
#ifndef __x86_64__
            fatal("only support x86_64 for now");
#endif
            elf64_rela_t *rel = (elf64_rela_t *)(elfbuf + shdr->sh_offset + idx * sizeof(elf64_rela_t));
            assert(rel->r_type == R_X86_64_PC32);

            elf64_sym_t *sym = (elf64_sym_t *)(elfbuf + symtab_shdr->sh_offset + rel->r_sym * sizeof(elf64_sym_t));
            u32 *loc = (u32 *)(text_addr + rel->r_offset);
            *loc = (u32)((i64)sym->st_value + rel->r_addend - (i64)rel->r_offset);
        }
    }

    return (u8 *)text_addr;
}
