include "header.asm"

patch system_start, start
start:
    ; palette
    push.2 0xfff push system_colour_0 write.2
    push.2 0xddd push system_colour_1 write.2
    push.2 0xccf push system_colour_2 write.2
    push.2 0x000 push system_colour_3 write.2

    break

patch screen_update, update
update:
    ; clear background
    push.2 0x0 push screen_x write.2
    push.2 0x0 push screen_y write.2
    push 0b11000010
    push screen_pixel write

    ; draw titlebar background
    push.2 0x0
    /loop: dup.2 push.2 0xc lt.2 jni {
        dup.2
        push screen_x write.2

        push 0b10000000
        push screen_pixel write

        inc.2
        jmi loop
    } drop.2
    push.2 0xc push screen_x write.2
    push 0b10000011 push screen_pixel write

    ; draw logo
    push.2 0x6 push screen_x write.2
    push.2 0x2 push screen_y write.2
    push.2 os_logo_sprite push screen_data write.2
    push 0b00001100 push screen_sprite write

    push 0b00001100
    jsi draw_default_mouse_cursor_at_mouse

    break


os_logo_sprite: [
    0b00111100
    0b01011010
    0b10100101
    0b10100101
    0b10100101
    0b10100101
    0b01011010
    0b00111100
]
