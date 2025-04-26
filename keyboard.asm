include "header.asm"

patch system_start, start
start:
    ; palette
    push.2 0x451 push system_colour_0 write.2
    push.2 0x782 push system_colour_1 write.2
    push.2 0xab4 push system_colour_2 write.2
    push.2 0xdd6 push system_colour_3 write.2

    push screen_width read.2
    push.2 0x8 sub.2
    push.2 0x2 div.2
    push.2 0x8 mul.2
    push.2 sprite_x store.2

    push screen_height read.2
    push.2 0x8 sub.2
    push.2 0x2 div.2
    push.2 0x8 mul.2
    push.2 sprite_y store.2

    break

sprite_x: rorg 0x2
sprite_y: rorg 0x2

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

    ;; TODO(felix): do based on keyboard input
    ;push.2 sprite_x load.2
    ;inc.2
    ;push.2 sprite_x store.2

    push 0x57 ; 'W'
    jsi key_down jni {
        push.2 sprite_y load.2
        push.2 0x1 sub.2
        push.2 sprite_y store.2
    }

    push 0x41 ; 'A'
    jsi key_down jni {
        push.2 sprite_x load.2
        push.2 0x1 sub.2
        push.2 sprite_x store.2
    }

    push 0x53 ; 'S'
    jsi key_down jni {
        push.2 sprite_y load.2
        inc.2
        push.2 sprite_y store.2
    }

    push 0x44 ; 'D'
    jsi key_down jni {
        push.2 sprite_x load.2
        inc.2
        push.2 sprite_x store.2
    }

    push.2 sprite_x load.2
    push.2 0x8 div.2
    push screen_x write.2

    push.2 sprite_y load.2
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
