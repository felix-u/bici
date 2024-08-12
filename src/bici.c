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
    action(jnq,    0x18)\
    action(jni,    0x19)\
    action(stash,  0x1a)\
    action(load,   0x1b)\
    action(store,  0x1c)\
    action(read,   0x1d)\
    action(write,  0x1e)\
    action(jmi,    0x80)/* NO MODE (== push;k)  */\
    action(jei,    0xa0)/* NO MODE (== push;k2) */\
    action(jsi,    0xc0)/* NO MODE (== push;kr) */\
    action(break,  0xe0)/* NO MODE (== push;kr2)*/\

enumdef(Device, u8) {
    device_console = 0x00,
};

// B = mode_bytes, b = mode_bits
#define op_cases(B, bi)\
    case op_jmi: case op_jei: case op_jni: case op_jsi: case op_break: panic("reached full-byte opcodes in generic switch case");\
    case op_push:   push##bi(load##bi(i + 1)); add = B + 1; break;\
    case op_drop:   discard(pop##bi()); break;\
    case op_nip:    { u##bi c = pop##bi(); pop##bi(); u##bi a = pop##bi(); push##bi(a); push##bi(c); } break;\
    case op_swap:   { u##bi c = pop##bi(), b = pop##bi(); push##bi(c); push##bi(b); } break;\
    case op_rot:    { u##bi c = pop##bi(), b = pop##bi(), a = pop##bi(); push##bi(b); push##bi(c); push##bi(a); } break;\
    case op_dup:    assume(!m.keep); push##bi(get##bi(1)); break;\
    case op_over:   assume(!m.keep); push##bi(get##bi(2)); break;\
    case op_eq:     push##bi(pop##bi() == pop##bi()); break;\
    case op_neq:    push##bi(pop##bi() != pop##bi()); break;\
    case op_gt:     { u##bi right = pop##bi(), left = pop##bi(); push##bi(left > right); } break; \
    case op_lt:     { u##bi right = pop##bi(), left = pop##bi(); push##bi(left < right); } break; \
    case op_add:    push##bi(pop##bi() + pop##bi()); break;\
    case op_sub:    { u##bi right = pop##bi(), left = pop##bi(); push##bi(left - right); } break; \
    case op_mul:    push##bi(pop##bi() * pop##bi()); break;\
    case op_div:    { u##bi right = pop##bi(), left = pop##bi(); push##bi(right == 0 ? 0 : (left / right)); } break; \
    case op_inc:    push##bi(pop##bi() + 1); break;\
    case op_not:    push##bi(~pop##bi()); break;\
    case op_and:    push##bi(pop##bi() & pop##bi()); break;\
    case op_or:     push##bi(pop##bi() | pop##bi()); break;\
    case op_xor:    push##bi(pop##bi() ^ pop##bi()); break; \
    case op_shift:  { u8 shift = pop8(), r = shift & 0x0f, l = (shift & 0xf0) >> 4; push##bi((u##bi)(pop##bi() << l >> r)); } break;\
    case op_jmp:    assume(m.size == size_short); i = pop16(); add = 0; break;\
    case op_jeq:    { u16 addr = pop16(); if (pop##bi()) i = addr; } break;\
    case op_jst:    stacks_set_ret(); push16(i); stacks_set_param(); add = pop##bi() + 1; break;\
    case op_stash:  { u##bi val = pop##bi(); stacks_set_ret(); push##bi(val); stacks_set_param(); } break;\
    case op_load:   push##bi(load##bi(pop16())); break;\
    case op_store:  store##bi(pop16(), pop##bi()); break;\
    case op_read:   panic("TODO"); break;\
    case op_write: {\
        u8 device_and_action = mem(s(--(*stack_ptr)));\
        Device device = device_and_action & 0xf0;\
        u8 action = device_and_action & 0x0f;\
        switch (device) {\
            case device_console: switch (action) {\
                case 0x0: {\
                    assume(m.size == size_byte);\
                    u16 str_addr = pop16();\
                    u8 str_len = load8(str_addr);\
                    String8 str = { .ptr = memory + str_addr + 1, .len = str_len };\
                    printf("%.*s", string_fmt(str));\
                } break;\
            } break;\
            default: panic("no such device");\
        }\
    } break;\
    default: panicf("unreachable %s{#%02x}", op_name(byte), byte);

enumdef(Op, u8) {
    #define op_def_enum(name, val) op_##name = val,
    for_op(op_def_enum)
    op_max_value_plus_one
};

const char *op_name_table[op_max_value_plus_one] = {
    #define op_set_name(name, val) [val] = #name,
    for_op(op_set_name)
};

static bool instruction_is_special(u8 instruction) {
    switch (instruction) {
        case op_jmi: case op_jei: case op_jni: case op_jsi: case op_break: return true;
        default: return false;
    }
}

static const char *op_name(u8 instruction) {
    return op_name_table[instruction_is_special(instruction) ? instruction : (instruction & 0x1f)];
}

enumdef(Stack, u8) { stack_param = 0, stack_ret = 1 };
enumdef(Size, u8)  { size_byte = 0, size_short = 1 };
structdef(Mode) { b8 keep; Stack stack; Size size; };

static const char *mode_name(u8 instruction) {
    if (instruction_is_special(instruction)) return ""; 
    switch ((instruction & 0xe0) >> 5) {
        case 0x1: return ";2";
        case 0x2: return ";r";
        case 0x3: return ";r2";
        case 0x4: return ";k";
        case 0x5: return ";k2";
        case 0x6: return ";kr";
        case 0x7: return ";kr2";
    }
    return "";
}

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

static u8 get8(u8 i_back) { return *(stack + (u8)(*stack_ptr - i_back)); }
static void push8(u8 byte) { stack[*stack_ptr] = byte; *stack_ptr += 1; }
static u8 pop8(void) { u8 val = s(*stack_ptr - 1); if (!keep) *stack_ptr -= 1; return val; }
static u8 load8(u16 addr) { return mem(addr); }
static void store8(u16 addr, u8 val) { mem(addr) = val; }

static u16 get16(u8 i_back) { return (u16)s(*stack_ptr - 2 * i_back + 1) | (u16)(s(*stack_ptr - 2 * i_back) << 8); }
static void push16(u16 byte2) { push8(byte2 >> 8); push8((u8)byte2); }
static u16 pop16(void) { u16 val = (u16)s(*stack_ptr - 1) | (u16)(s(*stack_ptr - 2) << 8); if (!keep) *stack_ptr -= 2; return val; }
static u16 load16(u16 addr) { return (u16)(((u16)mem(addr) << 8) | (u16)mem(addr + 1)); }
static void store16(u16 addr, u16 val) { mem(addr) = (u8)(val >> 8); mem(addr + 1) = (u8)val; } // TODO: ensure correct

static void run(char *path_biciasm) {
    Arena bici_mem = { .mem = memory, .cap = 0x10000 };
    String8 program = file_read(&bici_mem, path_biciasm, "rb", 0x10000);

    printf("MEMORY ===\n");
    for (u16 i = 0x100; i < program.len; i += 1) {
        u8 byte = memory[i];
        printf("[%4x]\t'%c'\t#%02x\t%s%s\n", i, byte, byte, op_name(byte), mode_name(byte));
    }
    printf("\nRUN ===\n");

    u16 add = 1;
    for (u16 i = 0x100; true; i += add) {
        add = 1;
        u8 byte = mem(i);
        Instruction instruction = instruction_from_byte(byte);
        keep = instruction.mode.keep;
        Mode m = instruction.mode;

        switch (instruction.mode.stack) {
            case stack_param: stacks_set_param(); break;
            case stack_ret: stacks_set_ret(); break;
            default: unreachable;
        }

        switch (byte) {
            case op_jmi:   panic("TODO");
            case op_jei:   panic("TODO");
            case op_jni:   if (!pop8()) { i = load16(i + 1); add = 0; } else add = 2; continue;
            case op_jsi:   panic("TODO");
            case op_break: goto break_run; break;
            default: break;
        }

        switch (instruction.mode.size) {
            case size_byte: switch (instruction.op) { op_cases(1, 8) } break;
            case size_short: switch (instruction.op) { op_cases(2, 16) } break;
            default: panicf("unreachable %s{#%02x}", op_name(byte), byte);
        }
    }
    break_run:

    printf("param  stack (bot->top): { "); 
    for (u8 i = 0; i < stacks.param_ptr; i += 1) printf("%02x ", stacks.param[i]);
    printf("}\nreturn stack (bot->top): { "); 
    for (u8 i = 0; i < stacks.ret_ptr; i += 1) printf("%02x ", stacks.ret[i]);
    printf("}\n");
}

