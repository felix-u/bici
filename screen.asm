include "header.asm"

patch system_start, start
start:
    push.2 0xbcc push system_colour_0 write.2
    push.2 0x332 push system_colour_3 write.2
    break

patch screen_update, update
update:
    ; clear background
    push 0b11000000
    push screen_pixel write

    push.2 0x0
    /while:
        dup.2
        push.2 EOF
        lt.2
        jni {
            dup.2
            jsi load_coordinates_at_address

            push screen_y write.2
            push screen_x write.2

            push 0x03
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
    0x8080 0x8380
    0x8084 0x8185 0x8285 0x8384
]

EOF:
org 0xffff
