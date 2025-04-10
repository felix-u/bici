include "header.asm"

patch screen_init, init
patch screen_update, update
patch screen_quit, quit

init:
    push.2 hello_world
    push   console_print
    write
    break

hello_world: [$ "Hello, World!" 0x0a ]

update: break
quit: break
