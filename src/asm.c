structdef(Label) { String8 name; u16 addr; };

structdef(Label_Ref) { String8 name; u16 loc; Size size; };

enumdef(Res_Mode, u8) { res_num, res_addr };
structdef(Res) { u16 pc; Res_Mode mode; };

structdef(Asm) {
    char *path_biciasm;
    u8 *assembly;
    u16 len, i;
    u8 out[0x10000];
    u16 pc;

    Label labels[0x100];
    u8 label_idx;

    Label_Ref label_refs[0x100];
    u8 label_ref_idx;

    Res forward_res[0x100];
    u8 forward_res_idx;
};

static void label_push(Asm *ctx, String8 name, u16 addr) {
    if (ctx->label_idx == UINT8_MAX) {
        errf("cannot add label '%.*s': maximum number of labels reached", string_fmt(name));
        abort();
    }
    ctx->labels[ctx->label_idx] = (Label){ .name = name, .addr = addr }; 
    ctx->label_idx += 1;
}

static u16 label_get_addr_of(Asm *ctx, String8 name) {
    for (u8 idx = 0; idx < ctx->label_idx; idx += 1) {
        if (!string8_eql(ctx->labels[idx].name, name)) continue;
        return ctx->labels[idx].addr;
    }
    errf("no such label '%.*s'", string_fmt(name));
    return 0;
}

structdef(Hex) {
    bool ok;
    u8 num_digits;
    union { u8 int8; u16 int16; } result;
};

static Hex parse_hex(Asm *ctx) {
    u16 beg_i = ctx->i;
    while (ctx->i < ctx->len && is_hex_digit_table[ctx->assembly[ctx->i]]) ctx->i += 1;
    u16 end_i = ctx->i;
    ctx->i -= 1;
    u8 num_digits = (u8)(end_i - beg_i);
    String8 hex_string = { .ptr = ctx->assembly + beg_i, .len = num_digits };

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

static String8 parse_alpha(Asm *ctx) {
    u16 beg_i = ctx->i;
    if (!is_alpha(ctx->assembly[ctx->i])) {
        errf(
            "expected alphabetic character or underscore, found byte '%c'/%d/#%x at '%s'[%zu]", 
            ctx->assembly[ctx->i], ctx->assembly[ctx->i], ctx->assembly[ctx->i], ctx->path_biciasm, ctx->i
        );
        return (String8){0};
    }

    ctx->i += 1;
    while (ctx->i < ctx->len && (is_alpha(ctx->assembly[ctx->i]))) ctx->i += 1;
    u16 end_i = ctx->i;
    ctx->i -= 1;
    return (String8){ .ptr = ctx->assembly + beg_i, .len = end_i - beg_i };
}

static void write(Asm *ctx, u8 byte) {
    if (ctx->pc == UINT16_MAX) { 
        err("exceeded maximum program size"); 
        abort(); 
    }
    ctx->out[ctx->pc] = byte;
    ctx->pc += 1;
} 

static void write2(Asm *ctx, u16 byte2) {
    write(ctx, (byte2 & 0xff00) >> 8); 
    write(ctx, byte2 & 0x00ff); 
}

static void forward_res_push(Asm *ctx, Res_Mode res_mode) {
    if (ctx->forward_res_idx == 0x0ff) {
        errf("cannot resolve address at '%s'[%d]: maximum number of resolutions reached", ctx->path_biciasm, ctx->i);
        ctx->pc = 0;
        return;
    }
    ctx->forward_res[ctx->forward_res_idx] = (Res){ .pc = ctx->pc, .mode = res_mode };
    ctx->forward_res_idx += 1;
}
static void forward_res_resolve_last(Asm *ctx) {
    if (ctx->forward_res_idx == 0) {
        errf("forward resolution marker at '%s'[%d] matches no opening marker", ctx->path_biciasm, ctx->i);
        ctx->pc = 0;
        return;
    }
    ctx->forward_res_idx -= 1;
    Res res = ctx->forward_res[ctx->forward_res_idx];
    switch (res.mode) {
        case res_num: ctx->out[res.pc] = (u8)(ctx->pc - 1 - res.pc); break;
        case res_addr: {
            u16 pc_bak = ctx->pc;
            ctx->pc = res.pc;
            write2(ctx, pc_bak);
            ctx->pc = pc_bak;
        } break;
        default: unreachable;
    }
}

static void compile_op(Asm *ctx, Mode mode, u8 op) {
    if (instruction_is_special(op)) { write(ctx, op); return; }
    write(ctx, op | (u8)(mode.size << 5) | (u8)(mode.stack << 6) | (u8)(mode.keep << 7));
}

static bool compile(Arena *arena, usize max_asm_filesize, char *_path_biciasm, char *path_bici) {
    String8 asm_file = file_read(arena, _path_biciasm, "rb", max_asm_filesize);
    if (asm_file.len == 0) return false;

    Asm ctx = { .path_biciasm = _path_biciasm, .assembly = asm_file.ptr, .len = (u16)asm_file.len, .pc = 0x100 };

    u16 add = 1;
    for (ctx.i = 0; ctx.i < ctx.len; ctx.i += add) {
        add = 1;
        if (is_whitespace(ctx.assembly[ctx.i])) continue;

        Mode mode = {0};

        switch (ctx.assembly[ctx.i]) {
            case '/': if (ctx.i + 1 < ctx.len && ctx.assembly[ctx.i + 1] == '/') {
                for (ctx.i += 2; ctx.i < ctx.len && ctx.assembly[ctx.i] != '\n';) ctx.i += 1;
            } continue;
            case '|': {
                ctx.i += 1;
                Hex new_pc = parse_hex(&ctx);
                if (!new_pc.ok) return false;
                ctx.pc = new_pc.result.int16;
            } continue;
            case '@': {
                ctx.i += 1;
                String8 name = parse_alpha(&ctx);
                ctx.label_refs[ctx.label_ref_idx] = (Label_Ref){ .name = name, .loc = ctx.pc, .size = size_short };
                ctx.label_ref_idx += 1;
                ctx.pc += 2;
            } continue;
            case '#': {
                ctx.i += 1; 
                Hex num = parse_hex(&ctx);
                if (!num.ok) return false;
                switch (num.num_digits) {
                    case 2: compile_op(&ctx, (Mode){ .size = size_byte }, op_push); write(&ctx, num.result.int8); break;
                    case 4: compile_op(&ctx, (Mode){ .size = size_short }, op_push); write2(&ctx, num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '*': {
                compile_op(&ctx, (Mode){ .size = size_byte }, op_push);
                ctx.i += 1;
                String8 name = parse_alpha(&ctx);
                ctx.label_refs[ctx.label_ref_idx] = (Label_Ref){ .name = name, .loc = ctx.pc, .size = size_byte };
                ctx.label_ref_idx += 1;
                ctx.pc += 1;
            } continue;
            case '&': {
                compile_op(&ctx, (Mode){ .size = size_short }, op_push);
                ctx.i += 1;
                String8 name = parse_alpha(&ctx);
                ctx.label_refs[ctx.label_ref_idx] = (Label_Ref){ .name = name, .loc = ctx.pc, .size = size_short };
                ctx.label_ref_idx += 1;
                ctx.pc += 2;
            } continue;
            case ':': {
                ctx.i += 1;
                String8 name = parse_alpha(&ctx);
                label_push(&ctx, name, ctx.pc);
            } continue;
            case '{': {
                ctx.i += 1;
                switch (ctx.assembly[ctx.i]) {
                    case '#': {
                        forward_res_push(&ctx, res_num);
                        ctx.pc += 1;
                    } continue;
                    default: {
                        forward_res_push(&ctx, res_addr);
                        ctx.pc += 2;
                        add = 0;
                    } continue;
                }
            } continue;
            case '}': forward_res_resolve_last(&ctx); continue;
            case '_': {
                ctx.i += 1;
                Hex num = parse_hex(&ctx);
                if (!num.ok) return false;
                switch (num.num_digits) {
                    case 2: write(&ctx, num.result.int8); break;
                    case 4: write2(&ctx, num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '"': {
                for (ctx.i += 1; ctx.i < ctx.len && !is_whitespace(ctx.assembly[ctx.i]); ctx.i += 1) {
                    write(&ctx, ctx.assembly[ctx.i]);
                }
                add = 0;
            } continue;
            default: break;
        }

        if (!is_alpha(ctx.assembly[ctx.i])) {
            errf("invalid byte '%c'/%d/#%x at '%s'[%zu]", ctx.assembly[ctx.i], ctx.assembly[ctx.i], ctx.assembly[ctx.i], ctx.path_biciasm, ctx.i);
            return false;
        }

        usize beg_i = ctx.i, end_i = beg_i;
        for (; ctx.i < ctx.len && !is_whitespace(ctx.assembly[ctx.i]); ctx.i += 1) {
            if ('a' <= ctx.assembly[ctx.i] && ctx.assembly[ctx.i] <= 'z') continue;
            end_i = ctx.i;
            if (ctx.assembly[ctx.i] == ';') for (ctx.i += 1; ctx.i < ctx.len;) {
                if (is_whitespace(ctx.assembly[ctx.i])) goto done;
                switch (ctx.assembly[ctx.i]) {
                    case 'k': mode.keep = true; break;
                    case 'r': mode.stack = stack_ret; break;
                    case '2': mode.size = size_short; break;
                    default: {
                        errf("expected one of ['k', 'r', '2'], found byte '%c'/%d/#%x at '%s'[%zu]", ctx.assembly[ctx.i], ctx.assembly[ctx.i], ctx.assembly[ctx.i], ctx.path_biciasm, ctx.i);
                        return false;
                    } break;
                }
                ctx.i += 1;
            }
        }
        done:
        if (end_i == beg_i) end_i = ctx.i;
        String8 lexeme = string8_range(asm_file, beg_i, end_i);

        if (false) {}
        #define compile_if_op_string(name, val)\
            else if (string8_eql(lexeme, string8(#name))) compile_op(&ctx, mode, val);
        for_op(compile_if_op_string)
        else {
            errf("invalid token '%.*s' at '%s'[%zu..%zu]", string_fmt(lexeme), ctx.path_biciasm, beg_i, end_i);
            return false;
        };

        continue;
    }
    if (ctx.pc == 0) return false;
    String8 rom = { .ptr = ctx.out, .len = ctx.pc };

    for (u8 idx = 0; idx < ctx.label_ref_idx; idx += 1) {
        Label_Ref ref = ctx.label_refs[idx];
        if (ref.name.len == 0) return false;
        ctx.pc = ref.loc;
        switch (ref.size) {
            case size_byte: write(&ctx, (u8)label_get_addr_of(&ctx, ref.name)); break;
            case size_short: write2(&ctx, label_get_addr_of(&ctx, ref.name)); break;
            default: unreachable;
        }
    }

    FILE *rom_file = file_open(path_bici, "wb");
    if (rom_file == 0) return false;
    file_write(rom_file, rom);
    fclose(rom_file);

    printf("ASM ===\n");
    for (usize idx = 0x100; idx < rom.len; idx += 1) printf("%02x ", ctx.out[idx]);
    putchar('\n');
    return true;
}
