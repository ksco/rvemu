# 手搓 RISC-V 高性能模拟器

《手搓 RISC-V 高性能模拟器》是中科院软件所 PLCT Lab 推出的一门公开课。在本课程中，我们将使用 C 语言从零开始实现一个高性能 RISC-V 64 位模拟器。在课程的最后，我们会得到一个代码量 4000 行左右的，零依赖的用户态程序模拟器，可以运行一些真实世界的程序，比如 Lua 4.0。如果读者好奇 JIT 模拟器的工作原理，那么本课程正是为你准备的。

我们提前实现了完整的参考代码：[ksco/rvemu](https://github.com/ksco/rvemu)。但本课程推荐的学习方式是自己根据对课程的理解手动实现，作者认为这样可以更扎实地掌握相应的知识。

其他刘阳主讲或参与的 RISC-V 课程：
- [从零开始实现链接器](https://ksco.cc/rvld/) - 刘阳主讲
- [从零开始实现 C 编译器](https://github.com/sunshaoce/rvcc-course) - 刘阳参与

[TOC]

## 课程资源

Bilibili 视频合集：[手搓 RISC-V 高性能模拟器（2023 年春季）](https://space.bilibili.com/296494084/channel/collectiondetail?sid=1245472)，每节课的链接和简介参见课程大纲。

> 如果二维码过期没有及时更新，请给刘阳发邮件：numbksco@gmail.com

QQ 群：

<img src="https://ksco.cool/4TAt" style="zoom:25%; float: left" />

微信群：

<img src="https://ksco.cool/pBFb" style="zoom:25%; float: left;" />

## 环境配置

读者需要有一个 x86-64 架构的 Ubuntu 20.04 或 22.04 环境，真机、虚拟机或者 Docker 皆可。

> 为了简化不必要的繁文缛节，本课程的代码有意地放弃了在不同平台和架构间的可移植性，只保证在 x86-64 架构的 Ubuntu 机器上可以正确运行。移植到其他平台和架构也只需要很少的改动，感兴趣的读者可以自行尝试。

准备好 Ubuntu 之后，读者需要安装 `clang` 和 [RISC-V GNU Compiler Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)。

注意工具链需要编译 RV64 Newlib 版本，如果你懒得自己编译，也可以选择使用 [Release 页面](https://github.com/riscv-collab/riscv-gnu-toolchain/releases)中预先编译好的压缩包，直接下载 `riscv64-elf-ubuntu-`  开头，且与读者 Ubuntu 版本相符的压缩包，解压后即可使用。

配置完成环境后，请确保 `riscv64-unknown-elf-gcc -v` 可以正确返回 `gcc` 的版本。

我们之所以选择使用 Newlib 版本的工具链而不是常见的 glibc，是因为 Newlib 只需要实现很少的系统调用就可以完成大部分的功能，这样可以简化我们模拟器的实现。

## 前置知识

尽管简单易懂是本课程的目标之一，但本课程确实不是面向初学者的课程。它默认读者已经有一些前置的基础知识，或者可通过自学的方式掌握。课程中在涉及到这些前置知识时，基本都是一笔带过，以下是一个列表。

### 1. Git

本课程默认读者可以熟练使用 Git 和 GitHub。

### 2. Makefile

我们在第一课中会使用 Makefile 搭建一个非常简单的构建系统，且后面的课程不再涉及 Makefile 的更改。理论上读者可以完全忽略这个东西，直接拷贝过来使用即可，但如果想要弄懂这个 Makefile 做了什么，则需要一些 Makefile 的基础知识。

### 3. C 语言

本课程默认读者熟悉 C 语言，课程中几乎不会对 C 语言的语法做出解释。

### 4. RISC-V 手册

本课程默认读者熟悉 RISC-V，尤其是 RV64GC 非特权指令集的部分。这部分的知识可以通过阅读 RISC-V 的官方手册掌握：Volume 1, Unprivileged Specification version 20191213 [[PDF](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf)]。

这本书也是一个不错的参考：http://www.riscbook.com/chinese/

## 课程答疑

尽管上面列出了很多前置知识，但我们仍然欢迎初学者参与学习。我们提供了多个渠道解答疑问，但请确保在提问前认真阅读过[提问的智慧](https://github.com/ryanhanwu/How-To-Ask-Questions-The-Smart-Way/blob/main/README-zh_CN.md)。

1. QQ 群/微信群
2. rvemu 的 Issues 区：https://github.com/ksco/rvemu/issues

## 课程安排

### 第一课：搭建开发环境、初始化项目

**视频链接**：[BV1uY4y1D7bJ](https://www.bilibili.com/video/BV1uY4y1D7bJ/) 时长：00:16:38



### 第二课：开始读取可执行文件

本节课和第三课我们解析了 ELF 文件，在第八课实现 JIT 的时候我们也需要解析 ELF 文件。第二、三节课会讲得稍微详细一点，第八课则直接一笔带过了。这部分的知识我们在[从零开始实现链接器](https://ksco.cc/rvld/)课程中有非常详细的介绍，感兴趣的读者可以去了解。

**视频链接**：[BV1jj411c7dJ](https://bilibili.com/video/BV1jj411c7dJ) 时长：00:29:04

**参考链接**：

[Executable and Linkable Format - Wikipedia](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)

[ELF64 File Header](https://fasterthanli.me/content/series/making-our-own-executable-packer/part-1/assets/elf64-file-header.bfa657ccd8ab3a7d.svg)



### 第三课：完成读取可执行文件

**视频链接**：[BV1ym4y1m75A](https://www.bilibili.com/video/BV1ym4y1m75A) 时长：00:36:34



### 第四课：模拟器框架

**视频链接**：[BV1FT411H7aR](https://www.bilibili.com/video/BV1FT411H7aR) 时长：00:32:16



### 第五课：Decoder 模块

**视频链接**：[BV1kz4y1Y7AC](https://www.bilibili.com/video/BV1kz4y1Y7AC) 时长：00:17:53



### 第六课：开始编写解释器模块

**视频链接**：[BV1ng4y157mb](https://www.bilibili.com/video/BV1ng4y157mb) 时长：00:25:12

**勘误**：

@Ryan：第五节课的 06:40 处有一个笔误 `u32 = opcode = OPCODE(data);`，因为最后直接复制了代码，所以没有检查出这个问题。



### 第七课：完成解释器模块

**视频链接**：[BV1CV4y1r7h2](https://www.bilibili.com/video/BV1CV4y1r7h2) 时长：00:55:11



### 第八课：使用 JIT 技术提高性能

**视频链接**：[BV19z4y1t75G](https://www.bilibili.com/video/BV19z4y1t75G) 时长：00:57:47



## 常见问题解答

这里记录了一些读者提出的问题，在提问之前可以先看看这里。

### 试运行期间记录到的问题

**@欧小橙：可以介绍一下主要的几个 C 文件的作用吗？**

答：

`cache.c`

用来实现 JIT 缓存。我们会把热点的代码块（block）编译成本机的机器码，然后存放到 JIT 缓存中。这样，后面如果再次执行到这个代码块，就可以直接以原生的速度来运行了。

> 这个做法基于一个朴素的假设：热点的代码块在未来大概率也是热点，所以我们花时间将其编译成本机的机器码是值得的。

这个文件实现了一个非常简陋的开放寻址哈希表。

`codegen.c`

用来实现 C 代码生成。热点的代码块会在这里会转换成等价的 C 代码。注意到我们生成的 C 代码的函数签名：`void start(volatile state_t *restrict state)`。

其中，`volatile` 关键字是提示编译器不要对 `state` 变量做一些激进的优化（比如删除掉一些看似没用实则非常有用的访存语句）。

而 `restrict` 关键字则是提示编译器 `state` 指针是访问 `state` 所指向内存的唯一方式，这可以让编译器有信心去做某些优化。

`compile.c`

用来将 C 代码编译成机器代码。我们调用了 `clang` 来将 `codegen` 模块生成的 C 代码编译成了本机的机器码，随后会被放入 JIT 缓存中。

这部分代码实现了一个迷你的链接器。主要的原因是 `clang` 在编译 C 代码的时候，有概率会生成一个 `.rodata section` 用于保存一些常量来提高代码的执行效率。这就要求我们在将机器代码放入到 JIT 缓存之前，必须先将 `.text section` 中的重定位解决。这部分的代码涉及到链接器的实现原理，视频中一笔带过了，读者感兴趣的话可以去看[从零开始实现链接器](https://ksco.cc/rvld/)课程。

`decode.c`

用于做指令解码。

`interp.c`

用于实现解释器。

`machine.c`

用于连接各个模块，将模拟器跑起来。

`mmu.c`

内存管理单元。我们使用 `mmap` 实现了一个高效的内存管理单元，关于这个部分的细节，可以参考下面的几个问题。

`syscall.c`

系统调用模块。我们使用了 Newlib，可以通过[此处](https://sourceware.org/newlib/)查看 Newlib 定义的系统调用。

> 简单起见，我们没有实现 `execve`、`fork` 这些系统调用，所以无法运行多进程的程序。

---



**@Ryan：为什么要将虚拟内存的地址分成 host 和 guest 两个，目前看完感觉 guest 的地址作用只是为了辅助定位，并没有实际的操作，也没有实际的值。**

答：因为程序本身是运行在 guest 空间的，比如程序里面可能会直接用 `ld` 指令读取某个 guest 空间的数据。所以这个区分和转换是必要的。

---



**@Ryan：模拟器是如何操作手动分配的栈内存的，有关对 `sp` 的操作，除了在 `machine_setup` 部分，其它地方没有看到。**

答：在程序运行期间，栈的操作是由程序自己完成的，模拟器（或者说操作系统）只需要检测一下溢出即可，但实际上我们（~~为了保持简单~~）也并没有检测。

---



**@Ryan：MMU 除了 `mmap` 是否有其它的方式来分配内存？**

可以直接 `malloc(4 * 1024 * 1024)` 这种方式来分配内存，但这种方式最大的问题是分配出来的内存地址是没办法提前知道的。所以 guest 和 host 之间的转换会更花时间，而且没办法做权限检查，但好处就是确实更简单一些。使用 `mmap` 则操作系统会帮我们做权限检查，但可移植性更差。

其他的还有模拟操作系统页表的实现方式，可以用 TLB 来提高性能。这种方式可以实现权限检查，但性能比 `mmap` 差一些。

---



**@Ryan：`machine_setup` 的详细解释？`sp` 减掉的几个 8 字节的值具体的信息（为什么是 8 字节以及大致说明）。** 

可以参考这个文档：https://www.win.tue.nl/~aeb/linux/hh/stack-layout.html

这个文档是 i386 架构的，但本质是一样的。

### 正式上线后记录到的问题

> 暂无





## 致谢

感谢课程试运行期间各位大佬提出的建议/做出的贡献，以下是一个名单，排名不分先后：

- 欧小橙
- Ryan
- 卡西莫多
- Reaper Lu
- Chiuan
