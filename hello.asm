include "header.asm"

patch system_start, start
start:
    ; palette
    push.2 0xfff
    push system_colour_1
    write.2

    push.2 alphabet
    push.2 0x10
    push.2 0x10
    jsi draw_text

    break

alphabet: [$ "ABCDEFGHIJKLMNOPQRSTUVWXYZ" ]
