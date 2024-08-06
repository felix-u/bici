#include "base/base_include.h"
#include "base/base_include.c"

enumdef(Op, u8) {
    op_push  = 0x00,
    op_drop  = 0x01,
    op_nip   = 0x02,
    op_swap  = 0x03,
    op_rot   = 0x04,
    op_dup   = 0x05,
    op_over  = 0x06,
    op_eq    = 0x07,
    op_neq   = 0x08,
    op_gt    = 0x09,
    op_lt    = 0x0a,
    op_add   = 0x0b,
    op_sub   = 0x0c,
    op_mul   = 0x0d,
    op_div   = 0x0e,
    op_inc   = 0x0f,
    op_and   = 0x10,
    op_or    = 0x11,
    op_xor   = 0x12,
    op_shift = 0x13,
    op_jump                = 0x14,
    op_jump_imm            = 0x15,
    op_jump_cond           = 0x16,
    op_jump_imm_cond       = 0x17,
    op_jump_stash_ret      = 0x18,
    op_jump_imm_stash_ret  = 0x19,
    op_stash     = 0x1a,
    op_load      = 0x1b,
    op_load_rel  = 0x1c,
    op_store     = 0x1d,
    op_store_rel = 0x1e,
    op_read      = 0x1f,
    op_write     = 0x20,
};

static u8 mode_keep, mode_16;
static u8 *s, *sp;

static u8 *device_table;

static u8 param_s[256], ret_s[256];
static u8 param_sp, ret_sp;
static void s_param(void) { s = param_s; sp = &param_sp; }
static void s_ret(void) { s = ret_s; sp = &ret_sp; }

static u8 *get(u16 i_back) { return s + *sp - i_back; }

static void push8(u8 byte) { s[*sp] = byte; *sp += 1; }
static u8 pop8(void) { u8 val = s[*sp - 1]; if (!mode_keep) *sp -= 1; return val; }

static void push16(u16 byte2) { memcpy(s + (*sp), &byte2, 2); }

int main(int argc, char **argv) {
    if (argc != 2) {
        err("usage: bici <rom>");
        return 1;
    }

    Arena arena = arena_init(65536);
    u8 *mem = arena.mem;
    device_table = arena_alloc(&arena, 256, 1);
    String8 program = file_read(&arena, argv[1], "rb");
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

    for (u8 i = 0; i < *sp; i += 1) printf("%d; ", s[i]);

    arena_deinit(&arena);
    return 0;
}
