include "header.asm"

patch system_colour_0, 0x451
patch system_colour_1, 0x782
patch system_colour_2, 0xab4
patch system_colour_3, 0xdd6

patch system_start, start
start:
    push screen_width read.2
    push.2 0x8 sub.2
    push.2 0x2 div.2
    push.2 0x8 mul.2
    dup.2
    push.2 target_sprite_x store.2
    push.2 real_sprite_x store.2

    push screen_height read.2
    push.2 0x8 sub.2
    push.2 0x2 div.2
    push.2 0x8 mul.2
    dup.2
    push.2 target_sprite_y store.2
    push.2 real_sprite_y store.2

    break

real_sprite_x: rorg 0x2
real_sprite_y: rorg 0x2
target_sprite_x: rorg 0x2
target_sprite_y: rorg 0x2

patch screen_update, update
update:
    ; clear background
    push.2 0x0 push screen_x write.2
    push.2 0x0 push screen_y write.2
    push 0b11000010
    push screen_pixel
    write

    push screen_height read.2
    push.2 window_height store.2

    push.2 control_hint_top
    push.2 0x20

    push.2 window_height load.2
    push.2 0x30 sub.2
    push 0b00001100
    jsi draw_text

    push.2 control_hint_bottom
    push.2 0x18
    push.2 window_height load.2
    push.2 0x28 sub.2
    push 0b00001100
    jsi draw_text

    push 0x57 ; 'W'
    jsi key_down jni {
        push.2 target_sprite_y load.2
        push.2 0x1 sub.2
        dup.2
        push.2 real_sprite_y store.2
        push.2 target_sprite_y store.2
    }

    push 0x41 ; 'A'
    jsi key_down jni {
        push.2 target_sprite_x load.2
        push.2 0x1 sub.2
        dup.2
        push.2 real_sprite_x store.2
        push.2 target_sprite_x store.2
    }

    push 0x53 ; 'S'
    jsi key_down jni {
        push.2 target_sprite_y load.2
        inc.2
        dup.2
        push.2 real_sprite_y store.2
        push.2 target_sprite_y store.2
    }

    push 0x44 ; 'D'
    jsi key_down jni {
        push.2 target_sprite_x load.2
        inc.2
        dup.2
        push.2 real_sprite_x store.2
        push.2 target_sprite_x store.2
    }

    ; if left click
    push mouse_left_button read
    push 0b11110000 and
    jni {
        push mouse_x read.2
        push.2 0x8 mul.2
        push.2 target_sprite_x store.2

        push mouse_y read.2
        push.2 0x8 mul.2
        push.2 target_sprite_y store.2
    }

    push.2 real_sprite_x load.2
    push.2 target_sprite_x load.2
    jsi interpolate
    push.2 real_sprite_x store.2

    push.2 real_sprite_y load.2
    push.2 target_sprite_y load.2
    jsi interpolate
    push.2 real_sprite_y store.2

    push.2 real_sprite_x load.2
    push.2 0x8 div.2
    push screen_x write.2

    push.2 real_sprite_y load.2
    push.2 0x8 div.2
    push screen_y write.2

    push.2 sprite
    push screen_data write.2
    push 0b00000100
    push screen_sprite write

    push 0b00000000
    jsi draw_default_mouse_cursor_at_mouse

    break

    /window_height: rorg 0x2

interpolate: ; (a, b: u16 -> u16)
    swap.2
    dup.2 push.2 initial_a store.2

    sub.2
    push.2 0x20 div.2

    push.2 initial_a load.2
    add.2

    jmp.r
    /initial_a: rorg 0x2

key_down: ; (key: u8 -> u8)
    push keyboard_key_value write
    push keyboard_key_state read
    push 0b00001111 and
    jmp.r

control_hint_top: [$ "W" ]
control_hint_bottom: [$ "ASD" ]

sprite: [
    0b00111100
    0b01111010
    0b11111101
    0b11111111
    0b10111111
    0b10111111
    0b01001110
    0b00111100
]

EOF:
org 0xffff
