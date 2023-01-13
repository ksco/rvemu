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

#define CODEGEN_UTIL_MATHH "#include <math.h>"

#define CODEGEN_UTIL_F32_CLASSIFY                                                                  \
    "union u32_f32 { uint32_t ui; float f; };                                                  \n" \
    "#define signF32UI(a) ((bool) ((uint32_t) (a)>>31))                                        \n" \
    "#define expF32UI(a) ((int_fast16_t) ((a)>>23) & 0xFF)                                     \n" \
    "#define fracF32UI(a) ((a) & 0x007FFFFF)                                                   \n" \
    "#define isNaNF32UI(a) (((~(a) & 0x7F800000) == 0) && ((a) & 0x007FFFFF))                  \n" \
    "#define isSigNaNF32UI(uiA) ((((uiA) & 0x7FC00000) == 0x7F800000) && ((uiA) & 0x003FFFFF)) \n" \
    "static uint16_t f32_classify(float a) {                                                   \n" \
    "    union u32_f32 uA;                                                                     \n" \
    "    uint32_t uiA;                                                                         \n" \
    "    uA.f = a;                                                                             \n" \
    "    uiA = uA.ui;                                                                          \n" \
    "    uint16_t infOrNaN = expF32UI(uiA) == 0xFF;                                            \n" \
    "    uint16_t subnormalOrZero = expF32UI(uiA) == 0;                                        \n" \
    "    bool sign = signF32UI(uiA);                                                           \n" \
    "    bool fracZero = fracF32UI(uiA) == 0;                                                  \n" \
    "    bool isNaN = isNaNF32UI(uiA);                                                         \n" \
    "    bool isSNaN = isSigNaNF32UI(uiA);                                                     \n" \
    "    return                                                                                \n" \
    "        (sign && infOrNaN && fracZero)          << 0 |                                    \n" \
    "        (sign && !infOrNaN && !subnormalOrZero) << 1 |                                    \n" \
    "        (sign && subnormalOrZero && !fracZero)  << 2 |                                    \n" \
    "        (sign && subnormalOrZero && fracZero)   << 3 |                                    \n" \
    "        (!sign && infOrNaN && fracZero)          << 7 |                                   \n" \
    "        (!sign && !infOrNaN && !subnormalOrZero) << 6 |                                   \n" \
    "        (!sign && subnormalOrZero && !fracZero)  << 5 |                                   \n" \
    "        (!sign && subnormalOrZero && fracZero)   << 4 |                                   \n" \
    "        (isNaN &&  isSNaN)                       << 8 |                                   \n" \
    "        (isNaN && !isSNaN)                       << 9;                                    \n" \
    "}                                                                                         \n" \

#define CODEGEN_UTIL_F64_CLASSIFY                                                                                                                        \
    "union u64_f64 { uint64_t ui; double f; };                                                                                                       \n" \
    "#define signF64UI(a) ((bool) ((uint64_t) (a)>>63))                                                                                              \n" \
    "#define expF64UI(a) ((int_fast16_t) ((a)>>52) & 0x7FF)                                                                                          \n" \
    "#define fracF64UI(a) ((a) & UINT64_C(0x000FFFFFFFFFFFFF))                                                                                       \n" \
    "#define isNaNF64UI(a) (((~(a) & UINT64_C(0x7FF0000000000000)) == 0) && ((a) & UINT64_C(0x000FFFFFFFFFFFFF)))                                    \n" \
    "#define isSigNaNF64UI(uiA) ((((uiA) & UINT64_C(0x7FF8000000000000)) == UINT64_C(0x7FF0000000000000)) && ((uiA) & UINT64_C(0x0007FFFFFFFFFFFF))) \n" \
    "static uint16_t f64_classify(double a) {                                                                                                             \n" \
    "    union u64_f64 uA;                                                                                                                           \n" \
    "    uint64_t uiA;                                                                                                                               \n" \
    "    uA.f = a;                                                                                                                                   \n" \
    "    uiA = uA.ui;                                                                                                                                \n" \
    "    uint16_t infOrNaN = expF64UI(uiA) == 0x7FF;                                                                                                      \n" \
    "    uint16_t subnormalOrZero = expF64UI(uiA) == 0;                                                                                                   \n" \
    "    bool sign = signF64UI(uiA);                                                                                                                 \n" \
    "    bool fracZero = fracF64UI(uiA) == 0;                                                                                                        \n" \
    "    bool isNaN = isNaNF64UI(uiA);                                                                                                               \n" \
    "    bool isSNaN = isSigNaNF64UI(uiA);                                                                                                           \n" \
    "    return                                                                                                                                      \n" \
    "        (sign && infOrNaN && fracZero)          << 0 |                                                                                          \n" \
    "        (sign && !infOrNaN && !subnormalOrZero) << 1 |                                                                                          \n" \
    "        (sign && subnormalOrZero && !fracZero)  << 2 |                                                                                          \n" \
    "        (sign && subnormalOrZero && fracZero)   << 3 |                                                                                          \n" \
    "        (!sign && infOrNaN && fracZero)          << 7 |                                                                                         \n" \
    "        (!sign && !infOrNaN && !subnormalOrZero) << 6 |                                                                                         \n" \
    "        (!sign && subnormalOrZero && !fracZero)  << 5 |                                                                                         \n" \
    "        (!sign && subnormalOrZero && fracZero)   << 4 |                                                                                         \n" \
    "        (isNaN &&  isSNaN)                       << 8 |                                                                                         \n" \
    "        (isNaN && !isSNaN)                       << 9;                                                                                          \n" \
    "}                                                                                                                                               \n" \
