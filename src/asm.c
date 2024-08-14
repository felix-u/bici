structdef(Label) { String8 name; u16 addr; };

structdef(Label_Ref) { String8 name; u16 loc; Size size; };

enumdef(Res_Mode, u8) { res_num, res_addr };
structdef(Res) { u16 pc; Res_Mode mode; };

typedef Array(u8) Array_u8;

structdef(Asm) {
    char *path_biciasm;
    String8 infile;
    u16 i;

    Array_Label labels;
    Array_Label_Ref label_refs;
    Array_Res forward_res;

    Array_u8 rom;
};

static void asm_label_push(Asm *ctx, String8 name, u16 addr) {
    if (ctx->labels.len == ctx->labels.cap) {
        errf("cannot add label '%.*s': maximum number of labels reached", string_fmt(name));
        abort();
    }
    ctx->labels.ptr[ctx->labels.len] = (Label){ .name = name, .addr = addr }; 
    ctx->labels.len += 1;
}

static u16 asm_label_get_addr_of(Asm *ctx, String8 name) {
    for (u8 i = 0; i < ctx->labels.len; i += 1) {
        if (!string8_eql(ctx->labels.ptr[i].name, name)) continue;
        return ctx->labels.ptr[i].addr;
    }
    errf("no such label '%.*s'", string_fmt(name));
    return 0;
}

structdef(Hex) {
    bool ok;
    u8 num_digits;
    union { u8 int8; u16 int16; } result;
};

static Hex asm_parse_hex(Asm *ctx) {
    u16 beg_i = ctx->i;
    while (ctx->i < ctx->infile.len && is_hex_digit_table[ctx->infile.ptr[ctx->i]]) ctx->i += 1;
    u16 end_i = ctx->i;
    ctx->i -= 1;
    u8 num_digits = (u8)(end_i - beg_i);
    String8 hex_string = { .ptr = ctx->infile.ptr + beg_i, .len = num_digits };

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
                num_digits, ctx->path_biciasm, beg_i, end_i);
            return (Hex){0};
        } break;
    }
}

static bool is_whitespace(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool is_alpha(u8 c) { return ('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || (c == '_'); }

static String8 asm_parse_alpha(Asm *ctx) {
    u16 beg_i = ctx->i;
    if (!is_alpha(ctx->infile.ptr[ctx->i])) {
        errf(
            "expected alphabetic character or underscore, found byte '%c'/%d/#%x at '%s'[%zu]", 
            ctx->infile.ptr[ctx->i], ctx->infile.ptr[ctx->i], ctx->infile.ptr[ctx->i], ctx->path_biciasm, ctx->i
        );
        return (String8){0};
    }

    ctx->i += 1;
    while (ctx->i < ctx->infile.len && (is_alpha(ctx->infile.ptr[ctx->i]))) ctx->i += 1;
    u16 end_i = ctx->i;
    ctx->i -= 1;
    return (String8){ .ptr = ctx->infile.ptr + beg_i, .len = end_i - beg_i };
}

static void asm_rom_write(Asm *ctx, u8 byte) {
    if (ctx->rom.len == ctx->rom.cap) {
        err("reached maximum rom size");
        abort();
    }
    array_push_assume_capacity(&ctx->rom, &byte);
} 

static void asm_rom_write2(Asm *ctx, u16 byte2) {
    asm_rom_write(ctx, (byte2 & 0xff00) >> 8); 
    asm_rom_write(ctx, byte2 & 0x00ff); 
}

static void forward_res_push(Asm *ctx, Res_Mode res_mode) {
    if (ctx->forward_res.len == 0x0ff) {
        errf("cannot resolve address at '%s'[%d]: maximum number of resolutions reached", ctx->path_biciasm, ctx->i);
        ctx->rom.len = 0;
        return;
    }
    ctx->forward_res.ptr[ctx->forward_res.len] = (Res){ .pc = (u16)ctx->rom.len, .mode = res_mode };
    ctx->forward_res.len += 1;
}
static void forward_res_resolve_last(Asm *ctx) {
    if (ctx->forward_res.len == 0) {
        errf("forward resolution marker at '%s'[%d] matches no opening marker", ctx->path_biciasm, ctx->i);
        ctx->rom.len = 0;
        return;
    }
    ctx->forward_res.len -= 1;
    Res res = ctx->forward_res.ptr[ctx->forward_res.len];
    switch (res.mode) {
        case res_num: ctx->rom.ptr[res.pc] = (u8)(ctx->rom.len - 1 - res.pc); break;
        case res_addr: {
            u16 pc_bak = (u16)ctx->rom.len;
            ctx->rom.len = res.pc;
            asm_rom_write2(ctx, pc_bak);
            ctx->rom.len = pc_bak;
        } break;
        default: unreachable;
    }
}

static void compile_op(Asm *ctx, Mode mode, u8 op) {
    if (instruction_is_special(op)) { asm_rom_write(ctx, op); return; }
    asm_rom_write(ctx, op | (u8)(mode.size << 5) | (u8)(mode.stack << 6) | (u8)(mode.keep << 7));
}

static String8 asm_compile(Arena *arena, usize max_asm_filesize, char *_path_biciasm) {
    static u8 rom_mem[0x10000];
    static Label labels_mem[0x100];
    static Label_Ref label_refs_mem[0x100];
    static Res forward_res_mem[0x100];

    Asm ctx = { 
        .path_biciasm = _path_biciasm, 
        .infile = file_read(arena, _path_biciasm, "rb", max_asm_filesize),
        .labels = { .ptr = labels_mem, .cap = 0x100 },
        .label_refs = { .ptr = label_refs_mem, .cap = 0x100 },
        .forward_res = { .ptr = forward_res_mem, .cap = 0x100 },
        .rom = { .ptr = rom_mem, .len = 0x100, .cap = 0x10000 },
    };
    if (ctx.infile.len == 0) return (String8){0};

    u16 add = 1;
    for (ctx.i = 0; ctx.i < ctx.infile.len; ctx.i += add) {
        add = 1;
        if (is_whitespace(ctx.infile.ptr[ctx.i])) continue;

        Mode mode = {0};

        switch (ctx.infile.ptr[ctx.i]) {
            case '/': if (ctx.i + 1 < ctx.infile.len && ctx.infile.ptr[ctx.i + 1] == '/') {
                for (ctx.i += 2; ctx.i < ctx.infile.len && ctx.infile.ptr[ctx.i] != '\n';) ctx.i += 1;
            } continue;
            case '|': {
                ctx.i += 1;
                Hex new_pc = asm_parse_hex(&ctx);
                if (!new_pc.ok) return (String8){0};
                ctx.rom.len = new_pc.result.int16;
            } continue;
            case '@': {
                ctx.i += 1;
                String8 name = asm_parse_alpha(&ctx);
                ctx.label_refs.ptr[ctx.label_refs.len] = (Label_Ref){ .name = name, .loc = (u16)ctx.rom.len, .size = size_short };
                ctx.label_refs.len += 1;
                ctx.rom.len += 2;
            } continue;
            case '#': {
                ctx.i += 1; 
                Hex num = asm_parse_hex(&ctx);
                if (!num.ok) return (String8){0};
                switch (num.num_digits) {
                    case 2: compile_op(&ctx, (Mode){ .size = size_byte }, op_push); asm_rom_write(&ctx, num.result.int8); break;
                    case 4: compile_op(&ctx, (Mode){ .size = size_short }, op_push); asm_rom_write2(&ctx, num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '*': {
                compile_op(&ctx, (Mode){ .size = size_byte }, op_push);
                ctx.i += 1;
                String8 name = asm_parse_alpha(&ctx);
                ctx.label_refs.ptr[ctx.label_refs.len] = (Label_Ref){ .name = name, .loc = (u16)ctx.rom.len, .size = size_byte };
                ctx.label_refs.len += 1;
                ctx.rom.len += 1;
            } continue;
            case '&': {
                compile_op(&ctx, (Mode){ .size = size_short }, op_push);
                ctx.i += 1;
                String8 name = asm_parse_alpha(&ctx);
                ctx.label_refs.ptr[ctx.label_refs.len] = (Label_Ref){ .name = name, .loc = (u16)ctx.rom.len, .size = size_short };
                ctx.label_refs.len += 1;
                ctx.rom.len += 2;
            } continue;
            case ':': {
                ctx.i += 1;
                String8 name = asm_parse_alpha(&ctx);
                asm_label_push(&ctx, name, (u16)ctx.rom.len);
            } continue;
            case '{': {
                ctx.i += 1;
                switch (ctx.infile.ptr[ctx.i]) {
                    case '#': {
                        forward_res_push(&ctx, res_num);
                        ctx.rom.len += 1;
                    } continue;
                    default: {
                        forward_res_push(&ctx, res_addr);
                        ctx.rom.len += 2;
                        add = 0;
                    } continue;
                }
            } continue;
            case '}': forward_res_resolve_last(&ctx); continue;
            case '_': {
                ctx.i += 1;
                Hex num = asm_parse_hex(&ctx);
                if (!num.ok) return (String8){0};
                switch (num.num_digits) {
                    case 2: asm_rom_write(&ctx, num.result.int8); break;
                    case 4: asm_rom_write2(&ctx, num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '"': {
                for (ctx.i += 1; ctx.i < ctx.infile.len && !is_whitespace(ctx.infile.ptr[ctx.i]); ctx.i += 1) {
                    asm_rom_write(&ctx, ctx.infile.ptr[ctx.i]);
                }
                add = 0;
            } continue;
            default: break;
        }

        if (!is_alpha(ctx.infile.ptr[ctx.i])) {
            errf("invalid byte '%c'/%d/#%x at '%s'[%zu]", ctx.infile.ptr[ctx.i], ctx.infile.ptr[ctx.i], ctx.infile.ptr[ctx.i], ctx.path_biciasm, ctx.i);
            return (String8){0};
        }

        usize beg_i = ctx.i, end_i = beg_i;
        for (; ctx.i < ctx.infile.len && !is_whitespace(ctx.infile.ptr[ctx.i]); ctx.i += 1) {
            if ('a' <= ctx.infile.ptr[ctx.i] && ctx.infile.ptr[ctx.i] <= 'z') continue;
            end_i = ctx.i;
            if (ctx.infile.ptr[ctx.i] == ';') for (ctx.i += 1; ctx.i < ctx.infile.len;) {
                if (is_whitespace(ctx.infile.ptr[ctx.i])) goto done;
                switch (ctx.infile.ptr[ctx.i]) {
                    case 'k': mode.keep = true; break;
                    case 'r': mode.stack = stack_ret; break;
                    case '2': mode.size = size_short; break;
                    default: {
                        errf("expected one of ['k', 'r', '2'], found byte '%c'/%d/#%x at '%s'[%zu]", ctx.infile.ptr[ctx.i], ctx.infile.ptr[ctx.i], ctx.infile.ptr[ctx.i], ctx.path_biciasm, ctx.i);
                        return (String8){0};
                    } break;
                }
                ctx.i += 1;
            }
        }
        done:
        if (end_i == beg_i) end_i = ctx.i;
        String8 lexeme = string8_range(ctx.infile, beg_i, end_i);

        if (false) {}
        #define compile_if_op_string(name, val)\
            else if (string8_eql(lexeme, string8(#name))) compile_op(&ctx, mode, val);
        for_op(compile_if_op_string)
        else {
            errf("invalid token '%.*s' at '%s'[%zu..%zu]", string_fmt(lexeme), ctx.path_biciasm, beg_i, end_i);
            return (String8){0};
        };

        continue;
    }
    if (ctx.rom.len == 0) return (String8){0};
    String8 rom = slice_from_array(ctx.rom);

    for (u8 i = 0; i < ctx.label_refs.len; i += 1) {
        Label_Ref ref = ctx.label_refs.ptr[i];
        if (ref.name.len == 0) return (String8){0};
        ctx.rom.len = ref.loc;
        switch (ref.size) {
            case size_byte: asm_rom_write(&ctx, (u8)asm_label_get_addr_of(&ctx, ref.name)); break;
            case size_short: asm_rom_write2(&ctx, asm_label_get_addr_of(&ctx, ref.name)); break;
            default: unreachable;
        }
    }

    printf("ASM ===\n");
    for (usize i = 0x100; i < rom.len; i += 1) printf("%02x ", rom.ptr[i]);
    putchar('\n');

    return rom;
}
