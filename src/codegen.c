#include "rvemu.h"

enum utils_t {
    mulhu,
    mulh,
    mulhsu,
    num_utils,
};

typedef struct {
    bool gp_reg[num_gp_regs];
    bool util[num_utils];
} tracer_t;

static void tracer_reset(tracer_t *t) {
    memset(t, 0, sizeof(tracer_t));
}

#define DEFINE_TRACE_USAGE(name)                                  \
    static void tracer_add_ ## name ## _usage(tracer_t *t, ...) { \
        va_list ap;                                               \
        va_start(ap, t);                                          \
        int n;                                                    \
        while ((n = va_arg(ap, int)) != -1)                       \
            t->name[n] = true;                                    \
        va_end(ap);                                               \
    }                                                             \

DEFINE_TRACE_USAGE(gp_reg);
DEFINE_TRACE_USAGE(util);

#define CODEGEN_UTIL_MULHU                               \
    "inline uint64_t mulhu(uint64_t a, uint64_t b) { \n" \
    "    uint64_t t;                                 \n" \
    "    uint32_t y1, y2, y3;                        \n" \
    "    uint64_t a0 = (uint32_t)a, a1 = a >> 32;    \n" \
    "    uint64_t b0 = (uint32_t)b, b1 = b >> 32;    \n" \
    "    t = a1*b0 + ((a0*b0) >> 32);                \n" \
    "    y1 = t;                                     \n" \
    "    y2 = t >> 32;                               \n" \
    "    t = a0*b1 + y1;                             \n" \
    "    y1 = t;                                     \n" \
    "    t = a1*b1 + y2 + (t >> 32);                 \n" \
    "    y2 = t;                                     \n" \
    "    y3 = t >> 32;                               \n" \
    "    return ((uint64_t)y3 << 32) | y2;           \n" \
    "}                                               \n" \

#define CODEGEN_UTIL_MULH                                          \
    "inline int64_t mulh(int64_t a, int64_t b) {               \n" \
    "    int negate = (a < 0) != (b < 0);                      \n" \
    "    uint64_t res = mulhu(a < 0 ? -a : a, b < 0 ? -b : b); \n" \
    "    return negate ? ~res + (a * b == 0) : res;            \n" \
    "}                                                         \n" \

#define CODEGEN_UTIL_MULHSU                                        \
    "inline int64_t mulhsu(int64_t a, uint64_t b) {            \n" \
    "    int negate = a < 0;                                   \n" \
    "    uint64_t res = mulhu(a < 0 ? -a : a, b);              \n" \
    "    return negate ? ~res + (a * b == 0) : res;            \n" \
    "}                                                         \n" \

static str_t tracer_append_utils(tracer_t *t, str_t s) {
    if (t->util[mulhu] || t->util[mulh] || t->util[mulhsu]) {
        s = str_append(s, CODEGEN_UTIL_MULHU);
    }
    if (t->util[mulh]) {
        s = str_append(s, CODEGEN_UTIL_MULH);
    }
    if (t->util[mulhsu]) {
        s = str_append(s, CODEGEN_UTIL_MULHSU);
    }

    return s;
}

static str_t tracer_append_prologue(tracer_t *t, str_t s) {
    static char buf[128] = {0};
    for (int i = 1; i < num_gp_regs; i++) {
        if (t->gp_reg[i]) {
            sprintf(buf, "    uint64_t x%d = state->gp_regs[%d];\n", i, i);
            s = str_append(s, buf);
        }
    }
    return s;
}

static str_t tracer_append_epilogue(tracer_t *t, str_t s) {
    static char buf[128] = {0};
    for (int i = 1; i < num_gp_regs; i++) {
        if (t->gp_reg[i]) {
            sprintf(buf, "    state->gp_regs[%d] = x%d;\n", i, i);
            s = str_append(s, buf);
        }
    }

    return s;
}

static char funcbuf[128] = {0};
static char funcbuf2[128] = {0};

#define REG_SET_VAL(reg, val)                                 \
    if ((reg) != 0) {                                         \
        sprintf(funcbuf, "    x%d = %ldLL;\n", (reg), (val)); \
        s = str_append(s, funcbuf);                           \
    }                                                         \

#define REG_SET_EXPR(reg, expr)                             \
    if ((reg) != 0) {                                       \
        sprintf(funcbuf, "    x%d = %s;\n", (reg), (expr)); \
        s = str_append(s, funcbuf);                         \
    }                                                       \

#define REG_GET(reg, name)                                          \
    if ((reg) == zero) {                                            \
        s = str_append(s, "    uint64_t " #name " = 0;\n");         \
    } else {                                                        \
        sprintf(funcbuf, "    uint64_t " #name " = x%d;\n", (reg)); \
        s = str_append(s, funcbuf);                                 \
    }                                                               \


#define MEM_LOAD(addr, typ, name)                                                              \
    sprintf(funcbuf, "    %s " #name " = *(%s *)(state->mem + %s);\n", (typ), (typ), (addr));  \
    s = str_append(s, funcbuf);                                                                \

#define MEM_STORE(addr, typ, data)                                                             \
    sprintf(funcbuf, "    *(%s *)(state->mem + %s) = (%s)" #data ";\n", (typ), (addr), (typ)); \
    s = str_append(s, funcbuf);                                                                \

static str_t func_empty(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    return s;
}

#define FUNC(typ)                                              \
    REG_GET(insn->rs1, rs1);                                   \
    sprintf(funcbuf2, "rs1 + (int64_t)%ldLL", (i64)insn->imm); \
    MEM_LOAD(funcbuf2, typ, rd);                               \
    REG_SET_EXPR(insn->rd, "rd");                              \
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rd, -1);  \
    return s;                                                  \

static str_t func_lb(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("int8_t");
}

static str_t func_lh(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("int16_t");
}

static str_t func_lw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("int32_t");
}

static str_t func_ld(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("int64_t");
}

static str_t func_lbu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint8_t");
}

static str_t func_lhu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint16_t");
}

static str_t func_lwu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint32_t");
}

#undef FUNC

#define FUNC(stmt)                                            \
    REG_GET(insn->rs1, rs1);                                  \
    stmt;                                                     \
    REG_SET_EXPR(insn->rd, funcbuf2);                         \
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rd, -1); \
    return s;                                                 \

static str_t func_addi(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "rs1 + (int64_t)%ldLL", (i64)insn->imm)));
}

static str_t func_slli(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "rs1 << %d", insn->imm & 0x3f)));
}

static str_t func_slti(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "(int64_t)rs1 < (int64_t)%ldLL ? 1 : 0", (i64)insn->imm)));
}

static str_t func_sltiu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "rs1 < %luULL ? 1 : 0", (i64)insn->imm)))
}

static str_t func_xori(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "rs1 ^ %ldLL", (i64)insn->imm)));
}

static str_t func_srli(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "rs1 >> %d", insn->imm & 0x3f)));
}

static str_t func_srai(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "(int64_t)rs1 >> %d", insn->imm & 0x3f)));
}

static str_t func_ori(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "rs1 | %luULL", (i64)insn->imm)));
}

static str_t func_andi(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "rs1 & %ldLL", (i64)insn->imm)));
}

#undef FUNC

static str_t func_auipc(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    u64 val = pc + (i64)insn->imm;
    REG_SET_VAL(insn->rd, val);

    tracer_add_gp_reg_usage(tracer, insn->rd, -1);
    return s;
}

#define FUNC(stmt)                                            \
    REG_GET(insn->rs1, rs1);                                  \
    stmt;                                                     \
    REG_SET_EXPR(insn->rd, funcbuf2);                         \
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rd, -1); \
    return s;                                                 \

static str_t func_addiw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "(int64_t)(int32_t)(rs1 + (int64_t)%ldLL)", (i64)insn->imm)));
}

static str_t func_slliw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "(int64_t)(int32_t)(rs1 << %d)", insn->imm & 0x1f)));
}

static str_t func_srliw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "(int64_t)(int32_t)((uint32_t)rs1 >> %d)", insn->imm & 0x1f)));
}

static str_t func_sraiw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((sprintf(funcbuf2, "(int64_t)((int32_t)rs1 >> %d)", insn->imm & 0x1f)));
}

#undef FUNC

#define FUNC(typ)                                              \
    REG_GET(insn->rs1, rs1);                                   \
    REG_GET(insn->rs2, rs2);                                   \
    sprintf(funcbuf2, "rs1 + (int64_t)%ldLL", (i64)insn->imm); \
    MEM_STORE(funcbuf2, typ, rs2);                             \
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rs2, -1); \
    return s;                                                  \

static str_t func_sb(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint8_t");
}

static str_t func_sh(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint16_t");
}

static str_t func_sw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint32_t");
}

static str_t func_sd(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint64_t");
}

#undef FUNC

#define FUNC(expr)                                                       \
    REG_GET(insn->rs1, rs1);                                             \
    REG_GET(insn->rs2, rs2);                                             \
    REG_SET_EXPR(insn->rd, expr);                                        \
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rs2, insn->rd, -1); \
    return s;                                                            \

static str_t func_add(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 + rs2");
}

static str_t func_sll(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 << (rs2 & 0x3f)");
}

static str_t func_slt(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("((int64_t)rs1 < (int64_t)rs2) ? 1 : 0");
}

static str_t func_sltu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
   FUNC("((uint64_t)rs1 < (uint64_t)rs2) ? 1 : 0");
}

static str_t func_xor(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 ^ rs2");
}

static str_t func_srl(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 >> (rs2 & 0x3f)");
}

static str_t func_or(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 | rs2");
}

static str_t func_and(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 & rs2");
}

static str_t func_mul(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 * rs2");
}

static str_t func_mulh(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    tracer_add_util_usage(tracer, mulh, -1);
    FUNC("mulh(rs1, rs2)");
}

static str_t func_mulhsu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    tracer_add_util_usage(tracer, mulhsu, -1);
    FUNC("mulhsu(rs1, rs2)");
}

static str_t func_mulhu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    tracer_add_util_usage(tracer, mulhu, -1);
    FUNC("mulhu(rs1, rs2)");
}

static str_t func_sub(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC(("rs1 - rs2"));
}

static str_t func_sra(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC(("(int64_t)rs1 >> (rs2 & 0x3f)"));
}

static str_t func_remu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(rs2 == 0 ? rs1 : rs1 % rs2)");
}

static str_t func_addw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(int64_t)(int32_t)(rs1 + rs2)");
}

static str_t func_sllw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(int64_t)(int32_t)(rs1 << (rs2 & 0x1f))");
}

static str_t func_srlw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(int64_t)(int32_t)((uint32_t)rs1 >> (rs2 & 0x1f))");
}

static str_t func_mulw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(int64_t)(int32_t)(rs1 * rs2)");
}

static str_t func_divw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(rs2 == 0 ? UINT64_MAX : (int32_t)((int64_t)(int32_t)rs1 / (int64_t)(int32_t)rs2))");
}

static str_t func_divuw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(rs2 == 0 ? UINT64_MAX : (int32_t)((uint32_t)rs1 / (uint32_t)rs2))");
}

static str_t func_remw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(rs2 == 0 ? (int64_t)(int32_t)rs1 : (int64_t)((int32_t)rs1 % (int32_t)rs2))");
}

static str_t func_remuw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(rs2 == 0 ? (int64_t)(int32_t)(uint32_t)rs1 : (int64_t)(int32_t)((uint32_t)rs1 % (uint32_t)rs2))");
}

static str_t func_subw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(int64_t)(int32_t)(rs1 - rs2)");
}

static str_t func_sraw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("(int64_t)(int32_t)((int32_t)rs1 >> (rs2 & 0x1f))");
}

#undef FUNC

#define FUNC(stmt) \
    REG_GET(insn->rs1, rs1);                                             \
    REG_GET(insn->rs2, rs2);                                             \
    stmt;                                                                \
    REG_SET_EXPR(insn->rd, "rd");                                        \
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rs2, insn->rd, -1); \
    return s;                                                            \

static str_t func_div(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((s = str_append(s,
        "    uint64_t rd = 0;                                   \n"
        "    if (rs2 == 0) {                                    \n"
        "        rd = UINT64_MAX;                               \n"
        "    } else if (rs1 == INT64_MIN && rs2 == UINT64_MAX) {\n"
        "        rd = INT64_MIN;                                \n"
        "    } else {                                           \n"
        "        rd = (int64_t)rs1 / (int64_t)rs2;              \n"
        "    }                                                  \n")));
}

static str_t func_divu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((s = str_append(s,
        "    uint64_t rd = 0;    \n"
        "    if (rs2 == 0) {     \n"
        "        rd = UINT64_MAX;\n"
        "    } else {            \n"
        "        rd = rs1 / rs2; \n"
        "    }                   \n")));
}

static str_t func_rem(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC((s = str_append(s,
        "    uint64_t rd = 0;                                   \n"
        "    if (rs2 == 0) {                                    \n"
        "        rd = rs1;                                      \n"
        "    } else if (rs1 == INT64_MIN && rs2 == UINT64_MAX) {\n"
        "        rd = 0;                                        \n"
        "    } else {                                           \n"
        "        rd = (int64_t)rs1 % (int64_t)rs2;              \n"
        "    }                                                  \n")));
}

#undef FUNC

static str_t func_lui(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    tracer_add_gp_reg_usage(tracer, insn->rd, -1);
    REG_SET_VAL(insn->rd, (i64)insn->imm);
    return s;
}

#define FUNC(typ, op)                                                  \
    REG_GET(insn->rs1, rs1);                                           \
    REG_GET(insn->rs2, rs2);                                           \
    u64 target_addr = pc + (i64)insn->imm;                             \
    sprintf(funcbuf, "    if ((%s)rs1 %s (%s)rs2) {\n", typ, op, typ); \
    s = str_append(s, funcbuf);                                        \
    sprintf(funcbuf, "        goto insn_%lx;\n", target_addr);         \
    s = str_append(s, funcbuf);                                        \
    s = str_append(s, "    }\n");                                      \
    stack_push(stack, target_addr);                                    \
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rs2, -1);         \
    return s;                                                          \

static str_t func_beq(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint64_t", "==");
}

static str_t func_bne(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint64_t", "!=");
}

static str_t func_blt(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("int64_t", "<");
}

static str_t func_bge(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("int64_t", ">=");
}

static str_t func_bltu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint64_t", "<");
}

static str_t func_bgeu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint64_t", ">=");
}

#undef FUNC

static str_t func_jalr(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    u64 return_addr = pc + (insn->rvc ? 2 : 4);
    REG_GET(insn->rs1, rs1);
    REG_SET_VAL(insn->rd, return_addr);

    s = str_append(s, "    state->exit_reason = indirect_branch;\n");
    sprintf(funcbuf, "    state->reenter_pc = (rs1 + (int64_t)%ldLL) & ~(uint64_t)1;\n",
            (i64)insn->imm);
    s = str_append(s, funcbuf);
    s = str_append(s, "    goto end;\n");
    s = str_append(s, "}\n");
    tracer_add_gp_reg_usage(tracer, insn->rs1, insn->rd, -1);
    return s;
}

static str_t func_jal(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    u64 return_addr = pc + 4;
    u64 target_addr = pc + (i64)insn->imm;

    REG_SET_VAL(insn->rd, return_addr);
    sprintf(funcbuf, "    goto insn_%lx;\n", target_addr);
    s = str_append(s, funcbuf);
    stack_push(stack, target_addr);
    s = str_append(s, "}\n");

    tracer_add_gp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_ecall(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    s = str_append(s, "    state->exit_reason = ecall;\n");
    sprintf(funcbuf, "    state->reenter_pc = %luULL;\n", pc + 4);
    s = str_append(s, funcbuf);
    s = str_append(s, "    goto end;\n");
    s = str_append(s, "}\n");
    return s;
}

#define FUNC(x)                                                                          \
    if (insn->rd) {                                                                      \
        switch (insn->csr) {                                                             \
        case fflags:                                                                     \
            sprintf(funcbuf, "x%d = (uint64_t)(state->fcsr & 0x1f);\n", insn->rd);       \
            break;                                                                       \
        case frm:                                                                        \
            sprintf(funcbuf, "x%d = (uint64_t)((state->fcsr & 0x7) >> 5);\n", insn->rd); \
            break;                                                                       \
        case fcsr:                                                                       \
            sprintf(funcbuf, "x%d = (uint64_t)(state->fcsr & 0xff);\n", insn->rd);       \
            break;                                                                       \
        default: fatal("unsupported csr");                                               \
        }                                                                                \
        s = str_append(s, funcbuf);                                                      \
    }                                                                                    \
    if (insn->rs1 != 0) {                                                                \
        switch (insn->csr) {                                                             \
        case fflags:                                                                     \
            sprintf(funcbuf, "state->fcsr |= (" x "%d & 0x1f);\n", insn->rs1);           \
            break;                                                                       \
        case frm:                                                                        \
            sprintf(funcbuf, "state->fcsr |= ((" x "%d & 0x7) << 5);\n", insn->rs1);     \
            break;                                                                       \
        case fcsr:                                                                       \
            sprintf(funcbuf, "state->fcsr |= (" x "%d & 0xff);\n", insn->rs1);           \
            break;                                                                       \
        default: fatal("unsupported csr");                                               \
        }                                                                                \
        s = str_append(s, funcbuf);                                                      \
    }                                                                                    \
    return s;                                                                            \

static str_t func_csrrs(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("x");
}

static str_t func_csrrsi(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("");
}

#undef FUNC

typedef str_t (func_t)(str_t, insn_t *, tracer_t *, stack_t *, u64);

func_t *funcs[] = {
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

#define CODEGEN_PROLOGUE                                \
    "enum exit_reason_t {                           \n" \
    "   indirect_branch,                            \n" \
    "   ecall,                                      \n" \
    "};                                             \n" \
    "typedef struct {                               \n" \
    "    enum exit_reason_t exit_reason;            \n" \
    "    uint64_t reenter_pc;                       \n" \
    "    uint64_t gp_regs[32];                      \n" \
    "    uint64_t fp_regs[32];                      \n" \
    "    uint64_t pc;                               \n" \
    "    uint8_t *restrict mem;                     \n" \
    "    uint32_t fcsr;                             \n" \
    "} state_t;                                     \n" \
    "void start(volatile state_t *restrict state) { \n" \

#define CODEGEN_EPILOGUE "}"

str_t machine_genblock(machine_t *m) {
    DECLEAR_STATIC_STR(body);

    static stack_t stack = {0};
    stack_reset(&stack);

    static set_t set;
    set_reset(&set);

    static tracer_t tracer;
    tracer_reset(&tracer);

    stack_push(&stack, m->state.pc);

    u64 pc = -1;

    while (stack_pop(&stack, &pc)) {
        if (!set_add(&set, pc)) {
            continue;
        }

        static char buf[128] = {0};
        static insn_t insn = {0};

        sprintf(buf, "insn_%lx: {\n", pc);
        body = str_append(body, buf);

        machine_decode(m, pc, &insn);
        body = funcs[insn.type](body, &insn, &tracer, &stack, pc);

        if (insn.cont) continue;

        pc += (insn.rvc ? 2 : 4);
        sprintf(buf, "    goto insn_%lx;\n", pc);
        body = str_append(body, buf);
        body = str_append(body, "}\n");
        stack_push(&stack, pc);
    }

    DECLEAR_STATIC_STR(source);
    source = str_append(source, "#include <stdint.h>\n");
    source = tracer_append_utils(&tracer, source);
    source = str_append(source, CODEGEN_PROLOGUE);
    source = tracer_append_prologue(&tracer, source);
    source = str_append(source, body);
    source = str_append(source, "end:\n");
    source = tracer_append_epilogue(&tracer, source);
    source = str_append(source, CODEGEN_EPILOGUE);

    return source;
}
