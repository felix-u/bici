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

**The [feature summary](#feature-summary) goes in depth on these points, with GIFs and screenshots.**

I've achieved every goal except the last. I know how to enable multitasking and provide a "real" OS experience, but it'll require a couple changes to the emulator (albeit without special-casing the OS) and some rather complex assembly routines. I think I could have managed it had I had a couple more days.


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

The `bici` CPU is 8-bit, with 16-bit operations. It has 64kb of memory and two 256-byte stacks: a working stack, for intermediate values and parameters, and a return stack which in practice is used as a call stack.

`bici` has 35 opcodes. Each instruction is one byte, consisting of a 5-bit opcode and 3 mode bits:
```cpp
bits {
    opcode: 5
    size:   1 // 0 if 8-bit, 1 if 16-bit
    stack:  1 // 0 if working stack, 1 if return stack
    keep:   1 // 0 if parameters are popped, 1 if parameters are kept
}
```
![Opcode slide](./assets/opcodes.png)

5 bits only allows for 32 operations, but not all opcodes support all modes so to get 35, I "overloaded" operations which didn't need all their mode bits. Since `push` takes an immediate, without popping any value, I used its `keep` variants (`push.k`, `push.2k`, and `push.kr`) as the byte values for 3 immediate jumps, which also have no use for the mode bits.

The opcode values are defined as an X-list, [here](https://github.com/felix-u/bici/blob/master/src/main.c#L6).

In the following table, the left-to-right stack value notation corresponds to bottom-to-top positions. Unsuffixed values are assumed to be the bit-width indicated by the instruction's size mode bit, while `n`-suffixed values are `n` bits wide regardless of the instruction mode. `{ret: ...}` notation indicates return stack manipulation. `{}` notation indicates modification to `pc`, the program counter. `mem[]` notation indicates memory read.

| Opcode | Stack before | Stack after                          | Takes immediate? |
| ------ | ------------ | -----------                          | ---------------- |
| break  |              |                                      | yes              |
| push   |              | immediate                            |                  |
| drop   | a b          | a                                    |                  |
| nip    | a b          | b                                    |                  |
| swap   | a b          | b a                                  |                  |
| rot    | a b c        | b c a                                |                  |
| dup    | a            | a a                                  |                  |
| over   | a b          | a b a                                |                  |
| eq     | a b          | (a == b)8                            |                  |
| neq    | a b          | (a != b)8                            |                  |
| gt     | a b          |  (a > b)8                            |                  |
| lt     | a b          | (a < b)8                             |                  |
| add    | a b          | (a + b)                              |                  |
| sub    | a b          | (a - b)                              |                  |
| mul    | a b          | (a * b)                              |                  |
| div    | a b          | (a / b)                              |                  |
| inc    | a            | (a + 1)                              |                  |
| not    | a            | ~a                                   |                  |
| and    | a b          | (a & b)                              |                  |
| or     | a b          | (a \| b)                             |                  |
| xor    | a b          | (a ^ b)                              |                  |
| shift  | a b          | b << ((a & 0xf0) >> 4) >> (a & 0x0f) |                  |
| jmp    | a.16         | { pc = a.16 }                        |                  |
| jme    | a b.16       | if (a) { pc = b.16 }                 |                  |
| jst    | a.16         |  {ret: pc} { pc = a.16 }             |                  |
| jne    | a b.16       |  if (!a) { pc = b.16 }               |                  |
| jni    | a            | if (!a) { pc = immediate.16 }        | yes              |
| stash  | a            | {ret: a}                             |                  |
| load   | a.16         | mem[a.16]                            |                  |
| store  | a b.16       | mem[b.16] = a                        |                  |
| read   | a            | mem[a]                               |                  |
| write  | a b          | mem[b] = a                           |                  |
| jmi    |              | { pc = immediate.16 }                | yes              |
| jei    | a            | if (a) { pc = immediate.16 }         | yes              |
| jsi    |              | {ret: pc} { pc = immediate.16 }      | yes              |

The first 256-byte page of the 64kb address space - the 0 page, or the device page - is reserved for special device operations using the `read` and `write` operations.

When interacting with the device page, we use `read` and `write` rather than `load` or `store` because the CPU intercepts these operations and may need to query the relevant values or perform device operations, before populating the relevant device page address or pushing a value from the device page onto the stack.

Each device has 16 bytes reserved in the device page, which can be used for a combination of 8-bit and 16-bit values or operations. At the moment there are [6 devices](https://github.com/felix-u/bici/blob/master/src/main.c#L45): `system`, `console`, `screen`, `mouse`, `keyboard`, and `file` (implemented in [these](https://github.com/felix-u/bici/blob/master/src/main.c#L264) [portions](https://github.com/felix-u/bici/blob/master/src/main.c#L418) of the emulator).

See the [`draw_text` routine](https://github.com/felix-u/bici/blob/master/header.asm#L102) in `header.asm` for an example of using the `screen` devices to render text.

When a ROM is executed, `bici` sets the program counter to the address stored at `system_start` in the device page and [executes](https://github.com/felix-u/bici/blob/master/src/main.c#L1291) until a `break` instruction is encountered.
Then, every frame, `bici` reads the address at `screen_update` in the device page, and [executes](https://github.com/felix-u/bici/blob/master/src/main.c#L1295) from that address until `break`. Finally, upon emulator quit, `bici` [executes](https://github.com/felix-u/bici/blob/master/src/main.c#L1299) the address at `system_quit` in the device page until a `break`.


## Assembly language reference

I've tried to emulate intel assembly syntax, with a few differences.

* Instructions are compiled via their opcode, and may be suffixed with modes `.2kr` (any combination following `.`). For example, to jump to the topmost address on the return stack (i.e. to return), write `jmp.r`. Some instructions take an immediate, as in `push label_or_number`.
* Number literals are either hexadecimal (`0x1234`) or binary (`0b01`).
* There may be multiple instructions on one line, for compactness (most instructions take no arguments, operating on the stack)
* Global labels are defined as `label:`, and local labels are defined as `/label:`. Local labels are only visible between the preceding global label and the next global label.
* Comments are from any `;` character to the end of the line
* A `{ ... }` pair will compile the 16-bit address of `}` at the location of `{`. Therefore, the equivalent of C's `if (1) { ... }` is `push 0x1 jni { ... }`.
* Any `[ ... ]` indicates insertion mode, where string literals (`"Hello"`) and numbers are compiled directly into the binary at the current compilation offset. If the opening `[` is suffixed with `$`, as in `[$ ... ]`, then the offset between the opening `[` and closing `]` (i.e. the total number of bytes inserted) is compiled at the location of `[$` as an 8-bit relative offset. This is useful for strings, as in `[$ "Hello" ]`, which is equivalent to `[ 0x5 "Hello" ]`.

There are a few directives:

* `include "file.asm"`, equivalent to C's `#include "file.c"`.
* `org number` sets the compilation offset to `number`.
* `rorg number` increments the compilation offset by `number`.
* `patch label, label_or_number` compiles the value of `label_or_number` to the address of `label`, without changing the compilation offset. This allows device page labels to be defined in `header.asm` and then patched in assembly files which include it.


## Programming for `bici`

TODO(felix): hello world

TODO(felix): routine parameter comment convention

TODO(felix): header.asm

TODO(felix): call and return

TODO(felix): local labels after return as local variables

Recall the three special routines `system_start`, `system_quit`, and `screen_update`.
Because the `break` instruction has a byte value of `0`, and because the assembler guarantees that untouched bytes remain 0, all of these routines are optional. For example, if you don't patch `screen_update`, `bici` reads 0 and goes to execute at address 0 in the ROM. This address is reserved, and so contains a `0` byte - an immediate `break`. Therefore, unsupplied special routines are still executed, essentially as no-ops.



## Feature summary

TODO(felix)


## Next steps

TODO(felix)


## Inspiration

TODO(felix): link uxn & varvara
