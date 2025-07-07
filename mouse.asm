include "header.asm"

patch system_colour_0, 0x4b8
patch system_colour_1, 0x335
patch system_colour_2, 0xefe
patch system_colour_3, 0x489

patch screen_update, update
update:
    ; if left click
    push mouse_left_button read
    push 0b11110000 and
    jni end_set_mouse_colour
        push.2 message_left_click
        push.2 message store.2

        ; if current_mouse_colour < last_mouse_colour
        push.2 current_mouse_colour load.2
        dup.2
        push.2 last_mouse_colour
        lt.2 jni else
            ; add 1 and store new mouse colour
            inc.2
            push.2 current_mouse_colour
            store.2
            jmi end_set_mouse_colour
        /else: ; wrap around to first colour
            drop.2
            push.2 first_mouse_colour
            push.2 current_mouse_colour
            store.2
    /end_set_mouse_colour:

    ; if right click
    push mouse_right_button read
    push 0b11110000 and
    jni end_set_background_colour
        push.2 message_right_click
        push.2 message store.2

        ; if current_background_colour < last_background_colour
        push.2 current_background_colour load.2
        dup.2
        push.2 last_background_colour
        lt.2 jni else2
            ; add 1 and store new background colour
            inc.2
            push.2 current_background_colour
            store.2
            jmi end_set_background_colour
        /else2: ; wrap around to first colour
            drop.2
            push.2 first_background_colour
            push.2 current_background_colour
            store.2
    /end_set_background_colour:

    ; clear background
    push.2 0x0 push screen_x write.2
    push.2 0x0 push screen_y write.2
    push.2 current_background_colour load.2 load
    push screen_pixel
    write

    push.2 message_header
    push.2 0x10 dup.2
    push.2 current_mouse_colour load.2 load
    jsi draw_text

    push.2 message load.2
    push.2 0x10
    push.2 0x20
    push.2 current_mouse_colour load.2 load
    jsi draw_text

    ; if left down
    push mouse_left_button read
    push 0b00001111 and
    jni end_draw_sin_wave
        push.2 sin_wave_frame_counter load
        inc
        ; reset if above max - basically modulo
        dup push 0x18 gt jni {
            push 0x0
            nip
        }
        dup push.2 sin_wave_frame_counter store

        ; if counter == 0
        push 0x0 eq jni {
            ; if current_sin_wave < last_sin_wave
            push.2 current_sin_wave load.2
            dup.2
            push.2 last_sin_wave
            lt.2 jni else3
                ; add 1 and store new sin wave sprite
                push.2 0x8 add.2
                push.2 current_sin_wave
                store.2
                jmi draw
            /else3: ; wrap around to first sin wave
                drop.2
                push.2 first_sin_wave
                push.2 current_sin_wave
                store.2
        }
        /draw:
        push mouse_x read.2
        push screen_x write.2

        push mouse_y read.2
        push.2 0x8 add.2
        push screen_y write.2

        push.2 current_sin_wave load.2
        push screen_data write.2

        push.2 current_mouse_colour load.2 load
        push screen_sprite write
    /end_draw_sin_wave:

    push.2 current_mouse_colour load.2 load
    jsi draw_default_mouse_cursor_at_mouse

    break

    /first_mouse_colour: [ 0b00000100 /last_mouse_colour: 0b00001000 ]
    /current_mouse_colour: [ first_mouse_colour ]
    /first_background_colour: [ 0b11000011 /last_background_colour: 0b11000000 ]
    /current_background_colour: [ first_background_colour ]
    /message_header: [$ "LAST ACTION:" ]
    /message: [ message_none ]
    /message_none: [$ "NONE" ]
    /message_left_click: [$ "LEFT CLICK" ]
    /message_right_click: [$ "RIGHT CLICK" ]

    /sin_wave_frame_counter: [ 0x0 ]
    /current_sin_wave: [ first_sin_wave ]
    [
        /first_sin_wave:
        0b00000000
        0b00000000
        0b00000000
        0b00111000
        0b01000100
        0b10000011
        0b00000000
        0b00000000

        0b00000000
        0b00000000
        0b00000000
        0b01110000
        0b10001000
        0b00000111
        0b00000000
        0b00000000

        0b00000000
        0b00000000
        0b00000000
        0b11100000
        0b00010001
        0b00001110
        0b00000000
        0b00000000

        0b00000000
        0b00000000
        0b00000000
        0b11000001
        0b00100010
        0b00011100
        0b00000000
        0b00000000

        0b00000000
        0b00000000
        0b00000000
        0b10000011
        0b01000100
        0b00111000
        0b00000000
        0b00000000

        0b00000000
        0b00000000
        0b00000000
        0b00000111
        0b10001000
        0b01110000
        0b00000000
        0b00000000

        0b00000000
        0b00000000
        0b00000000
        0b00001110
        0b00010001
        0b11100000
        0b00000000
        0b00000000

        0b00000000
        0b00000000
        0b00000000
        0b00011100
        0b00100010
        0b11000001
        0b00000000
        0b00000000

        /last_sin_wave:
        0b00000000
        0b00000000
        0b00000000
        0b00111000
        0b01000100
        0b10000011
        0b00000000
        0b00000000
    ]

include "footer.asm"
