#include "rvemu.h"

#include "interp_util.h"

static void func_empty(state_t *state, insn_t *insn) {}

#define FUNC(typ)                                          \
    u64 addr = state->gp_regs[insn->rs1] + (i64)insn->imm; \
    state->gp_regs[insn->rd] = *(typ *)TO_HOST(addr);      \

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
    FUNC((i64)((i32)rs1 >> (imm & 0x1f)));
}

#undef FUNC

static void func_auipc(state_t *state, insn_t *insn) {
    u64 val = state->pc + (i64)insn->imm;
    state->gp_regs[insn->rd] = val;
}

#define FUNC(typ)                                \
    u64 rs1 = state->gp_regs[insn->rs1];         \
    u64 rs2 = state->gp_regs[insn->rs2];         \
    *(typ *)TO_HOST(rs1 + insn->imm) = (typ)rs2; \

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

#define FUNC()                         \
    switch (insn->csr) {               \
    case fflags:                       \
    case frm:                          \
    case fcsr:                         \
        break;                         \
    default: fatal("unsupported csr"); \
    }                                  \
    state->gp_regs[insn->rd] = 0;      \

static void func_csrrw(state_t *state, insn_t *insn) { FUNC(); }
static void func_csrrs(state_t *state, insn_t *insn) { FUNC(); }
static void func_csrrc(state_t *state, insn_t *insn) { FUNC(); }
static void func_csrrwi(state_t *state, insn_t *insn) { FUNC(); }
static void func_csrrsi(state_t *state, insn_t *insn) { FUNC(); }
static void func_csrrci(state_t *state, insn_t *insn) { FUNC(); }

#undef FUNC

static void func_flw(state_t *state, insn_t *insn) {
    u64 addr = state->gp_regs[insn->rs1] + (i64)insn->imm;
    state->fp_regs[insn->rd].v = *(u32 *)TO_HOST(addr) | ((u64)-1 << 32);
}
static void func_fld(state_t *state, insn_t *insn) {
    u64 addr = state->gp_regs[insn->rs1] + (i64)insn->imm;
    state->fp_regs[insn->rd].v = *(u64 *)TO_HOST(addr);
}

#define FUNC(typ)                                \
    u64 rs1 = state->gp_regs[insn->rs1];         \
    u64 rs2 = state->fp_regs[insn->rs2].v;       \
    *(typ *)TO_HOST(rs1 + insn->imm) = (typ)rs2; \

static void func_fsw(state_t *state, insn_t *insn) {
    FUNC(u32);
}
static void func_fsd(state_t *state, insn_t *insn) {
    FUNC(u64);
}

#undef FUNC

#define FUNC(expr)                            \
    f32 rs1 = state->fp_regs[insn->rs1].f;    \
    f32 rs2 = state->fp_regs[insn->rs2].f;    \
    f32 rs3 = state->fp_regs[insn->rs3].f;    \
    state->fp_regs[insn->rd].f = (f32)(expr); \

static void func_fmadd_s(state_t *state, insn_t *insn) {
    FUNC(rs1 * rs2 + rs3);
}

static void func_fmsub_s(state_t *state, insn_t *insn) {
    FUNC(rs1 * rs2 - rs3);
}

static void func_fnmsub_s(state_t *state, insn_t *insn) {
    FUNC(-(rs1 * rs2) + rs3);
}

static void func_fnmadd_s(state_t *state, insn_t *insn) {
    FUNC(-(rs1 * rs2) - rs3);
}

#undef FUNC

#define FUNC(expr)                         \
    f64 rs1 = state->fp_regs[insn->rs1].d; \
    f64 rs2 = state->fp_regs[insn->rs2].d; \
    f64 rs3 = state->fp_regs[insn->rs3].d; \
    state->fp_regs[insn->rd].d = (expr);   \

static void func_fmadd_d(state_t *state, insn_t *insn) {
    FUNC(rs1 * rs2 + rs3);
}
static void func_fmsub_d(state_t *state, insn_t *insn) {
    FUNC(rs1 * rs2 - rs3);
}
static void func_fnmsub_d(state_t *state, insn_t *insn) {
    FUNC(-(rs1 * rs2) + rs3);
}
static void func_fnmadd_d(state_t *state, insn_t *insn) {
    FUNC(-(rs1 * rs2) - rs3);
}

#undef FUNC

#define FUNC(expr)                                                 \
    f32 rs1 = state->fp_regs[insn->rs1].f;                         \
    __attribute__((unused)) f32 rs2 = state->fp_regs[insn->rs2].f; \
    state->fp_regs[insn->rd].f = (f32)(expr);                      \

static void func_fadd_s(state_t *state, insn_t *insn) {
    FUNC(rs1 + rs2);
}

static void func_fsub_s(state_t *state, insn_t *insn) {
    FUNC(rs1 - rs2);
}

static void func_fmul_s(state_t *state, insn_t *insn) {
    FUNC(rs1 * rs2);
}

static void func_fdiv_s(state_t *state, insn_t *insn) {
    FUNC(rs1 / rs2);
}

static void func_fsqrt_s(state_t *state, insn_t *insn) {
    FUNC(sqrtf(rs1));
}

static void func_fmin_s(state_t *state, insn_t *insn) {
    FUNC(rs1 < rs2 ? rs1 : rs2);
}
static void func_fmax_s(state_t *state, insn_t *insn) {
    FUNC(rs1 > rs2 ? rs1 : rs2);
}

#undef FUNC

#define FUNC(expr)                                                 \
    f64 rs1 = state->fp_regs[insn->rs1].d;                         \
    __attribute__((unused)) f64 rs2 = state->fp_regs[insn->rs2].d; \
    state->fp_regs[insn->rd].d = (expr);                           \

static void func_fadd_d(state_t *state, insn_t *insn) {
    FUNC(rs1 + rs2);
}

static void func_fsub_d(state_t *state, insn_t *insn) {
    FUNC(rs1 - rs2);
}

static void func_fmul_d(state_t *state, insn_t *insn) {
    FUNC(rs1 * rs2);
}

static void func_fdiv_d(state_t *state, insn_t *insn) {
    FUNC(rs1 / rs2);
}

static void func_fsqrt_d(state_t *state, insn_t *insn) {
    FUNC(sqrt(rs1));
}

static void func_fmin_d(state_t *state, insn_t *insn) {
    FUNC(rs1 < rs2 ? rs1 : rs2);
}

static void func_fmax_d(state_t *state, insn_t *insn) {
    FUNC(rs1 > rs2 ? rs1 : rs2);
}

#undef FUNC

#define FUNC(n, x)                                                                    \
    u32 rs1 = state->fp_regs[insn->rs1].w;                                            \
    u32 rs2 = state->fp_regs[insn->rs2].w;                                            \
    state->fp_regs[insn->rd].v = (u64)fsgnj32(rs1, rs2, n, x) | ((uint64_t)-1 << 32); \

static void func_fsgnj_s(state_t *state, insn_t *insn) {
    FUNC(false, false);
}

static void func_fsgnjn_s(state_t *state, insn_t *insn) {
    FUNC(true, false);
}

static void func_fsgnjx_s(state_t *state, insn_t *insn) {
    FUNC(false, true);
}

#undef FUNC

#define FUNC(n, x)                                        \
    u64 rs1 = state->fp_regs[insn->rs1].v;                \
    u64 rs2 = state->fp_regs[insn->rs2].v;                \
    state->fp_regs[insn->rd].v = fsgnj64(rs1, rs2, n, x); \

static void func_fsgnj_d(state_t *state, insn_t *insn) {
    FUNC(false, false);
}
static void func_fsgnjn_d(state_t *state, insn_t *insn) {
    FUNC(true, false);
}
static void func_fsgnjx_d(state_t *state, insn_t *insn) {
    FUNC(false, true);
}

#undef FUNC

static void func_fcvt_w_s(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)(i32)llrintf(state->fp_regs[insn->rs1].f);
}

static void func_fcvt_wu_s(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)(i32)(u32)llrintf(state->fp_regs[insn->rs1].f);
}

static void func_fcvt_w_d(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)(i32)llrint(state->fp_regs[insn->rs1].d);
}

static void func_fcvt_wu_d(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)(i32)(u32)llrint(state->fp_regs[insn->rs1].d);
}

static void func_fcvt_s_w(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].f = (f32)(i32)state->gp_regs[insn->rs1];
}

static void func_fcvt_s_wu(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].f = (f32)(u32)state->gp_regs[insn->rs1];
}

static void func_fcvt_d_w(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].d = (f64)(i32)state->gp_regs[insn->rs1];
}

static void func_fcvt_d_wu(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].d = (f64)(u32)state->gp_regs[insn->rs1];
}

static void func_fmv_x_w(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)(i32)state->fp_regs[insn->rs1].w;
}
static void func_fmv_w_x(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].w = (u32)state->gp_regs[insn->rs1];
}

static void func_fmv_x_d(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = state->fp_regs[insn->rs1].v;
}

static void func_fmv_d_x(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].v = state->gp_regs[insn->rs1];
}

#define FUNC(expr)                         \
    f32 rs1 = state->fp_regs[insn->rs1].f; \
    f32 rs2 = state->fp_regs[insn->rs2].f; \
    state->gp_regs[insn->rd] = (expr);     \

static void func_feq_s(state_t *state, insn_t *insn) {
    FUNC(rs1 == rs2);
}

static void func_flt_s(state_t *state, insn_t *insn) {
    FUNC(rs1 < rs2);
}

static void func_fle_s(state_t *state, insn_t *insn) {
    FUNC(rs1 <= rs2);
}

#undef FUNC

#define FUNC(expr)                         \
    f64 rs1 = state->fp_regs[insn->rs1].d; \
    f64 rs2 = state->fp_regs[insn->rs2].d; \
    state->gp_regs[insn->rd] = (expr);     \

static void func_feq_d(state_t *state, insn_t *insn) {
    FUNC(rs1 == rs2);
}

static void func_flt_d(state_t *state, insn_t *insn) {
    FUNC(rs1 < rs2);
}

static void func_fle_d(state_t *state, insn_t *insn) {
    FUNC(rs1 <= rs2);
}

#undef FUNC

static void func_fclass_s(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = f32_classify(state->fp_regs[insn->rs1].f);
}

static void func_fclass_d(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = f64_classify(state->fp_regs[insn->rs1].d);
}

static void func_fcvt_l_s(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)llrintf(state->fp_regs[insn->rs1].f);
}

static void func_fcvt_lu_s(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (u64)llrintf(state->fp_regs[insn->rs1].f);
}

static void func_fcvt_l_d(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (i64)llrint(state->fp_regs[insn->rs1].d);
}

static void func_fcvt_lu_d(state_t *state, insn_t *insn) {
    state->gp_regs[insn->rd] = (u64)llrint(state->fp_regs[insn->rs1].d);
}

static void func_fcvt_s_l(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].f = (f32)(i64)state->gp_regs[insn->rs1];
}

static void func_fcvt_s_lu(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].f = (f32)(u64)state->gp_regs[insn->rs1];
}

static void func_fcvt_d_l(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].d = (f64)(i64)state->gp_regs[insn->rs1];
}

static void func_fcvt_d_lu(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].d = (f64)(u64)state->gp_regs[insn->rs1];
}

static void func_fcvt_s_d(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].f = (f32)state->fp_regs[insn->rs1].d;
}

static void func_fcvt_d_s(state_t *state, insn_t *insn) {
    state->fp_regs[insn->rd].d = (f64)state->fp_regs[insn->rs1].f;
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
    func_csrrw,
    func_csrrs,
    func_csrrc,
    func_csrrwi,
    func_csrrsi,
    func_csrrci,
    func_flw,
    func_fsw,
    func_fmadd_s,
    func_fmsub_s,
    func_fnmsub_s,
    func_fnmadd_s,
    func_fadd_s,
    func_fsub_s,
    func_fmul_s,
    func_fdiv_s,
    func_fsqrt_s,
    func_fsgnj_s,
    func_fsgnjn_s,
    func_fsgnjx_s,
    func_fmin_s,
    func_fmax_s,
    func_fcvt_w_s,
    func_fcvt_wu_s,
    func_fmv_x_w,
    func_feq_s,
    func_flt_s,
    func_fle_s,
    func_fclass_s,
    func_fcvt_s_w,
    func_fcvt_s_wu,
    func_fmv_w_x,
    func_fcvt_l_s,
    func_fcvt_lu_s,
    func_fcvt_s_l,
    func_fcvt_s_lu,
    func_fld,
    func_fsd,
    func_fmadd_d,
    func_fmsub_d,
    func_fnmsub_d,
    func_fnmadd_d,
    func_fadd_d,
    func_fsub_d,
    func_fmul_d,
    func_fdiv_d,
    func_fsqrt_d,
    func_fsgnj_d,
    func_fsgnjn_d,
    func_fsgnjx_d,
    func_fmin_d,
    func_fmax_d,
    func_fcvt_s_d,
    func_fcvt_d_s,
    func_feq_d,
    func_flt_d,
    func_fle_d,
    func_fclass_d,
    func_fcvt_w_d,
    func_fcvt_wu_d,
    func_fcvt_d_w,
    func_fcvt_d_wu,
    func_fcvt_l_d,
    func_fcvt_lu_d,
    func_fmv_x_d,
    func_fcvt_d_l,
    func_fcvt_d_lu,
    func_fmv_d_x,
};

void exec_block_interp(state_t *state) {
    static insn_t insn = {0};
    while (true) {
        u32 data = *(u32 *)TO_HOST(state->pc);
        insn_decode(&insn, data);

        funcs[insn.type](state, &insn);
        state->gp_regs[zero] = 0;

        if (insn.cont) break;

        state->pc += insn.rvc ? 2 : 4;
    }
}
