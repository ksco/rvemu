#include "rvemu.h"

int main(int argc, char *argv[]) {
    assert(argc > 1);

    machine_t machine = {0};
    machine_load_program(&machine, argv[1]);

    printf("host alloc: %lx\n", machine.mmu.host_alloc);
    return 0;
}
