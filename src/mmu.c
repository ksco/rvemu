#include "rvemu.h"

void mmu_load_elf(mmu_t *mmu, int fd) {
    u8 buf[sizeof(elf64_ehdr_t)];
    FILE *file = fdopen(fd, "rb");
    if (fread(buf, 1, sizeof(elf64_ehdr_t), file) != sizeof(elf64_ehdr_t)) {
        fatal("file too small");
    }

    elf64_ehdr_t *ehdr = (elf64_ehdr_t *)buf;

    if (*(u32 *)ehdr != *(u32 *)ELFMAG) {
        fatal("bad elf file");
    }

    if (ehdr->e_machine != EM_RISCV || ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        fatal("only riscv64 elf file is supported");
    }

    mmu->entry = (u64)ehdr->e_entry;
}
