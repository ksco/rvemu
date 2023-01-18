# `rvemu`

`rvemu` is a fast Linux emulator, which can run statically linked RV64 programs.

Yeah, it's a toy, but a fairly complete and fast one, which is perfect for learning how a JIT emulator works.

## Features

1. Faster than QEMU, can achieve native performance in some cases.
2. (Almost*) architecture independent, we've tested it under x86_64.
3. Tiny, and easy to understand.
4. Targeting RV64IMFDC w/ Newlib (only a small subset of syscalls is implemented, adding more).

> *Support for new architecture requires handling relocations in src/compile.c, but it's relatively easy. A few lines of code would do.

## Usage

```
make -j
riscv64-unknown-elf-gcc hello.c
./rvemu a.out
```

`rvemu` can only run under Linux at present, and `clang` needs to be installed to run, as rvemu uses `clang` to generate jit code.

## TODOs

- [ ] Add more syscalls.
- [ ] Consider using a mmaped MMU.

## Notes

1. `rvemu` uses `clang -O3` to generate highly optimized target code.

2. `rvemu` uses hardfloat technique to gain more performance, just like [NEMU](https://github.com/OpenXiangShan/NEMU), this actually violates the RISC-V standard, but it produces correct results in most cases, and it's way faster than softfloat.


## Benchmark

> All the tests is compiled with `riscv64-unknown-elf-gcc -O3` and runs on Intel Xeon Platinum 8269CY

### Recursive Fibonacci

```c
#include <stdio.h>

int fib(int n) {
    if (n == 0 || n == 1) return n;
    return fib(n-1) + fib(n-2);
}

int main(void) {
    printf("%d\n", fib(42));
    return 0;
}
```

#### rvemu

```
real    0m2.181s
user    0m2.146s
sys     0m0.035s
```

#### QEMU

```
real    0m2.800s
user    0m2.795s
sys     0m0.006s
```

#### Native

```
real    0m0.924s
user    0m0.921s
sys     0m0.002s
```

---

### [Prime Numbers](https://github.com/tsoding/prime-benchmark/blob/master/prime.c)

> In this very case, rvemu is 3x faster than QEMU!

#### rvemu

```
real    0m49.852s
user    0m49.778s
sys     0m0.069s
```

#### QEMU

```
real    2m58.996s
user    2m58.916s
sys     0m0.050s
```

#### Native

```
real    0m41.002s
user    0m40.976s
sys     0m0.017s
```

---

### [nbench](https://github.com/nfinit/ansibench/tree/master/nbench)

#### rvemu

```
BYTEmark (tm) Native Mode Benchmark ver. 2 (10/95)
NUMERIC SORT        :  Iterations/sec.:       1595.92  Index:  40.93
STRING SORT         :  Iterations/sec.:        138.96  Index:  62.09
BITFIELD            :  Iterations/sec.:  741057761.48  Index: 127.12
FP EMULATION        :  Iterations/sec.:        591.06  Index: 283.62
FOURIER             :  Iterations/sec.:       6284.49  Index:   7.15
ASSIGNMENT          :  Iterations/sec.:         84.68  Index: 322.24
IDEA                :  Iterations/sec.:      11070.72  Index: 169.32
HUFFMAN             :  Iterations/sec.:       5070.87  Index: 140.62
```

#### QEMU

```
BYTEmark (tm) Native Mode Benchmark ver. 2 (10/95)
NUMERIC SORT        :  Iterations/sec.:        821.90  Index:  21.08
STRING SORT         :  Iterations/sec.:         25.24  Index:  11.28
BITFIELD            :  Iterations/sec.:  406495858.33  Index:  69.73
FP EMULATION        :  Iterations/sec.:        162.26  Index:  77.86
FOURIER             :  Iterations/sec.:       5867.78  Index:   6.67
ASSIGNMENT          :  Iterations/sec.:         31.98  Index: 121.69
IDEA                :  Iterations/sec.:       5234.18  Index:  80.06
HUFFMAN             :  Iterations/sec.:       2205.89  Index:  61.17
```

#### Native

```
BYTEmark (tm) Native Mode Benchmark ver. 2 (10/95)
NUMERIC SORT        :  Iterations/sec.:       1525.98  Index:  39.13
STRING SORT         :  Iterations/sec.:       2381.78  Index: 1064.25
BITFIELD            :  Iterations/sec.:  781043985.89  Index: 133.98
FP EMULATION        :  Iterations/sec.:        732.83  Index: 351.65
FOURIER             :  Iterations/sec.:     149172.70  Index: 169.65
ASSIGNMENT          :  Iterations/sec.:         61.67  Index: 234.68
IDEA                :  Iterations/sec.:      11718.87  Index: 179.24
HUFFMAN             :  Iterations/sec.:       6274.18  Index: 173.98
```

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
4. https://github.com/OpenXiangShan/NEMU
