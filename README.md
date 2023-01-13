# `rvemu`

`rvemu` is a fast Linux emulator, which can run statically linked RV64 programs.

Yeah, it's a toy, but a fairly complete and fast one, which is perfect for learning how a JIT emulator works.

## Features

1. Fast, even close to native speed in some cases.
2. Platform independent, runs on all architectures.
3. Tiny, and easy to understand.
4. Targeting RV64IMFDC w/ Newlib (only a small subset of syscalls is implemented).

## TODOs

- [x] Implementing all RV64IMFDC instructions.
- [ ] Benchmark.
- [ ] Run real-world programs, like Lua or GIT.
- [ ] Memory mapped MMU.

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
