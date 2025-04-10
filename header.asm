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

org 0x30

org 0x100
