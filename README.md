# `rvemu`

`rvemu` is a fast Linux emulator, which can run statically linked RV64 programs.

Yeah, it's a toy, but a fairly complete and fast one, which is perfect for learning how a JIT emulator works.

## Features

1. Faster than QEMU in most cases.
2. Architecture independent, we've tested it under x86_64 and aarch64.
3. Tiny, and easy to understand.
4. Targeting RV64IMFDC w/ Newlib (only a small subset of syscalls is implemented, adding more).

## Usage

`rvemu` can only run under Linux at present, and `clang` needs to be installed to run, as rvemu uses `clang` to generate jit code.

## TODOs

- [ ] Add more syscalls.
- [ ] Consider using a mmaped MMU.

## Notes

1. `rvemu` uses `clang -O3` to generate target code. One interesting thing is that if you use `rvemu` to run unoptimized binaries, (such as those generated with `clang -O0`), you will get a huge performance boost, even much faster than the equivalent unoptimized native binaries.

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
BYTEmark (tm) Native Mode Benchmark ver. 2 (10/95)
NUMERIC SORT        :  Iterations/sec.:       1412.82  Index:  36.23
STRING SORT         :  Iterations/sec.:         71.06  Index:  31.75
BITFIELD            :  Iterations/sec.:  665885840.35  Index: 114.22
FP EMULATION        :  Iterations/sec.:        539.34  Index: 258.80
FOURIER             :  Iterations/sec.:       6721.18  Index:   7.64
```

#### QEMU

```
BYTEmark (tm) Native Mode Benchmark ver. 2 (10/95)
NUMERIC SORT        :  Iterations/sec.:        821.41  Index:  21.07
STRING SORT         :  Iterations/sec.:         25.23  Index:  11.27
BITFIELD            :  Iterations/sec.:  406361881.21  Index:  69.71
FP EMULATION        :  Iterations/sec.:        162.38  Index:  77.92
FOURIER             :  Iterations/sec.:       5872.21  Index:   6.68
```

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
4. https://github.com/OpenXiangShan/NEMU
