#include "base/base_include.h"
#include "base/base_include.c"

const char *usage = "usage: bici <com|run|script> <file...>";

#define for_op(action)\
    action(push,   0x00)\
    action(drop,   0x01)\
    action(nip,    0x02)\
    action(swap,   0x03)\
    action(rot,    0x04)\
    action(dup,    0x05)\
    action(over,   0x06)\
    action(eq,     0x07)\
    action(neq,    0x08)\
    action(gt,     0x09)\
    action(lt,     0x0a)\
    action(add,    0x0b)\
    action(sub,    0x0c)\
    action(mul,    0x0d)\
    action(div,    0x0e)\
    action(inc,    0x0f)\
    action(and,    0x10)\
    action(or,     0x11)\
    action(xor,    0x12)\
    action(shift,  0x13)\
    action(jmp,    0x14)\
    action(jmi,    0x15)\
    action(jeq,    0x16)\
    action(jei,    0x17)\
    action(jst,    0x18)\
    action(jsi,    0x19)\
    action(stash,  0x1a)\
    action(load,   0x1b)\
    action(loadr,  0x1c)\
    action(store,  0x1d)\
    action(storer, 0x1e)\
    action(read,   0x1f)\
    action(write,  0x20)\

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
static void push(u8 byte) { s[*sp] = byte; *sp += 1; }
static u8 pop(void) { u8 val = s[*sp - 1]; if (!mode_keep) *sp -= 1; return val; }

#define get2(i_back) (*((u16 *)(s + *sp) - (i_back)))
static void push2(u16 byte2) { push((u8)byte2); push(byte2 >> 8); }
static u16 pop2(void) { u16 val = s[*sp - 1] | (s[*sp - 2] << 8); if (!mode_keep) *sp -= 2; return val; }

static void run(char *path_biciasm) {
    printf("\nRUN ===\n");

    u8 mem[65536] = {0};
    Arena arena = { .mem = mem, .cap = 65536 };

    device_table = arena_alloc(&arena, 256, 1);
    usize max_bytes = 65536 - 0x100;
    String8 program = file_read(&arena, path_biciasm, "rb", max_bytes);
    u16 end = (u16)(0x100 + program.len);

    for (u16 i = 0x100; i < end; i += 1) {
        u8 byte = mem[i];
        Op op = byte & 0x1f;
        mode_keep   = (byte & 0x80) >> 7;
        u8 mode_ret = (byte & 0x40) >> 6;
        mode_16     = (byte & 0x20) >> 5;

        if (mode_ret) s_ret(); else s_param();

        if (mode_16) switch(op) {
            case op_push:  i += 1; push2(*(u16 *)(mem + i)); i += 1; break;
            case op_add:   { u16 left = get2(2), right = get2(1); pop2(); pop2(); push2(left + right); } break;
            case op_stash: { u16 val = pop2(); s_ret(); push2(val); s_param(); } break;
            default: errf("TODO { k:%d r:%d 2:%d op:#%02x }", mode_keep, mode_ret, mode_16, op); return;
        } else switch(op) {
            case op_push:   push(mem[++i]); break;
            case op_drop:   discard(pop()); break;
            case op_nip:    *get(2) = *get(1); *sp -= 1; break;
            case op_swap:   u8 temp2 = *get(2); *get(2) = *get(1); *get(1) = temp2; break;
            case op_rot:    u8 temp3 = *get(3); *get(3) = *get(2); *get(2) = *get(1); *get(1) = temp3; break;
            case op_dup:    push(*get(1)); break;
            case op_over:   push(*get(2)); break;
            case op_eq:     push(pop() == pop()); break;
            case op_neq:    push(pop() != pop()); break;
            case op_gt:     { u8 right = pop(), left = pop(); push(left > right); } break; 
            case op_lt:     { u8 right = pop(), left = pop(); push(left < right); } break; 
            case op_add:    push(pop() + pop()); break;
            case op_sub:    { u8 right = pop(), left = pop(); push(left - right); } break; 
            case op_mul:    push(pop() * pop()); break;
            case op_div:    { u8 right = pop(), left = pop(); push(left / right); } break; 
            case op_inc:    *get(1) += 1; break;
            case op_and:    push(pop() & pop()); break;
            case op_or:     push(pop() | pop()); break;
            case op_xor:    push(pop() ^ pop()); break; 
            case op_shift:  { u8 shift = pop(), r = shift & 0x0f, l = (shift & 0xf0) >> 4; push(pop() << l >> r); } break;
            case op_jmp:    i += pop(); break;
            case op_jmi:    i += mem[i + 1]; break;
            case op_jeq:    u8 rel_addr = pop(); if (pop()) i += rel_addr; break;
            case op_jei:    i += 1; if (pop()) i += mem[i]; break;
            case op_jst:    s_ret(); push2(i); s_param(); i += pop(); break;
            case op_jsi:    i += 1; s_ret(); push2(i); i += mem[i]; break;
            case op_stash:  { u8 val = pop(); s_ret(); push(val); s_param(); } break; 
            case op_load:   push(mem[pop()]); break;
            case op_loadr:  push(mem[i + pop()]); break;
            case op_store:  mem[pop()] = pop(); break;
            case op_storer: mem[i + pop()] = pop(); break;
            case op_read:   push(device_table[pop()]); break;
            case op_write:  device_table[pop()] = pop(); break;
            default: unreachable;
        }
    }

    printf("p: "); for (u8 i = 0; i < param_sp; i += 1) printf("%02x ", param_s[i]);
    printf("\nr: "); for (u8 i = 0; i < ret_sp; i += 1) printf("%02x ", ret_s[i]);
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
        if (compile(&arena, max_asm_filesize, argv[2], argv[3])) run(argv[3]);
    } else {
        errf("no such command '%.*s'\n%s", string_fmt(cmd), usage);
        return 1;
    }

    arena_deinit(&arena);
    return 0;
}
