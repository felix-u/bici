#define vm_for_op(action)\
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

enumdef(Vm_Device, u8) {
    vm_device_console = 0x00,
    vm_device_screen = 0x10,
};

// B = mode_bytes, b = mode_bits
#define vm_op_cases(B, bi)\
    /* TODO: for most, if not all, ops, use get##bi rather than pop##bi to work with mode.keep */\
    case vm_op_jmi: case vm_op_jei: case vm_op_jni: case vm_op_jsi: case vm_op_break: panic("reached full-byte opcodes in generic switch case");\
    case vm_op_push:   vm_push##bi(vm, vm_load##bi(vm, vm->pc + 1)); add = B + 1; break;\
    case vm_op_drop:   discard(vm_pop##bi(vm)); break;\
    case vm_op_nip:    { u##bi c = vm_pop##bi(vm); vm_pop##bi(vm); u##bi a = vm_pop##bi(vm); vm_push##bi(vm, a); vm_push##bi(vm, c); } break;\
    case vm_op_swap:   { u##bi c = vm_pop##bi(vm), b = vm_pop##bi(vm); vm_push##bi(vm, c); vm_push##bi(vm, b); } break;\
    case vm_op_rot:    { u##bi c = vm_pop##bi(vm), b = vm_pop##bi(vm), a = vm_pop##bi(vm); vm_push##bi(vm, b); vm_push##bi(vm, c); vm_push##bi(vm, a); } break;\
    case vm_op_dup:    assume(!vm->op_mode.keep); vm_push##bi(vm, vm_get##bi(vm, 1)); break;\
    case vm_op_over:   assume(!vm->op_mode.keep); vm_push##bi(vm, vm_get##bi(vm, 2)); break;\
    case vm_op_eq:     vm_push##bi(vm, vm_pop##bi(vm) == vm_pop##bi(vm)); break;\
    case vm_op_neq:    vm_push##bi(vm, vm_pop##bi(vm) != vm_pop##bi(vm)); break;\
    case vm_op_gt:     { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push##bi(vm, left > right); } break;\
    case vm_op_lt:     { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push##bi(vm, left < right); } break;\
    case vm_op_add:    vm_push##bi(vm, vm_pop##bi(vm) + vm_pop##bi(vm)); break;\
    case vm_op_sub:    { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push##bi(vm, left - right); } break;\
    case vm_op_mul:    vm_push##bi(vm, vm_pop##bi(vm) * vm_pop##bi(vm)); break;\
    case vm_op_div:    { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push##bi(vm, right == 0 ? 0 : (left / right)); } break;\
    case vm_op_inc:    vm_push##bi(vm, vm_pop##bi(vm) + 1); break;\
    case vm_op_not:    vm_push##bi(vm, ~vm_pop##bi(vm)); break;\
    case vm_op_and:    vm_push##bi(vm, vm_pop##bi(vm) & vm_pop##bi(vm)); break;\
    case vm_op_or:     vm_push##bi(vm, vm_pop##bi(vm) | vm_pop##bi(vm)); break;\
    case vm_op_xor:    vm_push##bi(vm, vm_pop##bi(vm) ^ vm_pop##bi(vm)); break;\
    case vm_op_shift:  { u8 shift = vm_pop8(vm), r = shift & 0x0f, l = (shift & 0xf0) >> 4; vm_push##bi(vm, (u##bi)(vm_pop##bi(vm) << l >> r)); } break;\
    case vm_op_jmp:    vm->pc = vm_pop16(vm); add = 0; break;\
    case vm_op_jeq:    { u16 addr = vm_pop16(vm); if (vm_pop##bi(vm)) { vm->pc = addr; add = 0; } } break;\
    case vm_op_jst:    vm->stack_active = stack_ret; vm_push16(vm, vm->pc + 1); vm->stack_active = stack_param; vm->pc = vm_pop16(vm); add = 0; break;\
    case vm_op_stash:  { u##bi val = vm_pop##bi(vm); vm->stack_active = stack_ret; vm_push##bi(vm, val); vm->stack_active = stack_param; } break;\
    case vm_op_load:   vm_push##bi(vm, vm_load##bi(vm, vm_pop16(vm))); break;\
    case vm_op_store:  { u16 addr = vm_pop16(vm); u##bi val = vm_pop##bi(vm); vm_store##bi(vm, addr, val); } break;\
    case vm_op_read:   {\
        u8 vm_device_and_action = vm_pop8(vm);\
        Vm_Device device = vm_device_and_action & 0xf0;\
        u8 action = vm_device_and_action & 0x0f;\
        discard(action);\
        switch (device) {\
            case vm_device_screen: {\
                vm_push16(vm, vm->screen.width); vm_push16(vm, vm->screen.height);\
            } break;\
            default: panicf("invalid device #%x for operation 'read'", device);\
        }\
    } break;\
    case vm_op_write:  {\
        u8 vm_device_and_action = vm_pop8(vm);\
        Vm_Device device = vm_device_and_action & 0xf0;\
        u8 action = vm_device_and_action & 0x0f;\
        switch (device) {\
            case vm_device_console: switch (action) {\
                case 0x0: {\
                    assume(vm->op_mode.size == vm_op_size_byte);\
                    u16 str_addr = vm_pop16(vm);\
                    u8 str_len = vm_load8(vm, str_addr);\
                    String8 str = { .ptr = vm->memory + str_addr + 1, .len = str_len };\
                    printf("%.*s", string_fmt(str));\
                } break;\
                default: panicf("[write] invalid action #%x for console device", action);\
            } break;\
            case vm_device_screen: switch (action) {\
                default: panicf("[write] invalid action #%x for screen device", action);\
            } break;\
            default: panicf("invalid device #%x for operation 'write'", device);\
        }\
    } break;\
    default: panicf("unreachable %s{#%02x}", vm_op_name(byte), byte);

enumdef(Vm_Op, u8) {
    #define vm_op_def_enum(name, val) vm_op_##name = val,
    vm_for_op(vm_op_def_enum)
    vm_op_max_value_plus_one
};

const char *vm_op_name_table[vm_op_max_value_plus_one] = {
    #define vm_op_set_name(name, val) [val] = #name,
    vm_for_op(vm_op_set_name)
};

static bool vm_instruction_is_special(u8 instruction) {
    switch (instruction) {
        case vm_op_jmi: case vm_op_jei: case vm_op_jni: case vm_op_jsi: case vm_op_break: return true;
        default: return false;
    }
}

static const char *vm_op_name(u8 instruction) {
    return vm_op_name_table[vm_instruction_is_special(instruction) ? instruction : (instruction & 0x1f)];
}

structdef(Vm_Stack) { u8 memory[0x100], ptr; };
enumdef(Vm_Stack_Active, u8) { stack_param = 0, stack_ret = 1 };
enumdef(Vm_Op_Size, u8)  { vm_op_size_byte = 0, vm_op_size_short = 1 };
structdef(Vm_Op_Mode) { b8 keep; Vm_Stack_Active stack; Vm_Op_Size size; };

static const char *mode_name(u8 instruction) {
    if (vm_instruction_is_special(instruction)) return ""; 
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

structdef(Vm_Instruction) { Vm_Op op; Vm_Op_Mode mode; };

static u8 vm_byte_from_instruction(Vm_Instruction instruction) {
    return (u8)(
        instruction.op | 
        (instruction.mode.keep << 7) | 
        (instruction.mode.stack << 6) | 
        (instruction.mode.size << 5)
    );
}

static Vm_Instruction vm_instruction_from_byte(u8 byte) {
    return (Vm_Instruction){ .op = byte & 0x1f, .mode = {
        .keep =  (byte & 0x80) >> 7,
        .stack = (byte & 0x40) >> 6,
        .size =  (byte & 0x20) >> 5,
    } };
}

structdef(Vm) {
    u8 memory[0x10000];
    Vm_Stack stacks[2];
    Vm_Stack_Active stack_active;
    u16 pc;
    Vm_Op_Mode op_mode;
    Screen screen;
};

#define vm_stack vm->stacks[vm->stack_active].memory
#define vm_sp vm->stacks[vm->stack_active].ptr

#define vm_s(ptr) vm_stack[(u8)(ptr)]
#define vm_mem(ptr) vm->memory[(u16)(ptr)]

static u8   vm_get8(Vm *vm, u8 i_back) { return *(vm_stack + (u8)(vm_sp - i_back)); }
static void vm_push8(Vm *vm, u8 byte) { vm_s(vm_sp) = byte; vm_sp += 1; }
static u8   vm_pop8(Vm *vm) { u8 val = vm_s(vm_sp - 1); if (!vm->op_mode.keep) vm_sp -= 1; return val; }
static u8   vm_load8(Vm *vm, u16 addr) { return vm_mem(addr); }
static void vm_store8(Vm *vm, u16 addr, u8 val) { vm_mem(addr) = val; }

static u16  vm_get16(Vm *vm, u8 i_back) { return (u16)vm_s(vm_sp - 2 * i_back + 1) | (u16)(vm_s(vm_sp - 2 * i_back) << 8); }
static void vm_push16(Vm *vm, u16 byte2) { vm_push8(vm, byte2 >> 8); vm_push8(vm, (u8)byte2); }
static u16  vm_pop16(Vm *vm) { u16 val = (u16)vm_s(vm_sp - 1) | (u16)(vm_s(vm_sp - 2) << 8); if (!vm->op_mode.keep) vm_sp -= 2; return val; }
static u16  vm_load16(Vm *vm, u16 addr) { return (u16)(((u16)vm_mem(addr) << 8) | (u16)vm_mem(addr + 1)); }
static void vm_store16(Vm *vm, u16 addr, u16 val) { vm_mem(addr) = (u8)(val >> 8); vm_mem(addr + 1) = (u8)val; } // TODO: ensure correct

static void vm_run_to_break(Vm *vm, u16 pc) {
    vm->pc = pc;
    u16 add = 1;
    for (; true; vm->pc += add) {
        add = 1;
        u8 byte = vm_mem(vm->pc);
        Vm_Instruction instruction = vm_instruction_from_byte(byte);
        vm->op_mode = instruction.mode;

        vm->stack_active = instruction.mode.stack;

        switch (byte) {
            case vm_op_break: return;
            case vm_op_jmi: vm->pc = vm_load16(vm, vm->pc + 1); add = 0; continue;
            case vm_op_jei: if ( vm_pop8(vm)) { vm->pc = vm_load16(vm, vm->pc + 1); add = 0; } else add = 3; continue;
            case vm_op_jni: if (!vm_pop8(vm)) { vm->pc = vm_load16(vm, vm->pc + 1); add = 0; } else add = 3; continue;
            case vm_op_jsi: vm->stack_active = stack_ret; vm_push16(vm, vm->pc + 3); vm->stack_active = stack_param; vm->pc = vm_load16(vm, vm->pc + 1); add = 0; continue;
            default: break;
        }

        switch (instruction.mode.size) {
            case vm_op_size_byte: switch (instruction.op) { vm_op_cases(1, 8) } break;
            case vm_op_size_short: switch (instruction.op) { vm_op_cases(2, 16) } break;
            default: panicf("unreachable %s{#%02x}", vm_op_name(byte), byte);
        }
    }
}

static void vm_run(String8 rom) {
    if (rom.len == 0) return;

    Vm vm = {0};
    memcpy(vm.memory, rom.ptr, rom.len);

    printf("MEMORY ===\n");
    for (u16 i = 0x100; i < rom.len; i += 1) {
        u8 byte = vm.memory[i];
        printf("[%4x]\t'%c'\t#%02x\t%s%s\n", i, byte, byte, vm_op_name(byte), mode_name(byte));
    }
    printf("\nRUN ===\n");

    vm.screen = screen_init(); 
    u16 vm_on_screen_init_pc = vm_load16(&vm, vm_device_screen);
    vm_run_to_break(&vm, vm_on_screen_init_pc);

    u16 vm_on_screen_update_pc = vm_load16(&vm, vm_device_screen | 0x02);
    u16 vm_on_screen_resize_pc = vm_load16(&vm, vm_device_screen | 0x06);
    do {
        if (vm.screen.state == screen_state_resized) vm_run_to_break(&vm, vm_on_screen_resize_pc);
        vm_run_to_break(&vm, vm_on_screen_update_pc);
    } while (screen_update(&vm.screen));

    u16 vm_on_screen_quit_pc = vm_load16(&vm, vm_device_screen | 0x04);
    vm_run_to_break(&vm, vm_on_screen_quit_pc);
    screen_quit(&vm.screen);

    printf("param  stack (bot->top): { "); 
    for (u8 i = 0; i < vm.stacks[stack_param].ptr; i += 1) printf("%02x ", vm.stacks[stack_param].memory[i]);
    printf("}\nreturn stack (bot->top): { "); 
    for (u8 i = 0; i < vm.stacks[stack_ret].ptr; i += 1) printf("%02x ", vm.stacks[stack_ret].memory[i]);
    printf("}\n");
}

