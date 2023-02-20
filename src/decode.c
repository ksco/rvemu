#include "rvemu.h"

#define QUADRANT(data) (((data) >> 0) & 0x3)

void insn_decode(insn_t *insn, u32 data) {
    u32 quadrant = QUADRANT(data);
    switch (quadrant) {
    case 0x0: fatal("unimplemented");
    case 0x1: fatal("unimplemented");
    case 0x2: fatal("unimplemented");
    case 0x3: fatal("unimplemented");
    default: unreachable();
    }
}
