# `rvemu`

`rvemu` is a fast Linux emulator, which can run statically linked RV64 programs.

Yeah, it's a toy, but a fairly complete and fast one, which is perfect for learning how a JIT emulator works.

## Features

1. Faster than QEMU in most cases.
2. Architecture independent, we've tested it under x86_64 and aarch64.
3. Tiny, and easy to understand.
4. Targeting RV64IMFDC w/ Newlib (only a small subset of syscalls is implemented, adding more).

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
NUMERIC SORT        :  Iterations/sec.:        476.15  Index:  12.21
STRING SORT         :  Iterations/sec.:         60.84  Index:  27.19
BITFIELD            :  Iterations/sec.:  185199297.70  Index:  31.77
FP EMULATION        :  Iterations/sec.:         41.56  Index:  19.94
FOURIER             :  Iterations/sec.:       5225.34  Index:   5.94
ASSIGNMENT          :  Iterations/sec.:         20.65  Index:  78.56
IDEA                :  Iterations/sec.:       2890.48  Index:  44.21
HUFFMAN             :  Iterations/sec.:        915.41  Index:  25.38
```

#### QEMU

```
BYTEmark (tm) Native Mode Benchmark ver. 2 (10/95)
NUMERIC SORT        :  Iterations/sec.:        445.17  Index:  11.42
STRING SORT         :  Iterations/sec.:         24.16  Index:  10.80
BITFIELD            :  Iterations/sec.:  144705665.68  Index:  24.82
FP EMULATION        :  Iterations/sec.:         36.46  Index:  17.50
FOURIER             :  Iterations/sec.:       5716.76  Index:   6.50
ASSIGNMENT          :  Iterations/sec.:         16.73  Index:  63.65
IDEA                :  Iterations/sec.:       1902.23  Index:  29.09
HUFFMAN             :  Iterations/sec.:        774.55  Index:  21.48
```

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
4. https://github.com/OpenXiangShan/NEMU
