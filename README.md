# `rvemu`

`rvemu` is a fast Linux emulator, which can run statically linked RV64 programs.

Yeah, it's a toy, but a fairly complete and fast one, which is perfect for learning how a JIT emulator works.

## Features

1. Faster than QEMU in most cases.
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
real    0m2.518s
user    0m2.483s
sys     0m0.035s
```

#### QEMU

```
real    0m2.800s
user    0m2.795s
sys     0m0.006s
```

---

### [Prime Numbers](https://github.com/tsoding/prime-benchmark/blob/master/prime.c)

> In this very case, rvemu is 3x faster than QEMU!

#### rvemu

```
real    0m54.067s
user    0m54.019s
sys     0m0.047s
```

#### QEMU

```
real    2m58.996s
user    2m58.916s
sys     0m0.050s
```

---

### [nbench](https://github.com/nfinit/ansibench/tree/master/nbench)

#### rvemu

```
NUMERIC SORT        :  Iterations/sec.:       1413.29  Index:  36.24
STRING SORT         :  Iterations/sec.:         71.17  Index:  31.80
BITFIELD            :  Iterations/sec.:  653766423.76  Index: 112.14
FP EMULATION        :  Iterations/sec.:        527.33  Index: 253.04
FOURIER             :  Iterations/sec.:       6345.69  Index:   7.22
ASSIGNMENT          :  Iterations/sec.:         75.19  Index: 286.12
IDEA                :  Iterations/sec.:      10489.30  Index: 160.43
HUFFMAN             :  Iterations/sec.:       4384.40  Index: 121.58
LU DECOMPOSITION    :  Iterations/sec.:        614.64  Index:  31.84
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
LU DECOMPOSITION    :  Iterations/sec.:        241.70  Index:  12.52
```

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
4. https://github.com/OpenXiangShan/NEMU
