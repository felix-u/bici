include "header.asm"

patch init_routine, init
patch update_routine, update
patch quit_routine, quit

init:
    push.2 hello_world
    push   console_print
    write
    break

hello_world: [$ "Hello, World!" 0x0a ]

update: break
quit: break
