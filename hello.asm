include "header.asm"

patch system_colour_0, 0xeeb
patch system_colour_1, 0x000

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

    push.2 alphabet_lowercase
    push.2 0x10
    push.2 0x50
    push 0b00000100
    jsi draw_text

    push 0b00000100
    jsi draw_default_mouse_cursor_at_mouse

    break

alphabet: [$ "ABCDEFGHIJKLMNOPQRSTUVWXYZ" ]

alphabet_lowercase: [$ "abcdefghijklmnopqrstuvwxyz" ]

hello: [$ "Hello, World!" ]

punctuation: [$ "!" 0x22 "#$%&'()*+,-./:;<=>?@[\]^_`{|}~" ]

numbers: [$ "0123456789" ]

EOF:
org 0xffff
