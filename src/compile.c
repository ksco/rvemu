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

    // .text section is always the third one
    u64 shoff = ehdr->e_shoff + 2*sizeof(elf64_shdr_t);
    elf64_shdr_t *shdr = (elf64_shdr_t *)(elfbuf + shoff);

    return cache_add(m->cache, m->state.pc, elfbuf + shdr->sh_offset, shdr->sh_size);
}
