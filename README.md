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

> All the tests is compiled with `riscv64-unknown-elf-gcc -O3` for RISC-V and `clang-15` for x86 native, and run on Intel Xeon Platinum 8269CY.

### [nbench](https://github.com/nfinit/ansibench/tree/master/nbench) (iterations/second)
#### NUMERIC SORT

```
   rvemu | #######################################################      | 1595.92
    QEMU | ############################                                 | 821.9
  Native | ############################################################ | 1735.89
```


#### STRING SORT

```
   rvemu | ###                                                          | 138.96
    QEMU | #                                                            | 25.24
  Native | ############################################################ | 2680.95
```


#### BITFIELD

```
   rvemu | #########################################################    | 741057761.48
    QEMU | ###############################                              | 406495858.33
  Native | ############################################################ | 783239543.75
```


#### FP EMULATION

```
   rvemu | ##################################                           | 591.06
    QEMU | #########                                                    | 162.26
  Native | ############################################################ | 1040.14
```


#### FOURIER

```
   rvemu | ###                                                          | 6284.49
    QEMU | ##                                                           | 5867.78
  Native | ############################################################ | 147247.94
```


#### ASSIGNMENT

```
   rvemu | ##################################################           | 84.68
    QEMU | ###################                                          | 31.98
  Native | ############################################################ | 102.54
```


#### IDEA

```
   rvemu | #############################################                | 11070.72
    QEMU | #####################                                        | 5234.18
  Native | ############################################################ | 14832.27
```


#### HUFFMAN

```
   rvemu | ############################################################ | 5070.87
    QEMU | ##########################                                   | 2205.89
  Native | ######################################################       | 4599.75
```

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
4. https://github.com/OpenXiangShan/NEMU
