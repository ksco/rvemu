enum gp_reg_type_t {
    zero, ra, sp, gp, tp,
    t0, t1, t2,
    s0, s1,
    a0, a1, a2, a3, a4, a5, a6, a7,
    s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
    t3, t4, t5, t6,
    num_gp_regs,
};

enum fp_reg_type_t {
    ft0, ft1, ft2, ft3, ft4, ft5, ft6, ft7,
    fs0, fs1,
    fa0, fa1, fa2, fa3, fa4, fa5, fa6, fa7,
    fs2, fs3, fs4, fs5, fs6, fs7, fs8, fs9, fs10, fs11,
    ft8, ft9, ft10, ft11,
    num_fp_regs,
};

typedef union {
    u64 v;
    u32 w;
    f64 d;
    f32 f;
} fp_reg_t;
