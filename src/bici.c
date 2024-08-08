#define for_op(action)\
    action(push,   0x00)/* NO k MODE */\
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
    action(not,    0x10)\
    action(and,    0x11)\
    action(or,     0x12)\
    action(xor,    0x13)\
    action(shift,  0x14)\
    action(jmp,    0x15)\
    action(jeq,    0x16)\
    action(jst,    0x17)\
    action(stash,  0x18)\
    action(load,   0x19)\
    action(loadr,  0x1a)\
    action(store,  0x1b)\
    action(storer, 0x1c)\
    action(read,   0x1d)\
    action(write,  0x1e)\
    action(jmi,    0x80)/* NO MODE (== push;k)  */\
    action(jei,    0xa0)/* NO MODE (== push;k2) */\
    action(jsi,    0xc0)/* NO MODE (== push;kr) */\
    action(break,  0xe0)/* NO MODE (== push;kr2)*/\

enumdef(Op, u8) {
    #define op_def_enum(name, val) op_##name = val,
    for_op(op_def_enum)
    op_max_value_plus_one
};

const char *op_name[op_max_value_plus_one] = {
    #define op_set_name(name, val) [val] = #name,
    for_op(op_set_name)
};

enumdef(Stack, u8) { stack_param = 0, stack_ret = 1 };
enumdef(Size, u8)  { size_byte = 0, size_short = 1 };
structdef(Mode) { b8 keep; Stack stack; Size size; };
structdef(Instruction) { Op op; Mode mode; };

static u8 byte_from_instruction(Instruction instruction) {
    return (u8)(
        instruction.op | 
        (instruction.mode.keep << 7) | 
        (instruction.mode.stack << 6) | 
        (instruction.mode.size << 5)
    );
}

static Instruction instruction_from_byte(u8 byte) {
    return (Instruction){ .op = byte & 0x1f, .mode = {
        .keep =  (byte & 0x80) >> 7,
        .stack = (byte & 0x40) >> 6,
        .size =  (byte & 0x20) >> 5,
    } };
}

#define print_arena_cap 256
static u8 print_arena_mem[print_arena_cap];
static Arena print_arena = { .mem = print_arena_mem, .cap = print_arena_cap };
static String8 instruction_print(Instruction instruction) {
    Arena_Temp print_temp = arena_temp_begin(&print_arena);

    u8 byte = byte_from_instruction(instruction);
    usize advance = string8_printf(&print_arena, print_arena_cap, "<0x%02x>{ op: ", byte).len;

    switch (byte) {
        case op_jmi: case op_jei: case op_jsi: case op_break: {
            advance += string8_printf(&print_arena, print_arena_cap - advance, "%s", op_name[byte]).len;
        } break;
        default: {
            advance += string8_printf(&print_arena, print_arena_cap - advance,
                "%s, mode: { keep: %d, stack: %d, size: %d }",
                op_name[instruction.op], instruction.mode.keep, instruction.mode.stack, instruction.mode.size
            ).len;
        } break;
    }
    
    string8_printf(&print_arena, print_arena_cap - advance, "}");
    String8 result = { .ptr = print_arena.mem, .len = print_arena.offset };
    arena_temp_end(print_temp);
    return result;
}

structdef(Stacks) {
    u8 param[0x100], param_ptr;
    u8 ret[0x100], ret_ptr;
};
static Stacks stacks;
static u8 *stack = stacks.param, *stack_ptr = &stacks.param_ptr;
static void stacks_set_param(void) { stack = stacks.param; stack_ptr = &stacks.param_ptr; }
static void stacks_set_ret(void) { stack = stacks.ret; stack_ptr = &stacks.ret_ptr; }

static b8 keep;

static u8 memory[0x10000];

#define s(ptr) stack[(u8)(ptr)]
#define mem(ptr) memory[(u16)(ptr)]

static u8 *get(u8 i_back) { return stack + (u8)(*stack_ptr - i_back); }
static void push(u8 byte) { stack[*stack_ptr] = byte; *stack_ptr += 1; }
static u8 pop(void) { u8 val = s(*stack_ptr - 1); if (!keep) *stack_ptr -= 1; return val; }

static u16 get2(u8 i_back) { return (u16)s(*stack_ptr - 2 * i_back + 1) | (u16)(s(*stack_ptr - 2 * i_back) << 8); }
static void push2(u16 byte2) { push(byte2 >> 8); push((u8)byte2); }
static u16 pop2(void) { u16 val = (u16)s(*stack_ptr - 1) | (u16)(s(*stack_ptr - 2) << 8); if (!keep) *stack_ptr -= 2; return val; }

static void run(char *path_biciasm) {
    printf("\nRUN ===\n");

    Arena arena = { .mem = memory, .cap = 0x10000 };
    file_read(&arena, path_biciasm, "rb", 0x10000);

    for (u16 i = 0x100; true; i += 1) {
        u8 byte = mem(i);
        switch (byte) {
            case op_jmi:   i += mem(i + 1); continue;
            case op_jei:   panic("TODO");
            case op_jsi:   panic("TODO");
            case op_break: goto break_run; break;
            default: break;
        }

        Instruction instruction = instruction_from_byte(byte);
        switch (instruction.mode.stack) {
            case stack_param: stacks_set_param(); break;
            case stack_ret: stacks_set_ret(); break;
            default: unreachable;
        }

        switch (instruction.mode.size) {
            case size_byte: switch (instruction.op) {
                case op_push:   push(mem(++i)); break;
                case op_drop:   discard(pop()); break;
                case op_nip:    *get(2) = *get(1); *stack_ptr -= 1; break;
                case op_swap:   { u8 temp2 = *get(2); *get(2) = *get(1); *get(1) = temp2; } break;
                case op_rot:    { u8 temp3 = *get(3); *get(3) = *get(2); *get(2) = *get(1); *get(1) = temp3; } break;
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
                case op_not:    push(~pop()); break;
                case op_and:    push(pop() & pop()); break;
                case op_or:     push(pop() | pop()); break;
                case op_xor:    push(pop() ^ pop()); break; 
                case op_shift:  { u8 shift = pop(), r = shift & 0x0f, l = (shift & 0xf0) >> 4; push((u8)(pop() << l >> r)); } break;
                case op_jmp:    i += pop(); break;
                case op_jmi:    i += mem(i + 1); break;
                case op_jeq:    { u8 rel_addr = pop(); if (pop()) i += rel_addr; } break;
                case op_jei:    i += 1; if (pop()) i += mem(i); break;
                case op_jst:    stacks_set_ret(); push2(i); stacks_set_param(); i += pop(); break;
                case op_jsi:    i += 1; stacks_set_ret(); push2(i); i += mem(i); break;
                case op_stash:  { u8 val = pop(); stacks_set_ret(); push(val); stacks_set_param(); } break; 
                case op_load:   push(mem(pop())); break;
                case op_loadr:  push(mem(i + pop())); break;
                case op_store:  memory[pop()] = pop(); break;
                case op_storer: memory[(u16)(i + pop())] = pop(); break;
                case op_read:   panic("TODO"); break;
                case op_write:  panic("TODO"); break;
                default: panicf("TODO %.*s", instruction_print(instruction));
            } break;
            case size_short: switch (instruction.op) {
                case op_push:  i += 1; push2(*(u16 *)(memory + i)); i += 1; break;
                case op_add:   { u16 left = get2(2), right = get2(1); pop2(); pop2(); push2(left + right); } break;
                case op_not:   push2(~pop2()); break;
                case op_stash: { u16 val = pop2(); stacks_set_ret(); push2(val); stacks_set_param(); } break;
                default: panicf("TODO %.*s", instruction_print(instruction));
            } break;
            default: unreachable;
        }
    }
    break_run:

    printf("param  stack (bot->top): "); 
    for (u8 i = 0; i < stacks.param_ptr; i += 1) printf("%02x ", stacks.param[i]);
    printf("\nreturn stack (bot->top): "); 
    for (u8 i = 0; i < stacks.ret_ptr; i += 1) printf("%02x ", stacks.ret[i]);
    putchar('\n');
}

