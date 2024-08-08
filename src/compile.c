static u8 out[0x10000];
static u32 pc = 0x100;

#define write(byte) {\
    if (pc == 655356 - 0x100) { err("exceeded maximum program size"); abort(); }\
    out[pc] = (byte);\
    pc += 1;\
} 

#define write2(byte2) { write(((byte2) & 0xff00) >> 8); write((byte2) & 0x00ff); }

static void compile_op(u8 mode_k, u8 mode_r, u8 mode_2, Op op) {
    write(op | (u8)(mode_2 << 5) | (u8)(mode_r << 6) | (u8)(mode_k << 7));
}

static bool is_whitespace(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool is_alpha(u8 c) { return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'); }

static bool compile(Arena *arena, usize max_asm_filesize, char *path_biciasm, char *path_bici) {
    String8 asm_file = file_read(arena, path_biciasm, "rb", max_asm_filesize);
    u8 *assembly = asm_file.ptr;
    usize len = asm_file.len;

    for (usize i = 0; i < len; i += 1) {
        if (is_whitespace(assembly[i])) continue;

        u8 mode_k = 0, mode_r = 0, mode_2 = 0;

        if (is_alpha(assembly[i])) {
            usize beg_i = i, end_i = beg_i;
            for (; i < len && !is_whitespace(assembly[i]); i += 1) {
                if ('a' <= assembly[i] && assembly[i] <= 'z') continue;
                end_i = i;
                if (assembly[i] == ';') for (i += 1; i < len;) {
                    if (is_whitespace(assembly[i])) goto done;
                    switch (assembly[i]) {
                        case 'k': mode_k = 1; break;
                        case 'r': mode_r = 1; break;
                        case '2': mode_2 = 1; break;
                        default: {
                            errf("expected one of ['k', 'r', '2'], found byte '%c'/%d/#%x at '%s'[%zu]", assembly[i], assembly[i], assembly[i], path_biciasm, i);
                            return false;
                        } break;
                    }
                    i += 1;
                }
            }
            done:
            if (end_i == beg_i) end_i = i;
            String8 lexeme = string8_range(asm_file, beg_i, end_i);

            if (false) {}
            #define compile_if_op_string(name, val)\
                else if (string8_eql(lexeme, string8(#name))) compile_op(mode_k, mode_r, mode_2, val);
            for_op(compile_if_op_string)
            else {
                errf("invalid token '%.*s' at '%s'[%zu..%zu]", string_fmt(lexeme), path_biciasm, beg_i, end_i);
                return false;
            };

            continue;
        }

        switch (assembly[i]) {
            case '#': {
                usize beg_i = i;
                i += 1; 
                u8 digit_num = 0;
                for (; i < len && !is_whitespace(assembly[i]); i += 1, digit_num += 1) {
                    if (!((assembly[i] >= '0' && assembly[i] <= '9') || (assembly[i] >= 'a' && assembly[i] <= 'f'))) {
                        errf("expected hex digit [0-9|a-f], found byte '%c'/%d/#%x at '%s'[%zu]", assembly[i], assembly[i], assembly[i], path_biciasm, i);
                        return false;
                    }
                }
                if (digit_num != 2 && digit_num != 4) {
                    errf("expected 2 digits (byte) or 4 digits (short), found %zu digits in number at '%s'[%zu]", digit_num, path_biciasm, beg_i);
                    return false;
                }
                const u8 *x = decimal_from_hex_char_table;
                switch (digit_num) {
                    case 2: {
                        u8 num = (x[assembly[beg_i + 1]] * 0x10) | x[assembly[beg_i + 2]];
                        compile_op(0, 0, 0, op_push);
                        write(num);
                    } break;
                    case 4: {
                        u16 num = (x[assembly[beg_i + 1]] * 0x1000) | (x[assembly[beg_i + 2]] * 0x100) | (x[assembly[beg_i + 3]] * 0x10) | x[assembly[beg_i + 4]];
                        compile_op(0, 0, 1, op_push);
                        write2(num);
                    } break;
                    default: unreachable;
                }
            } break;
            default: {
                errf("invalid byte '%c'/%d/#%x at '%s'[%zu]", assembly[i], assembly[i], assembly[i], path_biciasm, i);
                return false;
            } break;
        }
    }

    FILE *rom_file = file_open(path_bici, "wb");
    if (rom_file == 0) return false;
    String8 rom = { .ptr = out, .len = pc };
    file_write(rom_file, rom);
    fclose(rom_file);

    printf("ASM ===\n");
    for (usize i = 0x100; i < pc; i += 1) printf("%02x ", out[i]);
    putchar('\n');
    return true;
}
