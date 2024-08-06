#include "base/base_include.h"
#include "base/base_include.c"

const char *usage = "usage: bici <com|run|script> <file...>";

#define for_op(action)\
    action(push,  0x00)\
    action(drop,  0x01)\
    action(nip,   0x02)\
    action(swap,  0x03)\
    action(rot,   0x04)\
    action(dup,   0x05)\
    action(over,  0x06)\
    action(eq,    0x07)\
    action(neq,   0x08)\
    action(gt,    0x09)\
    action(lt,    0x0a)\
    action(add,   0x0b)\
    action(sub,   0x0c)\
    action(mul,   0x0d)\
    action(div,   0x0e)\
    action(inc,   0x0f)\
    action(and,   0x10)\
    action(or,    0x11)\
    action(xor,   0x12)\
    action(shift, 0x13)\
    action(jump,  0x14)\
    action(jump_imm,           0x15)\
    action(jump_cond,          0x16)\
    action(jump_imm_cond,      0x17)\
    action(jump_stash_ret,     0x18)\
    action(jump_imm_stash_ret, 0x19)\
    action(stash,     0x1a)\
    action(load,      0x1b)\
    action(load_rel,  0x1c)\
    action(store,     0x1d)\
    action(store_rel, 0x1e)\
    action(read,      0x1f)\
    action(write,     0x20)\

enumdef(Op, u8) {
    #define op_def_enum(name, val) op_##name = val,
    for_op(op_def_enum)
};

#include "compile.c"

static u8 *device_table;

static u8 param_s[256], ret_s[256];
static u8 param_sp, ret_sp;
static u8 mode_keep, mode_16;
static u8 *s, *sp = &param_sp;

static void s_param(void) { s = param_s; sp = &param_sp; }
static void s_ret(void) { s = ret_s; sp = &ret_sp; }

static u8 *get(u16 i_back) { return s + *sp - i_back; }

static void push8(u8 byte) { s[*sp] = byte; *sp += 1; }
static u8 pop8(void) { u8 val = s[*sp - 1]; if (!mode_keep) *sp -= 1; return val; }

static void push16(u16 byte2) { memcpy(s + (*sp), &byte2, 2); }

static void run(char *path_biciasm) {
    u8 mem[65536] = {0};
    Arena arena = { .mem = mem, .cap = 65536 };

    device_table = arena_alloc(&arena, 256, 1);
    usize max_bytes = 65536 - 0x100;
    String8 program = file_read(&arena, path_biciasm, "rb", max_bytes);
    u16 end = (u16)(0x100 + program.len);

    for (u16 i = 0x100; i < end; i += 1) {
        u8 byte = mem[i];
        Op op = byte & 0x3f;
        mode_keep   = (byte & 0x80) >> 7;
        u8 mode_ret = (byte & 0x40) >> 6;
        mode_16     = (byte & 0x20) >> 5;

        if (mode_ret) s_ret(); else s_param();

        if (mode_16) {
            unreachable;
        } else switch(op) {
            case op_push:  push8(mem[++i]); break;
            case op_drop:  discard(pop8()); break;
            case op_nip:   *get(2) = *get(1); *sp -= 1; break;
            case op_swap:  u8 temp2 = *get(2); *get(2) = *get(1); *get(1) = temp2; break;
            case op_rot:   u8 temp3 = *get(3); *get(3) = *get(2); *get(2) = *get(1); *get(1) = temp3; break;
            case op_dup:   push8(*get(1)); break;
            case op_over:  push8(*get(2)); break;
            case op_eq:    push8(pop8() == pop8()); break;
            case op_neq:   push8(pop8() != pop8()); break;
            case op_gt:    { u8 right = pop8(), left = pop8(); push8(left > right); } break; 
            case op_lt:    { u8 right = pop8(), left = pop8(); push8(left < right); } break; 
            case op_add:   push8(pop8() + pop8()); break;
            case op_sub:   { u8 right = pop8(), left = pop8(); push8(left - right); } break; 
            case op_mul:   push8(pop8() * pop8()); break;
            case op_div:   { u8 right = pop8(), left = pop8(); push8(left / right); } break; 
            case op_inc:   *get(1) += 1; break;
            case op_and:   push8(pop8() & pop8()); break;
            case op_or:    push8(pop8() | pop8()); break;
            case op_xor:   push8(pop8() ^ pop8()); break; 
            case op_shift: { 
                u8 shift = pop8(), rshift = shift & 0x0f, lshift = (shift & 0xf0) >> 4; 
                push8(pop8() << lshift >> rshift); 
            } break;
            case op_jump:               i += pop8(); break;
            case op_jump_imm:           i += mem[i + 1]; break;
            case op_jump_cond:          u8 rel_addr = pop8(); if (pop8()) i += rel_addr; break;
            case op_jump_imm_cond:      i += 1; if (pop8()) i += mem[i]; break;
            case op_jump_stash_ret:     s_ret(); push16(i); s_param(); i += pop8(); break;
            case op_jump_imm_stash_ret: i += 1; s_ret(); push16(i); i += mem[i]; break;
            case op_stash:     u8 val = pop8(); s_ret(); push8(val); s_param(); break; 
            case op_load:      push8(mem[pop8()]); break;
            case op_load_rel:  push8(mem[i + pop8()]); break;
            case op_store:     mem[pop8()] = pop8(); break;
            case op_store_rel: mem[i + pop8()] = pop8(); break;
            case op_read:      push8(device_table[pop8()]); break;
            case op_write:     device_table[pop8()] = pop8(); break;
            default: unreachable;
        }
    }

    printf("\nRUN ===\n");
    for (u8 i = 0; i < *sp; i += 1) printf("%02x ", s[i]);
    putchar('\n');
}

int main(int argc, char **argv) {
    if (argc < 3) {
        errf("%s", usage);
        return 1;
    }

    Arena arena = {0};

    String8 cmd = string8_from_cstring(argv[1]);
    if (string8_eql(cmd, string8("com"))) {
        if (argc != 4) {
            err("usage: bici com <file.biciasm> <file.bici>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        compile(&arena, max_asm_filesize, argv[2], argv[3]);
    } else if (string8_eql(cmd, string8("run"))) {
        run(argv[2]);
        return 0;
    } else if (string8_eql(cmd, string8("script"))) {
        if (argc != 4) {
            err("usage: bici script <file.biciasm> <file.bici>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        compile(&arena, max_asm_filesize, argv[2], argv[3]);
        run(argv[3]);
        return 0;
    } else {
        errf("no such command '%.*s'\n%s", cmd, usage);
        return 1;
    }

    arena_deinit(&arena);
    return 0;
}
