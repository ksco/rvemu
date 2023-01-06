#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "types.h"
#include "elfdef.h"
#include "reg.h"
#include "insn.h"

#define todo(msg) (fprintf(stderr, "warning: %s:%d [TODO] %s\n", __FILE__, __LINE__, msg))
#define fatal(msg) (fprintf(stderr, "fatal: %s:%d %s\n", __FILE__, __LINE__, msg), exit(1))
#define unreachable() (fatal("unreachable"), __builtin_unreachable())

#define ROUNDDOWN(x, k) ((x) & -(k))
#define ROUNDUP(x, k)   (((x) + (k)-1) & -(k))
#define MIN(x, y)       ((y) > (x) ? (x) : (y))
#define MAX(x, y)       ((y) < (x) ? (x) : (y))

/**
 * stack.c
 */
#define STACK_CAP 256
typedef struct {
    i64 top;
    u64 elems[STACK_CAP];
} stack_t;

void stack_push(stack_t *, u64);
bool stack_pop(stack_t *, u64 *);
void stack_reset(stack_t *);
void stack_print(stack_t *);

/**
 * str.c
 */
#define STR_MAX_PREALLOC (1024 * 1024)
#define STRHDR(s) ((strhdr_t *)((s)-(sizeof(strhdr_t))))

#define DECLEAR_STATIC_STR(name)   \
    static str_t name = NULL;  \
    if (name) str_clear(name); \
    else name = str_new();     \

typedef char * str_t;

typedef struct {
    u64 len;
    u64 alloc;
    char buf[];
} strhdr_t;

inline str_t str_new() {
    strhdr_t *h = (strhdr_t *)calloc(1, sizeof(strhdr_t));
    return h->buf;
}

inline size_t str_len(const str_t str) {
    return STRHDR(str)->len;
}

void str_clear(str_t);

str_t str_append(str_t, const char *);

/**
 * mmu.c
*/
typedef struct {
    u8* mem;
    u64 entry;
    u64 alloc;
    u64 cap;
} mmu_t;

void mmu_load_elf(mmu_t *, int, off_t);

inline void mmu_write(mmu_t *mmu, u64 addr, u8 *data, size_t len) {
    memcpy((void *)(mmu->mem + addr), (void *)data, len);
}

inline u32 mmu_read32(mmu_t *mmu, u64 addr) {
    return *(u32 *)(mmu->mem + addr);
}

inline u64 mmu_alloc(mmu_t *mmu, size_t sz) {
    u64 base = mmu->alloc;
    assert(base < mmu->cap);

    mmu->alloc += sz;
    assert(mmu->alloc <= mmu->cap);

    return base;
}

/**
 * cache.c
*/
#define CACHE_ENTRY_SIZE (64 * 1024)
#define CACHE_SIZE       (64 * 1024 * 1024)

typedef struct {
    u64 pc;
    u64 offset;
} cache_item_t;

typedef struct {
    u8 *jitcode;
    u64 offset;
    cache_item_t table[CACHE_ENTRY_SIZE];
} cache_t;

cache_t *new_cache();
u8 *cache_lookup(cache_t *, u64);
u8 *cache_add(cache_t *, u64, u8 *, size_t);

/**
 * static.c
*/
enum exit_reason_t {
    indirect_branch,
    ecall,
};

typedef struct {
    enum exit_reason_t exit_reason;
    u64 reenter_pc;
    u64 gp_regs[num_gp_regs];
    u64 fp_regs[32];
    u64 pc;
    u8 *mem;
} state_t;

void state_print_regs(state_t *);

typedef void (*func_t)(state_t *);

/**
 * machine.c
*/
typedef struct {
    state_t state;
    mmu_t mmu;
    cache_t *cache;
} machine_t;

inline u64 machine_get_gp_reg(machine_t *m, i32 reg) {
    assert(reg >= 0 && reg <= num_gp_regs);
    return m->state.gp_regs[reg];
}

inline void machine_set_gp_reg(machine_t *m, i32 reg, u64 data) {
    assert(reg >= 0 && reg <= num_gp_regs);
    m->state.gp_regs[reg] = data;
}

void machine_setup(machine_t *, int, char **);
str_t machine_genblock(machine_t *);
u8 *machine_compile(machine_t *, str_t);
enum exit_reason_t machine_step(machine_t *);
void machine_load_program(machine_t *, char*);

/**
 * set.c
*/

#define SET_SIZE (32 * 1024)

typedef struct {
    u64 table[SET_SIZE];
} set_t;

bool set_has(set_t *, u64);
bool set_add(set_t *, u64);
void set_reset(set_t *);
