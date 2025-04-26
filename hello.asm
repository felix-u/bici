include "header.asm"

patch system_start, start
start:
    push.2 hello_world
    push   console_print
    write.2

    ; palette
    push.2 0x000
    push system_colour_0
    write.2
    push.2 0xfff
    push system_colour_1
    write.2

    push 0x41 ; 'A'
    push.2 0x10
    push.2 0x10
    jsi draw_character

    push 0x42 ; 'B'
    push.2 0x19
    push.2 0x10
    jsi draw_character

    push 0x44
    push.2 0x22
    push.2 0x10
    jsi draw_character

    break

hello_world: [$ "Hello, World!" 0x0a ]

sprite: [
    0b11000011
    0b11000011
    0b11000011
    0b00000000
    0b00000000
    0b10000001
    0b01000010
    0b00111100
]
