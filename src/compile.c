static u8 out[65536 - 0x100];
static u32 pc;

#define write(byte) {\
    if (pc == 655356 - 0x100) { err("exceeded maximum program size"); abort(); }\
    out[pc] = (byte);\
    pc += 1;\
} 

#define write2(byte2) { write(((byte2) & 0xff00) >> 16); write((byte2) & 0x00ff); }

static void compile_op(Op op, bool mode_16, bool mode_ret, bool mode_keep) {
    write(op | (mode_16 << 5) | (mode_ret << 6) | (mode_keep << 7));
}

static void compile(Arena *arena, usize max_asm_filesize, char *path_biciasm, char *path_bici) {
    String8 asm_file = file_read(arena, path_biciasm, "rb", max_asm_filesize);
    u8 *asm = asm_file.ptr;
    usize len = asm_file.len;

    for (usize i = 0; i < len; i += 1) {
        switch (asm[i]) {
            case '#': {
                usize beg_i = i;
                i += 1; 
                u8 digit_num = 0;
                for (; i < len; i += 1, digit_num += 1) {
                    if (!((asm[i] >= '0' && asm[i] <= '9') || (asm[i] >= 'a' && asm[i] <= 'f'))) {
                        errf("expected hex digit [0-9|a-f], found byte '%c'/0d%d/0x%x at '%s'[%zu]", asm[i], asm[i], asm[i], path_biciasm, i);
                        return;
                    }
                }
                if (digit_num != 2 || digit_num != 4) {
                    errf("expected 2 digits (byte) or 4 digits (short), found %zu digits in number at '%s'[%zu]", digit_num, path_biciasm, beg_i);
                    return;
                }
                const u8 *x = decimal_from_hex_char_table;
                switch (digit_num) {
                    case 2: {
                        u8 num = (x[asm[beg_i + 1]] * 0x10) | x[asm[beg_i + 2]];
                        compile_op(op_push, 0, 0, 0);
                        write(num);
                    } break;
                    case 4: {
                        u16 num = (x[asm[beg_i + 1]] * 0x1000) | (x[asm[beg_i + 2]] * 0x100) | (x[asm[beg_i + 3]] * 0x10) | x[asm[beg_i + 4]];
                        compile_op(op_push, 1, 0, 0);
                        write2(num);
                    } break;
                    default: unreachable;
                }
            } break;
            default: {
                errf("invalid byte '%c'/0d%d/0x%x at '%s'[%zu]", asm[i], asm[i], asm[i], path_biciasm, i);
                return;
            } break;
        }
    }

    // TODO
    discard(path_bici);
}
