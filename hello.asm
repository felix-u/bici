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

    push.2 hello
    push.2 0x10
    push.2 0x20
    jsi draw_text

    push.2 punctuation
    push.2 0x10
    push.2 0x30
    jsi draw_text

    push.2 numbers
    push.2 0x10
    push.2 0x40
    jsi draw_text

    break

alphabet: [$ "ABCDEFGHIJKLMNOPQRSTUVWXYZ" ]

hello: [$ "HELLO, WORLD!" ]

punctuation: [$ "!" 0x22 "#$%&'()*+,-./:;<=>?@[\]^_`{|}~"  ]

numbers: [$ "0123456789" ]
