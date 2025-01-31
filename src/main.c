#define BASE_GRAPHICS 0
#include "base/base.c"

#include "raylib.h"

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

#define vm_screen_width 320
#define vm_screen_height 240

#define vm_for_opcode(action)\
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
    case vm_opcode_jeq:    { u16 addr = vm_pop16(vm); if (vm_pop##bi(vm)) { vm->program_counter = addr; add = 0; } } break;\
    case vm_opcode_jst:    {\
        vm->active_stack = stack_ret;\
        vm_push16(vm, vm->program_counter + 1);\
        vm->active_stack = stack_param;\
        vm->program_counter = vm_pop16(vm);\
        add = 0;\
    } break;\
    case vm_opcode_stash:  { u##bi val = vm_pop##bi(vm); vm->active_stack = stack_ret; vm_push##bi(vm, val); vm->active_stack = stack_param; } break;\
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
                    /* TODO(felix): check if this is print from SDL */ print("%", fmt(String, str));\
                } break;\
                default: panic("[write] invalid action #% for console device", fmt(u64, action, .base = 16));\
            } break;\
            case vm_device_screen: switch (action) {\
                case vm_screen_pixel: {\
                    Screen_Colour colour = vm_pop8(vm);\
                    if (colour >= screen_colour_count) panic("[write:screen/pixel] colour #% is invalid; there are only #% palette colours", fmt(u64, colour, .base = 16), fmt(u64, screen_colour_count));\
                    u16 y = vm_pop16(vm), x = vm_pop16(vm);\
                    if (x >= vm_screen_width || y >= vm_screen_height) panic("[write:screen/pixel] coordinate #%x#% is outside screen bounds #%x#%", fmt(u64, x, .base = 16), fmt(u64, y, .base = 16), fmt(u64, vm_screen_width, .base = 16), fmt(u64, vm_screen_height, .base = 16));\
                    vm->screen_pixels[y * vm_screen_width + x] = screen_palette[colour];\
                } break;\
                default: panic("[write] invalid action #% for screen device", fmt(u64, action, .base = 16));\
            } break;\
            default: panic("invalid device #% for operation 'write'", fmt(u64, device, .base = 16));\
        }\
    } break;\
    default: panic("unreachable %{#%}", fmt(cstring, (char *)vm_opcode_name(byte)), fmt(u64, byte, .base = 16));

enumdef(Vm_Opcode, u8) {
    #define vm_opcode_def_enum(name, val) vm_opcode_##name = val,
    vm_for_opcode(vm_opcode_def_enum)
    vm_opcode_max_value_plus_one
};

const char *vm_opcode_name_table[vm_opcode_max_value_plus_one] = {
    #define vm_opcode_set_name(name, val) [val] = #name,
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
enumdef(Vm_Stack_Id, u8) { stack_param = 0, stack_ret = 1 };
enumdef(Vm_Opcode_Size, u8)  { vm_opcode_size_byte = 0, vm_opcode_size_short = 1 };
structdef(Vm_Instruction_Mode) { b8 keep; Vm_Stack_Id stack; Vm_Opcode_Size size; };

structdef(Vm_Instruction) { Vm_Opcode opcode; Vm_Instruction_Mode mode; };

structdef(Vm) {
    u8 memory[0x10000];
    Vm_Stack stacks[2];
    Vm_Stack_Id active_stack;
    u16 program_counter;
    Vm_Instruction_Mode current_mode;
    Texture screen_texture;
    u32 screen_pixels[vm_screen_width * vm_screen_height];
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
                vm->active_stack = stack_ret;
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

static void vm_run(String rom) {
    if (rom.count == 0) return;

    Vm vm = {0};
    memcpy(vm.memory, rom.data, rom.count);

    print("MEMORY ===\n");
    for (u16 i = 0x100; i < rom.count; i += 1) {
        u8 byte = vm.memory[i];

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

        print("[%]\t'%'\t#%\t%;%\n", fmt(u64, i, .base = 16), fmt(char, byte), fmt(u64, byte, .base = 16), fmt(cstring, (char *)vm_opcode_name(byte)), fmt(String, mode_string));
    }
    print("\nRUN ===\n");

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);

    InitWindow(vm_screen_width, vm_screen_height, "bici");
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
    MaximizeWindow();

    Image screen_image = {
        .data = vm.screen_pixels,
        .width = vm_screen_width,
        .height = vm_screen_height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1,
    };

    vm.screen_texture = LoadTextureFromImage(screen_image);

    u16 vm_on_screen_init_pc = vm_load16(&vm, vm_device_screen);
    vm_run_to_break(&vm, vm_on_screen_init_pc);

    u16 vm_on_screen_update_pc = vm_load16(&vm, vm_device_screen | vm_screen_update);

    while (!WindowShouldClose()) {
        vm_run_to_break(&vm, vm_on_screen_update_pc);
        UpdateTexture(vm.screen_texture, vm.screen_pixels);

        BeginDrawing();
        ClearBackground((Color){ .a = 255 });
        {
            Rectangle texture_area = { .width = vm_screen_width, .height = vm_screen_height };
            Rectangle window_area = { .width = (f32)GetScreenWidth(), .height = (f32)GetScreenHeight() };
            Color draw_mask = { 255, 255, 255, 255 };
            DrawTexturePro(vm.screen_texture, texture_area, window_area, (Vector2){0}, 0, draw_mask);
        }
        EndDrawing();
    }

    u16 vm_on_screen_quit_pc = vm_load16(&vm, vm_device_screen | vm_screen_quit);
    vm_run_to_break(&vm, vm_on_screen_quit_pc);

    UnloadTexture(vm.screen_texture);
    CloseWindow();

    print("param  stack (bot->top): { ");
    for (u8 i = 0; i < vm.stacks[stack_param].data; i += 1) print("% ", fmt(u64, vm.stacks[stack_param].memory[i], .base = 16));
    print("}\nreturn stack (bot->top): { ");
    for (u8 i = 0; i < vm.stacks[stack_ret].data; i += 1) print("% ", fmt(u64, vm.stacks[stack_ret].memory[i], .base = 16));
    print("}\n");
}

structdef(Asm_Label_Definition) { String name; u16 address; };

structdef(Asm_Label_Usage) { String name; u16 index_in_input_bytes; Vm_Opcode_Size size; };

enumdef(Asm_Resolution_Kind, u8) { resolve_byte_count, resolve_address };
structdef(Asm_Resolution) { u16 index_in_output_bytes; Asm_Resolution_Kind kind; };

structdef(Asm) {
    char *input_relative_path;
    String input_bytes;
    u16 input_cursor;

    Array_Asm_Label_Definition label_definitions;
    Array_Asm_Label_Usage label_usages;
    Array_Asm_Resolution resolutions;

    Array_u8 output_bytes;
};

static u16 asm_get_address_of_label(Asm *context, String name) {
    for (u8 i = 0; i < context->label_definitions.count; i += 1) {
        if (!string_equal(context->label_definitions.data[i].name, name)) continue;
        return context->label_definitions.data[i].address;
    }
    err("no such label '%'", fmt(String, name));
    return 0;
}

structdef(Asm_Hex) {
    bool ok;
    u8 digit_count;
    union { u8 int8; u16 int16; } result;
};

static Asm_Hex asm_parse_hex(Asm *context) {
    u16 start_index = context->input_cursor;
    while (context->input_cursor < context->input_bytes.count && is_hex_digit[context->input_bytes.data[context->input_cursor]]) context->input_cursor += 1;
    u16 end_index = context->input_cursor;
    context->input_cursor -= 1;
    u8 digit_count = (u8)(end_index - start_index);
    String hex_string = { .data = context->input_bytes.data + start_index, .count = digit_count };

    switch (digit_count) {
        case 2: return (Asm_Hex){
            .ok = true,
            .digit_count = digit_count,
            .result.int8 = (u8)int_from_hex_string(hex_string),
        };
        case 4: return (Asm_Hex){
            .ok = true,
            .digit_count = digit_count,
            .result.int16 = (u16)int_from_hex_string(hex_string),
        };
        default: {
            err("expected 2 digits (byte) or 4 digits (short), found % digits in number at '%'[%..%]",
                fmt(u64, digit_count), fmt(cstring, context->input_relative_path), fmt(u64, start_index), fmt(u64, end_index)
            );
            return (Asm_Hex){0};
        } break;
    }
}

static bool asm_is_whitespace(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool asm_is_alpha(u8 c) { return ('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || (c == '_'); }

static String asm_parse_alpha(Asm *context) {
    u16 start_index = context->input_cursor;
    u8 first_char = context->input_bytes.data[context->input_cursor];
    if (!asm_is_alpha(first_char)) {
        err(
            "expected alphabetic character or underscore, found byte '%'/%/#% at '%'[%]",
            fmt(char, first_char), fmt(u64, first_char), fmt(u64, first_char, .base = 16), fmt(cstring, context->input_relative_path), fmt(u64, context->input_cursor)
        );
        return (String){0};
    }

    context->input_cursor += 1;
    while (context->input_cursor < context->input_bytes.count && (asm_is_alpha(context->input_bytes.data[context->input_cursor]))) context->input_cursor += 1;
    u16 end_index = context->input_cursor;
    context->input_cursor -= 1;
    return (String){ .data = context->input_bytes.data + start_index, .count = end_index - start_index };
}

static void asm_rom_write(Asm *context, u8 byte) {
    if (context->output_bytes.count == context->output_bytes.capacity) {
        err("reached maximum output_bytes size");
        abort();
    }
    array_push_assume_capacity(&context->output_bytes, &byte);
}

static void asm_rom_write2(Asm *context, u16 byte2) {
    asm_rom_write(context, (byte2 & 0xff00) >> 8);
    asm_rom_write(context, byte2 & 0x00ff);
}

static void asm_resolutions_push(Asm *context, Asm_Resolution_Kind resolution_kind) {
    if (context->resolutions.count == 0x0ff) {
        err("cannot resolve address at '%'[%]: maximum number of resolutions reached", fmt(cstring, context->input_relative_path), fmt(u64, context->input_cursor));
        context->output_bytes.count = 0;
        return;
    }
    Asm_Resolution resolution = { .index_in_output_bytes = (u16)context->output_bytes.count, .kind = resolution_kind };
    array_push_assume_capacity(&context->resolutions, &resolution);
}

static void asm_compile_instruction(Asm *context, Vm_Instruction_Mode mode, u8 opcode) {
    if (vm_opcode_is_special(opcode)) { asm_rom_write(context, opcode); return; }
    asm_rom_write(context, opcode | (u8)(mode.size << 5) | (u8)(mode.stack << 6) | (u8)(mode.keep << 7));
}

static String asm_compile(Arena *arena, usize max_asm_filesize, char *_path_biciasm) {
    static u8 rom_memory[0x10000];
    static Asm_Label_Definition label_definitions_memory[0x100];
    static Asm_Label_Usage label_usages_memory[0x100];
    static Asm_Resolution resolutions_memory[0x100];

    Asm context = {
        .input_relative_path = _path_biciasm,
        .input_bytes = file_read_bytes_relative_path(arena, _path_biciasm, max_asm_filesize),
        .label_definitions = { .data = label_definitions_memory, .capacity = 0x100 },
        .label_usages = { .data = label_usages_memory, .capacity = 0x100 },
        .resolutions = { .data = resolutions_memory, .capacity = 0x100 },
        .output_bytes = { .data = rom_memory, .count = 0x100, .capacity = 0x10000 },
    };
    if (context.input_bytes.count == 0) return (String){0};

    u16 cursor_step = 1;
    for (context.input_cursor = 0; context.input_cursor < context.input_bytes.count; context.input_cursor += cursor_step) {
        cursor_step = 1;
        if (asm_is_whitespace(context.input_bytes.data[context.input_cursor])) continue;

        Vm_Instruction_Mode mode = {0};

        switch (context.input_bytes.data[context.input_cursor]) {
            case '/': if (context.input_cursor + 1 < context.input_bytes.count && context.input_bytes.data[context.input_cursor + 1] == '/') {
                for (context.input_cursor += 2; context.input_cursor < context.input_bytes.count && context.input_bytes.data[context.input_cursor] != '\n';) context.input_cursor += 1;
            } continue;
            case '|': {
                context.input_cursor += 1;
                Asm_Hex new_program_counter = asm_parse_hex(&context);
                if (!new_program_counter.ok) return (String){0};
                context.output_bytes.count = new_program_counter.result.int16;
            } continue;
            case '@': {
                context.input_cursor += 1;
                String label_name = asm_parse_alpha(&context);
                Asm_Label_Usage label = { .name = label_name, .index_in_input_bytes = (u16)context.output_bytes.count, .size = vm_opcode_size_short };
                array_push_assume_capacity(&context.label_usages, &label);
                context.output_bytes.count += 2;
            } continue;
            case '#': {
                context.input_cursor += 1;
                Asm_Hex number = asm_parse_hex(&context);
                if (!number.ok) return (String){0};
                switch (number.digit_count) {
                    case 2: asm_compile_instruction(&context, (Vm_Instruction_Mode){ .size = vm_opcode_size_byte }, vm_opcode_push); asm_rom_write(&context, number.result.int8); break;
                    case 4: asm_compile_instruction(&context, (Vm_Instruction_Mode){ .size = vm_opcode_size_short }, vm_opcode_push); asm_rom_write2(&context, number.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '*': {
                asm_compile_instruction(&context, (Vm_Instruction_Mode){ .size = vm_opcode_size_byte }, vm_opcode_push);
                context.input_cursor += 1;
                String label_name = asm_parse_alpha(&context);
                Asm_Label_Usage label = { .name = label_name, .index_in_input_bytes = (u16)context.output_bytes.count, .size = vm_opcode_size_byte };
                array_push_assume_capacity(&context.label_usages, &label);
                context.output_bytes.count += 1;
            } continue;
            case '&': {
                asm_compile_instruction(&context, (Vm_Instruction_Mode){ .size = vm_opcode_size_short }, vm_opcode_push);
                context.input_cursor += 1;
                String label_name = asm_parse_alpha(&context);
                Asm_Label_Usage label = { .name = label_name, .index_in_input_bytes = (u16)context.output_bytes.count, .size = vm_opcode_size_short };
                array_push_assume_capacity(&context.label_usages, &label);
                context.output_bytes.count += 2;
            } continue;
            case ':': {
                context.input_cursor += 1;
                String label_name = asm_parse_alpha(&context);
                if (context.label_definitions.count == context.label_definitions.capacity) panic("cannot add label '%': maximum number of labels reached", fmt(String, label_name));
                Asm_Label_Definition label = { .name = label_name, .address = (u16)context.output_bytes.count };
                array_push_assume_capacity(&context.label_definitions, &label);
            } continue;
            case '{': {
                context.input_cursor += 1;
                switch (context.input_bytes.data[context.input_cursor]) {
                    case '#': {
                        asm_resolutions_push(&context, resolve_byte_count);
                        context.output_bytes.count += 1;
                    } continue;
                    default: {
                        asm_resolutions_push(&context, resolve_address);
                        context.output_bytes.count += 2;
                        cursor_step = 0;
                    } continue;
                }
            } continue;
            case '}': {
                if (context.resolutions.count == 0) {
                    err("forward resolution marker at '%'[%] matches no opening marker", fmt(cstring, context.input_relative_path), fmt(u64, context.input_cursor));
                    context.output_bytes.count = 0;
                    break;
                }

                Asm_Resolution resolution = slice_pop(context.resolutions);
                switch (resolution.kind) {
                    case resolve_byte_count: {
                        u8 byte_count = (u8)(context.output_bytes.count - 1 - resolution.index_in_output_bytes);
                        context.output_bytes.data[resolution.index_in_output_bytes] = byte_count;
                    } break;
                    case resolve_address: {
                        u16 current_address = (u16)context.output_bytes.count;
                        context.output_bytes.count = resolution.index_in_output_bytes;
                        asm_rom_write2(&context, current_address);
                        context.output_bytes.count = current_address;
                    } break;
                    default: unreachable;
                }

                continue;
            } break;
            case '_': {
                context.input_cursor += 1;
                Asm_Hex number = asm_parse_hex(&context);
                if (!number.ok) return (String){0};
                switch (number.digit_count) {
                    case 2: asm_rom_write(&context, number.result.int8); break;
                    case 4: asm_rom_write2(&context, number.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '"': {
                context.input_cursor += 1;
                for (; context.input_cursor < context.input_bytes.count && !asm_is_whitespace(context.input_bytes.data[context.input_cursor]); context.input_cursor += 1) {
                    asm_rom_write(&context, context.input_bytes.data[context.input_cursor]);
                }
                cursor_step = 0;
            } continue;
            default: break;
        }

        u8 first_char = context.input_bytes.data[context.input_cursor];
        if (!asm_is_alpha(first_char)) {
            err("invalid byte '%'/%/#% at '%'[%]",
                fmt(char, first_char), fmt(u64, first_char), fmt(u64, first_char, .base = 16), fmt(cstring, context.input_relative_path), fmt(u64, context.input_cursor)
            );
            return (String){0};
        }

        usize start_index = context.input_cursor, end_index = start_index;
        for (; context.input_cursor < context.input_bytes.count && !asm_is_whitespace(context.input_bytes.data[context.input_cursor]); context.input_cursor += 1) {
            u8 c = context.input_bytes.data[context.input_cursor];
            if ('a' <= c && c <= 'z') continue;
            end_index = context.input_cursor;
            if (c == ';') for (context.input_cursor += 1; context.input_cursor < context.input_bytes.count;) {
                c = context.input_bytes.data[context.input_cursor];
                if (asm_is_whitespace(c)) goto done;
                switch (c) {
                    case 'k': mode.keep = true; break;
                    case 'r': mode.stack = stack_ret; break;
                    case '2': mode.size = vm_opcode_size_short; break;
                    default: {
                        err("expected one of ['k', 'r', '2'], found byte '%'/%/#% at '%'[%]",
                            fmt(char, c), fmt(u64, c), fmt(u64, c, .base = 16), fmt(cstring, context.input_relative_path), fmt(u64, context.input_cursor)
                        );
                        return (String){0};
                    } break;
                }
                context.input_cursor += 1;
            }
        }
        done:
        if (end_index == start_index) end_index = context.input_cursor;
        String lexeme = string_range(context.input_bytes, start_index, end_index);

        if (false) {}
        #define compile_if_string_is_opcode(name, val)\
            else if (string_equal(lexeme, string(#name))) asm_compile_instruction(&context, mode, val);
        vm_for_opcode(compile_if_string_is_opcode)
        else {
            err("invalid token '%' at '%'[%..%]", fmt(String, lexeme), fmt(cstring, context.input_relative_path), fmt(u64, start_index), fmt(u64, end_index));
            return (String){0};
        };

        continue;
    }
    if (context.output_bytes.count == 0) return (String){0};
    String output_bytes = bit_cast(String) context.output_bytes;

    for (u8 i = 0; i < context.label_usages.count; i += 1) {
        Asm_Label_Usage ref = context.label_usages.data[i];
        if (ref.name.count == 0) return (String){0};
        context.output_bytes.count = ref.index_in_input_bytes;
        switch (ref.size) {
            case vm_opcode_size_byte: asm_rom_write(&context, (u8)asm_get_address_of_label(&context, ref.name)); break;
            case vm_opcode_size_short: asm_rom_write2(&context, asm_get_address_of_label(&context, ref.name)); break;
            default: unreachable;
        }
    }

    print("ASM ===\n");
    for (usize i = 0x100; i < output_bytes.count; i += 1) print("% ", fmt(u64, output_bytes.data[i], .base = 16));
    putchar('\n');

    return output_bytes;
}

#define usage "usage: bici <com|run|script> <file...>"

int main(int argc, char **argv) {
    if (argc < 3) {
        err("%", fmt(cstring, usage));
        return 1;
    }

    Arena arena = {0};

    String command = string_from_cstring(argv[1]);
    if (string_equal(command, string("com"))) {
        if (argc != 4) {
            err("usage: bici com <file.biciasm> <file.bici>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        String output_bytes = asm_compile(&arena, max_asm_filesize, argv[2]);
        file_write_bytes_to_relative_path(argv[3], output_bytes);
    } else if (string_equal(command, string("run"))) {
        arena = arena_init(0x10000);
        vm_run(file_read_bytes_relative_path(&arena, argv[2], 0x10000));
    } else if (string_equal(command, string("script"))) {
        if (argc != 3) {
            err("usage: bici script <file.biciasm>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        vm_run(asm_compile(&arena, max_asm_filesize, argv[2]));
    } else {
        err("no such command '%'\n%", fmt(String, command), fmt(cstring, usage));
        return 1;
    }

    arena_deinit(&arena);
    return 0;
}
