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

    ; coordinate
    push.2 0x10 dup.2
    push screen_x write.2
    push screen_y write.2

    ; sprite data
    push.2 sprite
    push screen_data
    write.2

    ; draw sprite
    push 0x0
    push screen_sprite
    write

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
