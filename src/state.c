#include "rvemu.h"

void state_print_regs(state_t *s) {
    static char *regnames[] = {
        "zero", "ra", "sp", "gp", "tp",
        "t0", "t1", "t2",
        "s0", "s1",
        "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
        "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
        "t3", "t4", "t5", "t6"
    };
    printf("pc:\t%016lx\n", s->pc);
    for (int i = 0; i < 32; i++) {
        printf("%s:\t%016lx\t", regnames[i], s->gp_regs[i]);
        if (i % 4 == 3) {
            printf("\n");
        }
    }
}
