#include "rvemu.h"

static void load_phdr(elf64_phdr_t* phdr, elf64_ehdr_t *ehdr, i64 i, FILE *file) {
    if (fseek(file, ehdr->e_phoff + ehdr->e_phentsize * i, SEEK_SET) != 0) {
        fatal("seek file failed");
    }
    if (fread((void *)phdr, 1, sizeof(elf64_phdr_t), file) != sizeof(elf64_phdr_t)) {
        fatal("file too small");
    }
}

static void mmu_load_segment(mmu_t *mmu, elf64_phdr_t *phdr, int fd) {
    i64 offset = phdr->p_offset;
    i64 vaddr = phdr->p_vaddr;
    i64 len = phdr->p_filesz;
    int ret = pread(fd, (void *)(mmu->mem + vaddr), len, offset);
    if (ret == -1) {
        fatal(strerror(errno));
    }
    mmu->alloc = MAX(mmu->alloc, phdr->p_memsz);
}

void mmu_load_elf(mmu_t *mmu, int fd, off_t sz) {
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

    elf64_phdr_t phdr;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        load_phdr(&phdr, ehdr,  i, file);
        if (phdr.p_type == PT_LOAD) {
            mmu_load_segment(mmu, &phdr, fd);
        }
    }
}
