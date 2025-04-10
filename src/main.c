#include "base/base.c"

enumdef(Screen_Colour, u8) {
    screen_c0a = 0, screen_c0b = 1,
    screen_c1a = 2, screen_c1b = 3,
    screen_c2a = 4, screen_c2b = 5,
    screen_c3a = 6, screen_c3b = 7,
    screen_colour_count,
};

static u32 screen_palette[screen_colour_count] = {
    [screen_c0a] = 0x000000ff, [screen_c0b] = 0x000000ff,
    [screen_c1a] = 0x808080ff, [screen_c1b] = 0x808080ff,
    [screen_c2a] = 0x2d7d9aff, [screen_c2b] = 0x2d7d9aff,
    [screen_c3a] = 0xffffffff, [screen_c3b] = 0xffffffff,
};

#define vm_screen_width 256
#define vm_screen_height 256

#define vm_for_opcode(action)\
    /*     name,   byte, is_immediate */\
    action(break,  0x00, 0)/* NO MODE */\
    action(push,   0x01, 1)/* NO k MODE */\
    action(drop,   0x02, 0)\
    action(nip,    0x03, 0)\
    action(swap,   0x04, 0)\
    action(rot,    0x05, 0)\
    action(dup,    0x06, 0)\
    action(over,   0x07, 0)\
    action(eq,     0x08, 0)\
    action(neq,    0x09, 0)\
    action(gt,     0x0a, 0)\
    action(lt,     0x0b, 0)\
    action(add,    0x0c, 0)\
    action(sub,    0x0d, 0)\
    action(mul,    0x0e, 0)\
    action(div,    0x0f, 0)\
    action(inc,    0x10, 0)\
    action(not,    0x11, 0)\
    action(and,    0x12, 0)\
    action(or,     0x13, 0)\
    action(xor,    0x14, 0)\
    action(shift,  0x15, 0)\
    action(jmp,    0x16, 0)\
    action(jme,    0x17, 0)\
    action(jst,    0x18, 0)\
    action(jne,    0x19, 0)\
    action(jni,    0x1a, 1)\
    action(stash,  0x1b, 0)\
    action(load,   0x1c, 0)\
    action(store,  0x1d, 0)\
    action(read,   0x1e, 0)\
    action(write,  0x1f, 0)\
    action(jmi,    0x80, 1)/* NO MODE (== push;k)  */\
    action(jei,    0xa0, 1)/* NO MODE (== push;k2) */\
    action(jsi,    0xc0, 1)/* NO MODE (== push;kr) */\
    /* UNUSED      0xe0, 0)   NO MODE (== push;kr2)*/\

enumdef(Vm_Device, u8) {
    vm_device_console = 0x00,
    vm_device_screen = 0x10,
};

enum Vm_Screen_Action {
    vm_screen_init   = 0x0,
    vm_screen_pixel  = 0x1,
    vm_screen_update = 0x2,
    vm_screen_quit   = 0x4,
};

// B = mode_bytes, b = mode_bits
#define vm_opcode_cases(B, bi)\
    /* TODO(felix): for most, if not all, ops, use get##bi rather than pop##bi to work with mode.keep */\
    case vm_opcode_jmi: case vm_opcode_jei: case vm_opcode_jni: case vm_opcode_jsi: case vm_opcode_break:\
        panic("reached full-byte opcodes in generic switch case");\
    case vm_opcode_push:   vm_push##bi(vm, vm_load##bi(vm, vm->program_counter + 1)); add = B + 1; break;\
    case vm_opcode_drop:   discard(vm_pop##bi(vm)); break;\
    case vm_opcode_nip:    { u##bi c = vm_pop##bi(vm); vm_pop##bi(vm); u##bi a = vm_pop##bi(vm); vm_push##bi(vm, a); vm_push##bi(vm, c); } break;\
    case vm_opcode_swap:   { u##bi c = vm_pop##bi(vm), b = vm_pop##bi(vm); vm_push##bi(vm, c); vm_push##bi(vm, b); } break;\
    case vm_opcode_rot:    { u##bi c = vm_pop##bi(vm), b = vm_pop##bi(vm), a = vm_pop##bi(vm); vm_push##bi(vm, b); vm_push##bi(vm, c); vm_push##bi(vm, a); } break;\
    case vm_opcode_dup:    assert(!vm->current_mode.keep); vm_push##bi(vm, vm_get##bi(vm, 1)); break;\
    case vm_opcode_over:   assert(!vm->current_mode.keep); vm_push##bi(vm, vm_get##bi(vm, 2)); break;\
    case vm_opcode_eq:     vm_push8(vm, vm_pop##bi(vm) == vm_pop##bi(vm)); break;\
    case vm_opcode_neq:    vm_push8(vm, vm_pop##bi(vm) != vm_pop##bi(vm)); break;\
    case vm_opcode_gt:     { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push8(vm, left > right); } break;\
    case vm_opcode_lt:     { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push8(vm, left < right); } break;\
    case vm_opcode_add:    vm_push##bi(vm, vm_pop##bi(vm) + vm_pop##bi(vm)); break;\
    case vm_opcode_sub:    { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push##bi(vm, left - right); } break;\
    case vm_opcode_mul:    vm_push##bi(vm, vm_pop##bi(vm) * vm_pop##bi(vm)); break;\
    case vm_opcode_div:    { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push##bi(vm, right == 0 ? 0 : (left / right)); } break;\
    case vm_opcode_inc:    vm_push##bi(vm, vm_pop##bi(vm) + 1); break;\
    case vm_opcode_not:    vm_push##bi(vm, ~vm_pop##bi(vm)); break;\
    case vm_opcode_and:    vm_push##bi(vm, vm_pop##bi(vm) & vm_pop##bi(vm)); break;\
    case vm_opcode_or:     vm_push##bi(vm, vm_pop##bi(vm) | vm_pop##bi(vm)); break;\
    case vm_opcode_xor:    vm_push##bi(vm, vm_pop##bi(vm) ^ vm_pop##bi(vm)); break;\
    case vm_opcode_shift:  { u8 shift = vm_pop8(vm), r = shift & 0x0f, l = (shift & 0xf0) >> 4; vm_push##bi(vm, (u##bi)(vm_pop##bi(vm) << l >> r)); } break;\
    case vm_opcode_jmp:    vm->program_counter = vm_pop16(vm); add = 0; break;\
    case vm_opcode_jme:    { u16 addr = vm_pop16(vm); if (vm_pop##bi(vm)) { vm->program_counter = addr; add = 0; } } break;\
    case vm_opcode_jst:    {\
        vm->active_stack = stack_return;\
        vm_push16(vm, vm->program_counter + 1);\
        vm->active_stack = stack_param;\
        vm->program_counter = vm_pop16(vm);\
        add = 0;\
    } break;\
    case vm_opcode_stash:  { u##bi val = vm_pop##bi(vm); vm->active_stack = stack_return; vm_push##bi(vm, val); vm->active_stack = stack_param; } break;\
    case vm_opcode_load:   { u16 addr = vm_pop16(vm); u##bi val = vm_load##bi(vm, addr); vm_push##bi(vm, val); } break;\
    case vm_opcode_store:  { u16 addr = vm_pop16(vm); u##bi val = vm_pop##bi(vm); vm_store##bi(vm, addr, val); } break;\
    case vm_opcode_read:   {\
        u8 vm_device_and_action = vm_pop8(vm);\
        Vm_Device device = vm_device_and_action & 0xf0;\
        u8 action = vm_device_and_action & 0x0f;\
        switch (device) {\
            case vm_device_screen: switch (action) {\
                default: panic("[read] invalid action #% for screen device", fmt(u64, action, .base = 16));\
            } break;\
            default: panic("invalid device #% for operation 'read'", fmt(u64, device, .base = 16));\
        }\
    } break;\
    case vm_opcode_write:  {\
        u8 vm_device_and_action = vm_pop8(vm);\
        Vm_Device device = vm_device_and_action & 0xf0;\
        u8 action = vm_device_and_action & 0x0f;\
        switch (device) {\
            case vm_device_console: switch (action) {\
                case 0x0: {\
                    assert(vm->current_mode.size == vm_opcode_size_byte);\
                    u16 str_addr = vm_pop16(vm);\
                    u8 str_count = vm_load8(vm, str_addr);\
                    String str = { .data = vm->memory + str_addr + 1, .count = str_count };\
                    print("%", fmt(String, str));\
                } break;\
                default: panic("[write] invalid action #% for console device", fmt(u64, action, .base = 16));\
            } break;\
            case vm_device_screen: switch (action) {\
                case vm_screen_pixel: {\
                    Screen_Colour colour = vm_pop8(vm);\
                    if (colour >= screen_colour_count) panic("[write:screen/pixel] colour #% is invalid; there are only #% palette colours", fmt(u64, colour, .base = 16), fmt(u64, screen_colour_count));\
                    u16 y = vm_pop16(vm), x = vm_pop16(vm);\
                    if (x >= vm_screen_width || y >= vm_screen_height) panic("[write:screen/pixel] coordinate #%x#% is outside screen bounds #%x#%", fmt(u64, x, .base = 16), fmt(u64, y, .base = 16), fmt(u64, vm_screen_width, .base = 16), fmt(u64, vm_screen_height, .base = 16));\
                    gfx_set_pixel(&vm->gfx, x, y, screen_palette[colour]);\
                } break;\
                default: panic("[write] invalid action #% for screen device", fmt(u64, action, .base = 16));\
            } break;\
            default: panic("invalid device #% for operation 'write'", fmt(u64, device, .base = 16));\
        }\
    } break;\
    default: panic("unreachable %{#%}", fmt(cstring, (char *)vm_opcode_name(byte)), fmt(u64, byte, .base = 16));

enumdef(Vm_Opcode, u8) {
    #define vm_opcode_def_enum(name, val, ...) vm_opcode_##name = val,
    vm_for_opcode(vm_opcode_def_enum)
    vm_opcode_max_value_plus_one
};

const char *vm_opcode_name_table[vm_opcode_max_value_plus_one] = {
    #define vm_opcode_set_name(name, val, ...) [val] = #name,
    vm_for_opcode(vm_opcode_set_name)
};

static bool vm_opcode_is_special(u8 instruction) {
    switch (instruction) {
        case vm_opcode_jmi: case vm_opcode_jei: case vm_opcode_jni: case vm_opcode_jsi: case vm_opcode_break: return true;
        default: return false;
    }
}

static const char *vm_opcode_name(u8 instruction) {
    return vm_opcode_name_table[vm_opcode_is_special(instruction) ? instruction : (instruction & 0x1f)];
}

structdef(Vm_Stack) { u8 memory[0x100], data; };
// TODO(felix): set stack_return to 0x40 and avoid shifting later
enumdef(Vm_Stack_Id, u8) { stack_param = 0, stack_return = 1 };
// TODO(felix): replace with numeric byte width 1, 2
enumdef(Vm_Opcode_Size, u8)  { vm_opcode_size_byte = 0, vm_opcode_size_short = 1 };
// TODO(felix): set keep to 0x20 and avoid shifting later
structdef(Vm_Instruction_Mode) { u8 keep; Vm_Stack_Id stack; Vm_Opcode_Size size; };

structdef(Vm_Instruction) { Vm_Opcode opcode; Vm_Instruction_Mode mode; };

structdef(Vm) {
    u8 memory[0x10000];
    Vm_Stack stacks[2];
    Vm_Stack_Id active_stack;
    u16 program_counter;
    Vm_Instruction_Mode current_mode;
    Gfx_Render_Context gfx;
};

#define vm_stack vm->stacks[vm->active_stack].memory
#define vm_sp vm->stacks[vm->active_stack].data

#define vm_s(data) vm_stack[(u8)(data)]
#define vm_mem(data) vm->memory[(u16)(data)]

static u8   vm_get8(Vm *vm, u8 i_back) { return *(vm_stack + (u8)(vm_sp - i_back)); }
static void vm_push8(Vm *vm, u8 byte) { vm_s(vm_sp) = byte; vm_sp += 1; }
static u8   vm_pop8(Vm *vm) { u8 val = vm_s(vm_sp - 1); if (!vm->current_mode.keep) vm_sp -= 1; return val; }
static u8   vm_load8(Vm *vm, u16 addr) { return vm_mem(addr); }
static void vm_store8(Vm *vm, u16 addr, u8 val) { vm_mem(addr) = val; }

static u16  vm_get16(Vm *vm, u8 i_back) { return (u16)vm_s(vm_sp - 2 * i_back + 1) | (u16)(vm_s(vm_sp - 2 * i_back) << 8); }
static void vm_push16(Vm *vm, u16 byte2) { vm_push8(vm, byte2 >> 8); vm_push8(vm, (u8)byte2); }
static u16  vm_pop16(Vm *vm) { u16 val = (u16)vm_s(vm_sp - 1) | (u16)(vm_s(vm_sp - 2) << 8); if (!vm->current_mode.keep) vm_sp -= 2; return val; }
static u16  vm_load16(Vm *vm, u16 addr) { return (u16)(((u16)vm_mem(addr) << 8) | (u16)vm_mem(addr + 1)); }
static void vm_store16(Vm *vm, u16 addr, u16 val) { vm_mem(addr) = (u8)(val >> 8); vm_mem(addr + 1) = (u8)val; } // TODO: ensure correct

static void vm_run_to_break(Vm *vm, u16 program_counter) {
    vm->program_counter = program_counter;
    u16 add = 1;
    for (; true; vm->program_counter += add) {
        add = 1;
        u8 byte = vm_mem(vm->program_counter);

        Vm_Instruction instruction = { .opcode = byte & 0x1f, .mode = {
            .keep =  (byte & 0x80) >> 7,
            .stack = (byte & 0x40) >> 6,
            .size =  (byte & 0x20) >> 5,
        } };

        vm->current_mode = instruction.mode;

        vm->active_stack = instruction.mode.stack;

        switch (byte) {
            case vm_opcode_break: return;
            case vm_opcode_jmi: vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; continue;
            case vm_opcode_jei: if ( vm_pop8(vm)) { vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; } else add = 3; continue;
            case vm_opcode_jni: if (!vm_pop8(vm)) { vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; } else add = 3; continue;
            case vm_opcode_jsi: {
                vm->active_stack = stack_return;
                vm_push16(vm, vm->program_counter + 3);
                vm->active_stack = stack_param;
                vm->program_counter = vm_load16(vm, vm->program_counter + 1);
                add = 0;
                continue;
            } break;
            default: break;
        }

        switch (instruction.mode.size) {
            case vm_opcode_size_byte: switch (instruction.opcode) { vm_opcode_cases(1, 8) } break;
            case vm_opcode_size_short: switch (instruction.opcode) { vm_opcode_cases(2, 16) } break;
            default: panic("unreachable %{#%}", fmt(cstring, (char *)vm_opcode_name(byte)), fmt(u64, byte, .base = 16));
        }
    }
}

static bool instruction_takes_immediate(Vm_Instruction instruction) {
    #define for_opcode_return_is_immediate(name, value, is_immediate) case vm_opcode_##name: return is_immediate;
    switch (instruction.opcode) {
        vm_for_opcode(for_opcode_return_is_immediate)
        default: unreachable;
    }
}

static u8 byte_from_instruction(Vm_Instruction instruction) {
    if (vm_opcode_is_special(instruction.opcode)) return instruction.opcode;
    u8 byte = (u8)(instruction.opcode
        | (instruction.mode.keep << 7)
        | (instruction.mode.stack << 6)
        | (instruction.mode.size << 5));
    return byte;
}

static void write_u16_swap_bytes(Array_u8 rom, usize index, u16 value) {
    assert(index + 1 < rom.capacity);
    u8 byte2 = (u8)(value);
    u8 byte1 = (u8)(value >> 8);
    rom.data[index] = byte1;
    rom.data[index + 1] = byte2;
}

static force_inline bool is_starting_symbol_character(u8 c) {
    return ascii_is_alpha(c) || c == '_';
}

enumdef(Token_Kind, u8) {
    token_kind_ascii_delimiter = 128,

    token_kind_symbol,
    token_kind_hexadecimal,
    token_kind_string,
    token_kind_opmode,

    token_kind_count,
};

typedef u8 File_Id;
#define max_file_id 255

structdef(Token) {
    u32 start_index;
    u8 length;
    Token_Kind kind;
    u16 value;
};

structdef(Token_Id) { File_Id file_id; i32 index; };

structdef(Label) {
    Token_Id token_id;
    u16 address;
    Array_Label children;
};

typedef u32 Label_Id;

structdef(Label_Reference) {
    Token_Id token_id;
    union { u16 destination_address; Token_Id destination_label_token_id; };
    u8 width;
    bool is_patch;
    Label_Id parent_id;
};

structdef(Scoped_Reference) {
    u16 address;
    Token_Id token_id;
    bool is_relative;
};

structdef(Insertion_Mode) { bool is_active, has_relative_reference; };

structdef(Input_File) { String name, bytes; Array_Token tokens; };

structdef(Assembler_Context) {
    Array_Input_File files;
    Array_Label labels;
    Array_Label_Reference label_references;
    Array_Scoped_Reference scoped_references;
    Insertion_Mode insertion_mode;
};

static Token_Id token_id_shift(Token_Id token_id, i32 shift) {
    Token_Id result = token_id;
    result.index += shift;
    return result;
}

static Input_File *token_get_file(Assembler_Context *context, Token_Id token_id) {
    File_Id file_id = token_id.file_id;
    Input_File *file = &context->files.data[file_id];
    return file;
}

static Token *token_get(Assembler_Context *context, Token_Id token_id) {
    Input_File file = *token_get_file(context, token_id);
    i32 index = token_id.index;
    if (index < 0 || (usize)index >= file.tokens.count) {
        static Token nil_token = {0};
        return &nil_token;
    }
    Token *token = &file.tokens.data[index];
    return token;
}

static String token_lexeme(Assembler_Context *context, Token_Id token_id) {
    Token token = *token_get(context, token_id);
    Input_File *file = token_get_file(context, token_id);
    String asm = file->bytes;
    String token_string = string_range(asm, token.start_index, token.start_index + token.length);
    return token_string;
}

static int parse_error(Assembler_Context *context, File_Id file_id, u32 token_start_index, u8 token_length) {
    String text = context->files.data[file_id].bytes;
    String file_name = context->files.data[file_id].name;
    // TODO(felix): print line and column, and highlight error in line
    String lexeme = string_range(text, token_start_index, token_start_index + token_length);
    print("note: '%' in file '%'[%..%]\n", fmt(String, lexeme), fmt(String, file_name), fmt(usize, token_start_index), fmt(usize, token_start_index + token_length));
    return 1;
}

#define usage "usage: bici <com|run|script> <file...>"

int main(int argc, char **argv) {
    if (argc < 3) {
        log_error("%", fmt(cstring, usage));
        return 1;
    }

    enumdef(Command, u8) {
        command_compile, command_run, command_script,
        command_count,
    };

    String command_string = string_from_cstring(argv[1]);

    Command command = 0;
    if (string_equal(command_string, string("com"))) {
        if (argc != 4) {
            log_error("usage: bici com <file.asm> <file.rom>");
            return 1;
        }
        command = command_compile;
    } else if (string_equal(command_string, string("run"))) {
        if (argc != 3) {
            log_error("usage: bici run <file.rom>");
            return 1;
        }
        command = command_run;
    } else if (string_equal(command_string, string("script"))) {
        if (argc != 3) {
            log_error("usage: bici script <file.asm>");
            return 1;
        }
        command = command_script;
    } else {
        log_error("no such command '%'\n%", fmt(String, command_string), fmt(cstring, usage));
        return 1;
    }

    Arena arena = arena_init(8 * 1024 * 1024);

    char *input_filepath = argv[2];
    usize max_filesize = 0x10000;
    String input_bytes = file_read_bytes_relative_path(&arena, input_filepath, max_filesize);

    Array_u8 rom = { .data = (u8[0x10000]){0}, .capacity = 0x10000 };

    bool should_compile = command == command_compile || command == command_script;
    if (!should_compile) rom = (Array_u8)array_from_slice(input_bytes);
    else {
        Assembler_Context context = {
            .files = { .arena = &arena },
            .labels = { .arena = &arena },
            .label_references = { .arena = &arena },
            .scoped_references = { .arena = &arena },
        };

        Label nil_label = { .children.arena = &arena };
        array_push(&context.labels, &nil_label);

        Input_File main_file = { .bytes = input_bytes, .name = string_from_cstring(input_filepath), .tokens = { .arena = &arena } };
        array_push(&context.files, &main_file);

        structdef(File_Cursor) { Token_Id token_id; u32 asm_cursor; };
        Array_File_Cursor file_stack = { .arena = &arena };
        File_Cursor main_file_cursor = {0};
        array_push(&file_stack, &main_file_cursor);

        while (file_stack.count > 0) {
            File_Cursor file_cursor = slice_pop_assume_not_empty(file_stack);
            File_Id file_id = file_cursor.token_id.file_id;
            Input_File *file = &context.files.data[file_id];
            String asm = file->bytes;

            for (u32 asm_cursor = file_cursor.asm_cursor; asm_cursor < asm.count;) {
                while (asm_cursor < asm.count && ascii_is_whitespace(asm.data[asm_cursor])) asm_cursor += 1;
                if (asm_cursor == asm.count) break;

                u32 start_index = asm_cursor;
                Token_Kind kind = 0;
                if (is_starting_symbol_character(asm.data[asm_cursor])) {
                    kind = token_kind_symbol;
                    asm_cursor += 1;

                    for (; asm_cursor < asm.count; asm_cursor += 1) {
                        u8 c = asm.data[asm_cursor];
                        bool is_symbol_character = is_starting_symbol_character(c) || ascii_is_decimal(c);
                        if (!is_symbol_character) break;
                    }
                } else switch (asm.data[asm_cursor]) {
                    case '0': {
                        kind = token_kind_hexadecimal;

                        if (asm_cursor + 1 == asm.count || asm.data[asm_cursor + 1] != 'x') {
                            log_error("expected 'x' after '0' to begin hexadecimal literal");
                            return parse_error(&context, file_id, asm_cursor + 1, 1);
                        }
                        asm_cursor += 2;
                        start_index = asm_cursor;

                        if (asm_cursor == asm.count) {
                            log_error("expected hexadecimal digits following '0x'");
                            return parse_error(&context, file_id, 0, 0);
                        }

                        for (; asm_cursor < asm.count; asm_cursor += 1) {
                            u8 c = asm.data[asm_cursor];
                            if (ascii_is_hexadecimal(c)) continue;
                            if (ascii_is_whitespace(c)) break;
                            log_error("expected hexadecimal digits here");
                            return parse_error(&context, file_id, asm_cursor, 1);
                        }
                    } break;
                    case ';': {
                        while (asm_cursor < asm.count && asm.data[asm_cursor] != '\n') asm_cursor += 1;
                        asm_cursor += 1;
                        continue;
                    } break;
                    case ':': case '$': case '{': case '}': case '[': case ']': case ',': case '/': {
                        kind = asm.data[asm_cursor];
                        asm_cursor += 1;
                    } break;
                    case '"': {
                        kind = token_kind_string;

                        asm_cursor += 1;
                        start_index = asm_cursor;
                        for (; asm_cursor < asm.count; asm_cursor += 1) {
                            u8 c = asm.data[asm_cursor];
                            if (c == '"') break;
                            if (c == '\n') {
                                log_error("expected '\"' to close string before newline");
                                return parse_error(&context, file_id, start_index, (u8)(asm_cursor - start_index));
                            }
                        }

                        if (asm_cursor == asm.count) {
                            log_error("expected '\"' to close string before end of file");
                            return parse_error(&context, file_id, start_index, (u8)(asm_cursor - start_index));
                        }

                        assert(asm.data[asm_cursor] == '"');
                        asm_cursor += 1;
                    } break;
                    case '.': {
                        kind = token_kind_opmode;

                        asm_cursor += 1;
                        start_index = asm_cursor;
                        for (; asm_cursor < asm.count; asm_cursor += 1) {
                            u8 c = asm.data[asm_cursor];
                            if (ascii_is_whitespace(c)) break;
                            if (c != '2' && c != 'k' && c != 'r') {
                                log_error("only characters '2', 'k', and 'r' are valid op modes");
                                return parse_error(&context, file_id, asm_cursor, 1);
                            }
                        }

                        usize length = asm_cursor - start_index;
                        if (length == 0 || length > 3) {
                            log_error("a valid op mode contains 1, 2, or 3 characters (as in op.2kr), but this one has %", fmt(usize, length));
                            return parse_error(&context, file_id, start_index, (u8)length);
                        }
                    } break;
                    default: {
                        log_error("invalid syntax '%'", fmt(char, asm.data[asm_cursor]));
                        return parse_error(&context, file_id, asm_cursor, 1);
                    }
                }

                usize length = asm_cursor - start_index;
                if (kind == token_kind_string) length -= 1;
                assert(length <= 255);

                Token token = {
                    .start_index = start_index,
                    .length = (u8)length,
                    .kind = kind,
                };
                array_push(&file->tokens, &token);
            }

            bool parsing_relative_label = false;
            for (Token_Id file_token_id = file_cursor.token_id; (usize)file_token_id.index < file->tokens.count; file_token_id.index += 1) {
                Token token = *token_get(&context, file_token_id);
                String token_string = token_lexeme(&context, file_token_id);

                if (context.insertion_mode.is_active) {
                    switch (token.kind) {
                        case token_kind_symbol: case ']': case '$': case token_kind_hexadecimal: case token_kind_string: break;
                        default: {
                            log_error("insertion mode only supports addresses (e.g. label) and numeric literals");
                            return parse_error(&context, file_id, token.start_index, token.length);
                        }
                    };
                }

                Token_Id next_id = token_id_shift(file_token_id, 1);
                Token next = *token_get(&context, next_id);
                String next_string = token_lexeme(&context, next_id);

                switch (token.kind) {
                    case '{': {
                        Scoped_Reference reference = { .address = (u16)rom.count };
                        array_push(&context.scoped_references, &reference);
                        rom.count += 2;
                    } break;
                    case '}': {
                        if (context.scoped_references.count == 0) {
                            log_error("'}' has no matching '{'");
                            return parse_error(&context, file_id, token.start_index, token.length);
                        }

                        Scoped_Reference reference = slice_pop_assume_not_empty(context.scoped_references);
                        if (reference.is_relative) {
                            log_error("expected to resolve absolute reference (from '{') but found unresolved relative reference");
                            return parse_error(&context, file_id, token.start_index, token.length);
                        }

                        write_u16_swap_bytes(rom, reference.address, (u16)rom.count);
                    } break;
                    case '[': {
                        if (context.insertion_mode.is_active) {
                            log_error("cannot enter insertion mode while already in insertion mode");
                            return parse_error(&context, file_id, token.start_index, token.length);
                        }
                        context.insertion_mode.is_active = true;

                        bool compile_byte_count_to_closing_bracket = next.kind == '$';
                        if (compile_byte_count_to_closing_bracket) {
                            context.insertion_mode.has_relative_reference = true;
                            Scoped_Reference reference = { .address = (u16)rom.count, .is_relative = true };
                            array_push(&context.scoped_references, &reference);
                            rom.count += 1;
                            file_token_id = token_id_shift(file_token_id, 1);
                        }
                    } break;
                    case ']': {
                        if (!context.insertion_mode.is_active) {
                            log_error("']' has no matching '['");
                            return parse_error(&context, file_id, token.start_index, token.length);
                        }

                        if (context.insertion_mode.has_relative_reference) {
                            Scoped_Reference reference = slice_pop_assume_not_empty(context.scoped_references);
                            if (!reference.is_relative) {
                                log_error("expected to resolve relative reference (from '[$') but found unresolved absolute reference; is there an unmatched '{'?");
                                return parse_error(&context, file_id, token.start_index, token.length);
                            }

                            usize relative_difference = rom.count - reference.address - 1;
                            if (relative_difference > 255) {
                                log_error("relative difference from '[$' is % bytes, but the maximum is 255", fmt(usize, relative_difference));
                                return parse_error(&context, file_id, token.start_index, token.length);
                            }

                            u8 *location_to_patch = &rom.data[reference.address];
                            *location_to_patch = (u8)relative_difference;
                        }

                        context.insertion_mode = (Insertion_Mode){0};
                    } break;
                    case '/': {
                        if (next.kind != token_kind_symbol) {
                            log_error("expected relative label following '/'");
                            return parse_error(&context, file_id, next.start_index, next.length);
                        }
                        parsing_relative_label = true;
                    } break;
                    case token_kind_symbol: {
                        if (parsing_relative_label) {
                            if (next.kind != ':') {
                                log_error("expected ':' ending relative label definition");
                                return parse_error(&context, file_id, next.start_index, next.length);
                            }
                        }

                        bool is_label = next.kind == ':';
                        if (is_label) {
                            for_slice (Label *, label, context.labels) {
                                String label_string = token_lexeme(&context, label->token_id);
                                if (string_equal(label_string, token_string)) {
                                    log_error("redefinition of label '%'", fmt(String, token_string));
                                    return parse_error(&context, file_id, token.start_index, token.length);
                                }
                            }

                            Label new_label = { .token_id = file_token_id, .address = (u16)rom.count, .children.arena = &arena };

                            Array_Label *labels = 0;
                            if (parsing_relative_label) {
                                Label *parent = &slice_get_last_assume_not_empty(context.labels);
                                Array_Label *parent_labels = &parent->children;
                                labels = parent_labels;

                                for_slice (Label *, label, *parent_labels) {
                                    String label_string = token_lexeme(&context, label->token_id);
                                    if (string_equal(label_string, token_string)) {
                                        log_error("redefinition of label '%'", fmt(String, token_string));
                                        return parse_error(&context, file_id, token.start_index, token.length);
                                    }
                                }

                                parsing_relative_label = false;
                            } else {
                                labels = &context.labels;
                            }

                            array_push(labels, &new_label);

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        }

                        Vm_Instruction instruction = {0};
                        bool is_opcode = false;
                        #define for_opcode_test_string(name, byte, ...) else if (string_equal(token_string, string(#name))) { instruction.opcode = byte; is_opcode = true; }
                        if (false) {}
                        vm_for_opcode(for_opcode_test_string)
                        if (is_opcode) {
                            bool explicit_mode = next.kind == token_kind_opmode;
                            if (explicit_mode) {
                                for_slice (u8 *, c, next_string) {
                                    switch (*c) {
                                        case '2': instruction.mode.size = vm_opcode_size_short; break;
                                        case 'k': instruction.mode.keep = 0x80; break;
                                        case 'r': instruction.mode.stack = stack_return; break;
                                        default: unreachable;
                                    }
                                }
                                file_token_id = token_id_shift(file_token_id, 1);
                            }

                            u8 byte = byte_from_instruction(instruction);
                            rom.data[rom.count] = byte;
                            rom.count += 1;

                            if (instruction_takes_immediate(instruction)) {
                                next = *token_get(&context, token_id_shift(file_token_id, 1));
                                switch (next.kind) {
                                    case token_kind_symbol: case token_kind_hexadecimal: case '{': break;
                                    default: {
                                        log_error("instruction '%' takes an immediate, but no label or numeric literal is given", fmt(cstring, (char *)vm_opcode_name(instruction.opcode)));
                                        return parse_error(&context, file_id, next.start_index, next.length);
                                    }
                                }

                                if (next.kind == '{') continue;

                                u8 width = 1;
                                if (instruction.mode.size == vm_opcode_size_short || vm_opcode_is_special(instruction.opcode)) width = 2;

                                if (next.kind == token_kind_symbol) {
                                    Label_Reference reference = {
                                        .token_id = token_id_shift(file_token_id, 1),
                                        .destination_address = (u16)rom.count,
                                        .width = width,
                                        .parent_id = (Label_Id)(context.labels.count - 1),
                                    };
                                    array_push(&context.label_references, &reference);
                                    rom.count += reference.width;
                                } else {
                                    assert(next.kind == token_kind_hexadecimal);
                                    next_string = token_lexeme(&context, token_id_shift(file_token_id, 1));

                                    // TODO(felix): bounds check
                                    u16 value = (u16)int_from_hex_string(next_string);

                                    if (width == 1) {
                                        if (value > 255) {
                                            log_error("attempt to supply 16-bit value to instruction taking 8-bit immediate (% is greater than 255); did you mean to use mode '.2'?", fmt(u16, value));
                                            return parse_error(&context, file_id, next.start_index, next.length);
                                        }
                                        rom.data[rom.count] = (u8)value;
                                        rom.count += 1;
                                    } else {
                                        assert(width == 2);
                                        write_u16_swap_bytes(rom, rom.count, value);
                                        rom.count += 2;
                                    }
                                }
                                file_token_id = token_id_shift(file_token_id, 1);
                            }

                            break;
                        }

                        if (string_equal(token_string, string("org"))) {
                            if (next.kind != token_kind_hexadecimal) {
                                log_error("expected numeric literal (byte offset) after directive 'org'");
                                return parse_error(&context, file_id, next.start_index, next.length);
                            }
                            // TODO(felix): bounds check
                            u16 value = (u16)int_from_hex_string(next_string);
                            rom.count = value;

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        } else if (string_equal(token_string, string("patch"))) {
                            if (next.kind != token_kind_symbol) {
                                log_error("expected label to indicate destination offset as first argument to directive 'patch'");
                                return parse_error(&context, file_id, next.start_index, next.length);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            Label_Reference reference = { .is_patch = true, .destination_label_token_id = file_token_id, .parent_id = (Label_Id)(context.labels.count - 1) };

                            next = *token_get(&context, token_id_shift(file_token_id, 1));
                            if (next.kind != ',') {
                                log_error("expected ',' after between first and second arguments to directive 'patch'");
                                return parse_error(&context, file_id, next.start_index, next.length);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            next = *token_get(&context, token_id_shift(file_token_id, 1));
                            if (next.kind != token_kind_symbol) {
                                log_error("expected label to indicate address as second argument to directive 'patch'");
                                return parse_error(&context, file_id, next.start_index, next.length);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            reference.token_id = file_token_id;
                            array_push(&context.label_references, &reference);
                        } else if (string_equal(token_string, string("include"))) {
                            if (next.kind != token_kind_string) {
                                log_error("expected string following directive 'include'");
                                return parse_error(&context, file_id, next.start_index, next.length);
                            }

                            char *include_filepath = cstring_from_string(&arena, next_string);
                            if (string_equal(string_from_cstring(include_filepath), file->name)) {
                                log_error("cyclic inclusion of file '%'", fmt(String, file->name));
                                return parse_error(&context, file_id, next.start_index, next.length);
                            }
                            String include_bytes = file_read_bytes_relative_path(&arena, include_filepath, 0x10000);
                            if (include_bytes.count == 0) return 1;

                            Input_File new_file = { .bytes = include_bytes, .name = string_from_cstring(include_filepath), .tokens = { .arena = &arena } };
                            File_Id new_file_id = (File_Id)context.files.count;
                            array_push(&context.files, &new_file);

                            File_Cursor current = { .asm_cursor = (u32)asm.count, .token_id = token_id_shift(file_token_id, 2) };
                            array_push(&file_stack, &current);

                            File_Cursor next_file = { .asm_cursor = 0, .token_id = { .file_id = new_file_id } };
                            array_push(&file_stack, &next_file);
                            goto switch_file;
                        }

                        Label_Reference reference = { .token_id = file_token_id, .destination_address = (u16)rom.count, .width = 2, .parent_id = (Label_Id)(context.labels.count - 1) };
                        array_push(&context.label_references, &reference);
                        rom.count += reference.width;
                    } break;
                    case token_kind_hexadecimal: {
                        if (!context.insertion_mode.is_active) {
                            log_error("standalone hexadecimal literals are only supported in insertion mode (e.g. in [ ... ])");
                            return parse_error(&context, file_id, token.start_index, token.length);
                        }

                        // TODO(felix): bounds check
                        u16 value = (u16)int_from_hex_string(token_string);

                        bool is_byte = token_string.count <= 2;
                        if (is_byte) {
                            assert(value < 256);
                            rom.data[rom.count] = (u8)value;
                            rom.count += 1;
                            break;
                        }

                        bool is_short = true;
                        if (is_short) {
                            write_u16_swap_bytes(rom, rom.count, value);
                            rom.count += 2;
                            break;
                        }

                        unreachable;
                    } break;
                    case token_kind_string: {
                        if (!context.insertion_mode.is_active) {
                            log_error("strings can only be used in insertion mode (e.g. inside [ ... ])");
                            return parse_error(&context, file_id, token.start_index, token.length);
                        }

                        assert(rom.count + token_string.count <= 0x10000);
                        memcpy(rom.data + rom.count, token_string.data, token_string.count);
                        rom.count += token_string.count;
                    } break;
                    default: {
                        log_error("invalid syntax");
                        return parse_error(&context, file_id, token.start_index, token.length);
                    }
                }
            }

            if (context.scoped_references.count != 0) {
                Scoped_Reference reference = context.scoped_references.data[0];
                Token token = *token_get(&context, reference.token_id);
                log_error("anonymous reference ('{') without matching '}'");
                return parse_error(&context, file_id, token.start_index, token.length);
            }

            switch_file:;
        }

        for_slice (Label_Reference *, reference, context.label_references) {
            Label *source_label = 0, *destination_label = 0;
            Label **to_match[] = { &source_label, &destination_label };
            Token_Id match_against[] = { reference->token_id, reference->destination_label_token_id };
            usize match_count = reference->is_patch ? 2 : 1;

            for (usize i = 0; i < match_count; i += 1) {
                Label **match = to_match[i];

                Token_Id token_id = match_against[i];
                Token token = *token_get(&context, token_id);

                String reference_string = token_lexeme(&context, token_id);

                for_slice (Label *, label, context.labels) {
                    String label_string = token_lexeme(&context, label->token_id);
                    if (!string_equal(reference_string, label_string)) continue;
                    *match = label;
                    goto done;
                }

                Label parent = context.labels.data[reference->parent_id];
                for_slice (Label *, label, parent.children) {
                    String label_string = token_lexeme(&context, label->token_id);
                    if (!string_equal(reference_string, label_string)) continue;
                    *match = label;
                    goto done;
                }

                done:
                if (*match == 0) {
                    log_error("no such label '%'", fmt(String, reference_string));
                    return parse_error(&context, token_id.file_id, token.start_index, token.length);
                }
            }

            if (reference->is_patch) {
                write_u16_swap_bytes(rom, destination_label->address, source_label->address);
            } else switch (reference->width) {
                case 1: {
                    u8 *location_to_patch = &rom.data[reference->destination_address];
                    assert(source_label->address <= 255);
                    *location_to_patch = (u8)source_label->address;
                } break;
                case 2: {
                    write_u16_swap_bytes(rom, reference->destination_address, source_label->address);
                } break;
                default: unreachable;
            }
        }

        if (command == command_compile) {
            assert(argc == 4);
            char *output_path = argv[3];
            file_write_bytes_to_relative_path(output_path, bit_cast(String) rom);
        }
    }

    bool should_run = command == command_script || command == command_run;
    if (should_run) {
        if (rom.count == 0) return 1;

        Vm vm = {0};
        memcpy(vm.memory, rom.data, rom.count);

        Arena *persistent_arena = &arena;
        Arena_Temp temp = arena_temp_begin(persistent_arena);
        String_Builder builder = { .arena = persistent_arena };
        {
            string_builder_print(&builder, "MEMORY ===\n");
            for (u16 token_id = 0x100; token_id < rom.count; token_id += 1) {
                u8 byte = vm.memory[token_id];

                String mode_string = string("");
                if (!vm_opcode_is_special(byte)) switch ((byte & 0xe0) >> 5) {
                    case 0x1: mode_string = string("2"); break;
                    case 0x2: mode_string = string("r"); break;
                    case 0x3: mode_string = string("r2"); break;
                    case 0x4: mode_string = string("k"); break;
                    case 0x5: mode_string = string("k2"); break;
                    case 0x6: mode_string = string("kr"); break;
                    case 0x7: mode_string = string("kr2"); break;
                }

                string_builder_print(&builder, "[%]\t'%'\t#%\t%;%\n",
                    fmt(u16, token_id, .base = 16), fmt(char, byte), fmt(u8, byte, .base = 16), fmt(cstring, (char *)vm_opcode_name(byte)), fmt(String, mode_string)
                );
            }
            string_builder_print(&builder, "\nRUN ===\n");
        }
        os_write(bit_cast(String) builder);
        arena_temp_end(temp);

        Gfx_Render_Context *gfx = &vm.gfx;
        *gfx = gfx_window_create(persistent_arena, "bici", vm_screen_width, vm_screen_height);
        gfx->font = gfx_font_default_3x5;

        // TODO(felix): program should control this
        gfx_clear(gfx, 0);
        u16 vm_on_screen_init_pc = vm_load16(&vm, vm_device_screen);
        vm_run_to_break(&vm, vm_on_screen_init_pc);

        u16 vm_on_screen_update_pc = vm_load16(&vm, vm_device_screen | vm_screen_update);

        while (!gfx_window_should_close(gfx)) {
            vm_run_to_break(&vm, vm_on_screen_update_pc);
        }

        u16 vm_on_screen_quit_pc = vm_load16(&vm, vm_device_screen | vm_screen_quit);
        vm_run_to_break(&vm, vm_on_screen_quit_pc);

        print("param  stack (bot->top): { ");
        for (u8 token_id = 0; token_id < vm.stacks[stack_param].data; token_id += 1) print("% ", fmt(u64, vm.stacks[stack_param].memory[token_id], .base = 16));
        print("}\nreturn stack (bot->top): { ");
        for (u8 token_id = 0; token_id < vm.stacks[stack_return].data; token_id += 1) print("% ", fmt(u64, vm.stacks[stack_return].memory[token_id], .base = 16));
        print("}\n");
    }

    arena_deinit(&arena);
    return 0;
}
