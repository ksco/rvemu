#include "rvemu.h"

#define QUADRANT(data) (((data) >>  0) & 0x3 )

/**
 * normal types
*/
#define OPCODE(data) (((data) >>  2) & 0x1f)
#define RD(data)     (((data) >>  7) & 0x1f)
#define RS1(data)    (((data) >> 15) & 0x1f)
#define RS2(data)    (((data) >> 20) & 0x1f)
#define FUNCT3(data) (((data) >> 12) & 0x7 )
#define FUNCT7(data) (((data) >> 25) & 0x7f)
#define IMM116(data) (((data) >> 26) & 0x3f)

static inline insn_t insn_utype_read(u32 data) {
    return (insn_t) {
        .imm = (i32)data & 0xfffff000,
        .rd = RD(data),
    };
}

static inline insn_t insn_itype_read(u32 data) {
    return (insn_t) {
        .imm = (i32)data >> 20,
        .rs1 = RS1(data),
        .rd = RD(data),
    };
}

static inline insn_t insn_jtype_read(u32 data) {
    u32 imm20   = (data >> 31) & 0x1;
    u32 imm101  = (data >> 21) & 0x3ff;
    u32 imm11   = (data >> 20) & 0x1;
    u32 imm1912 = (data >> 12) & 0xff;

    i32 imm = (imm20 << 20) | (imm1912 << 12) | (imm11 << 11) | (imm101 << 1);
    imm = (imm << 11) >> 11;

    return (insn_t) {
        .imm = imm,
        .rd = RD(data),
    };
}

static inline insn_t insn_btype_read(u32 data) {
    u32 imm12  = (data >> 31) & 0x1;
    u32 imm105 = (data >> 25) & 0x3f;
    u32 imm41  = (data >>  8) & 0xf;
    u32 imm11  = (data >>  7) & 0x1;

    i32 imm = (imm12 << 12) | (imm11 << 11) |(imm105 << 5) | (imm41 << 1);
    imm = (imm << 19) >> 19;

    return (insn_t) {
        .imm = imm,
        .rs1 = RS1(data),
        .rs2 = RS2(data),
    };
}

static inline insn_t insn_rtype_read(u32 data) {
    return (insn_t) {
        .rs1 = RS1(data),
        .rs2 = RS2(data),
        .rd = RD(data),
    };
}

static inline insn_t insn_stype_read(u32 data) {
    u32 imm115 = (data >> 25) & 0x7f;
    u32 imm40  = (data >>  7) & 0x1f;

    i32 imm = (imm115 << 5) | imm40;
    imm = (imm << 20) >> 20;
    return (insn_t) {
        .imm = imm,
        .rs1 = RS1(data),
        .rs2 = RS2(data),
    };
}

static inline insn_t insn_csrtype_read(u32 data) {
    return (insn_t) {
        .csr = data >> 20,
        .rs1 = RS1(data),
        .rd =  RD(data),
    };
}

/**
 * compressed types
*/
#define COPCODE(data)     (((data) >> 13) & 0x7 )
#define CFUNCT1(data)     (((data) >> 12) & 0x1 )
#define CFUNCT2LOW(data)  (((data) >>  5) & 0x3 )
#define CFUNCT2HIGH(data) (((data) >> 10) & 0x3 )
#define RP1(data)         (((data) >>  7) & 0x7 )
#define RP2(data)         (((data) >>  2) & 0x7 )
#define RC1(data)         (((data) >>  7) & 0x1f)
#define RC2(data)         (((data) >>  2) & 0x1f)

static inline insn_t insn_catype_read(u16 data) {
    return (insn_t) {
        .rd = RP1(data) + 8,
        .rs2 = RP2(data) + 8,
        .rvc = true,
    };
}

static inline insn_t insn_crtype_read(u16 data) {
    return (insn_t) {
        .rs1 = RC1(data),
        .rs2 = RC2(data),
        .rvc = true,
    };
}

static inline insn_t insn_citype_read(u16 data) {
    u32 imm40 = (data >>  2) & 0x1f;
    u32 imm5  = (data >> 12) & 0x1;
    i32 imm = (imm5 << 5) | imm40;
    imm = (imm << 26) >> 26;

    return (insn_t) {
        .imm = imm,
        .rd = RC1(data),
        .rvc = true,
    };
}

static inline insn_t insn_citype_read2(u16 data) {
    u32 imm86 = (data >>  2) & 0x7;
    u32 imm43 = (data >>  5) & 0x3;
    u32 imm5  = (data >> 12) & 0x1;

    i32 imm = (imm86 << 6) | (imm43 << 3) | (imm5 << 5);

    return (insn_t) {
        .imm = imm,
        .rd = RC1(data),
        .rvc = true,
    };
}

static inline insn_t insn_citype_read3(u16 data) {
    u32 imm5  = (data >>  2) & 0x1;
    u32 imm87 = (data >>  3) & 0x3;
    u32 imm6  = (data >>  5) & 0x1;
    u32 imm4  = (data >>  6) & 0x1;
    u32 imm9  = (data >> 12) & 0x1;

    i32 imm = (imm5 << 5) | (imm87 << 7) | (imm6 << 6) | (imm4 << 4) | (imm9 << 9);
    imm = (imm << 22) >> 22;

    return (insn_t) {
        .imm = imm,
        .rd = RC1(data),
        .rvc = true,
    };
}

static inline insn_t insn_citype_read4(u16 data) {
    u32 imm5  = (data >> 12) & 0x1;
    u32 imm42 = (data >>  4) & 0x7;
    u32 imm76 = (data >>  2) & 0x3;

    i32 imm = (imm5 << 5) | (imm42 << 2) | (imm76 << 6);

    return (insn_t) {
        .imm = imm,
        .rd = RC1(data),
        .rvc = true,
    };
}

static inline insn_t insn_citype_read5(u16 data) {
    u32 imm1612 = (data >>  2) & 0x1f;
    u32 imm17   = (data >> 12) & 0x1;

    i32 imm = (imm1612 << 12) | (imm17 << 17);
    imm = (imm << 14) >> 14;
    return (insn_t) {
        .imm = imm,
        .rd = RC1(data),
        .rvc = true,
    };
}

static inline insn_t insn_cbtype_read(u16 data) {
    u32 imm5  = (data >>  2) & 0x1;
    u32 imm21 = (data >>  3) & 0x3;
    u32 imm76 = (data >>  5) & 0x3;
    u32 imm43 = (data >> 10) & 0x3;
    u32 imm8  = (data >> 12) & 0x1;

    i32 imm = (imm8 << 8) | (imm76 << 6) | (imm5 << 5) | (imm43 << 3) | (imm21 << 1);
    imm = (imm << 23) >> 23;

    return (insn_t) {
        .imm = imm,
        .rs1 = RP1(data) + 8,
        .rvc = true,
    };
}

static inline insn_t insn_cbtype_read2(u16 data) {
    u32 imm40 = (data >>  2) & 0x1f;
    u32 imm5  = (data >> 12) & 0x1;
    i32 imm = (imm5 << 5) | imm40;
    imm = (imm << 26) >> 26;

    return (insn_t) {
        .imm = imm,
        .rd = RP1(data) + 8,
        .rvc = true,
    };
}

static inline insn_t insn_cstype_read(u16 data) {
    u32 imm76 = (data >>  5) & 0x3;
    u32 imm53 = (data >> 10) & 0x7;

    i32 imm = ((imm76 << 6) | (imm53 << 3));

    return (insn_t) {
        .imm = imm,
        .rs1 = RP1(data) + 8,
        .rs2 = RP2(data) + 8,
        .rvc = true,
    };
}

static inline insn_t insn_cstype_read2(u16 data) {
    u32 imm6  = (data >>  5) & 0x1;
    u32 imm2  = (data >>  6) & 0x1;
    u32 imm53 = (data >> 10) & 0x7;

    i32 imm = ((imm6 << 6) | (imm2 << 2) | (imm53 << 3));

    return (insn_t) {
        .imm = imm,
        .rs1 = RP1(data) + 8,
        .rs2 = RP2(data) + 8,
        .rvc = true,
    };
}

static inline insn_t insn_cjtype_read(u16 data) {
    u32 imm5  = (data >>  2) & 0x1;
    u32 imm31 = (data >>  3) & 0x7;
    u32 imm7  = (data >>  6) & 0x1;
    u32 imm6  = (data >>  7) & 0x1;
    u32 imm10 = (data >>  8) & 0x1;
    u32 imm98 = (data >>  9) & 0x3;
    u32 imm4  = (data >> 11) & 0x1;
    u32 imm11 = (data >> 12) & 0x1;

    i32 imm = ((imm5 << 5) | (imm31 << 1) | (imm7 << 7) | (imm6 << 6) |
               (imm10 << 10) | (imm98 << 8) | (imm4 << 4) | (imm11 << 11));
    imm = (imm << 20) >> 20;
    return (insn_t) {
        .imm = imm,
        .rvc = true,
    };
}

static inline insn_t insn_cltype_read(u16 data) {
    u32 imm6  = (data >>  5) & 0x1;
    u32 imm2  = (data >>  6) & 0x1;
    u32 imm53 = (data >> 10) & 0x7;

    i32 imm = (imm6 << 6) | (imm2 << 2) | (imm53 << 3);

    return (insn_t) {
        .imm = imm,
        .rs1 = RP1(data) + 8,
        .rd  = RP2(data) + 8,
        .rvc = true,
    };
}

static inline insn_t insn_cltype_read2(u16 data) {
    u32 imm76 = (data >>  5) & 0x3;
    u32 imm53 = (data >> 10) & 0x7;

    i32 imm = (imm76 << 6) | (imm53 << 3);

    return (insn_t) {
        .imm = imm,
        .rs1 = RP1(data) + 8,
        .rd  = RP2(data) + 8,
        .rvc = true,
    };
}

static inline insn_t insn_csstype_read(u16 data) {
    u32 imm86 = (data >>  7) & 0x7;
    u32 imm53 = (data >> 10) & 0x7;

    i32 imm = (imm86 << 6) | (imm53 << 3);

    return (insn_t) {
        .imm = imm,
        .rs2 = RC2(data),
        .rvc = true,
    };
}

static inline insn_t insn_csstype_read2(u16 data) {
    u32 imm76 = (data >> 7) & 0x3;
    u32 imm52 = (data >> 9) & 0xf;

    i32 imm = (imm76 << 6) | (imm52 << 2);

    return (insn_t) {
        .imm = imm,
        .rs2 = RC2(data),
        .rvc = true,
    };
}

static inline insn_t insn_ciwtype_read(u16 data) {
    u32 imm3  = (data >>  5) & 0x1;
    u32 imm2  = (data >>  6) & 0x1;
    u32 imm96 = (data >>  7) & 0xf;
    u32 imm54 = (data >> 11) & 0x3;

    i32 imm = (imm3 << 3) | (imm2 << 2) | (imm96 << 6) | (imm54 << 4);

    return (insn_t) {
        .imm = imm,
        .rd = RP2(data) + 8,
        .rvc = true,
    };
}

void machine_decode(u32 data, insn_t *insn) {
    u32 quadrant = QUADRANT(data);
    switch (quadrant) {
    case 0x0: {
        u32 copcode = COPCODE(data);

        switch (copcode) {
        case 0x0: /* C.ADDI4SPN */
            *insn = insn_ciwtype_read(data);
            insn->rs1 = sp;
            insn->type = insn_addi;
            assert(insn->imm != 0);
            return;
        case 0x2: /* C.LW */
            *insn = insn_cltype_read(data);
            insn->type = insn_lw;
            return;
        case 0x3: /* C.LD */
            *insn = insn_cltype_read2(data);
            insn->type = insn_ld;
            return;
        case 0x6: /* C.SW */
            *insn = insn_cstype_read2(data);
            insn->type = insn_sw;
            return;
        case 0x7: /* C.SD */
            *insn = insn_cstype_read(data);
            insn->type = insn_sd;
            return;
        default: fatal("unimplemented");
        }
    }
    unreachable();
    case 0x1: {
        u32 copcode = COPCODE(data);

        switch (copcode) {
        case 0x0: /* C.ADDI */
            *insn = insn_citype_read(data);
            insn->rs1 = insn->rd;
            insn->type = insn_addi;
            return;
        case 0x1: /* C.ADDIW */
            *insn = insn_citype_read(data);
            assert(insn->rd != 0);
            insn->rs1 = insn->rd;
            insn->type = insn_addiw;
            return;
        case 0x2: /* C.LI */
            *insn = insn_citype_read(data);
            insn->rs1 = zero;
            insn->type = insn_addi;
            return;
        case 0x3: {
            i32 rd = RC1(data);
            if (rd == 2) { /* C.ADDI16SP */
                *insn = insn_citype_read3(data);
                assert(insn->imm != 0);
                insn->rs1 = insn->rd;
                insn->type = insn_addi;
                return;
            } else { /* C.LUI */
                *insn = insn_citype_read5(data);
                assert(insn->imm != 0);
                insn->type = insn_lui;
                return;
            }
        }
        unreachable();
        case 0x4: {
            u32 cfunct2high = CFUNCT2HIGH(data);

            switch (cfunct2high) {
            case 0x0:   /* C.SRLI */
            case 0x1:   /* C.SRAI */
            case 0x2: { /* C.ANDI */
                *insn = insn_cbtype_read2(data);
                insn->rs1 = insn->rd;

                if (cfunct2high == 0x0) {
                    insn->type = insn_srli;
                } else if (cfunct2high == 0x1) {
                    insn->type = insn_srai;
                } else {
                    insn->type = insn_andi;
                }
                return;
            }
            unreachable();
            case 0x3: {
                u32 cfunct1 = CFUNCT1(data);

                switch (cfunct1) {
                case 0x0: {
                    u32 cfunct2low = CFUNCT2LOW(data);

                    *insn = insn_catype_read(data);
                    insn->rs1 = insn->rd;

                    switch (cfunct2low) {
                    case 0x0: /* C.SUB */
                        insn->type = insn_sub;
                        break;
                    case 0x1: /* C.XOR */
                        insn->type = insn_xor;
                        break;
                    case 0x2: /* C.OR */
                        insn->type = insn_or;
                        break;
                    case 0x3: /* C.AND */
                        insn->type = insn_and;
                        break;
                    default: unreachable();
                    }
                    return;
                }
                unreachable();
                case 0x1: {
                    u32 cfunct2low = CFUNCT2LOW(data);

                    *insn = insn_catype_read(data);
                    insn->rs1 = insn->rd;

                    switch (cfunct2low) {
                    case 0x0: /* C.SUBW */
                        insn->type = insn_subw;
                        break;
                    case 0x1: /* C.ADDW */
                        insn->type = insn_addw;
                        break;
                    default: unreachable();
                    }
                    return;
                }
                unreachable();
                default: unreachable();
                }
            }
            unreachable();
            default: unreachable();
            }
        }
        unreachable();
        case 0x5: /* C.J */
            *insn = insn_cjtype_read(data);
            insn->rd = zero;
            insn->type = insn_jal;
            insn->cont = true;
            return;
        case 0x6: /* C.BEQZ */
        case 0x7: /* C.BNEZ */
            *insn = insn_cbtype_read(data);
            insn->rs2 = zero;
            insn->type = copcode == 0x6 ? insn_beq : insn_bne;
            return;
        default: fatal("unrecognized copcode");
        }
    }
    unreachable();
    case 0x2: {
        u32 copcode = COPCODE(data);
        switch (copcode) {
        case 0x0: /* C.SLLI */
            *insn = insn_citype_read(data);
            insn->rs1 = insn->rd;
            insn->type = insn_slli;
            return;
        case 0x2: /* C.LWSP */
            *insn = insn_citype_read4(data);
            assert(insn->rd != 0);
            insn->rs1 = sp;
            insn->type = insn_lw;
            return;
        case 0x3: /* C.LDSP */
            *insn = insn_citype_read2(data);
            assert(insn->rd != 0);
            insn->rs1 = sp;
            insn->type = insn_ld;
            return;
        case 0x4: {
            u32 cfunct1 = CFUNCT1(data);

            switch (cfunct1) {
            case 0x0: {
                *insn = insn_crtype_read(data);

                if (insn->rs2 == 0) { /* C.JR */
                    assert(insn->rs1 != 0);
                    insn->rd = zero;
                    insn->type = insn_jalr;
                    insn->cont = true;
                } else { /* C.MV */
                    insn->rd = insn->rs1;
                    insn->rs1 = zero;
                    insn->type = insn_add;
                }
                return;
            }
            unreachable();
            case 0x1: {
                *insn = insn_crtype_read(data);
                if (insn->rs1 == 0 && insn->rs2 == 0) { /* C.EBREAK */
                    fatal("unimplmented");
                } else if (insn->rs2 == 0) { /* C.JALR */
                    insn->rd = ra;
                    insn->type = insn_jalr;
                    insn->cont = true;
                } else { /* C.ADD */
                    insn->rd = insn->rs1;
                    insn->type = insn_add;
                }
                return;
            }
            unreachable();
            default: unreachable();
            }
        }
        unreachable();
        case 0x6: /* C.SWSP */
            *insn = insn_csstype_read2(data);
            insn->rs1 = sp;
            insn->type = insn_sw;
            return;
        case 0x7: /* C.SDSP */
            *insn = insn_csstype_read(data);
            insn->rs1 = sp;
            insn->type = insn_sd;
            return;
        default: fatal("unrecognized copcode");
        }
    }
    unreachable();
    case 0x3: {
        u32 opcode = OPCODE(data);
        switch (opcode) {
        case 0x0: {
            u32 funct3 = FUNCT3(data);

            *insn = insn_itype_read(data);
            switch (funct3) {
            case 0x0: /* LB */
                insn->type = insn_lb;
                return;
            case 0x1: /* LH */
                insn->type = insn_lh;
                return;
            case 0x2: /* LW */
                insn->type = insn_lw;
                return;
            case 0x3: /* LD */
                insn->type = insn_ld;
                return;
            case 0x4: /* LBU */
                insn->type = insn_lbu;
                return;
            case 0x5: /* LHU */
                insn->type = insn_lhu;
                return;
            case 0x6: /* LWU */
                insn->type = insn_lwu;
                return;
            default: unreachable();
            }
        }
        unreachable();
        case 0x3: {
            u32 funct3 = FUNCT3(data);

            switch (funct3) {
            case 0x0: { /* FENCE */
                insn_t _insn = {0};
                *insn = _insn;
                insn->type = insn_fence;
                return;
            }
            case 0x1: { /* FENCE.I */
                insn_t _insn = {0};
                *insn = _insn;
                insn->type = insn_fence_i;
                return;
            }
            default: unreachable();
            }
        }
        unreachable();
        case 0x4: {
            u32 funct3 = FUNCT3(data);

            *insn = insn_itype_read(data);
            switch (funct3) {
            case 0x0: /* ADDI */
                insn->type = insn_addi;
                return;
            case 0x1: {
                u32 imm116 = IMM116(data);
                if (imm116 == 0) { /* SLLI */
                    insn->type = insn_slli;
                } else {
                    unreachable();
                }
                return;
            }
            unreachable();
            case 0x2: /* SLTI */
                insn->type = insn_slti;
                return;
            case 0x3: /* SLTIU */
                insn->type = insn_sltiu;
                return;
            case 0x4: /* XORI */
                insn->type = insn_xori;
                return;
            case 0x5: {
                u32 imm116 = IMM116(data);

                if (imm116 == 0x0) { /* SRLI */
                    insn->type = insn_srli;
                } else if (imm116 == 0x10) { /* SRAI */
                    insn->type = insn_srai;
                } else {
                    unreachable();
                }
                return;
            }
            unreachable();
            case 0x6: /* ORI */
                insn->type = insn_ori;
                return;
            case 0x7: /* ANDI */
                insn->type = insn_andi;
                return;
            default: fatal("unrecognized funct3");
            }
        unreachable();
        }
        case 0x5: /* AUIPC */
            *insn = insn_utype_read(data);
            insn->type = insn_auipc;
            return;
        case 0x6: {
            u32 funct3 = FUNCT3(data);
            u32 funct7 = FUNCT7(data);

            *insn = insn_itype_read(data);

            switch (funct3) {
            case 0x0: /* ADDIW */
                insn->type = insn_addiw;
                return;
            case 0x1: /* SLLIW */
                assert(funct7 == 0);
                insn->type = insn_slliw;
                return;
            case 0x5: {
                switch (funct7) {
                case 0x0: /* SRLIW */
                    insn->type = insn_srliw;
                    return;
                case 0x20: /* SRAIW */
                    insn->type = insn_sraiw;
                    return;
                default: unreachable();
                }
            }
            unreachable();
            default: fatal("unimplemented");
            }
        }
        unreachable();
        case 0x8: {
            u32 funct3 = FUNCT3(data);

            *insn = insn_stype_read(data);
            switch (funct3) {
            case 0x0: /* SB */
                insn->type = insn_sb;
                return;
            case 0x1: /* SH */
                insn->type = insn_sh;
                return;
            case 0x2: /* SW */
                insn->type = insn_sw;
                return;
            case 0x3: /* SD */
                insn->type = insn_sd;
                return;
            default: unreachable();
            }
        }
        unreachable();
        case 0xc: {
            *insn = insn_rtype_read(data);

            u32 funct3 = FUNCT3(data);
            u32 funct7 = FUNCT7(data);

            switch (funct7) {
            case 0x0: {
                switch (funct3) {
                case 0x0: /* ADD */
                    insn->type = insn_add;
                    return;
                case 0x1: /* SLL */
                    insn->type = insn_sll;
                    return;
                case 0x2: /* SLT */
                    insn->type = insn_slt;
                    return;
                case 0x3: /* SLTU */
                    insn->type = insn_sltu;
                    return;
                case 0x4: /* XOR */
                    insn->type = insn_xor;
                    return;
                case 0x5: /* SRL */
                    insn->type = insn_srl;
                    return;
                case 0x6: /* OR */
                    insn->type = insn_or;
                    return;
                case 0x7: /* AND */
                    insn->type = insn_and;
                    return;
                default: unreachable();
                }
            }
            unreachable();
            case 0x1: {
                switch (funct3) {
                case 0x0: /* MUL */
                    insn->type = insn_mul;
                    return;
                case 0x1: /* MULH */
                    insn->type = insn_mulh;
                    return;
                case 0x2: /* MULHSU */
                    insn->type = insn_mulhsu;
                    return;
                case 0x3: /* MULHU */
                    insn->type = insn_mulhu;
                    return;
                case 0x4: /* DIV */
                    insn->type = insn_div;
                    return;
                case 0x5: /* DIVU */
                    insn->type = insn_divu;
                    return;
                case 0x6: /* REM */
                    insn->type = insn_rem;
                    return;
                case 0x7: /* REMU */
                    insn->type = insn_remu;
                    return;
                default: unreachable();
                }
            }
            unreachable();
            case 0x20: {
                switch (funct3) {
                case 0x0: /* SUB */
                    insn->type = insn_sub;
                    return;
                case 0x5: /* SRA */
                    insn->type = insn_sra;
                    return;
                default: unreachable();
                }
            }
            unreachable();
            default: unreachable();
            }
        }
        unreachable();
        case 0xd: /* LUI */
            *insn = insn_utype_read(data);
            insn->type = insn_lui;
            return;
        case 0xe: {
            *insn = insn_rtype_read(data);

            u32 funct3 = FUNCT3(data);
            u32 funct7 = FUNCT7(data);

            switch (funct7) {
            case 0x0: {
                switch (funct3) {
                case 0x0: /* ADDW */
                    insn->type = insn_addw;
                    return;
                case 0x1: /* SLLW */
                    insn->type = insn_sllw;
                    return;
                case 0x5: /* SRLW */
                    insn->type = insn_srlw;
                    return;
                default: unreachable();
                }
            }
            unreachable();
            case 0x1: {
                switch (funct3) {
                case 0x0: /* MULW */
                    insn->type = insn_mulw;
                    return;
                case 0x4: /* DIVW */
                    insn->type = insn_divw;
                    return;
                case 0x5: /* DIVUW */
                    insn->type = insn_divuw;
                    return;
                case 0x6: /* REMW */
                    insn->type = insn_remw;
                    return;
                case 0x7: /* REMUW */
                    insn->type = insn_remuw;
                    return;
                default: unreachable();
                }
            }
            unreachable();
            case 0x20: {
                switch (funct3) {
                case 0x0: /* SUBW */
                    insn->type = insn_subw;
                    return;
                case 0x5: /* SRAW */
                    insn->type = insn_sraw;
                    return;
                default: unreachable();
                }
            }
            unreachable();
            default: unreachable();
            }
        }
        unreachable();
        case 0x18: {
            *insn = insn_btype_read(data);

            u32 funct3 = FUNCT3(data);
            switch (funct3) {
            case 0x0: /* BEQ */
                insn->type = insn_beq;
                return;
            case 0x1: /* BNE */
                insn->type = insn_bne;
                return;
            case 0x4: /* BLT */
                insn->type = insn_blt;
                return;
            case 0x5: /* BGE */
                insn->type = insn_bge;
                return;
            case 0x6: /* BLTU */
                insn->type = insn_bltu;
                return;
            case 0x7: /* BGEU */
                insn->type = insn_bgeu;
                return;
            default: unreachable();
            }
        }
        unreachable();
        case 0x19: /* JALR */
            *insn = insn_itype_read(data);
            insn->type = insn_jalr;
            insn->cont = true;
            return;
        case 0x1b: /* JAL */
            *insn = insn_jtype_read(data);
            insn->type = insn_jal;
            insn->cont = true;
            return;
        case 0x1c: {
            if (data == 0x73) { /* ECALL */
                insn->type = insn_ecall;
                insn->cont = true;
                return;
            }

            u32 funct3 = FUNCT3(data);
            *insn = insn_csrtype_read(data);
            switch(funct3) {
            case 0x2: /* CSRRS */
                insn->type = insn_csrrs;
                return;
            case 0x6: /* CSRRSI */
                insn->type = insn_csrrsi;
                return;
            default: unreachable();
            }
        }
        unreachable();
        default: unreachable();
        }
    }
    unreachable();
    default: unreachable();
    }
}
