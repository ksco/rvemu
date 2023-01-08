enum gp_reg_type_t {
    zero, ra, sp, gp, tp,
    t0, t1, t2,
    s0, s1,
    a0, a1, a2, a3, a4, a5, a6, a7,
    s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
    t3, t4, t5, t6,
    num_gp_regs,
};

typedef union {
    struct { i64 v; } x;
    struct { u64 v; } xu;
    struct { i32 v; u8 _pad[4]; } w;
    struct { u32 v; u8 _pad[4]; } wu;
    struct { i16 v; u8 _pad[6]; } h;
    struct { u16 v; u8 _pad[6]; } hu;
    struct { i8  v; u8 _pad[7]; } b;
    struct { u8  v; u8 _pad[7]; } bu;
} gp_reg_t;
