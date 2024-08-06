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
static u8 *stack, *stack_counter;

static u8 parameter_stack[256], return_stack[256];
static u8 parameter_stack_counter, return_stack_counter;
#define set_parameter_stack(void) { stack = parameter_stack; stack_counter = &parameter_stack_counter; }
#define set_return_stack(void) { stack = return_stack; stack_counter = &return_stack_counter; }

#define get(i_back) (stack + (*stack_counter) - ((i_back) * mode_size))

#define pop() (mode_16 ? pop_u16() : pop_u8())
static u8 pop_u8(void) { 
    if (!mode_keep) *stack_counter -= (1 + mode_keep); 
    return stack[*stack_counter]; 
}
static u16 pop_u16(void) {
    if (!mode_keep) *stack_counter -= 2 * (1 + mode_keep); 
    return *((u16 *)get(0)); 
}

static void push_u8(u8 item) { stack[*stack_counter] = item; *stack_counter += 1; }
static void push_u16(u16 item) { memmove(get(0), &item, 2); *stack_counter += 2; }
static force_inline void push(void *item) {
    if (mode_16) push_u16(*(u16 *)item);
    else push_u8(*(u8 *)item);
}

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

        if (mode_return) { set_return_stack(); }
        else { set_parameter_stack(); }

        switch (op) {
            case op_push: push(&rom_file.ptr[++i]); break;
            case op_drop: discard(pop()); break;
            case op_nip:  memmove(stack + *stack_counter - 2 * mode_size, stack + *stack_counter - 1 * mode_size, mode_size); *stack_counter -= mode_size;
            case op_swap: {
                void *temp2 = get(2);
                memmove(get(2), get(1), mode_size);
                memmove(get(1), temp2, mode_size);
            } break;
            case op_rot: {
                void *temp3 = get(3);
                memmove(get(3), get(2), mode_size);
                memmove(get(2), get(1), mode_size);
                memmove(get(1), temp3, mode_size);
            } break;
            case op_dup:  { void *top = get(1); push(top); } break;
            case op_over: { void *second = get(2); push(second); } break;
            case op_eq:   { u16 right = (u16)pop(), left = (u16)pop(), result = (left == right); push(&result); } break;
            case op_neq:  { u16 right = (u16)pop(), left = (u16)pop(), result = (left != right); push(&result); } break;
            case op_gt:   { u16 right = (u16)pop(), left = (u16)pop(), result = (left > right); push(&result); } break;
            case op_lt:   { u16 right = (u16)pop(), left = (u16)pop(), result = (left < right); push(&result); } break;
            case op_add:  if (mode_16) push_u16(pop_u16() + pop_u16()); else push_u8(pop_u8() + pop_u8()); break;
            case op_add:  if (mode_16) push_u16(pop_u16() + pop_u16()); else push_u8(pop_u8() + pop_u8()); break;
            case op_sub:  { 
                if (mode_16) {
                    u16 right = pop_u16(), left = pop_u16(); push_u16(left - right);
                } else {
                    u8 right = pop_u8(), left = pop_u8(); push_u8(left - right);
                }
            } break;
            case op_mul: {
                if (mode_16) {
                    u16 right = pop_u16(), left = pop_u16(); push_u16(left * right);
                } else {
                    u8 right = pop_u8(), left = pop_u8(); push_u8(left * right);
                }
            } break;
            case op_div: {
                if (mode_16) {
                    u16 right = pop_u16(), left = pop_u16(); push_u16(left / right);
                } else {
                    u8 right = pop_u8(), left = pop_u8(); push_u8(left / right);
                }
            } break;
            case op_inc: { void *top = get(1); if (mode_16) *(u16 *)top += 1; else *(u8 *)top += 1; } break;
            case op_jump:               i += pop(); break;
            case op_jump_imm:           i += rom_file.ptr[i + 1]; break;
            case op_jump_cond:          u8 relative_addr = pop(); if (pop()) i += relative_addr; break;
            case op_jump_imm_cond:      i += 1; if (pop()) i += rom_file.ptr[i]; break;
            case op_jump_stash_ret:     set_return_stack(); push_u16(i); set_parameter_stack(); i += pop_u16(); break; 
            case op_jump_imm_stash_ret: set_return_stack(); push_u16(i); i += rom_file.ptr[i + 1]; break;
            case op_return:             i = return_pop(); break;
            case op_stash: return_push(pop()); break;
            default: unreachable;
        }
    }

    for (u8 i = 0; i < stack_counter; i += 1) printf("%d; ", stack[i]);

    arena_deinit(&arena);
    return 0;
}
