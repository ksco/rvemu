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

`rvemu` can only run under Linux, and `clang` needs to be installed to run, as rvemu uses `clang` to generate jit code.

## Notes

1. `rvemu` uses `clang -O3` to generate highly optimized target code.

2. `rvemu` uses hardfloat technique to gain more performance, just like [NEMU](https://github.com/OpenXiangShan/NEMU), this actually violates the RISC-V standard, but it produces correct results in most cases, and it's way faster than softfloat.

3. `rvemu` uses a linear-mapped MMU similar to [blink](https://github.com/jart/blink), which is really fast.


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
    printf("%d", fib(42));
    return 0;
}
```


```
   rvemu | ###############################################              | 2.181s
    QEMU | ############################################################ | 2.8s
  Native | ####################                                         | 0.924s
```

---



### [Prime Numbers](https://github.com/tsoding/prime-benchmark/blob/master/prime.c)

> In this very case, rvemu is 3x faster than QEMU!

```
   rvemu | #################                                            | 49.852s
    QEMU | ############################################################ | 178.916s
  Native | ##############                                               | 41.002
```

---



### [nbench](https://github.com/nfinit/ansibench/tree/master/nbench) (iterations/second)


#### NUMERIC SORT

```
   rvemu | ############################################################ | 1595.92
    QEMU | ###############################                              | 821.9
  Native | #########################################################    | 1525.98
```

#### STRING SORT

```
   rvemu | ####                                                         | 138.96
    QEMU | #                                                            | 25.24
  Native | ############################################################ | 2381.78
```

#### BITFIELD

```
   rvemu | #########################################################    | 741057761.48
    QEMU | ###############################                              | 406495858.33
  Native | ############################################################ | 781043985.89
```

#### FP EMULATION

```
   rvemu | ################################################             | 591.06
    QEMU | #############                                                | 162.26
  Native | ############################################################ | 732.83
```

#### FOURIER

```
   rvemu | ###                                                          | 6284.49
    QEMU | ##                                                           | 5867.78
  Native | ############################################################ | 149172.7
```

#### ASSIGNMENT

```
   rvemu | ############################################################ | 84.68
    QEMU | #######################                                      | 31.98
  Native | ############################################                 | 61.67
```

#### IDEA

```
   rvemu | #########################################################    | 11070.72
    QEMU | ###########################                                  | 5234.18
  Native | ############################################################ | 11718.87
```

#### HUFFMAN

```
   rvemu | ################################################             | 5070.87
    QEMU | #####################                                        | 2205.89
  Native | ############################################################ | 6274.18
```

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
4. https://github.com/OpenXiangShan/NEMU
