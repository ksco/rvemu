#include "rvemu.h"

enum exit_reason_t machine_step(machine_t *m) {
    while (true) {
        m->state.exit_reason = none;
        exec_block_interp(&m->state);
        assert(m->state.exit_reason != none);

        if (m->state.exit_reason == indirect_branch ||
            m->state.exit_reason == direct_branch) {
            m->state.pc = m->state.reenter_pc;
            continue;
        }

        break;
    }

    m->state.pc = m->state.reenter_pc;
    assert(m->state.exit_reason == ecall);
    return ecall;
}

void machine_load_program(machine_t *m, char *prog) {
    int fd = open(prog, O_RDONLY);
    if (fd == -1) {
        fatal(strerror(errno));
    }

    mmu_load_elf(&m->mmu, fd);
    close(fd);

    m->state.pc = (u64)m->mmu.entry;
}
