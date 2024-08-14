#define for_op(action)\
    action(break,  0x00)/* NO MODE */\
    action(push,   0x01)/* NO k MODE */\
    action(drop,   0x02)\
    action(nip,    0x03)\
    action(swap,   0x04)\
    action(rot,    0x05)\
    action(dup,    0x06)\
    action(over,   0x07)\
    action(eq,     0x08)\
    action(neq,    0x09)\
    action(gt,     0x0a)\
    action(lt,     0x0b)\
    action(add,    0x0c)\
    action(sub,    0x0d)\
    action(mul,    0x0e)\
    action(div,    0x0f)\
    action(inc,    0x10)\
    action(not,    0x11)\
    action(and,    0x12)\
    action(or,     0x13)\
    action(xor,    0x14)\
    action(shift,  0x15)\
    action(jmp,    0x16)\
    action(jeq,    0x17)\
    action(jst,    0x18)\
    action(jnq,    0x19)\
    action(jni,    0x1a)\
    action(stash,  0x1b)\
    action(load,   0x1c)\
    action(store,  0x1d)\
    action(read,   0x1e)\
    action(write,  0x1f)\
    action(jmi,    0x80)/* NO MODE (== push;k)  */\
    action(jei,    0xa0)/* NO MODE (== push;k2) */\
    action(jsi,    0xc0)/* NO MODE (== push;kr) */\
    /* UNUSED      0xe0)   NO MODE (== push;kr2)*/\

enumdef(Device, u8) {
    device_console = 0x00,
    device_screen = 0x10,
};

// B = mode_bytes, b = mode_bits
#define op_cases(B, bi)\
    case op_jmi: case op_jei: case op_jni: case op_jsi: case op_break: panic("reached full-byte opcodes in generic switch case");\
    case op_push:   push##bi(vm, load##bi(vm, i + 1)); add = B + 1; break;\
    case op_drop:   discard(pop##bi(vm)); break;\
    case op_nip:    { u##bi c = pop##bi(vm); pop##bi(vm); u##bi a = pop##bi(vm); push##bi(vm, a); push##bi(vm, c); } break;\
    case op_swap:   { u##bi c = pop##bi(vm), b = pop##bi(vm); push##bi(vm, c); push##bi(vm, b); } break;\
    case op_rot:    { u##bi c = pop##bi(vm), b = pop##bi(vm), a = pop##bi(vm); push##bi(vm, b); push##bi(vm, c); push##bi(vm, a); } break;\
    case op_dup:    assume(!m.keep); push##bi(vm, get##bi(vm, 1)); break;\
    case op_over:   assume(!m.keep); push##bi(vm, get##bi(vm, 2)); break;\
    case op_eq:     push##bi(vm, pop##bi(vm) == pop##bi(vm)); break;\
    case op_neq:    push##bi(vm, pop##bi(vm) != pop##bi(vm)); break;\
    case op_gt:     { u##bi right = pop##bi(vm), left = pop##bi(vm); push##bi(vm, left > right); } break; \
    case op_lt:     { u##bi right = pop##bi(vm), left = pop##bi(vm); push##bi(vm, left < right); } break; \
    case op_add:    push##bi(vm, pop##bi(vm) + pop##bi(vm)); break;\
    case op_sub:    { u##bi right = pop##bi(vm), left = pop##bi(vm); push##bi(vm, left - right); } break; \
    case op_mul:    push##bi(vm, pop##bi(vm) * pop##bi(vm)); break;\
    case op_div:    { u##bi right = pop##bi(vm), left = pop##bi(vm); push##bi(vm, right == 0 ? 0 : (left / right)); } break; \
    case op_inc:    push##bi(vm, pop##bi(vm) + 1); break;\
    case op_not:    push##bi(vm, ~pop##bi(vm)); break;\
    case op_and:    push##bi(vm, pop##bi(vm) & pop##bi(vm)); break;\
    case op_or:     push##bi(vm, pop##bi(vm) | pop##bi(vm)); break;\
    case op_xor:    push##bi(vm, pop##bi(vm) ^ pop##bi(vm)); break; \
    case op_shift:  { u8 shift = pop8(vm), r = shift & 0x0f, l = (shift & 0xf0) >> 4; push##bi(vm, (u##bi)(pop##bi(vm) << l >> r)); } break;\
    case op_jmp:    i = pop16(vm); add = 0; break;\
    case op_jeq:    { u16 addr = pop16(vm); if (pop##bi(vm)) { i = addr; add = 0; } } break;\
    case op_jst:    stacks_set_ret(vm); push16(vm, i + 1); stacks_set_param(vm); i = pop16(vm); add = 0; break;\
    case op_stash:  { u##bi val = pop##bi(vm); stacks_set_ret(vm); push##bi(vm, val); stacks_set_param(vm); } break;\
    case op_load:   push##bi(vm, load##bi(vm, pop16(vm))); break;\
    case op_store:  { u16 addr = pop16(vm); u##bi val = pop##bi(vm); store##bi(vm, addr, val); } break;\
    case op_read:   {\
        u8 device_and_action = pop8(vm);\
        Device device = device_and_action & 0xf0;\
        u8 action = device_and_action & 0x0f;\
        discard(action);\
        switch (device) {\
            case device_screen: {\
                u16 width = 0, height = 0; screen_get_width_height(&width, &height);\
                push16(vm, width); push16(vm, height);\
            } break;\
            default: panicf("invalid device #%x for operation 'read'", device);\
        }\
    } break;\
    case op_write:  {\
        u8 device_and_action = pop8(vm);\
        Device device = device_and_action & 0xf0;\
        u8 action = device_and_action & 0x0f;\
        switch (device) {\
            case device_console: switch (action) {\
                case 0x0: {\
                    assume(m.size == size_byte);\
                    u16 str_addr = pop16(vm);\
                    u8 str_len = load8(vm, str_addr);\
                    String8 str = { .ptr = vm->memory + str_addr + 1, .len = str_len };\
                    printf("%.*s", string_fmt(str));\
                } break;\
                default: panicf("[write] invalid action #%x for console device", action);\
            } break;\
            case device_screen: switch (action) {\
                case 0x1: screen_init(); break;\
                case 0x2: screen_update(); break;\
                case 0x3: screen_quit(); break;\
                default: panicf("[write] invalid action #%x for screen device", action);\
            } break;\
            default: panicf("invalid device #%x for operation 'write'", device);\
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

structdef(Vm) {
    u8 memory[0x10000];
    u8 param_stack[0x100], param_stack_ptr;
    u8 ret_stack[0x100], ret_stack_ptr;
    u8 *stack, *stack_ptr;

    b8 keep;
};

static void stacks_set_param(Vm *vm) { vm->stack = vm->param_stack; vm->stack_ptr = &vm->param_stack_ptr; }
static void stacks_set_ret(Vm *vm) { vm->stack = vm->ret_stack; vm->stack_ptr = &vm->ret_stack_ptr; }

#define s(ptr) vm->stack[(u8)(ptr)]
#define mem(ptr) vm->memory[(u16)(ptr)]

static u8 get8(Vm *vm, u8 i_back) { return *(vm->stack + (u8)(*(vm->stack_ptr) - i_back)); }
static void push8(Vm *vm, u8 byte) { vm->stack[*(vm->stack_ptr)] = byte; *(vm->stack_ptr) += 1; }
static u8 pop8(Vm *vm) { u8 val = s(*(vm->stack_ptr) - 1); if (!vm->keep) *(vm->stack_ptr) -= 1; return val; }
static u8 load8(Vm *vm, u16 addr) { return mem(addr); }
static void store8(Vm *vm, u16 addr, u8 val) { mem(addr) = val; }

static u16 get16(Vm *vm, u8 i_back) { return (u16)s(*(vm->stack_ptr) - 2 * i_back + 1) | (u16)(s(*(vm->stack_ptr) - 2 * i_back) << 8); }
static void push16(Vm *vm, u16 byte2) { push8(vm, byte2 >> 8); push8(vm, (u8)byte2); }
static u16 pop16(Vm *vm) { u16 val = (u16)s(*(vm->stack_ptr) - 1) | (u16)(s(*(vm->stack_ptr) - 2) << 8); if (!vm->keep) *(vm->stack_ptr) -= 2; return val; }
static u16 load16(Vm *vm, u16 addr) { return (u16)(((u16)mem(addr) << 8) | (u16)mem(addr + 1)); }
static void store16(Vm *vm, u16 addr, u16 val) { mem(addr) = (u8)(val >> 8); mem(addr + 1) = (u8)val; } // TODO: ensure correct

static void vm_run(Vm *vm, String8 rom) {
    memcpy(vm->memory, rom.ptr, rom.len);

    // TODO
    vm->stack = vm->param_stack;
    vm->stack_ptr = &vm->param_stack_ptr;

    printf("MEMORY ===\n");
    for (u16 i = 0x100; i < rom.len; i += 1) {
        u8 byte = vm->memory[i];
        printf("[%4x]\t'%c'\t#%02x\t%s%s\n", i, byte, byte, op_name(byte), mode_name(byte));
    }
    printf("\nRUN ===\n");

    u16 add = 1;
    for (u16 i = 0x100; true; i += add) {
        add = 1;
        u8 byte = mem(i);
        Instruction instruction = instruction_from_byte(byte);
        vm->keep = instruction.mode.keep;
        Mode m = instruction.mode;

        switch (instruction.mode.stack) {
            case stack_param: stacks_set_param(vm); break;
            case stack_ret: stacks_set_ret(vm); break;
            default: unreachable;
        }

        switch (byte) {
            case op_jmi:   i = load16(vm, i + 1); add = 0; continue;
            case op_jei:   if ( pop8(vm)) { i = load16(vm, i + 1); add = 0; } else add = 3; continue;
            case op_jni:   if (!pop8(vm)) { i = load16(vm, i + 1); add = 0; } else add = 3; continue;
            case op_jsi:   stacks_set_ret(vm); push16(vm, i + 3); stacks_set_param(vm); i = load16(vm, i + 1); add = 0; continue;
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
    for (u8 i = 0; i < vm->param_stack_ptr; i += 1) printf("%02x ", vm->param_stack[i]);
    printf("}\nreturn stack (bot->top): { "); 
    for (u8 i = 0; i < vm->ret_stack_ptr; i += 1) printf("%02x ", vm->ret_stack[i]);
    printf("}\n");
}

