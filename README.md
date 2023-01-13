# `rvemu`

`rvemu` is a fast Linux emulator, which can run statically linked RV64 programs.

Yeah, it's a toy, but a fairly complete and fast one, which is perfect for learning how a JIT emulator works.

## Features

1. Fast.
2. Platform independent, but only tested it under x86_64 so far.
3. Tiny, and easy to understand.
4. Targeting RV64IMFDC w/ Newlib (only a small subset of syscalls is implemented).

## Benchmark
```
CPU:   Intel Xeon Platinum 8269CY
BENCH: nbench https://github.com/nfinit/ansibench/tree/master/nbench
```

### rvemu

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

### QEMU

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
