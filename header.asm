system:
    rorg 0x2 ; address 0 reserved
    system_end:      rorg 0x2
    system_start:    rorg 0x2
    system_quit:     rorg 0x2
    system_colour_0: rorg 0x2
    system_colour_1: rorg 0x2
    system_colour_2: rorg 0x2
    system_colour_3: rorg 0x2

org 0x10 console:
             console_print:

org 0x20 screen:
    screen_update: rorg 0x2
    screen_width:  rorg 0x2
    screen_height: rorg 0x2
    screen_x:      rorg 0x2
    screen_y:      rorg 0x2
    screen_pixel:  rorg 0x1
    screen_sprite: rorg 0x1
    screen_data:   rorg 0x2
    screen_auto:   rorg 0x1

org 0x30 mouse:
    mouse_x: rorg 0x2
    mouse_y: rorg 0x2
    mouse_left_button: rorg 0x1
    mouse_right_button: rorg 0x1

org 0x40 keyboard:
    keyboard_key_value: rorg 0x1
    keyboard_key_state: rorg 0x1

org 0x50 file:
    file_name:   rorg 0x2
    file_length: rorg 0x2
    file_cursor: rorg 0x2
    file_read:   rorg 0x2
    file_copy:   rorg 0x2

org 0x60

org 0x100

patch system_end, EOF

cast_u16_from_u8: ; (value: u8 -> u16)
    push 0x0 swap
    jmp.r

string_width: ; (string_address: u16 -> u16)
    load
    jsi.2 cast_u16_from_u8
    push.2 0x8 mul.2
    jmp.r

mouse_in_rectangle: ; (x, y, width, height: u16 -> u8)
    push.2 height store.2
    push.2 width store.2
    push.2 y store.2
    push.2 x store.2

    ; if mouse_x < x
    push mouse_x read.2
    push.2 x load.2
    lt.2 jni.2 { push 0x0 jmp.r }

    ; if mouse_x > x + width
    push mouse_x read.2
    push.2 x load.2
    push.2 width load.2
    add.2
    gt.2 jni.2 { push 0x0 jmp.r }

    ; if mouse_y < y
    push mouse_y read.2
    push.2 y load.2
    lt.2 jni.2 { push 0x0 jmp.r }

    ; if mouse_y > y + height
    push mouse_y read.2
    push.2 y load.2
    push.2 height load.2
    add.2
    gt.2 jni.2 { push 0x0 jmp.r }

    push 0x1
    jmp.r

    /x: rorg 0x2
    /y: rorg 0x2
    /width: rorg 0x2
    /height: rorg 0x2

mouse_in_text: ; (top_left_x, top_left_y, string_address: u16 -> u8)
    jsi.2 string_width
    push.2 0x8
    jsi.2 mouse_in_rectangle
    jmp.r

draw_text: ; (string_address, x, y: u16, text_colour: u8 -> _)
    push.2 text_colour store

    push screen_y write.2
    push screen_x write.2

    push 0b00000001
    push screen_auto write

    ; store character count
    dup.2
    load
    push.2 count store

    ; store address of first character
    push.2 0x1 add.2
    push.2 address store.2

    push.2 0x0
    /loop:
        dup.2

        push.2 count load
        push 0x0 swap ; cast to u16

        lt.2
        jni.2 {
            dup.2
            push.2 index store.2

            dup.2

            ; load address
            push.2 address load.2

            ; add to current offset and load character
            add.2
            load

            ; cast character to u16
            push 0x0
            swap

            ; byte offset from beginning of font sprites
            push.2 0x8
            mul.2

            ; address of glyph
            push.2 font
            add.2
            push screen_data
            write.2

            ; draw
            push.2 text_colour load
            push screen_sprite
            write

            push.2 0x1 add.2
            jmi.2 loop
        }
        drop.2

    jmp.r

    /text_colour: rorg 0x1
    /count: rorg 0x2
    /index: rorg 0x2
    /address: rorg 0x2

draw_default_mouse_cursor_at_mouse: ; (colour: u8 -> _)
    push mouse_x read.2
    push screen_x write.2

    push mouse_y read.2
    push screen_y write.2

    push.2 mouse_cursor_sprite
    push screen_data write.2

    push screen_sprite write

    jmp.r

mouse_cursor_sprite: [
    0b10000000
    0b11000000
    0b11100000
    0b11110000
    0b11111000
    0b11100000
    0b01010000
    0b00010000
]

; TODO(felix): mechanism to conditionally include? to decrease binary size for programs not using the font,
;              while keeping the fonts in this file

font:
    rorg 0x100 ; skip to ' ' * 8 (bytes per glyph), i.e. the first visible ASCII character
    /space: rorg 0x8
    /exclamation: [
        rorg 0x1
        0b00011000
        0b00011000
        0b00011000
        0b00011000
        0b00000000
        0b00011000
        0b00011000
    ]
    /double_quote: [
        rorg 0x1
        0b00000000
        0b01101100
        0b01101100
        0b01101100
        0b01101100
        0b00000000
        0b00000000
    ]
    /hashtag: [
        rorg 0x1
        0b01101100
        0b01101100
        0b11111110
        0b01101100
        0b11111110
        0b01101100
        0b01101100
    ]
    /dollar: [
        rorg 0x1
        0b01111100
        0b11010110
        0b11010000
        0b01111100
        0b00010110
        0b11010110
        0b01111100
    ]
    /percent: [
        rorg 0x1
        0b00000000
        0b01100110
        0b01001100
        0b00011000
        0b00110000
        0b01100100
        0b11001100
    ]
    /and: [
        rorg 0x1
        0b01110000
        0b11011000
        0b11011000
        0b01110010
        0b11011010
        0b11001100
        0b01110110
    ]
    /quote: [
        rorg 0x1
        0b00000000
        0b00011000
        0b00011000
        0b00011000
        0b00011000
        0b00000000
        0b00000000
    ]
    /left_bracket: [
        rorg 0x1
        0b00011000
        0b00110000
        0b01100000
        0b01100000
        0b01100000
        0b00110000
        0b00011000
    ]
    /right_bracket: [
        rorg 0x1
        0b00110000
        0b00011000
        0b00001100
        0b00001100
        0b00001100
        0b00011000
        0b00110000
    ]
    /asterisk: [
        rorg 0x1
        0b00000000
        0b01010100
        0b00101000
        0b11111110
        0b00101000
        0b01010100
        0b00000000
    ]
    /plus: [
        rorg 0x1
        0b00000000
        0b00010000
        0b00010000
        0b01111100
        0b00010000
        0b00010000
        0b00000000
    ]
    /comma: [
        rorg 0x1
        0b00000000
        0b00000000
        0b00000000
        0b00000000
        0b00011000
        0b00011000
        0b00110000
    ]
    /minus: [
        rorg 0x1
        0b00000000
        0b00000000
        0b00000000
        0b01111100
        0b00000000
        0b00000000
        0b00000000
    ]
    /dot: [
        rorg 0x1
        0b00000000
        0b00000000
        0b00000000
        0b00000000
        0b00000000
        0b00011000
        0b00011000
    ]
    /slash: [
        rorg 0x1
        0b00000000
        0b00000110
        0b00001100
        0b00011000
        0b00110000
        0b01100000
        0b11000000
    ]
    /_0: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11001110
        0b11010110
        0b11100110
        0b11000110
        0b01111100
    ]
    /_1: [
        rorg 0x1
        0b00011000
        0b00111000
        0b00011000
        0b00011000
        0b00011000
        0b00011000
        0b00111100
    ]
    /_2: [
        rorg 0x1
        0b01111100
        0b11000110
        0b00000110
        0b00011100
        0b00110000
        0b01100000
        0b11111110
    ]
    /_3: [
        rorg 0x1
        0b01111100
        0b11000110
        0b00000110
        0b00011100
        0b00000110
        0b11000110
        0b01111100
    ]
    /_4: [
        rorg 0x1
        0b00001100
        0b00011100
        0b00111100
        0b01101100
        0b11111110
        0b00001100
        0b00001100
    ]
    /_5: [
        rorg 0x1
        0b11111110
        0b11000000
        0b11111100
        0b00000110
        0b00000110
        0b11000110
        0b01111100
    ]
    /_6: [
        rorg 0x1
        0b00111100
        0b01100000
        0b11000000
        0b11111100
        0b11000110
        0b11000110
        0b01111100
    ]
    /_7: [
        rorg 0x1
        0b11111110
        0b00000110
        0b00001100
        0b00011000
        0b00110000
        0b01100000
        0b11000000
    ]
    /_8: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000110
        0b01111100
        0b11000110
        0b11000110
        0b01111100
    ]
    /_9: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000110
        0b01111110
        0b00000110
        0b00001100
        0b01111000
    ]
    /colon: [
        rorg 0x1
        0b00000000
        0b00011000
        0b00011000
        0b00000000
        0b00011000
        0b00011000
        0b00000000
    ]
    /semicolon: [
        rorg 0x1
        0b00000000
        0b00011000
        0b00011000
        0b00000000
        0b00011000
        0b00011000
        0b00110000
    ]
    /less_than: [
        rorg 0x1
        0b00011000
        0b00110000
        0b01100000
        0b11000000
        0b01100000
        0b00110000
        0b00011000
    ]
    /equals: [
        rorg 0x1
        0b00000000
        0b01111100
        0b01111100
        0b00000000
        0b01111100
        0b01111100
        0b00000000
    ]
    /greater_than: [
        rorg 0x1
        0b00110000
        0b00011000
        0b00001100
        0b00000110
        0b00001100
        0b00011000
        0b00110000
    ]
    /question: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000110
        0b00011100
        0b00110000
        0b00000000
        0b00110000
    ]
    /at: [
        rorg 0x1
        0b00111100
        0b01100110
        0b11001110
        0b11001010
        0b11001110
        0b01100000
        0b00111100
    ]
    /A: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000110
        0b11111110
        0b11000110
        0b11000110
        0b11000110
    ]
    /B: [
        rorg 0x1
        0b11111100
        0b11000110
        0b11000110
        0b11111100
        0b11000110
        0b11000110
        0b11111100
    ]
    /C: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000000
        0b11000000
        0b11000000
        0b11000110
        0b01111100
    ]
    /D: [
        rorg 0x1
        0b11111000
        0b11001100
        0b11000110
        0b11000110
        0b11000110
        0b11001100
        0b11111000
    ]
    /E: [
        rorg 0x1
        0b11111110
        0b11000000
        0b11000000
        0b11111100
        0b11000000
        0b11000000
        0b11111110
    ]
    /F: [
        rorg 0x1
        0b11111110
        0b11000000
        0b11000000
        0b11111100
        0b11000000
        0b11000000
        0b11000000
    ]
    /G: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000000
        0b11001110
        0b11000110
        0b11000110
        0b01111100
    ]
    /H: [
        rorg 0x1
        0b11000110
        0b11000110
        0b11000110
        0b11111110
        0b11000110
        0b11000110
        0b11000110
    ]
    /I: [
        rorg 0x1
        0b11111100
        0b00110000
        0b00110000
        0b00110000
        0b00110000
        0b00110000
        0b11111100
    ]
    /J: [
        rorg 0x1
        0b01111110
        0b00001100
        0b00001100
        0b00001100
        0b00001100
        0b11001100
        0b01111000
    ]
    /K: [
        rorg 0x1
        0b11000110
        0b11001100
        0b11011000
        0b11110000
        0b11011000
        0b11001100
        0b11000110
    ]
    /L: [
        rorg 0x1
        0b11000000
        0b11000000
        0b11000000
        0b11000000
        0b11000000
        0b11000000
        0b11111110
    ]
    /M: [
        rorg 0x1
        0b11000110
        0b11101110
        0b11111110
        0b11010110
        0b11010110
        0b11000110
        0b11000110
    ]
    /N: [
        rorg 0x1
        0b11000110
        0b11100110
        0b11110110
        0b11011110
        0b11001110
        0b11000110
        0b11000110
    ]
    /O: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000110
        0b11000110
        0b11000110
        0b11000110
        0b01111100
    ]
    /P: [
        rorg 0x1
        0b11111100
        0b11000110
        0b11000110
        0b11000110
        0b11111100
        0b11000000
        0b11000000
    ]
    /Q: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000110
        0b11000110
        0b11001010
        0b11001100
        0b01110110
    ]
    /R: [
        rorg 0x1
        0b11111100
        0b11000110
        0b11000110
        0b11000110
        0b11111100
        0b11001100
        0b11000110
    ]
    /S: [
        rorg 0x1
        0b01111100
        0b11000110
        0b11000000
        0b01111100
        0b00000110
        0b11000110
        0b01111100
    ]
    /T: [
        rorg 0x1
        0b11111100
        0b00110000
        0b00110000
        0b00110000
        0b00110000
        0b00110000
        0b00110000
    ]
    /U: [
        rorg 0x1
        0b11000110
        0b11000110
        0b11000110
        0b11000110
        0b11000110
        0b11000110
        0b01111100
    ]
    /V: [
        rorg 0x1
        0b11000110
        0b11000110
        0b11000110
        0b11000110
        0b01101100
        0b00111000
        0b00010000
    ]
    /W: [
        rorg 0x1
        0b11000110
        0b11000110
        0b11010110
        0b11010110
        0b11111110
        0b11101110
        0b11000110
    ]
    /X: [
        rorg 0x1
        0b11000110
        0b11000110
        0b01111100
        0b00111000
        0b01111100
        0b11000110
        0b11000110
    ]
    /Y: [
        rorg 0x1
        0b11001100
        0b11001100
        0b11001100
        0b01111000
        0b00110000
        0b00110000
        0b00110000
    ]
    /Z: [
        rorg 0x1
        0b11111110
        0b00000110
        0b00001100
        0b00111000
        0b01100000
        0b11000000
        0b11111110
    ]
    /left_square_bracket: [
        rorg 0x1
        0b01111110
        0b01100000
        0b01100000
        0b01100000
        0b01100000
        0b01100000
        0b01111110
    ]
    /backslash: [
        rorg 0x1
        0b00000000
        0b11000000
        0b01100000
        0b00110000
        0b00011000
        0b00001100
        0b00000110
    ]
    /right_square_bracket: [
        rorg 0x1
        0b01111110
        0b00000110
        0b00000110
        0b00000110
        0b00000110
        0b00000110
        0b01111110
    ]
    /caret: [
        rorg 0x1
        0b00010000
        0b00111000
        0b01101100
        0b11000110
        0b00000000
        0b00000000
        0b00000000
    ]
    /underscore: [
        rorg 0x1
        0b00000000
        0b00000000
        0b00000000
        0b00000000
        0b00000000
        0b00000000
        0b11111110
    ]
    /grave: [
        rorg 0x1
        0b00000000
        0b01100000
        0b00110000
        0b00011000
        0b00000000
        0b00000000
        0b00000000
    ]
    /a: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11111000
        0b00001100
        0b01111100
        0b11001100
        0b11111100
    ]
    /b: [
        rorg 0x1
        0b11000000
        0b11000000
        0b11111000
        0b11001100
        0b11001100
        0b11001100
        0b11111000
    ]
    /c: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01111000
        0b11001100
        0b11000000
        0b11001100
        0b01111000
    ]
    /d: [
        rorg 0x1
        0b00001100
        0b00001100
        0b01111100
        0b11001100
        0b11001100
        0b11001100
        0b01111100
    ]
    /e: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01111000
        0b11001100
        0b11111100
        0b11000000
        0b01111100
    ]
    /f: [
        rorg 0x1
        0b00111000
        0b01100000
        0b11111000
        0b01100000
        0b01100000
        0b01100000
        0b01100000
    ]
    /g: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01111100
        0b11001100
        0b11111100
        0b00001100
        0b11111000
    ]
    /h: [
        rorg 0x1
        0b11000000
        0b11000000
        0b11111000
        0b11001100
        0b11001100
        0b11001100
        0b11001100
    ]
    /i: [
        rorg 0x1
        0b00110000
        0b00000000
        0b11110000
        0b00110000
        0b00110000
        0b00110000
        0b11111100
    ]
    /j: [
        rorg 0x1
        0b00011000
        0b00000000
        0b01111000
        0b00011000
        0b00011000
        0b00011000
        0b11110000
    ]
    /k: [
        rorg 0x1
        0b11000000
        0b11000000
        0b11001100
        0b11011000
        0b11110000
        0b11011000
        0b11001100
    ]
    /l: [
        rorg 0x1
        0b11110000
        0b00110000
        0b00110000
        0b00110000
        0b00110000
        0b00110000
        0b11111100
    ]
    /m: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11101100
        0b11010110
        0b11010110
        0b11010110
        0b11000110
    ]
    /n: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11111000
        0b11001100
        0b11001100
        0b11001100
        0b11001100
    ]
    /o: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01111000
        0b11001100
        0b11001100
        0b11001100
        0b01111000
    ]
    /p: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01111000
        0b11001100
        0b11001100
        0b11111000
        0b11000000
    ]
    /q: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01111000
        0b11001100
        0b11001100
        0b01111100
        0b00001110
    ]
    /r: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11011000
        0b11101100
        0b11000000
        0b11000000
        0b11000000
    ]
    /s: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01111100
        0b11000000
        0b01111000
        0b00001100
        0b11111000
    ]
    /t: [
        rorg 0x1
        0b01100000
        0b01100000
        0b11111000
        0b01100000
        0b01100000
        0b01100000
        0b00111000
    ]
    /u: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11001100
        0b11001100
        0b11001100
        0b11001100
        0b01111000
    ]
    /v: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11001100
        0b11001100
        0b11001100
        0b01111000
        0b00110000
    ]
    /w: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11000110
        0b11010110
        0b11010110
        0b11010110
        0b11101100
    ]
    /x: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11001100
        0b01111000
        0b00110000
        0b01111000
        0b11001100
    ]
    /y: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11001100
        0b11001100
        0b01111100
        0b00001100
        0b01111000
    ]
    /z: [
        rorg 0x1
        0b00000000
        0b00000000
        0b11111100
        0b00001100
        0b00111000
        0b01100000
        0b11111100
    ]
    /left_curly: [
        rorg 0x1
        0b00111000
        0b01100000
        0b01100000
        0b11000000
        0b01100000
        0b01100000
        0b00111000
    ]
    /pipe: [
        rorg 0x1
        0b00010000
        0b00010000
        0b00010000
        0b00010000
        0b00010000
        0b00010000
        0b00010000
    ]
    /right_curly: [
        rorg 0x1
        0b00111000
        0b00001100
        0b00001100
        0b00000110
        0b00001100
        0b00001100
        0b00111000
    ]
    /tilde: [
        rorg 0x1
        0b00000000
        0b00000000
        0b01110110
        0b11011100
        0b00000000
        0b00000000
        0b00000000
    ]
