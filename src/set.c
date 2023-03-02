#include "rvemu.h"

static inline u64 hash(u64 elem) {
    return elem % SET_SIZE;
}

bool set_has(set_t *set, u64 elem) {
    assert(elem != 0);

    u64 index = hash(elem);

    while (set->table[index] != 0) {
        if (set->table[index] == elem) {
            return true;
        }
    }

    return false;
}

#define MAX_SEARCH_COUNT 32

bool set_add(set_t *set, u64 elem) {
    assert(elem != 0);

    u64 index = hash(elem);
    u64 search_count = 0;
    while (set->table[index] != 0) {
        if (set->table[index] == elem) return false;

        index++;
        index = hash(index);

        assert(++search_count <= MAX_SEARCH_COUNT);
    }

    set->table[index] = elem;
    return true;
}

void set_reset(set_t *set) {
    memset(set, 0, sizeof(set_t));
}
