#include "rvemu.h"

#define sys_icache_invalidate(addr, size) \
  __builtin___clear_cache((char *)(addr), (char *)(addr) + (size));

static u64 hash(u64 pc) {
    return pc % CACHE_ENTRY_SIZE;
}

cache_t *new_cache() {
    cache_t *cache = (cache_t *)calloc(1, sizeof(cache_t));
    cache->jitcode = (u8 *)mmap(NULL, CACHE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    return cache;
}

#define MAX_SEARCH_COUNT 32
#define CACHE_HOT_COUNT  100000

#define CACHE_IS_HOT (cache->table[index].hot >= CACHE_HOT_COUNT)

u8 *cache_lookup(cache_t *cache, u64 pc) {
    assert(pc != 0);

    u64 index = hash(pc);

    while (cache->table[index].pc != 0) {
        if (cache->table[index].pc == pc) {
            if (CACHE_IS_HOT)
                return cache->jitcode + cache->table[index].offset;
            break;
        }

        index++;
        index = hash(index);
    }

    return NULL;
}

static inline u64 align_to(u64 val, u64 align) {
  if (align == 0) return val;
  return (val + align - 1) & ~(align - 1);
}

u8 *cache_add(cache_t *cache, u64 pc, u8 *code, size_t sz, u64 align) {
    cache->offset = align_to(cache->offset, align);
    assert(cache->offset + sz <= CACHE_SIZE);

    u64 index = hash(pc);
    u64 search_count = 0;
    while (cache->table[index].pc != 0) {
        if (cache->table[index].pc == pc) {
            break;
        }

        index++;
        index = hash(index);

        assert(++search_count <= MAX_SEARCH_COUNT);
    }

    memcpy(cache->jitcode + cache->offset, code, sz);
    cache->table[index].pc = pc;
    cache->table[index].offset = cache->offset;
    cache->offset += sz;
    sys_icache_invalidate(cache->jitcode + cache->table[index].offset, sz);
    return cache->jitcode + cache->table[index].offset;
}

bool cache_hot(cache_t *cache, u64 pc) {
    u64 index = hash(pc);
    u64 search_count = 0;
    while (cache->table[index].pc != 0) {
        if (cache->table[index].pc == pc) {
            cache->table[index].hot = MIN(++cache->table[index].hot, CACHE_HOT_COUNT);
            return CACHE_IS_HOT;
        }

        index++;
        index = hash(index);

        assert(++search_count <= MAX_SEARCH_COUNT);
    }

    cache->table[index].pc = pc;
    cache->table[index].hot = 1;
    return false;
}
