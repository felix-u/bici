#include "base/base_include.h"
#include "base/base_include.c"

enumdef(Op, u8) {
    op_push = 0x00,
    op_drop = 0x01,
    op_nip  = 0x02,
    op_swap = 0x03,
    op_rot  = 0x04,
    op_dup  = 0x05,
    op_over = 0x06,
    op_eq   = 0x07,
    op_neq  = 0x08,
    op_gt   = 0x09,
    op_lt   = 0x0a,
    op_add  = 0x0b,
    op_sub  = 0x0c,
    op_mul  = 0x0d,
    op_div  = 0x0e,
    op_inc  = 0x0f,
    op_jump                = 0x10,
    op_jump_imm            = 0x11,
    op_jump_cond           = 0x12,
    op_jump_imm_cond       = 0x13,
    op_jump_stash_ret      = 0x14,
    op_jump_imm_stash_ret  = 0x15,
    op_return              = 0x16,
    op_stash = 0x17,
};

static u8 mode_keep, mode_16;
#define mode_size (1 + mode_16)
static u8 *s, *sp;

static u8 param_s[256], ret_s[256];
static u8 param_sp, ret_sp;
static void s_param(void) { s = param_s; sp = &param_sp; }
static void s_ret(void) { s = ret_s; sp = &ret_sp; }

static void push8(u8 byte) { s[*sp] = byte; *sp += 1; }
static u8 pop8(void) { *sp -= 1; return s[*sp]; }

static void push16(u16 byte2) { memcpy(s + (*sp), &byte2, 2); }

int main(int argc, char **argv) {
    if (argc != 2) {
        err("usage: bici <rom>");
        return 1;
    }

    Arena arena = arena_init(65536);
    String8 rom_file = file_read(&arena, argv[1], "rb");

    for (u16 i = 0; i < (u16)rom_file.len; i += 1) {
        u8 byte = rom_file.ptr[i];
        Op op = byte & 0x3f;
        mode_keep      = (byte & 0x80) >> 7;
        u8 mode_return = (byte & 0x40) >> 6;
        mode_16        = (byte & 0x20) >> 5;

        if (mode_return) s_ret(); else s_param();

        if (mode_16) {
            unreachable;
        } else switch(op) {
            case op_push: push8(rom_file.ptr[++i]); break;
            case op_drop: discard(pop8()); break;
            case op_nip:  s[*sp - 2] = s[*sp - 1]; *sp -= 1; break;
            case op_swap: u8 temp2 = s[*sp - 2]; s[*sp - 2] = s[*sp - 1]; s[*sp - 1] = temp2; break;
            case op_rot:  u8 temp3 = s[*sp - 3]; s[*sp - 3] = s[*sp - 2]; s[*sp - 2] = s[*sp - 1]; s[*sp - 1] = temp3; break;
            case op_dup:  push8(s[*sp - 1]); break;
            case op_over: push8(s[*sp - 2]); break;
            case op_eq:   push8(pop8() == pop8()); break;
            case op_neq:  push8(pop8() != pop8()); break;
            case op_gt:   { u8 right = pop8(), left = pop8(); push8(left > right); } break; 
            case op_lt:   { u8 right = pop8(), left = pop8(); push8(left < right); } break; 
            case op_add:  push8(pop8() + pop8()); break;
            case op_sub:  { u8 right = pop8(), left = pop8(); push8(left - right); } break; 
            case op_mul:  push8(pop8() * pop8()); break;
            case op_div:  { u8 right = pop8(), left = pop8(); push8(left / right); } break; 
            case op_inc:  s[*sp - 1] += 1; break;
            case op_jump:               i += pop8(); break;
            case op_jump_imm:           i += rom_file.ptr[i + 1]; break;
            case op_jump_cond:          u8 relative_addr = pop8(); if (pop8()) i += relative_addr; break;
            case op_jump_imm_cond:      i += 1; if (pop8()) i += rom_file.ptr[i]; break;
            case op_jump_stash_ret:     s_ret(); push16(i); s_param(); i += pop8(); break;
            case op_jump_imm_stash_ret: i += 1; s_ret(); push16(i); i += rom_file.ptr[i]; break;
            case op_return:             s_ret(); i = pop8(); s_param(); break;
            default: unreachable;
        }
    }

    for (u8 i = 0; i < *sp; i += 1) printf("%d; ", s[i]);

    arena_deinit(&arena);
    return 0;
}
