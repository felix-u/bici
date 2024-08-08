static char *path_biciasm;
static u8 *assembly;
static u16 len, i;

static u8 out[0x10000];
static u32 pc = 0x100;

structdef(Hex) {
    bool ok;
    u8 num_digits;
    union { u8 int8; u16 int16; } result;
};

static Hex parse_hex(void) {
    u16 beg_i = i;
    while (i < len && is_hex_digit_table[assembly[i]]) i += 1;
    u16 end_i = i;
    u8 num_digits = (u8)(end_i - beg_i);
    String8 hex_string = { .ptr = assembly + beg_i, .len = num_digits };

    switch (num_digits) {
        case 2: return (Hex){
            .ok = true,
            .num_digits = num_digits,
            .result.int8 = (u8)decimal_from_hex_string8(hex_string),
        };
        case 4: return (Hex){
            .ok = true,
            .num_digits = num_digits,
            .result.int16 = (u16)decimal_from_hex_string8(hex_string),
        };
        default: {
            errf("expected 2 digits (byte) or 4 digits (short), found %d digits in number at '%s'[%d..%d]",
                num_digits, path_biciasm, beg_i, end_i);
            return (Hex){0};
        } break;
    }
}

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

static bool compile(Arena *arena, usize max_asm_filesize, char *_path_biciasm, char *path_bici) {
    path_biciasm = _path_biciasm;
    String8 asm_file = file_read(arena, path_biciasm, "rb", max_asm_filesize);
    assembly = asm_file.ptr;
    len = (u16)asm_file.len;

    for (i = 0; i < len; i += 1) {
        if (is_whitespace(assembly[i])) continue;

        u8 mode_k = 0, mode_r = 0, mode_2 = 0;

        switch (assembly[i]) {
            case '/': if (i + 1 < len && assembly[i + 1] == '/') {
                for (i += 2; i < len && assembly[i] != '\n';) i += 1;
                continue;
            } break;
            case '|': {
                i += 1;
                Hex new_pc = parse_hex();
                if (!new_pc.ok) return false;
                pc = new_pc.result.int16;
                continue;
            } break;
            case '#': {
                i += 1; 
                Hex num = parse_hex();
                if (!num.ok) return false;
                switch (num.num_digits) {
                    case 2: compile_op(0, 0, 0, op_push); write(num.result.int8); break;
                    case 4: compile_op(0, 0, 1, op_push); write2(num.result.int16); break;
                    default: unreachable;
                }
                continue;
            } break;
            default: break;
        }

        if (!is_alpha(assembly[i])) {
            errf("invalid byte '%c'/%d/#%x at '%s'[%zu]", assembly[i], assembly[i], assembly[i], path_biciasm, i);
            return false;
        }

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

    if (pc == 0) return false;

    FILE *rom_file = file_open(path_bici, "wb");
    if (rom_file == 0) return false;
    String8 rom = { .ptr = out, .len = pc };
    file_write(rom_file, rom);
    fclose(rom_file);

    printf("ASM ===\n");
    for (usize idx = 0x100; idx < pc; idx += 1) printf("%02x ", out[idx]);
    putchar('\n');
    return true;
}
