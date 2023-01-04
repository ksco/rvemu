#include "rvemu.h"

static u64 hash(u64 pc) {
    return pc % CACHE_ENTRY_SIZE;
}

cache_t *new_cache() {
    cache_t *cache = (cache_t *)calloc(1, sizeof(cache_t));
    cache->jitcode = (u8 *)mmap(NULL, CACHE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    return cache;
}

u8 *cache_lookup(cache_t *cache, u64 pc) {
    assert(pc != 0);

    u64 index = hash(pc);

    while (cache->table[index].pc != 0) {
        if (cache->table[index].pc == pc) {
            return cache->jitcode + cache->table[index].offset;
        }

        index++;
        index = hash(index);
    }

    return NULL;
}

#define MAX_SEARCH_COUNT 32

u8 *cache_add(cache_t *cache, u64 pc, u8 *code, size_t sz) {
    assert(cache->offset + sz <= CACHE_SIZE);

    u64 index = hash(pc);
    u64 search_count = 0;
    while (cache->table[index].pc != 0) {
        if (cache->table[index].pc == pc) {
            return cache->jitcode + cache->table[index].offset;
        }

        index++;
        index = hash(index);

        assert(++search_count <= MAX_SEARCH_COUNT);
    }

    memcpy(cache->jitcode + cache->offset, code, sz);
    cache->table[index].pc = pc;
    cache->table[index].offset = cache->offset;
    cache->offset += sz;
    return cache->jitcode + cache->table[index].offset;
}
