#include "rvemu.h"

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
    "} state_t;                                     \n" \
    "void start(volatile state_t *restrict state) { \n" \

#define CODEGEN_EPILOGUE "}"


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


#define REG_SET_VAL(reg, val)                                \
    if ((reg) != 0) {                                        \
        sprintf(buf, "    x%d = %ldLL;\n", (reg), (val));    \
        body = str_append(body, buf);                        \
    }                                                        \

#define REG_SET_EXPR(reg, expr)                         \
    if ((reg) != 0) {                                   \
        sprintf(buf, "    x%d = %s;\n", (reg), (expr)); \
        body = str_append(body, buf);                   \
    }                                                   \

#define REG_GET(reg, name)                                        \
    if ((reg) == zero) {                                          \
        body = str_append(body, "    uint64_t " #name " = 0;\n"); \
    } else {                                                      \
        sprintf(buf, "    uint64_t " #name " = x%d;\n", (reg));   \
        body = str_append(body, buf);                             \
    }                                                             \


#define MEM_LOAD(addr, typ, name)                                                         \
    sprintf(buf, "    %s " #name " = *(%s *)(state->mem + %s);\n", (typ), (typ), (addr)); \
    body = str_append(body, buf);                                                         \

#define MEM_STORE(addr, typ, data)                                                         \
    sprintf(buf, "    *(%s *)(state->mem + %s) = (%s)" #data ";\n", (typ), (addr), (typ)); \
    body = str_append(body, buf);                                                          \

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

        static char buf[8 * 1024] = {0};
        static char tmpbuf[128] = {0};
        u32 data = mmu_read32(&m->mmu, pc);

        sprintf(buf, "insn_%lx: {\n", pc);
        body = str_append(body, buf);

        u32 quadrant = QUADRANT(data);
        bool rvc = true;
        switch (quadrant) {
        case 0x0: {
            u32 copcode = COPCODE(data);

            switch (copcode) {
            case 0x0: { /* C.ADDI4SPN */
                insn_ciwtype insn = insn_ciwtype_read(data);
                assert(insn.imm != 0);
                REG_GET(sp, rs1);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                REG_SET_EXPR(insn.rd, tmpbuf);

                tracer_add_gp_reg_usage(&tracer, sp, insn.rd, -1);
            }
            break;
            case 0x2: { /* C.LW */
                insn_cltype insn = insn_cltype_read(data);
                REG_GET(insn.rs1, rs1);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_LOAD(tmpbuf, "int32_t", rd);
                REG_SET_EXPR(insn.rd, "(int64_t)rd");

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rd, -1);
            }
            break;
            case 0x3: { /* C.LD */
                insn_cltype insn = insn_cltype_read2(data);
                REG_GET(insn.rs1, rs1);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_LOAD(tmpbuf, "int64_t", rd);
                REG_SET_EXPR(insn.rd, "rd");

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rd, -1);
            }
            break;
            case 0x6: { /* C.SW */
                insn_cstype insn = insn_cstype_read2(data);
                REG_GET(insn.rs1, rs1);
                REG_GET(insn.rs2, rs2);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_STORE(tmpbuf, "uint32_t", rs2);

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, -1);
            }
            break;
            case 0x7: { /* C.SD */
                insn_cstype insn = insn_cstype_read(data);
                REG_GET(insn.rs1, rs1);
                REG_GET(insn.rs2, rs2);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_STORE(tmpbuf, "uint64_t", rs2);

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, -1);
            }
            break;
            default: fatal("unimplemented");
            }
        }
        break;
        case 0x1: {
            u32 copcode = COPCODE(data);

            switch (copcode) {
            case 0x0: { /* C.ADDI */
                insn_citype insn = insn_citype_read(data);
                REG_GET(insn.rd, rd);
                sprintf(tmpbuf, "rd + (int64_t)%ldLL", (i64)insn.imm);
                REG_SET_EXPR(insn.rd, tmpbuf);

                tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
            }
            break;
            case 0x1: { /* C.ADDIW */
                insn_citype insn = insn_citype_read(data);
                assert(insn.rd != 0);
                REG_GET(insn.rd, rd);
                sprintf(tmpbuf, "(int64_t)(int32_t)(rd + (int64_t)%ldLL)", (i64)insn.imm);
                REG_SET_EXPR(insn.rd, tmpbuf);

                tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
            }
            break;
            case 0x2: { /* C.LI */
                insn_citype insn = insn_citype_read(data);
                REG_SET_VAL(insn.rd, (i64)insn.imm);

                tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
            }
            break;
            case 0x3: {
                i32 rd = RC1(data);
                if (rd == 2) { /* C.ADDI16SP */
                    insn_citype insn = insn_citype_read3(data);
                    assert(insn.imm != 0);
                    REG_GET(insn.rd, rd);
                    sprintf(tmpbuf, "rd + (int64_t)%ldLL", (i64)insn.imm);
                    REG_SET_EXPR(insn.rd, tmpbuf);
                } else { /* C.LUI */
                    insn_citype insn = insn_citype_read5(data);
                    assert(insn.imm != 0);
                    sprintf(tmpbuf, "(int64_t)%ldLL", (i64)insn.imm);
                    REG_SET_EXPR(insn.rd, tmpbuf);
                }

                tracer_add_gp_reg_usage(&tracer, rd, -1);
            }
            break;
            case 0x4: {
                u32 cfunct2high = CFUNCT2HIGH(data);

                switch (cfunct2high) {
                case 0x0:   /* C.SRLI */
                case 0x1:   /* C.SRAI */
                case 0x2: { /* C.ANDI */
                    insn_cbtype insn = insn_cbtype_read2(data);

                    REG_GET(insn.rs1, rs1);
                    if (cfunct2high == 0x0) {
                        sprintf(tmpbuf, "rs1 >> %d", insn.imm & 0x3f);
                    } else if (cfunct2high == 0x1) {
                        sprintf(tmpbuf, "(int64_t)rs1 >> %d", insn.imm & 0x3f);
                    } else {
                        sprintf(tmpbuf, "rs1 & %ldLL", (i64)insn.imm);
                    }
                    REG_SET_EXPR(insn.rs1, tmpbuf);

                    tracer_add_gp_reg_usage(&tracer, insn.rs1, -1);
                }
                break;
                case 0x3: {
                    u32 cfunct1 = CFUNCT1(data);

                    switch (cfunct1) {
                    case 0x0: {
                        u32 cfunct2low = CFUNCT2LOW(data);

                        insn_catype insn = insn_catype_read(data);
                        REG_GET(insn.rs1, rs1);
                        REG_GET(insn.rs2, rs2);

                        char *expr = NULL;
                        switch (cfunct2low) {
                        case 0x0: /* C.SUB */
                            expr = "rs1 - rs2";
                            break;
                        case 0x1: /* C.XOR */
                            expr = "rs1 ^ rs2";
                            break;
                        case 0x2: /* C.OR */
                            expr = "rs1 | rs2";
                            break;
                        case 0x3: /* C.AND */
                            expr = "rs1 & rs2";
                            break;
                        default: unreachable();
                        }

                        REG_SET_EXPR(insn.rs1, expr);

                        tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, -1);
                    }
                    break;
                    case 0x1: {
                        u32 cfunct2low = CFUNCT2LOW(data);

                        insn_catype insn = insn_catype_read(data);
                        REG_GET(insn.rs1, rs1);
                        REG_GET(insn.rs2, rs2);

                        char *expr = NULL;
                        switch (cfunct2low) {
                        case 0x0: /* C.SUBW */
                            expr = "(int64_t)(int32_t)(rs1 - rs2)";
                            break;
                        case 0x1: /* C.ADDW */
                            expr = "(int64_t)(int32_t)(rs1 + rs2)";
                            break;
                        default: unreachable();
                        }

                        REG_SET_EXPR(insn.rs1, expr);

                        tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, -1);
                    }
                    break;
                    default: fatal("unrecognized cfunct1");
                    }
                }
                break;
                default: fatal("unrecognized cfunct2high");
                }
            }
            break;
            case 0x5: { /* C.J */
                insn_cjtype insn = insn_cjtype_read(data);
                u64 target_addr = pc + (i64)insn.imm;
                sprintf(buf, "    goto insn_%lx;\n", target_addr);
                body = str_append(body, buf);
                stack_push(&stack, target_addr);

                body = str_append(body, "}\n");
                continue;
            }
            break;
            case 0x6:   /* C.BEQZ */
            case 0x7: { /* C.BNEZ */
                insn_cbtype insn = insn_cbtype_read(data);

                REG_GET(insn.rs1, rs1);
                char *cmpop = copcode == 0x6 ? "==" : "!=";
                u64 target_addr = pc + (i64)insn.imm;

                sprintf(buf, "    if ((uint64_t)rs1 %s 0) {\n", cmpop);
                body = str_append(body, buf);
                sprintf(buf, "        goto insn_%lx;\n", target_addr);
                body = str_append(body, buf);
                body = str_append(body, "    }\n");

                stack_push(&stack, target_addr);
                tracer_add_gp_reg_usage(&tracer, insn.rs1, -1);
            }
            break;
            default: fatal("unrecognized copcode");
            }
        }
        break;
        case 0x2: {
            u32 copcode = COPCODE(data);
            switch (copcode) {
            case 0x0: { /* C.SLLI */
                insn_citype insn = insn_citype_read(data);

                REG_GET(insn.rd, rd);
                sprintf(tmpbuf, "rd << %d", insn.imm & 0x3f);
                REG_SET_EXPR(insn.rd, tmpbuf);

                tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
            }
            break;
            case 0x2: { /* C.LWSP */
                insn_citype insn = insn_citype_read4(data);
                assert(insn.rd != 0);
                REG_GET(sp, rs1);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_LOAD(tmpbuf, "int32_t", rd);
                REG_SET_EXPR(insn.rd, "rd");

                tracer_add_gp_reg_usage(&tracer, sp, insn.rd, -1);
            }
            break;
            case 0x3: { /* C.LDSP */
                insn_citype insn = insn_citype_read2(data);
                assert(insn.rd != 0);
                REG_GET(sp, rs1);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_LOAD(tmpbuf, "int64_t", rd);
                REG_SET_EXPR(insn.rd, "rd");

                tracer_add_gp_reg_usage(&tracer, sp, insn.rd, -1);
            }
            break;
            case 0x4: {
                u32 cfunct1 = CFUNCT1(data);

                switch (cfunct1) {
                case 0x0: {
                    insn_crtype insn = insn_crtype_read(data);

                    if (insn.rs2 == 0) { /* C.JR */
                        assert(insn.rd != 0);
                        REG_GET(insn.rd, rd);
                        body = str_append(body, "    state->exit_reason = indirect_branch;\n");
                        body = str_append(body, "    state->reenter_pc = rd & ~(uint64_t)1;\n");
                        body = str_append(body, "    goto end;\n");
                        body = str_append(body, "}\n");

                        tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
                        continue;
                    } else { /* C.MV */
                        REG_GET(insn.rs2, rs2);
                        REG_SET_EXPR(insn.rd, "rs2");

                        tracer_add_gp_reg_usage(&tracer, insn.rs2, insn.rd, -1);
                    }
                }
                break;
                case 0x1: {
                    insn_crtype insn = insn_crtype_read(data);
                    if (insn.rd == 0 && insn.rs2 == 0) { /* C.EBREAK */
                        fatal("unimplmented");
                    } else if (insn.rs2 == 0) { /* C.JALR */
                        insn_crtype insn = insn_crtype_read(data);
                        u64 return_addr = pc + 2;
                        REG_GET(insn.rd, rd);
                        REG_SET_VAL(ra, return_addr);

                        body = str_append(body, "    state->exit_reason = indirect_branch;\n");
                        sprintf(tmpbuf, "    state->reenter_pc = rd & ~(uint64_t)1;\n");
                        body = str_append(body, tmpbuf);
                        body = str_append(body, "    goto end;\n");
                        body = str_append(body, "}\n");

                        tracer_add_gp_reg_usage(&tracer, ra, insn.rd, -1);
                        continue;
                    } else { /* C.ADD */
                        REG_GET(insn.rd, rs1);
                        REG_GET(insn.rs2, rs2);
                        REG_SET_EXPR(insn.rd, "rs1 + rs2");

                        tracer_add_gp_reg_usage(&tracer, insn.rd, insn.rs2, -1);
                    }
                }
                break;
                default: fatal("unrecognized cfunct1");
                }
            }
            break;
            case 0x6: { /* C.SWSP */
                insn_csstype insn = insn_csstype_read2(data);
                REG_GET(sp, rs1);
                REG_GET(insn.rs2, rs2);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_STORE(tmpbuf, "uint32_t", rs2);

                tracer_add_gp_reg_usage(&tracer, sp, insn.rs2, -1);
            }
            break;
            case 0x7: { /* C.SDSP */
                insn_csstype insn = insn_csstype_read(data);
                REG_GET(sp, rs1);
                REG_GET(insn.rs2, rs2);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_STORE(tmpbuf, "uint64_t", rs2);

                tracer_add_gp_reg_usage(&tracer, sp, insn.rs2, -1);
            }
            break;
            default: fatal("unrecognized copcode");
            }
        }
        break;
        case 0x3: {
            rvc = false;
            u32 opcode = OPCODE(data);
            switch (opcode) {
            case 0x0: {
                u32 funct3 = FUNCT3(data);

                char *typ = NULL;
                switch (funct3) {
                case 0x0: /* LB */
                    typ = "int8_t";
                    break;
                case 0x1: /* LH */
                    typ = "int16_t";
                    break;
                case 0x2: /* LW */
                    typ = "int32_t";
                    break;
                case 0x3: /* LD */
                    typ = "int64_t";
                    break;
                case 0x4: /* LBU */
                    typ = "uint8_t";
                    break;
                case 0x5: /* LHU */
                    typ = "uint16_t";
                    break;
                case 0x6: /* LWU */
                    typ = "uint32_t";
                    break;
                default: unreachable();
                }

                insn_itype insn = insn_itype_read(data);
                REG_GET(insn.rs1, rs1);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_LOAD(tmpbuf, typ, rd);
                REG_SET_EXPR(insn.rd, "rd");

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rd, -1);
            }
            break;
            case 0x3: {
                u32 funct3 = FUNCT3(data);

                switch (funct3) {
                case 0x0: { /* FENCE */
                }
                break;
                case 0x1: { /* FENCE.I */
                }
                break;
                default: unreachable();
                }
            }
            break;
            case 0x4: {
                u32 funct3 = FUNCT3(data);

                insn_itype insn = insn_itype_read(data);
                REG_GET(insn.rs1, rs1);

                switch (funct3) {
                case 0x0: { /* ADDI */
                    sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                }
                break;
                case 0x1: {
                    u32 imm116 = IMM116(data);
                    if (imm116 == 0) { /* SLLI */
                        sprintf(tmpbuf, "rs1 << %d", insn.imm & 0x3f);
                    } else {
                        unreachable();
                    }
                }
                break;
                case 0x2: { /* SLTI */
                    sprintf(tmpbuf, "(int64_t)rs1 < (int64_t)%ldLL ? 1 : 0", (i64)insn.imm);
                }
                break;
                case 0x3: { /* SLTIU */
                    sprintf(tmpbuf, "rs1 < %luULL ? 1 : 0", (i64)insn.imm);
                }
                break;
                case 0x4: { /* XORI */
                    sprintf(tmpbuf, "rs1 ^ %ldLL", (i64)insn.imm);
                }
                break;
                case 0x5: {
                    u32 imm116 = IMM116(data);

                    if (imm116 == 0x0) { /* SRLI */
                        sprintf(tmpbuf, "rs1 >> %d", insn.imm & 0x3f);
                    } else if (imm116 == 0x10) { /* SRAI */
                        sprintf(tmpbuf, "(int64_t)rs1 >> %d", insn.imm & 0x3f);
                    } else {
                        unreachable();
                    }
                }
                break;
                case 0x6: { /* ORI */
                    sprintf(tmpbuf, "rs1 | %luULL", (i64)insn.imm);
                }
                break;
                case 0x7: { /* ANDI */
                    sprintf(tmpbuf, "rs1 & %ldLL", (i64)insn.imm);
                }
                break;
                default: fatal("unrecognized funct3");
                }
                REG_SET_EXPR(insn.rd, tmpbuf);

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rd, -1);
            }
            break;
            case 0x5: { /* AUIPC */
                insn_utype insn = insn_utype_read(data);
                u64 val = pc + (i64)insn.imm;
                REG_SET_VAL(insn.rd, val);

                tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
            }
            break;
            case 0x6: {
                u32 funct3 = FUNCT3(data);
                u32 funct7 = FUNCT7(data);

                insn_itype insn = insn_itype_read(data);
                REG_GET(insn.rs1, rs1);

                switch (funct3) {
                case 0x0: { /* ADDIW */
                    sprintf(tmpbuf, "(int64_t)(int32_t)(rs1 + (int64_t)%ldLL)", (i64)insn.imm);
                }
                break;
                case 0x1: { /* SLLIW */
                    assert(funct7 == 0);
                    sprintf(tmpbuf, "(int64_t)(int32_t)(rs1 << %d)", insn.imm & 0x1f);
                }
                break;
                case 0x5: {
                    switch (funct7) {
                    case 0x0: { /* SRLIW */
                        sprintf(tmpbuf, "(int64_t)(int32_t)((uint32_t)rs1 >> %d)", insn.imm & 0x1f);
                    }
                    break;
                    case 0x20: { /* SRAIW */
                        sprintf(tmpbuf, "(int64_t)((int32_t)rs1 >> %d)", insn.imm & 0x1f);
                    }
                    break;
                    default: unreachable();
                    }
                }
                break;
                default: fatal("unimplemented");
                }

                REG_SET_EXPR(insn.rd, tmpbuf);
                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rd, -1);
            }
            break;
            case 0x8: {
                u32 funct3 = FUNCT3(data);

                insn_stype insn = insn_stype_read(data);
                char *typ = NULL;
                switch (funct3) {
                case 0x0: /* SB */
                    typ = "uint8_t";
                    break;
                case 0x1: /* SH */
                    typ = "uint16_t";
                    break;
                case 0x2: /* SW */
                    typ = "uint32_t";
                    break;
                case 0x3: /* SD */
                    typ = "uint64_t";
                    break;
                default: unreachable();
                }

                REG_GET(insn.rs1, rs1);
                REG_GET(insn.rs2, rs2);
                sprintf(tmpbuf, "rs1 + (int64_t)%ldLL", (i64)insn.imm);
                MEM_STORE(tmpbuf, typ, rs2);

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, -1);
            }
            break;
            case 0xc: {
                insn_rtype insn = insn_rtype_read(data);

                REG_GET(insn.rs1, rs1);
                REG_GET(insn.rs2, rs2);

                u32 funct3 = FUNCT3(data);
                u32 funct7 = FUNCT7(data);

                char *expr = NULL;
                switch (funct7) {
                case 0x0: {
                    switch (funct3) {
                    case 0x0: { /* ADD */
                        expr = "rs1 + rs2";
                    }
                    break;
                    case 0x1: { /* SLL */
                        expr = "rs1 << (rs2 & 0x3f)";
                    }
                    break;
                    case 0x2: { /* SLT */
                        expr = "((int64_t)rs1 < (int64_t)rs2) ? 1 : 0";
                    }
                    break;
                    case 0x3: { /* SLTU */
                        expr = "((uint64_t)rs1 < (uint64_t)rs2) ? 1 : 0";
                    }
                    break;
                    case 0x4: { /* XOR */
                        expr = "rs1 ^ rs2";
                    }
                    break;
                    case 0x5: { /* SRL */
                        expr = "rs1 >> (rs2 & 0x3f)";
                    }
                    break;
                    case 0x6: { /* OR */
                        expr = "rs1 | rs2";
                    }
                    break;
                    case 0x7: { /* AND */
                        expr = "rs1 & rs2";
                    }
                    break;
                    default: unreachable();
                    }

                    REG_SET_EXPR(insn.rd, expr);
                    tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, insn.rd, -1);
                }
                break;
                case 0x1: {
                    switch (funct3) {
                    case 0x0: { /* MUL */
                        body = str_append(body, "    uint64_t rd = rs1 * rs2;\n");
                    }
                    break;
                    case 0x1: { /* MULH */
                        body = str_append(body, "    uint64_t rd = mulh(rs1, rs2);\n");

                        tracer_add_util_usage(&tracer, mulh, -1);
                    }
                    break;
                    case 0x2: { /* MULHSU */
                        body = str_append(body, "    uint64_t rd = mulhsu(rs1, rs2);\n");

                        tracer_add_util_usage(&tracer, mulhsu, -1);
                    }
                    break;
                    case 0x3: { /* MULHU */
                        body = str_append(body, "    uint64_t rd = mulhu(rs1, rs2);\n");

                        tracer_add_util_usage(&tracer, mulhu, -1);
                    }
                    break;
                    case 0x4: { /* DIV */
                        body = str_append(body,
                            "    uint64_t rd = 0;                                   \n"
                            "    if (rs2 == 0) {                                    \n"
                            "        rd = UINT64_MAX;                               \n"
                            "    } else if (rs1 == INT64_MIN && rs2 == UINT64_MAX) {\n"
                            "        rd = INT64_MIN;                                \n"
                            "    } else {                                           \n"
                            "        rd = (int64_t)rs1 / (int64_t)rs2;              \n"
                            "    }                                                  \n");
                    }
                    break;
                    case 0x5: { /* DIVU */
                        body = str_append(body,
                            "    uint64_t rd = 0;    \n"
                            "    if (rs2 == 0) {     \n"
                            "        rd = UINT64_MAX;\n"
                            "    } else {            \n"
                            "        rd = rs1 / rs2; \n"
                            "    }                   \n");
                    }
                    break;
                    case 0x6: { /* REM */
                        body = str_append(body,
                            "    uint64_t rd = 0;                                   \n"
                            "    if (rs2 == 0) {                                    \n"
                            "        rd = rs1;                                      \n"
                            "    } else if (rs1 == INT64_MIN && rs2 == UINT64_MAX) {\n"
                            "        rd = 0;                                        \n"
                            "    } else {                                           \n"
                            "        rd = (int64_t)rs1 % (int64_t)rs2;              \n"
                            "    }                                                  \n");
                    }
                    break;
                    case 0x7: { /* REMU */
                        body = str_append(body, "    uint64_t rd = (rs2 == 0 ? rs1 : rs1 % rs2);\n");
                    }
                    break;
                    default: unreachable();
                    }

                    REG_SET_EXPR(insn.rd, "rd");
                    tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, insn.rd, -1);
                }
                break;
                case 0x20: {
                    switch (funct3) {
                    case 0x0: { /* SUB */
                        expr = "rs1 - rs2";
                    }
                    break;
                    case 0x5: { /* SRA */
                        expr = "(int64_t)rs1 >> (rs2 & 0x3f)";
                    }
                    break;
                    default: unreachable();
                    }

                    REG_SET_EXPR(insn.rd, expr);
                    tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, insn.rd, -1);
                }
                break;
                default: unreachable();
                }
            }
            break;
            case 0xd: { /* LUI */
                insn_utype insn = insn_utype_read(data);
                REG_SET_VAL(insn.rd, (i64)insn.imm);

                tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
            }
            break;
            case 0xe: {
                insn_rtype insn = insn_rtype_read(data);

                REG_GET(insn.rs1, rs1);
                REG_GET(insn.rs2, rs2);

                u32 funct3 = FUNCT3(data);
                u32 funct7 = FUNCT7(data);

                char *expr = NULL;
                switch (funct7) {
                case 0x0: {
                    switch (funct3) {
                    case 0x0: { /* ADDW */
                        expr = "(int64_t)(int32_t)(rs1 + rs2)";
                    }
                    break;
                    case 0x1: { /* SLLW */
                        expr = "(int64_t)(int32_t)(rs1 << (rs2 & 0x1f))";
                    }
                    break;
                    case 0x5: { /* SRLW */
                        expr = "(int64_t)(int32_t)((uint32_t)rs1 >> (rs2 & 0x1f))";
                    }
                    break;
                    default: unreachable();
                    }
                }
                break;
                case 0x1: {
                    switch (funct3) {
                    case 0x0: { /* MULW */
                        expr = "(int64_t)(int32_t)(rs1 * rs2)";
                    }
                    break;
                    case 0x4: { /* DIVW */
                        expr = "(rs2 == 0 ? UINT64_MAX : (int32_t)((int64_t)(int32_t)rs1 / (int64_t)(int32_t)rs2))";
                    }
                    break;
                    case 0x5: { /* DIVUW */
                        expr = "(rs2 == 0 ? UINT64_MAX : (int32_t)((uint32_t)rs1 / (uint32_t)rs2))";
                    }
                    break;
                    case 0x6: { /* REMW */
                        expr = "(rs2 == 0 ? (int64_t)(int32_t)rs1 : (int64_t)((int32_t)rs1 % (int32_t)rs2))";
                    }
                    break;
                    case 0x7: { /* REMUW */
                        expr = "(rs2 == 0 ? (int64_t)(int32_t)(uint32_t)rs1 : (int64_t)(int32_t)((uint32_t)rs1 % (uint32_t)rs2))";
                    }
                    break;
                    default: unreachable();
                    }
                }
                break;
                case 0x20: {
                    switch (funct3) {
                    case 0x0: { /* SUBW */
                        expr = "(int64_t)(int32_t)(rs1 - rs2)";
                    }
                    break;
                    case 0x5: { /* SRAW */
                        expr = "(int64_t)(int32_t)((int32_t)rs1 >> (rs2 & 0x1f))";
                    }
                    break;
                    default: unreachable();
                    }
                }
                break;
                default: unreachable();
                }

                REG_SET_EXPR(insn.rd, expr);

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, insn.rd, -1);
            }
            break;
            case 0x18: {
                insn_btype insn = insn_btype_read(data);

                u32 funct3 = FUNCT3(data);
                char *cmptype = NULL;
                char *cmpop = NULL;
                switch (funct3) {
                case 0x0: { /* BEQ */
                    cmptype = "int64_t";
                    cmpop = "==";
                }
                break;
                case 0x1: { /* BNE */
                    cmptype = "int64_t";
                    cmpop = "!=";
                }
                break;
                case 0x4: { /* BLT */
                    cmptype = "int64_t";
                    cmpop = "<";
                }
                break;
                case 0x5: { /* BGE */
                    cmptype = "int64_t";
                    cmpop = ">=";
                }
                break;
                case 0x6: { /* BLTU */
                    cmptype = "uint64_t";
                    cmpop = "<";
                }
                break;
                case 0x7: { /* BGEU */
                    cmptype = "uint64_t";
                    cmpop = ">=";
                }
                break;
                default: unreachable();
                }

                REG_GET(insn.rs1, rs1);
                REG_GET(insn.rs2, rs2);
                u64 target_addr = pc + (i64)insn.imm;
                sprintf(buf, "    if ((%s)rs1 %s (%s)rs2) {\n", cmptype, cmpop, cmptype);
                body = str_append(body, buf);
                sprintf(buf, "        goto insn_%lx;\n", target_addr);
                body = str_append(body, buf);
                body = str_append(body, "    }\n");

                stack_push(&stack, target_addr);
                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rs2, -1);
            }
            break;
            case 0x19: { /* JALR */
                insn_itype insn = insn_itype_read(data);
                u64 return_addr = pc + 4;
                REG_GET(insn.rs1, rs1);
                REG_SET_VAL(insn.rd, return_addr);

                body = str_append(body, "    state->exit_reason = indirect_branch;\n");
                sprintf(tmpbuf, "    state->reenter_pc = (rs1 + (int64_t)%ldLL) & ~(uint64_t)1;\n", (i64)insn.imm);
                body = str_append(body, tmpbuf);
                body = str_append(body, "    goto end;\n");
                body = str_append(body, "}\n");

                tracer_add_gp_reg_usage(&tracer, insn.rs1, insn.rd, -1);
                continue;
            }
            break;
            case 0x1b: { /* JAL */
                insn_jtype insn = insn_jtype_read(data);
                u64 return_addr = pc + 4;
                u64 target_addr = pc + (i64)insn.imm;

                REG_SET_VAL(insn.rd, return_addr);
                sprintf(buf, "    goto insn_%lx;\n", target_addr);
                body = str_append(body, buf);
                stack_push(&stack, target_addr);
                body = str_append(body, "}\n");

                tracer_add_gp_reg_usage(&tracer, insn.rd, -1);
                continue;
            }
            break;
            case 0x1c: {
                if (data == 0x73) { /* ECALL */
                    body = str_append(body, "    state->exit_reason = ecall;\n");
                    sprintf(tmpbuf, "    state->reenter_pc = %luULL;\n", pc + 4);
                    body = str_append(body, tmpbuf);
                    body = str_append(body, "    goto end;\n");
                    body = str_append(body, "}\n");
                } else {
                    fatal("unimplemented");
                }
                continue;
            }
            break;
            default: fatal("unrecognized opcode");
            }
        }
        break;
        default: fatal("unrecognized quadrant");
        }

        pc += (rvc ? 2 : 4);
        sprintf(tmpbuf, "    goto insn_%lx;\n", pc);
        body = str_append(body, tmpbuf);
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
