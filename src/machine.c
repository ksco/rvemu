#include "rvemu.h"

void machine_setup(machine_t *m, int argc, char *argv[]) {
    size_t stack_size = 32 * 1024;
    u64 stack = mmu_alloc(&m->mmu, stack_size);
    m->state.gp_regs[sp] = stack + stack_size;

    u64 program = mmu_alloc(&m->mmu, 4 * 1024);
    u8 *name = (u8 *)"a.out";
    mmu_write(&m->mmu, program, name, strlen((char *)name));

    m->state.gp_regs[sp] -= 8; // auxp
    m->state.gp_regs[sp] -= 8; // envp
    m->state.gp_regs[sp] -= 8; // argv end

    u64 args = argc - 1;
    for (int i = args; i > 0; i--) {
        size_t len = strlen(argv[i]);
        u64 addr = mmu_alloc(&m->mmu, len+1);
        mmu_write(&m->mmu, addr, (u8 *)argv[i], len);
        m->state.gp_regs[sp] -= 8; // argv[i]
        mmu_write(&m->mmu, m->state.gp_regs[sp], (u8 *)&addr, sizeof(u64));
    }

    m->state.gp_regs[sp] -= 8; // argc
    mmu_write(&m->mmu, m->state.gp_regs[sp], (u8 *)&args, sizeof(u64));
}

enum exit_reason_t machine_step(machine_t *m) {
    while(true) {
        u8 *code = cache_lookup(m->cache, m->state.pc);
        if (code == NULL) {
            str_t source = machine_genblock(m);
            code = machine_compile(m, source);
        }

        while (true) {
            ((func_t)code)(&m->state);

            if (m->state.exit_reason == indirect_branch) {
                code = cache_lookup(m->cache, m->state.reenter_pc);
                if (code != NULL) continue;
            }

            break;
        }

        m->state.pc = m->state.reenter_pc;
        switch (m->state.exit_reason) {
        case indirect_branch:
            // continue execution
            break;
        case ecall:
            return ecall;
        default:
            unreachable();
        }
    }
}

void machine_load_program(machine_t *m, char* prog) {
    int fd = open(prog, O_RDONLY);
    if (fd == -1) {
        fatal(strerror(errno));
    }

    struct stat st;
    if (fstat(fd, &st) == -1 || !st.st_size) {
        fatal(strerror(errno));
    }

    mmu_load_elf(&m->mmu, fd, st.st_size);
    close(fd);

    m->state.pc = (u64)m->mmu.entry;
}
