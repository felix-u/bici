static u8 out[65536 - 0x100];
static u32 pc;

#define write(byte) {\
    if (pc == 655356 - 0x100) { err("exceeded maximum program size"); abort(); }\
    out[pc] = (byte);\
    pc += 1;\
} 

#define write2(byte2) { write(((byte2) & 0xff00) >> 16); write((byte2) & 0x00ff); }

static void compile_op(u8 mode_k, u8 mode_r, u8 mode_2, Op op) {
    write(op | (mode_2 << 5) | (mode_r << 6) | (mode_k << 7));
}

static bool is_whitespace(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool compile(Arena *arena, usize max_asm_filesize, char *path_biciasm, char *path_bici) {
    String8 asm_file = file_read(arena, path_biciasm, "rb", max_asm_filesize);
    u8 *asm = asm_file.ptr;
    usize len = asm_file.len;

    for (usize i = 0; i < len; i += 1) {
        if (is_whitespace(asm[i])) continue;

        u8 mode_k = 0, mode_r = 0, mode_2 = 0;

        if (isalpha(asm[i])) {
            usize beg_i = i, end_i = i;
            for (; i < len && !is_whitespace(asm[i]); i += 1) {
                if ('a' <= asm[i] && asm[i] <= 'z') continue;
                end_i = i;
                if (asm[i] == ';') {
                    i += 1;
                    for (; i < len && !is_whitespace(asm[i]); i += 1) switch (asm[i]) {
                        case 'k': mode_k = 1; break;
                        case 'r': mode_r = 1; break;
                        case '2': mode_2 = 1; break;
                        default: {
                            errf("expected one of ['k', 'r', '2'], found byte '%c'/%d/#%x at '%s'[%zu]", asm[i], asm[i], asm[i], path_biciasm, i);
                            return false;
                        } break;
                    }
                }
            }
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

        switch (asm[i]) {
            case '#': {
                usize beg_i = i;
                i += 1; 
                u8 digit_num = 0;
                for (; i < len && !is_whitespace(asm[i]); i += 1, digit_num += 1) {
                    if (!((asm[i] >= '0' && asm[i] <= '9') || (asm[i] >= 'a' && asm[i] <= 'f'))) {
                        errf("expected hex digit [0-9|a-f], found byte '%c'/%d/#%x at '%s'[%zu]", asm[i], asm[i], asm[i], path_biciasm, i);
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
                        u8 num = (x[asm[beg_i + 1]] * 0x10) | x[asm[beg_i + 2]];
                        compile_op(0, 0, 0, op_push);
                        write(num);
                    } break;
                    case 4: {
                        u16 num = (x[asm[beg_i + 1]] * 0x1000) | (x[asm[beg_i + 2]] * 0x100) | (x[asm[beg_i + 3]] * 0x10) | x[asm[beg_i + 4]];
                        compile_op(0, 0, 1, op_push);
                        write2(num);
                    } break;
                    default: unreachable;
                }
            } break;
            default: {
                errf("invalid byte '%c'/%d/#%x at '%s'[%zu]", asm[i], asm[i], asm[i], path_biciasm, i);
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
    for (usize i = 0; i < pc; i += 1) printf("%02x ", out[i]);
    putchar('\n');
    return true;
}
