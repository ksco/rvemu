#include "rvemu.h"

inline uint64_t mulhu(uint64_t a, uint64_t b) {
    uint64_t t;
    uint32_t y1, y2, y3;
    uint64_t a0 = (uint32_t)a, a1 = a >> 32;
    uint64_t b0 = (uint32_t)b, b1 = b >> 32;
    t = a1*b0 + ((a0*b0) >> 32);
    y1 = t;
    y2 = t >> 32;
    t = a0*b1 + y1;
    y1 = t;
    t = a1*b1 + y2 + (t >> 32);
    y2 = t;
    y3 = t >> 32;
    return ((uint64_t)y3 << 32) | y2;
}

inline int64_t mulh(int64_t a, int64_t b) {
    int negate = (a < 0) != (b < 0);
    uint64_t res = mulhu(a < 0 ? -a : a, b < 0 ? -b : b);
    return negate ? ~res + (a * b == 0) : res;
}

inline int64_t mulhsu(int64_t a, uint64_t b) {
    int negate = a < 0;
    uint64_t res = mulhu(a < 0 ? -a : a, b);
    return negate ? ~res + (a * b == 0) : res;
}

static void func_empty(state_t *state, insn_t *insn) {}

#define FUNC(typ)                                           \
    u64 addr = state->gp_regs[insn->rs1] + (i64)insn->imm;  \
    state->gp_regs[insn->rd] = *(typ *)(state->mem + addr); \

static void func_lb(state_t *state, insn_t *insn) {
    FUNC(i8);
}

static void func_lh(state_t *state, insn_t *insn) {
    FUNC(i16);
}

static void func_lw(state_t *state, insn_t *insn) {
    FUNC(i32);
}

static void func_ld(state_t *state, insn_t *insn) {
    FUNC(i64);
}

static void func_lbu(state_t *state, insn_t *insn) {
    FUNC(u8);
}

static void func_lhu(state_t *state, insn_t *insn) {
    FUNC(u16);
}

static void func_lwu(state_t *state, insn_t *insn) {
    FUNC(u32);
}

#undef FUNC

#define FUNC(expr)                       \
    u64 rs1 = state->gp_regs[insn->rs1]; \
    i64 imm = (i64)insn->imm;            \
    state->gp_regs[insn->rd] = (expr);   \

static void func_addi(state_t *state, insn_t *insn) {
    FUNC(rs1 + imm);
}

static void func_slli(state_t *state, insn_t *insn) {
    FUNC(rs1 << (imm & 0x3f));
}

static void func_slti(state_t *state, insn_t *insn) {
    FUNC((i64)rs1 < (i64)imm);
}

static void func_sltiu(state_t *state, insn_t *insn) {
    FUNC((u64)rs1 < (u64)imm);
}

static void func_xori(state_t *state, insn_t *insn) {
    FUNC(rs1 ^ imm);
}

static void func_srli(state_t *state, insn_t *insn) {
    FUNC(rs1 >> (imm & 0x3f));
}

static void func_srai(state_t *state, insn_t *insn) {
    FUNC((i64)rs1 >> (imm & 0x3f));
}

static void func_ori(state_t *state, insn_t *insn) {
    FUNC(rs1 | (u64)imm);
}

static void func_andi(state_t *state, insn_t *insn) {
    FUNC(rs1 & (u64)imm);
}

static void func_addiw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)(rs1 + imm));
}

static void func_slliw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)(rs1 << (imm & 0x1f)));
}

static void func_srliw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)((u32)rs1 >> (imm & 0x1f)));
}

static void func_sraiw(state_t *state, insn_t *insn) {
    FUNC((i64)((i32)rs1 + (imm & 0x1f)));
}

#undef FUNC

static void func_auipc(state_t *state, insn_t *insn) {
    u64 val = state->pc + (i64)insn->imm;
    state->gp_regs[insn->rd] = val;
}

#define FUNC(typ)                                      \
    u64 rs1 = state->gp_regs[insn->rs1];               \
    u64 rs2 = state->gp_regs[insn->rs2];               \
    *(typ *)(state->mem + rs1 + insn->imm) = (typ)rs2; \

static void func_sb(state_t *state, insn_t *insn) {
    FUNC(u8);
}

static void func_sh(state_t *state, insn_t *insn) {
    FUNC(u16);
}

static void func_sw(state_t *state, insn_t *insn) {
    FUNC(u32);
}

static void func_sd(state_t *state, insn_t *insn) {
    FUNC(u64);
}

#undef FUNC

#define FUNC(expr) \
    u64 rs1 = state->gp_regs[insn->rs1]; \
    u64 rs2 = state->gp_regs[insn->rs2]; \
    state->gp_regs[insn->rd] = (expr);   \

static void func_add(state_t *state, insn_t *insn) {
    FUNC(rs1 + rs2);
}

static void func_sll(state_t *state, insn_t *insn) {
    FUNC(rs1 << (rs2 & 0x3f));
}

static void func_slt(state_t *state, insn_t *insn) {
    FUNC((i64)rs1 < (i64)rs2);
}

static void func_sltu(state_t *state, insn_t *insn) {
    FUNC((u64)rs1 < (u64)rs2);
}

static void func_xor(state_t *state, insn_t *insn) {
    FUNC(rs1 ^ rs2);
}

static void func_srl(state_t *state, insn_t *insn) {
    FUNC(rs1 >> (rs2 & 0x3f));
}

static void func_or(state_t *state, insn_t *insn) {
    FUNC(rs1 | rs2);
}

static void func_and(state_t *state, insn_t *insn) {
    FUNC(rs1 & rs2);
}

static void func_mul(state_t *state, insn_t *insn) {
    FUNC(rs1 * rs2);
}

static void func_mulh(state_t *state, insn_t *insn) {
    FUNC(mulh(rs1, rs2));
}

static void func_mulhsu(state_t *state, insn_t *insn) {
    FUNC(mulhsu(rs1, rs2));
}

static void func_mulhu(state_t *state, insn_t *insn) {
    FUNC(mulhu(rs1, rs2));
}

static void func_sub(state_t *state, insn_t *insn) {
    FUNC(rs1 - rs2);
}

static void func_sra(state_t *state, insn_t *insn) {
    FUNC((i64)rs1 >> (rs2 & 0x3f));
}

static void func_remu(state_t *state, insn_t *insn) {
    FUNC(rs2 == 0 ? rs1 : rs1 % rs2);
}

static void func_addw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)(rs1 + rs2));
}

static void func_sllw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)(rs1 << (rs2 & 0x1f)));
}

static void func_srlw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)((u32)rs1 >> (rs2 & 0x1f)));
}

static void func_mulw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)(rs1 * rs2));
}

static void func_divw(state_t *state, insn_t *insn) {
    FUNC(rs2 == 0 ? UINT64_MAX : (i32)((i64)(i32)rs1 / (i64)(i32)rs2));
}

static void func_divuw(state_t *state, insn_t *insn) {
    FUNC(rs2 == 0 ? UINT64_MAX : (i32)((u32)rs1 / (u32)rs2));
}

static void func_remw(state_t *state, insn_t *insn) {
    FUNC(rs2 == 0 ? (i64)(i32)rs1 : (i64)(i32)((i64)(i32)rs1 % (i64)(i32)rs2));
}

static void func_remuw(state_t *state, insn_t *insn) {
    FUNC(rs2 == 0 ? (i64)(i32)(u32)rs1 : (i64)(i32)((u32)rs1 % (u32)rs2));
}

static void func_subw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)(rs1 - rs2));
}

static void func_sraw(state_t *state, insn_t *insn) {
    FUNC((i64)(i32)((i32)rs1 >> (rs2 & 0x1f)));
}

#undef FUNC

static void func_div(state_t *state, insn_t *insn) {
    u64 rs1 = state->gp_regs[insn->rs1];
    u64 rs2 = state->gp_regs[insn->rs2];
    u64 rd = 0;
    if (rs2 == 0) {
        rd = UINT64_MAX;
    } else if (rs1 == INT64_MIN && rs2 == UINT64_MAX) {
        rd = INT64_MIN;
    } else {
        rd = (i64)rs1 / (i64)rs2;
    }
    state->gp_regs[insn->rd] = rd;
}

static void func_divu(state_t *state, insn_t *insn) {
    u64 rs1 = state->gp_regs[insn->rs1];
    u64 rs2 = state->gp_regs[insn->rs2];
    u64 rd = 0;
    if (rs2 == 0) {
        rd = UINT64_MAX;
    } else {
        rd = rs1 / rs2;
    }
    state->gp_regs[insn->rd] = rd;
}

static void func_rem(state_t *state, insn_t *insn) {
    u64 rs1 = state->gp_regs[insn->rs1];
    u64 rs2 = state->gp_regs[insn->rs2];
    u64 rd = 0;
    if (rs2 == 0) {
        rd = rs1;
    } else if (rs1 == INT64_MIN && rs2 == UINT64_MAX) {
        rd = 0;
    } else {
        rd = (i64)rs1 % (i64)rs2;
    }
    state->gp_regs[insn->rd] = rd;
}

static void func_lui(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)insn->imm;
}

#define FUNC(expr)                                   \
    u64 rs1 = state->gp_regs[insn->rs1];             \
    u64 rs2 = state->gp_regs[insn->rs2];             \
    u64 target_addr = state->pc + (i64)insn->imm;    \
    if (expr) {                                      \
        state->reenter_pc = state->pc = target_addr; \
        state->exit_reason = direct_branch;          \
        insn->cont = true;                           \
    }                                                \

static void func_beq(state_t *state, insn_t *insn) {
    FUNC((u64)rs1 == (u64)rs2);
}

static void func_bne(state_t *state, insn_t *insn) {
    FUNC((u64)rs1 != (u64)rs2);
}

static void func_blt(state_t *state, insn_t *insn) {
    FUNC((i64)rs1 < (i64)rs2);
}

static void func_bge(state_t *state, insn_t *insn) {
    FUNC((i64)rs1 >= (i64)rs2);
}

static void func_bltu(state_t *state, insn_t *insn) {
    FUNC((u64)rs1 < (u64)rs2);
}

static void func_bgeu(state_t *state, insn_t *insn) {
    FUNC((u64)rs1 >= (u64)rs2);
}

#undef FUNC

static void func_jalr(state_t *state, insn_t *insn) {
    u64 rs1 = state->gp_regs[insn->rs1];
    state->gp_regs[insn->rd] = state->pc + (insn->rvc ? 2 : 4);
    state->exit_reason = indirect_branch;
    state->reenter_pc = (rs1 + (i64)insn->imm) & ~(u64)1;
}

static void func_jal(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = state->pc + (insn->rvc ? 2 : 4);
    state->reenter_pc = state->pc = state->pc + (i64)insn->imm;
    state->exit_reason = direct_branch;
}

static void func_ecall(state_t *state, insn_t *insn) {
    state->exit_reason = ecall;
    state->reenter_pc = state->pc + 4;
}

#define FUNC(expr)                                                      \
    if (insn->rd) {                                                     \
        switch (insn->csr) {                                            \
        case fflags:                                                    \
            state->gp_regs[insn->rd] = (u64)(state->fcsr & 0x1f);       \
            break;                                                      \
        case frm:                                                       \
            state->gp_regs[insn->rd] = (u64)((state->fcsr & 0x7) >> 5); \
            break;                                                      \
        case fcsr:                                                      \
            state->gp_regs[insn->rd] = (u64)(state->fcsr & 0xff);       \
            break;                                                      \
        default: fatal("unsupported csr");                              \
        }                                                               \
    }                                                                   \
    if (insn->rs1 != 0) {                                               \
        switch (insn->csr) {                                            \
        case fflags:                                                    \
            state->fcsr |= ((expr) & 0x1f);                             \
            break;                                                      \
        case frm:                                                       \
            state->fcsr |= (((expr) & 0x7) << 5);                       \
            break;                                                      \
        case fcsr:                                                      \
            state->fcsr |= ((expr) & 0xff);                             \
            break;                                                      \
        default: fatal("unsupported csr");                              \
        }                                                               \
    }                                                                   \

static void func_csrrs(state_t *state, insn_t *insn) {
    FUNC(state->gp_regs[insn->rs1]);
}

static void func_csrrsi(state_t *state, insn_t *insn) {
    FUNC(insn->rs1);
}

typedef void (func_t)(state_t *, insn_t *);

static func_t *funcs[] = {
    func_lb,
    func_lh,
    func_lw,
    func_ld,
    func_lbu,
    func_lhu,
    func_lwu,
    func_empty, // fence
    func_empty, // fence_i
    func_addi,
    func_slli,
    func_slti,
    func_sltiu,
    func_xori,
    func_srli,
    func_srai,
    func_ori,
    func_andi,
    func_auipc,
    func_addiw,
    func_slliw,
    func_srliw,
    func_sraiw,
    func_sb,
    func_sh,
    func_sw,
    func_sd,
    func_add,
    func_sll,
    func_slt,
    func_sltu,
    func_xor,
    func_srl,
    func_or,
    func_and,
    func_mul,
    func_mulh,
    func_mulhsu,
    func_mulhu,
    func_div,
    func_divu,
    func_rem,
    func_remu,
    func_sub,
    func_sra,
    func_lui,
    func_addw,
    func_sllw,
    func_srlw,
    func_mulw,
    func_divw,
    func_divuw,
    func_remw,
    func_remuw,
    func_subw,
    func_sraw,
    func_beq,
    func_bne,
    func_blt,
    func_bge,
    func_bltu,
    func_bgeu,
    func_jalr,
    func_jal,
    func_ecall,
    func_csrrs,
    func_csrrsi,
};

void exec_block_interp(state_t *state) {
    static insn_t insn = {0};
    while (true) {
        u32 data = *(u32 *)(state->mem + state->pc);
        machine_decode(data, &insn);

        funcs[insn.type](state, &insn);
        state->gp_regs[zero] = 0;

        if (insn.cont) break;

        state->pc += insn.rvc ? 2 : 4;
    }
}
