#include "rvemu.h"

#define BINBUF_CAP 64 * 1024

static u8 elfbuf[BINBUF_CAP] = {0};
static u8 *binbuf = elfbuf + sizeof(elf64_ehdr_t);

u8 *machine_compile(machine_t *m, str_t source) {
    int saved_stdout = dup(STDOUT_FILENO);
    int outp[2];

    if (pipe(outp) != 0) fatal("cannot make a pipe");
    dup2(outp[1], STDOUT_FILENO);
    close(outp[1]);

    FILE *f;
    f = popen(
        "clang -O3 -march=native "
        "-static -nostdlib -c -xc "
        "-o /dev/stdout -", "w");
    if (f == NULL) fatal("cannot compile program");
    fwrite(source, 1, str_len(source), f);
    pclose(f);
    fflush(stdout);

    size_t binsz = read(outp[0], elfbuf, 64 * 1024) - sizeof(elf64_ehdr_t);
    dup2(saved_stdout, STDOUT_FILENO);

    return cache_add(m->cache, m->state.pc, binbuf, binsz);
}
