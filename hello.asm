include "header.asm"

patch system_start, start
start:
    push.2 hello_world
    push   console_print
    write.2
    break

hello_world: [$ "Hello, World!" 0x0a ]
