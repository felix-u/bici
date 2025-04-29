include "header.asm"

patch system_colour_0, 0xeee
patch system_colour_1, 0xccc
patch system_colour_2, 0x3aa
patch system_colour_3, 0x000

org 0x8000
OS_START:

patch screen_update, update
update:
    ; clear background
    push.2 0x0 push screen_x write.2
    push.2 0x0 push screen_y write.2
    push 0b11000010
    push screen_pixel write

    ; BEGIN DRAW TITLEBAR =====================================================

    ; draw titlebar background
    push.2 0x0
    /loop: dup.2 push.2 0xc lt.2 jni {
        dup.2
        push screen_y write.2

        push 0b10000000
        push screen_pixel write

        inc.2
        jmi loop
    } drop.2
    push.2 0xc push screen_y write.2
    push 0b10000011 push screen_pixel write

    ; draw logo
    push.2 0x6 push screen_x write.2
    push.2 0x2 push screen_y write.2
    push.2 os_logo_sprite push screen_data write.2
    push 0b00001100 push screen_sprite write

    ; draw text
    push.2 os_titlebar_text
    push.2 0x14
    push.2 0x2
    push 0b00001100
    jsi draw_text

    ; END DRAW TITLEBAR =======================================================

    ; BEGIN DRAW FILE TEXT ====================================================

    ; for 0..default_program_count
    push 0x0
    /loop10: dup push.2 default_program_count load lt jni {
        dup

        ; save index
        dup push.2 current_program_label_index store

        ; draw floppy
        jsi current_program_label_x_coordinate push.2 0xc sub.2 push screen_x write.2
        jsi current_program_label_y_coordinate inc.2 push screen_y write.2
        push.2 floppy_icon_sprite push screen_data write.2
        push 0b01001101 push screen_sprite write

        ; save program name address address = default_programs + current index
        push 0x2 mul ; sizeof(address) = 2
        jsi cast_u16_from_u8
        push.2 default_programs
        add.2 ; &default_programs[index]
        load.2 push.2 current_program_name store.2

        ; draw file name
        push.2 current_program_name load.2
        jsi current_program_label_x_coordinate
        jsi current_program_label_y_coordinate
        push 0b00001100
        jsi draw_text

        ; BEGIN CLICK CHECK ===================================================

        jsi current_program_label_x_coordinate
        jsi current_program_label_y_coordinate
        push.2 current_program_name load.2
        jsi mouse_in_text
        push 0x16 mul ; (<< 4 = 0b00010000 so that anding with the mouse click value works)

        push mouse_left_button read
        push 0b11110000 and

        and jni {
            ; set up current file for querying
            push.2 current_program_name load.2
            push file_name write.2

            ; error if file_length == 0
            push file_length read.2
            push.2 0x0 eq.2 jni {
                ; TODO(felix): nicer solution here
                push.2 error_file_not_found push console_print write.2
                jmi end_click_check
            }

            ; error if file_length < 256
            push file_length read.2
            push.2 0x100 lt.2 jni {
                ; TODO(felix): nicer solution here
                push.2 error_file_invalid push console_print write.2
                jmi end_click_check
            }

            ; TODO(felix): check if ROM fits

            ; we want to read file[system_end] to get the length of the rom
            push.2 system_end
            push file_cursor write.2
            push file_read read.2

            ; error if system_end < 256
            dup.2 push.2 0x100 lt.2 jni {
                ; TODO(felix): nicer solution here
                drop.2
                push.2 error_length_invalid push console_print write.2
                jmi end_click_check
            }

            ; when we copy the file into memory, we only copy as much as we need to
            push file_length write.2

            ; copy
            push.2 0x0 ; TODO(felix): we in fact need to do something else than overwriting the current ROM
            ; jsi get_new_program_memory_location
            push file_copy write.2

            jsi save_current_program_routine_addresses
        }

        /end_click_check:
        ; END CLICK CHECK =====================================================

        inc
        jmi loop10
    } drop

    ; END DRAW FILE TEXT ======================================================

    push 0b00001100
    jsi draw_default_mouse_cursor_at_mouse

    break

    /current_program_name: rorg 0x2
    /current_program_label_index: rorg 0x1
    /current_program_label_x_coordinate: ; (_ -> u16)
        push.2 0x1c
        jmp.r
    /current_program_label_y_coordinate: ; (_ -> u16)
        push.2 current_program_label_index load
        jsi cast_u16_from_u8
        push.2 0x10 mul.2
        push.2 0x20 add.2
        jmp.r
    /error_file_not_found: [$ "File not found" 0xa ]
    /error_file_invalid:   [$ "File invalid" 0xa ]
    /error_length_invalid: [$ "Rom does not store valid length" 0xa ]

default_program_count: [ 0x4 ]
default_programs: [ hello_rom keyboard_rom mouse_rom screen_rom ]
    /hello_rom: [$ "hello.rom" ]
    /keyboard_rom: [$ "keyboard.rom" ]
    /mouse_rom: [$ "mouse.rom" ]
    /screen_rom: [$ "screen.rom" ]

os_titlebar_text: [$ "tinyOS" ]

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

floppy_icon_sprite: [
    0b11111111
    0b10000001
    0b10100101
    0b10000001
    0b10100101
    0b10011001
    0b10000001
    0b11111111
]

save_current_program_routine_addresses: ; (_ -> _)
    ; TODO(felix): get actual current program
    push.2 program_1_memory push.2 program_memory store.2

    jmp.r

    /program_memory: rorg 0x2

get_new_program_memory_location: ; (_ -> u16)
    push.2 program_1_memory
    jmp.r

program_1:
    program_1_memory:

EOF:
org 0xffff
