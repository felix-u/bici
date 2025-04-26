system:
    rorg 0x2 ; address 0 reserved
    system_working_stack: rorg 0x1
    system_return_stack:  rorg 0x1
    system_start:         rorg 0x2
    system_quit:          rorg 0x2
    system_colour_0:      rorg 0x2
    system_colour_1:      rorg 0x2
    system_colour_2:      rorg 0x2
    system_colour_3:      rorg 0x2

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

org 0x40

org 0x100

draw_text: ; (string_address, x, y: u16 -> _)
    push screen_y write.2
    push screen_x write.2

    push 0b00000001
    push screen_auto write

    ; store character count
    dup.2
    load
    push.2 count store

    ; store address of first character
    inc.2
    push.2 address store.2

    push.2 0x0
    /loop:
        dup.2

        push.2 count load
        push 0x0 swap ; cast to u16

        lt.2
        jni {
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
            push 0b00000100
            push screen_sprite
            write

            inc.2
            jmi loop
        }
        drop.2

    jmp.r

    /count: rorg 0x2
    /index: rorg 0x2
    /address: rorg 0x2

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
    rorg 0xd0 ; skip 'a'..'z'
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
