include "header.asm"

patch system_colour_0, 0xbcc
patch system_colour_3, 0x332

patch screen_update, update
update:
    ; clear background
    push.2 0x0 push screen_x write.2
    push.2 0x0 push screen_y write.2
    push 0b11000000
    push screen_pixel write

    push.2 0x0
    /while:
        dup.2
        push.2 EOF
        lt.2
        jni.2 {
            dup.2
            jsi.2 load_coordinates_at_address

            push screen_y write.2
            push screen_x write.2

            push 0x03
            push screen_pixel
            write

            push.2 0x2
            add.2
            jmi.2 while
        }
        drop.2
    break

cast_16_from_8:
    push 0x0
    swap
    jmp.2r

load_coordinates_at_address:
    dup.2
    load
    jsi.2 cast_16_from_8
    swap.2
    push.2 0x1 add.2
    load
    jsi.2 cast_16_from_8
    jmp.2r

smiley: [
    ; 0x0 ; align
    0x8080 0x8380
    0x8084 0x8185 0x8285 0x8384
]

include "footer.asm"
