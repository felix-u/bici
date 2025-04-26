include "header.asm"

patch system_start, start
start:
    ; palette
    push.2 0x033 push system_colour_0 write.2
    push.2 0x29d push system_colour_1 write.2
    push.2 0xd38 push system_colour_2 write.2
    push.2 0xabb push system_colour_3 write.2

    break

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

    push.2 current_mouse_colour load.2 load
    jsi draw_default_mouse_cursor_at_mouse

    break

    /first_mouse_colour: [ 0b00000100 /last_mouse_colour: 0b00001000 ]
    /current_mouse_colour: [ first_mouse_colour ]
    /first_background_colour: [ 0b11000000 /last_background_colour: 0b11000011 ]
    /current_background_colour: [ first_background_colour ]
    /message_header: [$ "LAST ACTION:" ]
    /message: [ message_none ]
    /message_none: [$ "NONE" ]
    /message_left_click: [$ "LEFT CLICK" ]
    /message_right_click: [$ "RIGHT CLICK" ]
