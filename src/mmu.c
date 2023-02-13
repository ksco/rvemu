#include "rvemu.h"

static void load_phdr(elf64_phdr_t *phdr, elf64_ehdr_t *ehdr, i64 i, FILE *file) {
    if (fseek(file, ehdr->e_phoff + ehdr->e_phentsize * i, SEEK_SET) != 0) {
        fatal("seek file failed");
    }

    if (fread((void *)phdr, 1, sizeof(elf64_phdr_t), file) != sizeof(elf64_phdr_t)) {
        fatal("file too small");
    }
}

static int flags_to_mmap_prot(u32 flags) {
    return (flags & PF_R ? PROT_READ : 0) |
           (flags & PF_W ? PROT_WRITE : 0) |
           (flags & PF_X ? PROT_EXEC : 0);
}

static void mmu_load_segment(mmu_t *mmu, elf64_phdr_t *phdr, int fd) {
    int page_size = getpagesize();
    u64 offset = phdr->p_offset;
    u64 vaddr = TO_HOST(phdr->p_vaddr);
    u64 aligned_vaddr = ROUNDDOWN(vaddr, page_size);
    u64 filesz = phdr->p_filesz + (vaddr - aligned_vaddr);
    u64 memsz = phdr->p_memsz + (vaddr - aligned_vaddr);
    int prot = flags_to_mmap_prot(phdr->p_flags);

    u64 addr = (u64)mmap((void *)aligned_vaddr, filesz, prot, MAP_PRIVATE | MAP_FIXED,
                        fd, ROUNDDOWN(offset, page_size));
    assert(addr == aligned_vaddr);

    u64 remaining_bss = ROUNDUP(memsz, page_size) - ROUNDUP(filesz, page_size);
    if (remaining_bss > 0) {
        u64 addr = (u64)mmap((void *)(aligned_vaddr + ROUNDUP(filesz, page_size)),
                            remaining_bss, prot, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
        assert(addr == aligned_vaddr + ROUNDUP(filesz, page_size));
    }

    mmu->host_alloc = MAX(mmu->host_alloc, (aligned_vaddr + ROUNDUP(memsz, page_size)));

    mmu->base = mmu->alloc = TO_GUEST(mmu->host_alloc);
}

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

    elf64_phdr_t phdr;
    for (i64 i = 0; i < ehdr->e_phnum; i++) {
        load_phdr(&phdr, ehdr, i, file);

        if (phdr.p_type == PT_LOAD) {
            mmu_load_segment(mmu, &phdr, fd);
        }
    }
}
