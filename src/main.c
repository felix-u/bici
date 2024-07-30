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
};

static u8 stack[256];
static u8 stack_counter;
static force_inline u8 stack_pop(void) { return stack[--stack_counter]; }
static force_inline void stack_push(u8 byte) { stack[stack_counter++] = byte; }
static force_inline u8 stack_get(u8 i) { return stack[stack_counter - i]; }
static force_inline u8 *stack_loc(u8 i) { return stack + stack_counter - i; }

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
        switch (op) {
            case op_push: stack_push(rom_file.ptr[++i]); break;
            case op_drop: discard(stack_pop()); break;
            case op_nip:  *stack_loc(2) = stack_get(1); discard(stack_pop()); break;
            case op_swap: u8 temp1 = stack_get(1); *stack_loc(1) = stack_get(2); *stack_loc(2) = temp1; break;
            case op_rot:  u8 temp3 = stack_get(3); *stack_loc(3) = stack_get(2); *stack_loc(2) = stack_get(1); *stack_loc(1) = temp3; break;
            case op_dup:  stack_push(stack_get(1)); break;
            case op_over: stack_push(stack_get(2)); break;
            case op_eq: stack_push(stack_pop() == stack_pop()); break;
            case op_neq: stack_push(stack_pop() != stack_pop()); break;
            default: unreachable;
        }
    }

    for (u8 i = 0; i < stack_counter; i += 1) printf("%d; ", stack[i]);

    arena_deinit(&arena);
    return 0;
}
