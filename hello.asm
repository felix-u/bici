include "header.asm"

patch system_start, start
start:
    ; palette
    push.2 0xeeb
    push system_colour_0
    write.2
    push.2 0x000
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

    push.2 alphabet
    push.2 0x10
    push.2 0x10
    push 0b00000100
    jsi draw_text

    push.2 hello
    push.2 0x10
    push.2 0x20
    push 0b00000100
    jsi draw_text

    push.2 punctuation
    push.2 0x10
    push.2 0x30
    push 0b00000100
    jsi draw_text

    push.2 numbers
    push.2 0x10
    push.2 0x40
    push 0b00000100
    jsi draw_text

    push 0b00000100
    jsi draw_default_mouse_cursor_at_mouse

    break

alphabet: [$ "ABCDEFGHIJKLMNOPQRSTUVWXYZ" ]

hello: [$ "HELLO, WORLD!" ]

punctuation: [$ "!" 0x22 "#$%&'()*+,-./:;<=>?@[\]^_`{|}~" ]

numbers: [$ "0123456789" ]
