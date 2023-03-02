#include "rvemu.h"

static inline size_t str_avail(const str_t str) {
    strhdr_t *h = STRHDR(str);
    return h->alloc - h->len;
}

static str_t str_make_room(str_t str, size_t addlen) {
    size_t avail = str_avail(str);
    if (avail >= addlen) return str;

    size_t curlen = str_len(str);
    size_t newlen = curlen + addlen;
    assert(newlen > curlen);

    if (newlen < STR_MAX_PREALLOC) {
        newlen *= 2;
    } else {
        newlen += STR_MAX_PREALLOC;
    }

    strhdr_t *h = (strhdr_t *)realloc(STRHDR(str), sizeof(strhdr_t) + newlen + 1);
    h->alloc = newlen;
    return h->buf;
}

static inline void str_setlen(str_t str, size_t newlen) {
    STRHDR(str)->len = newlen;
}

str_t str_append(str_t str, const char *t) {
    size_t len = strlen(t);
    str = str_make_room(str, len);
    size_t curlen = str_len(str);
    memcpy(str + curlen, t, len);
    str_setlen(str, curlen + len);
    str[curlen + len] = '\0';
    return str;
}

void str_clear(str_t str) {
    str_setlen(str, 0);
    str[0] = '\0';
}
