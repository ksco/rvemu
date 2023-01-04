# rvemu

rvemu is a fast Linux emulator, which can run statically linked RV64GC programs at near-native speed.

Yeah, it's a toy, but a fairly complete and fast one, which is perfect for learning how a JIT emulator works.

Based on this project, PLCT Lab plans to launch an open course "Implementing a JIT Emulator from Scratch". The course is in Chinese.

## Features

1. Fast, much faster than QEMU.
2. Runs on all architectures.
3. Tiny, and easy to understand.

## TODOs

- [ ] Implementing all RV64GC instructions.
- [ ] Better MMU and JIT cache.
- [ ] Implementing all of the Newlib syscalls.
- [ ] Benchmark.
- [ ] Run real-world programs, like Lua or GIT.

## Refs

Inspired by

1. https://github.com/gamozolabs/fuzz_with_emus
2. https://github.com/jart/blink
3. https://github.com/michaeljclark/rv8
