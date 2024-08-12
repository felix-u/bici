static char *path_biciasm;
static u8 *assembly;
static u16 len, i;

static u8 out[0x10000];
static u16 pc = 0x100;

static u16 forward_res[0x100];
static u8 forward_res_idx;

structdef(Label) { String8 name; u16 addr; };
static Label labels[0x100];
static u8 label_idx;
#define label_push(name, addr) {\
    if (label_idx == 0x99) {\
        errf("cannot add label '%.*s': maximum number of labels reached", string_fmt(name));\
        pc = 0;\
    }\
    labels[label_idx++] = (Label){ name, addr };\
}
static u16 label_get_addr_of(String8 name) {
    for (u8 idx = 0; idx < label_idx; idx += 1) {
        if (!string8_eql(labels[idx].name, name)) continue;
        return labels[idx].addr;
    }
    errf("no such label '%.*s'", string_fmt(name));
    return 0;
}
 
structdef(Label_Ref) { String8 name; u16 loc; };
static Label_Ref label_refs[0x100];
static u8 label_ref_idx;

structdef(Hex) {
    bool ok;
    u8 num_digits;
    union { u8 int8; u16 int16; } result;
};

static Hex parse_hex(void) {
    u16 beg_i = i;
    while (i < len && is_hex_digit_table[assembly[i]]) i += 1;
    u16 end_i = i;
    i -= 1;
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

static bool is_whitespace(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool is_alpha(u8 c) { return ('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || (c == '_'); }

static String8 parse_alpha(void) {
    u16 beg_i = i;
    if (!is_alpha(assembly[i])) {
        errf(
            "expected alphabetic character or underscore, found byte '%c'/%d/#%x at '%s'[%zu]", 
            assembly[i], assembly[i], assembly[i], path_biciasm, i
        );
        return (String8){0};
    }

    i += 1;
    while (i < len && (is_alpha(assembly[i]))) i += 1;
    u16 end_i = i;
    i -= 1;
    return (String8){ .ptr = assembly + beg_i, .len = end_i - beg_i };
}

#define write(byte) {\
    out[pc] = (byte);\
    pc += 1;\
    if (pc == 0) { err("exceeded maximum program size"); abort(); }\
} 

#define write2(byte2) { write(((byte2) & 0xff00) >> 8); write((byte2) & 0x00ff); }

static void compile_op(Mode mode, Op op) {
    write(op | (u8)(mode.size << 5) | (u8)(mode.stack << 6) | (u8)(mode.keep << 7));
}

static bool compile(Arena *arena, usize max_asm_filesize, char *_path_biciasm, char *path_bici) {
    path_biciasm = _path_biciasm;
    String8 asm_file = file_read(arena, path_biciasm, "rb", max_asm_filesize);
    if (asm_file.len == 0) return false;

    assembly = asm_file.ptr;
    len = (u16)asm_file.len;

    u16 add = 1;
    for (i = 0; i < len; i += add) {
        add = 1;
        if (is_whitespace(assembly[i])) continue;

        Mode mode = {0};

        switch (assembly[i]) {
            case '/': if (i + 1 < len && assembly[i + 1] == '/') {
                for (i += 2; i < len && assembly[i] != '\n';) i += 1;
            } continue;
            case '|': {
                i += 1;
                Hex new_pc = parse_hex();
                if (!new_pc.ok) return false;
                pc = new_pc.result.int16;
            } continue;
            case '#': {
                i += 1; 
                Hex num = parse_hex();
                if (!num.ok) return false;
                switch (num.num_digits) {
                    case 2: compile_op((Mode){ .size = size_byte }, op_push); write(num.result.int8); break;
                    case 4: compile_op((Mode){ .size = size_short }, op_push); write2(num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '&': {
                compile_op((Mode){ .size = size_short }, op_push);
                i += 1;
                String8 name = parse_alpha();
                label_refs[label_ref_idx] = (Label_Ref){ .name = name, .loc = pc };
                label_ref_idx += 1;
                pc += 2;
            } continue;
            case ':': {
                i += 1;
                String8 name = parse_alpha();
                label_push(name, pc);
            } continue;
            case '{': {
                forward_res[forward_res_idx] = pc;
                forward_res_idx += 1;
                pc += 1;
                add = 2; // TODO: handle more forward resolution modes than just '#', which we're skipping over here
            } continue;
            case '}': { // TODO: more forward resolution modes
                i += 1;
                forward_res_idx -= 1;
                u16 res_pc = forward_res[forward_res_idx];
                out[res_pc] = (u8)(pc - 1 - res_pc);
            } continue;
            case '_': {
                i += 1;
                Hex num = parse_hex();
                if (!num.ok) return false;
                switch (num.num_digits) {
                    case 2: write(num.result.int8); break;
                    case 4: write2(num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '"': {
                for (i += 1; i < len && !is_whitespace(assembly[i]); i += 1) {
                    write(assembly[i]);
                }
                add = 0;
            } continue;
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
                    case 'k': mode.keep = true; break;
                    case 'r': mode.stack = stack_ret; break;
                    case '2': mode.size = size_short; break;
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
            else if (string8_eql(lexeme, string8(#name))) compile_op(mode, val);
        for_op(compile_if_op_string)
        else {
            errf("invalid token '%.*s' at '%s'[%zu..%zu]", string_fmt(lexeme), path_biciasm, beg_i, end_i);
            return false;
        };

        continue;
    }
    if (pc == 0) return false;
    String8 rom = { .ptr = out, .len = pc };

    for (u8 idx = 0; idx < label_ref_idx; idx += 1) {
        Label_Ref ref = label_refs[idx];
        if (ref.name.len == 0) return false;
        pc = ref.loc;
        write2(label_get_addr_of(ref.name));
    }

    FILE *rom_file = file_open(path_bici, "wb");
    if (rom_file == 0) return false;
    file_write(rom_file, rom);
    fclose(rom_file);

    printf("ASM ===\n");
    for (usize idx = 0x100; idx < rom.len; idx += 1) printf("%02x ", out[idx]);
    putchar('\n');
    return true;
}
