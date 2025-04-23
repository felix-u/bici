include "header.asm"

patch system_start, start
start:
    push.2 0x000
    push system_colour_0 write.2
    push.2 0xfff
    push system_colour_1 write.2
    push.2 0xf00
    push system_colour_2 write.2
    push.2 0x0ff
    push system_colour_3 write.2

    push.2 0x0
    /while:
        dup.2
        push.2 EOF
        lt.2
        jni {
            dup.2
            jsi load_coordinates_at_address
            push 0x01
            push screen_pixel
            write

            push.2 0x2
            add.2
            jmi while
        }
        drop.2
    break

cast_16_from_8:
    push 0x0
    swap
    jmp.r

load_coordinates_at_address:
    dup.2
    load
    jsi cast_16_from_8
    swap.2
    inc.2
    load
    jsi cast_16_from_8
    jmp.r

smiley: [
    0x00 ; align
    0x8080 0x8380
    0x8084 0x8185 0x8285 0x8384
]

diagonal_line: [
    0x50a0
    0x58b0
    0x60c0
    0x68d0
    0x70e0
    0x78f0
]

EOF:
