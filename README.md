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

I've achieved every goal except the last. I know how to enable multitasking and provide a "real" OS experience, but it'll require a couple changes to the emulator (albeit without special-casing the OS) and some rather complex assembly routines. I think I could have managed it by the deadline had I had a couple more deadlines. Still, I've done a lot in the time I had.

The rest of the sections in this document detail:

* [Codebase organisation](#codebase-organisation)
* The [CPU reference](#cpu-reference)
* The [assembly language reference](#assembly-language-reference)
* The [feature summary](#feature-summary), going in depth on the numbered points above, with GIFs
* [Next steps](#next-steps)
* [Inspiration](#inspiration)


## Codebase organisation

TODO(felix)


## CPU reference

TODO(felix)


## Assembly language reference

TODO(felix)


## Feature summary

TODO(felix)


## Next steps

TODO(felix)


## Inspiration

TODO(felix): link uxn & varvara
