#include "rvemu.h"

u8 *machine_compile(machine_t *m, str_t source) {
    static char name[] = "/tmp/rvemuXXXXXX";

    // using `clang` to compile it to an ELF file.
    DECLEAR_STATIC_STR(clang_str);

    clang_str = str_append(clang_str,
        "clang -O3 -march=native -static "
        "-nostdlib "
        "-xc -c -o ");
    clang_str = str_append(clang_str, name);
    clang_str = str_append(clang_str, ".obj - ");

    FILE *f;
    f = popen(clang_str, "w");
    if (f == NULL) fatal("cannot compile program");
    fwrite(source, 1, str_len(source), f);
    pclose(f);

    // using `objcopy` to turn it into a binary
    DECLEAR_STATIC_STR(objcopy_str);

    objcopy_str = str_append(objcopy_str,
        "objcopy -O binary "
        "--only-section=.text ");
    objcopy_str = str_append(objcopy_str, name);
    objcopy_str = str_append(objcopy_str, ".obj ");
    objcopy_str = str_append(objcopy_str, name);
    objcopy_str = str_append(objcopy_str, ".bin");

    f = popen(objcopy_str, "r");
    if (f == NULL) fatal("cannot generate bin file");
    pclose(f);

    DECLEAR_STATIC_STR(binname_str);

    binname_str = str_append(binname_str, name);
    binname_str = str_append(binname_str, ".bin");

    f = fopen(binname_str, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    static u8 *binbuf = NULL;
    if (binbuf) {
        binbuf = realloc(binbuf, fsize);
    } else {
        binbuf = malloc(fsize);
    }

    size_t ret = fread(binbuf, 1, fsize, f);
    assert(ret == fsize);
    fclose(f);

    return cache_add(m->cache, m->state.pc, binbuf, fsize);
}
