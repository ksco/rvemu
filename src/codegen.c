#include "rvemu.h"

typedef struct {
    bool gp_reg[num_gp_regs];
    bool fp_reg[num_fp_regs];
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
DEFINE_TRACE_USAGE(fp_reg);

static str_t tracer_append_prologue(tracer_t *t, str_t s) {
    static char buf[128] = {0};

    for (int i = 1; i < num_gp_regs; i++) {
        if (!t->gp_reg[i]) continue;
        sprintf(buf, "    uint64_t x%d = state->gp_regs[%d];\n", i, i);
        s = str_append(s, buf);
    }

    for (int i = 0; i < num_fp_regs; i++) {
        if (!t->fp_reg[i]) continue;
        sprintf(buf, "    fp_reg_t f%d = state->fp_regs[%d];\n", i, i);
        s = str_append(s, buf);
    }

    return s;
}

static str_t tracer_append_epilogue(tracer_t *t, str_t s) {
    static char buf[128] = {0};

    for (int i = 1; i < num_gp_regs; i++) {
        if (!t->gp_reg[i]) continue;
        sprintf(buf, "    state->gp_regs[%d] = x%d;\n", i, i);
        s = str_append(s, buf);
    }

    for (int i = 0; i < num_fp_regs; i++) {
        if (!t->fp_reg[i]) continue;
        sprintf(buf, "    state->fp_regs[%d] = f%d;\n", i, i);
        s = str_append(s, buf);
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

#define FREG_SET_EXPR(reg, expr, field)                            \
    sprintf(funcbuf, "    f%d." #field " = %s;\n", (reg), (expr)); \
    s = str_append(s, funcbuf);                                    \

#define FREG_GET(reg, name, typ, field)                                    \
    sprintf(funcbuf, "    " #typ " " #name " = f%d." #field ";\n", (reg)); \
    s = str_append(s, funcbuf);                                            \

#define MEM_LOAD(addr, typ, name)                                                       \
    sprintf(funcbuf, "    %s " #name " = *(%s *)TO_HOST(%s);\n", (typ), (typ), (addr)); \
    s = str_append(s, funcbuf);                                                         \

#define MEM_STORE(addr, typ, data)                                                \
    sprintf(funcbuf, "    *(%s *)TO_HOST(%s) = (%s)" #data ";\n", (typ), (addr), (typ)); \
    s = str_append(s, funcbuf);                                                   \

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
    FUNC((sprintf(funcbuf2, "rs1 & %luULL", (i64)insn->imm)));
}

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

static str_t func_auipc(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    u64 val = pc + (i64)insn->imm;
    REG_SET_VAL(insn->rd, val);

    tracer_add_gp_reg_usage(tracer, insn->rd, -1);
    return s;
}

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
    FUNC("(rs2 == 0 ? (int64_t)(int32_t)rs1 : (int64_t)(int32_t)((int64_t)(int32_t)rs1 % (int64_t)(int32_t)rs2))");
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
    u64 return_addr = pc + (insn->rvc ? 2 : 4);
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

#define FUNC()                                         \
    switch (insn->csr) {                               \
    case fflags:                                       \
    case frm:                                          \
    case fcsr:                                         \
        break;                                         \
    default: fatal("unsupported csr");                 \
    }                                                  \
    if (insn->rd) {                                    \
        sprintf(funcbuf, "    x%d = 0;\n", insn->rd);  \
        tracer_add_gp_reg_usage(tracer, insn->rd, -1); \
        s = str_append(s, funcbuf);                    \
    }                                                  \
    return s;                                          \

static str_t func_csrrw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_csrrs(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_csrrc(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_csrrwi(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_csrrsi(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_csrrci(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

#undef FUNC

#define FUNC(typ, expr)                                        \
    REG_GET(insn->rs1, rs1);                                   \
    sprintf(funcbuf2, "rs1 + (int64_t)%ldLL", (i64)insn->imm); \
    MEM_LOAD(funcbuf2, typ, rd);                               \
    FREG_SET_EXPR(insn->rd, expr, v);                          \
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);            \
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);             \
    return s;                                                  \

static str_t func_flw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint32_t", "rd | ((uint64_t)-1 << 32)");
}

static str_t func_fld(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint64_t", "rd");
}

#undef FUNC

#define FUNC(typ)                                              \
    REG_GET(insn->rs1, rs1);                                   \
    FREG_GET(insn->rs2, rs2, uint64_t, v);                     \
    sprintf(funcbuf2, "rs1 + (int64_t)%ldLL", (i64)insn->imm); \
    MEM_STORE(funcbuf2, typ, rs2);                             \
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);            \
    tracer_add_fp_reg_usage(tracer, insn->rs2, -1);            \
    return s;                                                  \

static str_t func_fsw(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint32_t");
}

static str_t func_fsd(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("uint64_t");
}

#undef FUNC

#define FUNC(expr)                                                                  \
    FREG_GET(insn->rs1, rs1, float, f);                                             \
    FREG_GET(insn->rs2, rs2, float, f);                                             \
    FREG_GET(insn->rs3, rs3, float, f);                                             \
    FREG_SET_EXPR(insn->rd, expr, f);                                               \
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rs2, insn->rs3, insn->rd, -1); \
    return s;                                                                       \

static str_t func_fmadd_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 * rs2 + rs3");
}

static str_t func_fmsub_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 * rs2 - rs3");
}

static str_t func_fnmsub_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("-(rs1 * rs2) + rs3");
}

static str_t func_fnmadd_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("-(rs1 * rs2) - rs3");
}

#undef FUNC

#define FUNC(expr)                                                                   \
    FREG_GET(insn->rs1, rs1, double, d);                                             \
    FREG_GET(insn->rs2, rs2, double, d);                                             \
    FREG_GET(insn->rs3, rs3, double, d);                                             \
    FREG_SET_EXPR(insn->rd, expr, d);                                                \
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rs2, insn->rs3, insn->rd, -1);  \
    return s;                                                                        \

static str_t func_fmadd_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 * rs2 + rs3");
}

static str_t func_fmsub_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 * rs2 - rs3");
}

static str_t func_fnmsub_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("-(rs1 * rs2) + rs3");
}

static str_t func_fnmadd_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("-(rs1 * rs2) - rs3");
}

#undef FUNC

#define FUNC(expr)                                                       \
    FREG_GET(insn->rs1, rs1, float, f);                                  \
    FREG_GET(insn->rs2, rs2, float, f);                                  \
    FREG_SET_EXPR(insn->rd, expr, f);                                    \
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rs2, insn->rd, -1); \
    return s;                                                            \

static str_t func_fadd_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 + rs2");
}

static str_t func_fsub_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 - rs2");
}

static str_t func_fmul_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 * rs2");
}

static str_t func_fdiv_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 / rs2");
}

static str_t func_fmin_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 < rs2 ? rs1 : rs2");
}

static str_t func_fmax_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 > rs2 ? rs1 : rs2");
}

#undef FUNC

static str_t func_fcvt_s_w(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(float)(int32_t)rs1", f);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_fcvt_s_wu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(float)(uint32_t)rs1", f);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_fcvt_d_w(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(double)(int32_t)rs1", d);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_fcvt_d_wu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(double)(uint32_t)rs1", d);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_fmv_x_w(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FREG_GET(insn->rs1, rs1, uint32_t, w);
    REG_SET_EXPR(insn->rd, "(int64_t)(int32_t)rs1");
    tracer_add_gp_reg_usage(tracer, insn->rd, -1);
    tracer_add_fp_reg_usage(tracer, insn->rs1, -1);
    return s;
}

static str_t func_fmv_w_x(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(uint32_t)rs1", w);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_fmv_x_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FREG_GET(insn->rs1, rs1, uint64_t, v);
    REG_SET_EXPR(insn->rd, "rs1");
    tracer_add_gp_reg_usage(tracer, insn->rd, -1);
    tracer_add_fp_reg_usage(tracer, insn->rs1, -1);
    return s;
}


static str_t func_fmv_d_x(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "rs1", v);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

#define FUNC(expr)                                             \
    FREG_GET(insn->rs1, rs1, float, f);                        \
    FREG_GET(insn->rs2, rs2, float, f);                        \
    REG_SET_EXPR(insn->rd, expr);                              \
    tracer_add_gp_reg_usage(tracer, insn->rd, -1);             \
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rs2, -1); \
    return s;                                                  \

static str_t func_feq_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 == rs2");
}

static str_t func_flt_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 < rs2");
}

static str_t func_fle_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 <= rs2");
}

#undef FUNC

#define FUNC(expr)                                             \
    FREG_GET(insn->rs1, rs1, double, d);                       \
    FREG_GET(insn->rs2, rs2, double, d);                       \
    REG_SET_EXPR(insn->rd, expr);                              \
    tracer_add_gp_reg_usage(tracer, insn->rd, -1);             \
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rs2, -1); \
    return s;                                                  \

static str_t func_feq_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 == rs2");
}

static str_t func_flt_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 < rs2");
}

static str_t func_fle_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 <= rs2");
}

#undef FUNC

static str_t func_fcvt_s_l(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(float)(int64_t)rs1", f);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_fcvt_s_lu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(float)(uint64_t)rs1", f);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

#define FUNC(expr)                                                       \
    FREG_GET(insn->rs1, rs1, double, d);                                 \
    FREG_GET(insn->rs2, rs2, double, d);                                 \
    FREG_SET_EXPR(insn->rd, expr, d);                                    \
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rs2, insn->rd, -1); \
    return s;                                                            \

static str_t func_fadd_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 + rs2");
}

static str_t func_fsub_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 - rs2");
}

static str_t func_fmul_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 * rs2");
}

static str_t func_fdiv_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 / rs2");
}

static str_t func_fmin_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 < rs2 ? rs1 : rs2");
}

static str_t func_fmax_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC("rs1 > rs2 ? rs1 : rs2");
}

#undef FUNC

static str_t func_fcvt_s_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FREG_GET(insn->rs1, rs1, double, d);
    FREG_SET_EXPR(insn->rd, "(float)rs1", f);
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rd, -1);
    return s;
}

static str_t func_fcvt_d_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FREG_GET(insn->rs1, rs1, float, f);
    FREG_SET_EXPR(insn->rd, "(double)rs1", d);
    tracer_add_fp_reg_usage(tracer, insn->rs1, insn->rd, -1);
    return s;
}

static str_t func_fcvt_d_l(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(double)(int64_t)rs1", d);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

static str_t func_fcvt_d_lu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    REG_GET(insn->rs1, rs1);
    FREG_SET_EXPR(insn->rd, "(double)(uint64_t)rs1", d);
    tracer_add_gp_reg_usage(tracer, insn->rs1, -1);
    tracer_add_fp_reg_usage(tracer, insn->rd, -1);
    return s;
}

#define FUNC() \
    s = str_append(s, "    state->exit_reason = interp;\n");   \
    sprintf(funcbuf, "    state->reenter_pc = %luULL;\n", pc); \
    s = str_append(s, funcbuf);                                \
    s = str_append(s, "    goto end;\n");                      \
    s = str_append(s, "}\n");                                  \
    insn->cont = true;                                         \
    return s;                                                  \

static str_t func_mulh(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_mulhsu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_mulhu(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsqrt_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_w_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_wu_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_w_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_wu_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fclass_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fclass_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_l_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_lu_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_l_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fcvt_lu_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsgnj_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsgnjn_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsgnjx_s(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsgnj_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsgnjn_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsgnjx_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

static str_t func_fsqrt_d(str_t s, insn_t *insn, tracer_t *tracer, stack_t *stack, u64 pc) {
    FUNC();
}

#undef FUNC

typedef str_t (func_t)(str_t, insn_t *, tracer_t *, stack_t *, u64);

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

#define CODEGEN_PROLOGUE                                \
    "#define OFFSET 0x088800000000ULL               \n" \
    "#define TO_HOST(addr) (addr + OFFSET)          \n" \
    "enum exit_reason_t {                           \n" \
    "   none,                                       \n" \
    "   direct_branch,                              \n" \
    "   indirect_branch,                            \n" \
    "   interp,                                     \n" \
    "   ecall,                                      \n" \
    "};                                             \n" \
    "typedef union {                                \n" \
    "    uint64_t v;                                \n" \
    "    uint32_t w;                                \n" \
    "    double d;                                  \n" \
    "    float f;                                   \n" \
    "} fp_reg_t;                                    \n" \
    "typedef struct {                               \n" \
    "    enum exit_reason_t exit_reason;            \n" \
    "    uint64_t reenter_pc;                       \n" \
    "    uint64_t gp_regs[32];                      \n" \
    "    fp_reg_t fp_regs[32];                      \n" \
    "    uint64_t pc;                               \n" \
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

        u32 data = *(u32 *)TO_HOST(pc);
        insn_decode(&insn, data);
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
    source = str_append(source, "#include <stdbool.h>\n");
    source = str_append(source, CODEGEN_PROLOGUE);
    source = tracer_append_prologue(&tracer, source);
    source = str_append(source, body);
    source = str_append(source, "end:;\n");
    source = tracer_append_epilogue(&tracer, source);
    source = str_append(source, CODEGEN_EPILOGUE);

    return source;
}
