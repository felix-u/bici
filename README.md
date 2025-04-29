# bici - emulator & OS for fictional 8-bit CPU with 64kb memory

TODO(felix): gif showing usage


## Overview

I began this project with a working emulator, but with no interaction capability and with the only output mechanisms being console prints and plotting individual pixels on the screen. The assembly language was weird and limited.

My goals for the project period were to add or achieve the following:

1.  [x] An intel-inspired, clearer assembly syntax, with more convenience features
2.  [x] The ability to draw lines, rectangles, and 8x8 sprites
3.  [x] The ability to draw ASCII text
4.  [x] The ability to query mouse position and button state
5.  [x] The ability to query keyboard input
6.  [x] The ability to query the local filesystem
7.  [x] The ability to statically and dynamically set the four-colour system palette within ROMs
8.  [x] The ability to draw to the background layer (where the 0-colour is transparent) and the foreground layer
9.  [x] An operating system which is a ROM like any other and is programmed in the VM's assembly, not special-cased in the emulation code
10. [x] Said OS can act as a loader for ROMs
11. [x] Said OS can run ROMs without any changes to them (i.e. the ROMs can run unchanged both on "bare metal" and in the OS)
12. [ ] Said OS can window ROMs and run multiple ROMs concurrently, providing multitasking capability

I've achieved every goal except the last. I know how to enable multitasking and provide a "real" OS experience, but it'll require a couple changes to the emulator (albeit without special-casing the OS) and some rather complex assembly routines. I think I could have managed it had I had a couple more days.

The rest of the sections in this document contain:

* [Codebase organisation](#codebase-organisation)
* [Compilation and usage](#compilation-and-usage)
* The [CPU reference](#cpu-reference)
* The [assembly language reference](#assembly-language-reference)
* An overview of [programming for `bici`](#programming-for-bici)
* The [feature summary](#feature-summary), going in depth on the numbered points above, with GIFs
* [Next steps](#next-steps)
* [Inspiration](#inspiration)


## Codebase organisation

This is a monorepo containing the code for the assembler, emulator, example ROMs, and OS.

The assembler and emulator are written in C11. I don't use any libraries, and my personal standard library's software renderer interfaces with Win32 directly.

The code for the assembler begins [here](https://github.com/felix-u/bici/blob/master/src/main.c#L700) and the code for the emulator begins [here](https://github.com/felix-u/bici/blob/master/src/main.c#L1290).

The assembly code for the example programs are in [`hello.asm`](./hello.asm), [`keyboard.asm`](./keyboard.asm), [`mouse.asm`](./mouse.asm), and [`screen.asm`](./screen.asm). The OS is implemented in [`os.asm`](./os.asm). For convenience, all assembly programs include [`header.asm`](./header.asm), which I elaborate on [later](#programming-for-bici).


## Compilation and usage

I want to get my software renderer up and running on Linux and web in the near future, but for now I've only implemented it for Win32, so that's the only compilation target for `bici`.

With the MSVC toolchain installed, run `build.bat`. You can also run `build.bat clang`. Either of these commands will produce `build/bici.exe`.

Usage: `bici <com|run|script> <file...>`, meaning one of:
```sh
bici com file.asm file.rom
bici run file.rom
bici script file.asm # compile and run in one go
```


## CPU reference

TODO(felix)


## Assembly language reference

TODO(felix)


## Programming for `bici`

TODO(felix)


## Feature summary

TODO(felix)


## Next steps

TODO(felix)


## Inspiration

TODO(felix): link uxn & varvara
