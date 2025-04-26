include "header.asm"

patch system_start, start
start:
    ; palette
    push.2 0x033
    push system_colour_0
    write.2
    push.2 0x29d
    push system_colour_1
    write.2

    break

patch screen_update, update
update:
    ; clear background
    push.2 0x0 push screen_x write.2
    push.2 0x0 push screen_y write.2
    push 0b11000000
    push screen_pixel
    write

    push mouse_x read.2
    push screen_x write.2

    push mouse_y read.2
    push screen_y write.2

    push.2 mouse_cursor_sprite
    push screen_data write.2

    push 0x0
    push screen_sprite write

    break

mouse_cursor_sprite: [
    0b10000000
    0b11000000
    0b11100000
    0b11110000
    0b11111000
    0b11100000
    0b01010000
    0b00010000
]
