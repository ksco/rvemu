#include "rvemu.h"

int main(int argc, char *argv[]) {
    assert(argc > 1);

    machine_t machine = {0};
    machine.mmu.cap = 64 * 1024 * 1024;
    machine.mmu.mem = (u8 *)calloc(machine.mmu.cap, 1);
    machine.cache = new_cache();
    machine.state.mem = machine.mmu.mem;
    machine_load_program(&machine, argv[1]);
    machine_setup(&machine, argc, argv);

    while(true) {
        enum exit_reason_t reason = machine_step(&machine);

        switch (reason) {
        case ecall: {
            u64 syscall = machine_get_gp_reg(&machine, a7);
            switch (syscall) {
            case 57: { /* CLOSE */
                i64 fd = machine_get_gp_reg(&machine, a0);
                if (fd > 2) {
                    machine_set_gp_reg(&machine, a0, close(fd));
                }
            }
            break;
            case 64: { /* WRITE */
                i64 fd = machine_get_gp_reg(&machine, a0);
                u64 ptr = machine_get_gp_reg(&machine, a1);
                u64 len = machine_get_gp_reg(&machine, a2);
                static char *buf = NULL;
                if (buf) {
                    buf = (char *)realloc(buf, len + 1);
                } else {
                    buf = (char *)malloc(len + 1);
                }
                buf[len] = 0;
                memcpy(buf, (char *)(machine.mmu.mem + ptr), len);
                FILE *f = fdopen(fd, "w");
                fprintf(f, "%s", buf);
                fflush(f);
                machine_set_gp_reg(&machine, a0, len);
            }
            break;
            case 80: { /* FSTAT */
                i64 fd = machine_get_gp_reg(&machine, a0);
                u64 stat_addr = machine_get_gp_reg(&machine, a1);
                struct stat s = {0};
                machine_set_gp_reg(&machine, a0, (uint64_t)fstat(fd, &s));
                mmu_write(&machine.mmu, stat_addr, (u8 *)&s, sizeof(s));
            }
            break;
            case 93: { /* EXIT */
                int exit_code = machine_get_gp_reg(&machine, a0);
                exit(exit_code);
            }
            break;
            case 169: { /* GETTIMEOFDAY */
                u64 tv_addr = machine_get_gp_reg(&machine, a0);
                u64 tz_addr = machine_get_gp_reg(&machine, a1);

                struct timezone *tz = NULL;
                if (tz_addr != 0) {
                   tz = (struct timezone *)(machine.mmu.mem + tz_addr);
                }

                struct timeval *tv = (struct timeval *)(machine.mmu.mem + tv_addr);
                machine_set_gp_reg(&machine, a0, gettimeofday(tv, tz));
            }
            break;
            case 214: { /* SBRK */
                int incr = machine_get_gp_reg(&machine, a0);
                assert(incr >= 0);
                u64 base = mmu_alloc(&machine.mmu, incr);
                machine_set_gp_reg(&machine, a0, base);
            }
            break;
            default: printf("%ld\n", syscall); fatal("handle syscall");
            }
        }
        break;
        default: unreachable();
        }
    }

    return 0;
}
