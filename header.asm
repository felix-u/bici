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

org 0x30

org 0x100

draw_character: ; (character: u8, x, y: u16 -> _)
    push screen_y write.2
    push screen_x write.2

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

    ; draw sprite
    push 0x0
    push screen_sprite
    write

    jmp.r

draw_text: ; (string_address, x, y: u16 -> _)
    push.2 y store.2
    push.2 x store.2

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

            ; draw character

            push.2 x load.2
            push.2 index load.2
            push.2 0x8 mul.2
            add.2

            push.2 y load.2

            jsi draw_character

            inc.2
            jmi loop
        }
        drop.2

    jmp.r

    /count: rorg 0x2
    /index: rorg 0x2
    /address: rorg 0x2
    /x: rorg 0x2
    /y: rorg 0x2

; TODO(felix): mechanism to conditionally include? to decrease binary size for programs not using the font,
;              while keeping the fonts in this file

font:
    rorg 0x208 ; skip to 'A' * 8 (bytes per glyph)
    /A: [
        0b00000000
        0b01111100
        0b11000110
        0b11000110
        0b11111110
        0b11000110
        0b11000110
        0b11000110
    ]
    /B: [
        0b00000000
        0b11111100
        0b11000110
        0b11000110
        0b11111100
        0b11000110
        0b11000110
        0b11111100
    ]
    /C: [
        0b00000000
        0b01111100
        0b11000110
        0b11000000
        0b11000000
        0b11000000
        0b11000110
        0b01111100
    ]
    /D: [
        0b00000000
        0b11111000
        0b11001100
        0b11000110
        0b11000110
        0b11000110
        0b11001100
        0b11111000
    ]
