org 0x00 console_print:
org 0x10 screen:
    org 0x10 [init]
    org 0x11 pixel:
    org 0x12 [update]
    org 0x14 [quit]

org 0x100

init:
    push.2 hello_world
    push   console_print
    write
    break

hello_world: [$ "Hello, World!" 0x0a ]

update: break
quit: break
