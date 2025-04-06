org 0x00 console_print:
org 0x10 screen:
    org 0x10 [init]
    org 0x11 pixel:
    org 0x12 [update]
    org 0x14 [quit]

org 0x100

update: break
quit: break

init:
    push.2 0x0
    while:
        dup.2
        push.2 EOF
        lt.2
        jni {
            dup.2
            jsi load_coords_at_addr
            push 0x06
            push pixel
            write

            push.2 0x2
            add.2
            jmi while
        }
        drop.2
    break

cast_16_from_8:
    push 0x0
    swap
    jmp.r

load_coords_at_addr:
    dup.2
    load
    push 0x02
    div
    jsi cast_16_from_8
    swap.2
    inc.2
    load
    push 0x02
    div
    jsi cast_16_from_8
    jmp.r

smiley: [
    0x0606 0x1206
    0x060c 0x080e 0x0a10 0x0c10 0x0e10 0x100e 0x120c
]

diagonal_line: [
    0x1020
    0x2040
    0x3060
    0x4080
    0x50a0
    0x60c0
    0x70e0
    0x80f0
]

EOF:
