#include "base/base.c"

#define vm_screen_initial_width 640
#define vm_screen_initial_height 360

enumdef(Vm_Mode, u8) {
    VM_MODE_SHORT = 1 << 5,
    VM_MODE_RETURN_STACK = 1 << 6,
    VM_MODE_KEEP = 1 << 7,
    VM_MODE_MASK = VM_MODE_SHORT | VM_MODE_RETURN_STACK | VM_MODE_KEEP,
};
#define VM_OPCODE_MASK (~(VM_MODE_MASK))

#define VM_OPCODE_TABLE \
    /*     name,   byte, is_immediate */\
    X(break,  0x00, 0)/* NO MODE */\
    X(push,   0x01, 1)/* NO k MODE */\
    X(drop,   0x02, 0)\
    X(swap,   0x03, 0)\
    X(rot,    0x04, 0)\
    X(dup,    0x05, 0)\
    X(over,   0x06, 0)\
    X(eq,     0x07, 0)\
    X(neq,    0x08, 0)\
    X(gt,     0x09, 0)\
    X(lt,     0x0a, 0)\
    X(add,    0x0b, 0)\
    X(sub,    0x0c, 0)\
    X(mul,    0x0d, 0)\
    X(div,    0x0e, 0)\
    X(not,    0x0f, 0)\
    X(and,    0x10, 0)\
    X(or,     0x11, 0)\
    X(xor,    0x12, 0)\
    X(shift,  0x13, 0)\
    X(jmp,    0x14, 0)\
    X(jme,    0x15, 0)\
    X(jst,    0x16, 0)\
    X(stash,  0x17, 0)\
    X(load,   0x18, 0)\
    X(store,  0x19, 0)\
    X(read,   0x1a, 0)\
    X(write,  0x1b, 0)\
    X(jmi,    0x1c, 1)/* OBLIGATORY MODE .2 */\
    X(jei,    0x1d, 1)/* OBLIGATORY MODE .2 */\
    X(jsi,    0x1e, 1)/* OBLIGATORY MODE .2 */\
    X(jni,    0x1f, 1)/* OBLIGATORY MODE .2 */\

enumdef(Vm_Device, u8) {
    vm_device_system   = 0x00,
    vm_device_console  = 0x10,
    vm_device_screen   = 0x20,
    vm_device_mouse    = 0x30,
    vm_device_keyboard = 0x40,
    vm_device_file     = 0x50,
};

enumdef(Vm_System_Action, u8) {
    vm_system_reserved = 0x0,
    vm_system_end      = 0x2,
    vm_system_start    = 0x4,
    vm_system_quit     = 0x6,
    vm_system_colour_0 = 0x8,
    vm_system_colour_1 = 0xa,
    vm_system_colour_2 = 0xc,
    vm_system_colour_3 = 0xe,
};

enumdef(Vm_Console_Action, u8) {
    vm_console_print = 0x0,
};

enum Vm_Screen_Action {
    vm_screen_update = 0x0,
    vm_screen_width  = 0x2,
    vm_screen_height = 0x4,
    vm_screen_x      = 0x6,
    vm_screen_y      = 0x8,
    vm_screen_pixel  = 0xa,
    vm_screen_sprite = 0xb,
    vm_screen_data   = 0xc,
    vm_screen_auto   = 0xe,
};

enumdef(Vm_Mouse_Action, u8) {
    vm_mouse_x = 0x0,
    vm_mouse_y = 0x2,
    vm_mouse_left_button = 0x4,
    vm_mouse_right_button = 0x5,
};

enumdef(Vm_Keyboard_Action, u8) {
    vm_keyboard_key_value = 0x0,
    vm_keyboard_key_state = 0x1,
};

enumdef(Vm_File_Action, u8) {
    vm_file_name    = 0x0,
    vm_file_length  = 0x2,
    vm_file_cursor = 0x4,
    vm_file_read    = 0x6,
    vm_file_copy    = 0x8,
};

enumdef(Vm_Opcode, u8) {
    #define X(name, val, ...) vm_opcode_##name = val,
    VM_OPCODE_TABLE
    #undef X
    vm_opcode_max_value_plus_one
};

static const char *vm_opcode_name(u8 instruction) {
    static const char *vm_opcode_name_table[vm_opcode_max_value_plus_one] = {
        #define X(name, val, ...) [val] = #name,
        VM_OPCODE_TABLE
        #undef X
    };
    return vm_opcode_name_table[instruction];
}

structdef(Vm_Stack) { u8 memory[0x100], data; };

structdef(Vm) {
    Arena *arena;

    u8 memory[0x10000];
    Vm_Stack stacks[2];
    u8 active_stack;
    u16 program_counter;
    u8 current_mode;
    Gfx_Render_Context gfx;

    u8 *file_bytes;
};

static force_inline u16 get16(u8 *address) {
    u16 result = 0;
    result |= (u16)*address << 8;
    result |= (u16)*(address + 1);
    return result;
}

static force_inline void set16(u8 *address, u16 value) {
    *address = (u8)(value >> 8);
    *(address + 1) = (u8)value;
}

#define vm_stack vm->stacks[vm->active_stack].memory
#define vm_sp vm->stacks[vm->active_stack].data

#define vm_s(data) vm_stack[(u8)(data)]
#define vm_mem(data) vm->memory[(u16)(data)]

static u8   vm_get8(Vm *vm, u8 i_back) { return *(vm_stack + (u8)(vm_sp - i_back)); }
static void vm_push8(Vm *vm, u8 byte) { vm_s(vm_sp) = byte; vm_sp += 1; }
static u8   vm_pop8(Vm *vm) { u8 val = vm_s(vm_sp - 1); if (!(vm->current_mode & VM_MODE_KEEP)) vm_sp -= 1; return val; }
static u8   vm_load8(Vm *vm, u16 addr) { return vm_mem(addr); }
static void vm_store8(Vm *vm, u16 addr, u8 val) { vm_mem(addr) = val; }

static u16  vm_get16(Vm *vm, u8 i_back) { return (u16)vm_s(vm_sp - 2 * i_back + 1) | (u16)(vm_s(vm_sp - 2 * i_back) << 8); }
static void vm_push16(Vm *vm, u16 byte2) { vm_push8(vm, byte2 >> 8); vm_push8(vm, (u8)byte2); }
static u16  vm_pop16(Vm *vm) { u16 val = (u16)vm_s(vm_sp - 1) | (u16)(vm_s(vm_sp - 2) << 8); if (!(vm->current_mode & VM_MODE_KEEP)) vm_sp -= 2; return val; }
static u16  vm_load16(Vm *vm, u16 addr) { return (u16)(((u16)vm_mem(addr) << 8) | (u16)vm_mem(addr + 1)); }
static void vm_store16(Vm *vm, u16 addr, u16 val) { vm_mem(addr) = (u8)(val >> 8); vm_mem(addr + 1) = (u8)val; } // TODO: ensure correct

static inline u32 rgb_from_colour(Vm *vm, u8 colour_index) {
    assert(colour_index < 4);
    Vm_System_Action colour_offset = vm_system_colour_0 + colour_index * 2;
    u16 colour = get16(&vm->memory[vm_device_system | colour_offset]);
    u32 r = ((u32)colour & 0x0f00) >> 8;
    u32 g = ((u32)colour & 0x00f0) >> 4;
    u32 b = ((u32)colour & 0x000f) >> 0;
    u32 rgb = (r << 20) | (r << 16) | (g << 12) | (g << 8) | (b << 4) | b;
    return rgb;
}

static void vm_run_to_break(Vm *vm, u16 program_counter) {
    vm->program_counter = program_counter;
    u16 add = 1;
    for (; true; vm->program_counter += add) {
        add = 1;
        u8 instruction = vm_mem(vm->program_counter);

        vm->current_mode = instruction & VM_MODE_MASK;
        vm->active_stack = instruction & VM_MODE_RETURN_STACK;

        u8 instruction_width = ((instruction & VM_MODE_SHORT) >> 5) + 1; // 1 or 2
        discard(instruction_width); // TODO(felix);

        switch (instruction & VM_OPCODE_MASK) {
            case vm_opcode_break: return;
            case vm_opcode_jmi: vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; continue;
            case vm_opcode_jei: if ( vm_pop8(vm)) { vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; } else add = 3; continue;
            case vm_opcode_jni: if (!vm_pop8(vm)) { vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; } else add = 3; continue;
            case vm_opcode_jsi: {
                vm->active_stack = VM_MODE_RETURN_STACK;
                vm_push16(vm, vm->program_counter + 3);
                vm->active_stack = 0;
                vm->program_counter = vm_load16(vm, vm->program_counter + 1);
                add = 0;
                continue;
            } break;
            default: break;
        }

        switch (instruction & VM_MODE_SHORT) {
            case 0: switch (instruction & VM_OPCODE_MASK) {
                case vm_opcode_jmi: case vm_opcode_jei: case vm_opcode_jni: case vm_opcode_jsi: case vm_opcode_break:
                    panic("reached full-byte opcodes in generic switch case");
                case vm_opcode_push:   vm_push8(vm, vm_load8(vm, vm->program_counter + 1)); add = 1 + 1; break;
                case vm_opcode_drop:   discard(vm_pop8(vm)); break;
                case vm_opcode_swap:   { u8 c = vm_pop8(vm), b = vm_pop8(vm); vm_push8(vm, c); vm_push8(vm, b); } break;
                case vm_opcode_rot:    { u8 c = vm_pop8(vm), b = vm_pop8(vm), a = vm_pop8(vm); vm_push8(vm, b); vm_push8(vm, c); vm_push8(vm, a); } break;
                case vm_opcode_dup:    assert(!(vm->current_mode & VM_MODE_KEEP)); vm_push8(vm, vm_get8(vm, 1)); break;
                case vm_opcode_over:   assert(!(vm->current_mode & VM_MODE_KEEP)); vm_push8(vm, vm_get8(vm, 2)); break;
                case vm_opcode_eq:     vm_push8(vm, vm_pop8(vm) == vm_pop8(vm)); break;
                case vm_opcode_neq:    vm_push8(vm, vm_pop8(vm) != vm_pop8(vm)); break;
                case vm_opcode_gt:     { u8 right = vm_pop8(vm), left = vm_pop8(vm); vm_push8(vm, left > right); } break;
                case vm_opcode_lt:     { u8 right = vm_pop8(vm), left = vm_pop8(vm); vm_push8(vm, left < right); } break;
                case vm_opcode_add:    vm_push8(vm, vm_pop8(vm) + vm_pop8(vm)); break;
                case vm_opcode_sub:    { u8 right = vm_pop8(vm), left = vm_pop8(vm); vm_push8(vm, left - right); } break;
                case vm_opcode_mul:    vm_push8(vm, vm_pop8(vm) * vm_pop8(vm)); break;
                case vm_opcode_div:    { u8 right = vm_pop8(vm), left = vm_pop8(vm); vm_push8(vm, right == 0 ? 0 : (left / right)); } break;
                case vm_opcode_not:    vm_push8(vm, ~vm_pop8(vm)); break;
                case vm_opcode_and:    vm_push8(vm, vm_pop8(vm) & vm_pop8(vm)); break;
                case vm_opcode_or:     vm_push8(vm, vm_pop8(vm) | vm_pop8(vm)); break;
                case vm_opcode_xor:    vm_push8(vm, vm_pop8(vm) ^ vm_pop8(vm)); break;
                case vm_opcode_shift:  { u8 shift = vm_pop8(vm), r = shift & 0x0f, l = (shift & 0xf0) >> 4; vm_push8(vm, (u8)(vm_pop8(vm) << l >> r)); } break;
                case vm_opcode_jmp:    vm->program_counter = vm_pop16(vm); add = 0; break;
                case vm_opcode_jme:    { u16 addr = vm_pop16(vm); if (vm_pop8(vm)) { vm->program_counter = addr; add = 0; } } break;
                case vm_opcode_jst:    {
                    vm->active_stack = VM_MODE_RETURN_STACK;
                    vm_push16(vm, vm->program_counter + 1);
                    vm->active_stack = 0;
                    vm->program_counter = vm_pop16(vm);
                    add = 0;
                } break;
                case vm_opcode_stash:  { // TODO(felix): fix
                    u8 val = vm_pop8(vm);
                    vm->active_stack = vm->active_stack == 0 ? VM_MODE_RETURN_STACK : 0;
                    vm_push8(vm, val);
                    vm->active_stack = vm->active_stack == 0 ? VM_MODE_RETURN_STACK : 0;
                } break;
                case vm_opcode_load:   { u16 addr = vm_pop16(vm); u8 val = vm_load8(vm, addr); vm_push8(vm, val); } break;
                case vm_opcode_store:  { u16 addr = vm_pop16(vm); u8 val = vm_pop8(vm); vm_store8(vm, addr, val); } break;
                case vm_opcode_read:   {
                    u8 vm_device_and_action = vm_pop8(vm);
                    Vm_Device device = vm_device_and_action & 0xf0;
                    u8 action = vm_device_and_action & 0x0f;
                    u8 to_push = 0;
                    switch (device) {
                        case vm_device_mouse: switch (action) {
                            case vm_mouse_left_button: {
                                if (vm->gfx.frame_info.mouse_left_clicked) to_push |= 0xf0;
                                if (vm->gfx.frame_info.mouse_left_down)    to_push |= 0x0f;
                            } break;
                            case vm_mouse_right_button: {
                                if (vm->gfx.frame_info.mouse_right_clicked) to_push |= 0xf0;
                                if (vm->gfx.frame_info.mouse_right_down)    to_push |= 0x0f;
                            } break;
                            default: panic("[read.1] invalid action #% for mouse device", fmt(u8, action, .base = 16));
                        } break;
                        case vm_device_keyboard: switch (action) {
                            case vm_keyboard_key_state: {
                                u8 key = vm->memory[vm_device_keyboard | vm_keyboard_key_value];
                                bool key_down = vm->gfx.frame_info.key_is_down[key];
                                if (key_down) to_push |= 0x0f;
                                bool key_pressed = key_down && !vm->gfx.frame_info.key_was_down_last_frame[key];
                                if (key_pressed) to_push |= 0xf0;
                            } break;
                            default: panic("[read.1] invalid action #% for keyboard device", fmt(u8, action, .base = 16));
                        } break;
                        default: panic("[read.1] invalid device #%", fmt(u64, device, .base = 16));
                    }
                    vm_push8(vm, to_push);
                    // // TODO(felix): not sure about this. do I use the same address for different purposes depending on whether I'm reading or writing?
                    // vm->memory[vm_device_and_action] = to_push;
                } break;
                case vm_opcode_write:  {
                    u8 vm_device_and_action = vm_pop8(vm);
                    Vm_Device device = vm_device_and_action & 0xf0;
                    u8 action = vm_device_and_action & 0x0f;
                    u8 argument = vm_pop8(vm);
                    switch (device) {
                        case vm_device_screen: {
                            u8 *x_address = &vm->memory[vm_device_screen | vm_screen_x];
                            u8 *y_address = &vm->memory[vm_device_screen | vm_screen_y];
                            u16 x = get16(x_address);
                            u16 y = get16(y_address);
                            switch (action) {
                                case vm_screen_pixel: {
                                    if (x >= vm_screen_initial_width) break;
                                    if (y >= vm_screen_initial_height) break;

                                    u8 fill_x = argument & 0x80;
                                    u8 fill_y = argument & 0x40;

                                    u8 colour = argument & 0x0f;
                                    if (colour > 3) panic("[write:screen/pixel] colour #% is invalid; there are only #% palette colours", fmt(u64, colour, .base = 16), fmt(u64, 4));

                                    u32 rgb = rgb_from_colour(vm, colour);

                                    u16 width = fill_x ? (vm_screen_initial_width - x) : 1;
                                    u16 height = fill_y ? (vm_screen_initial_height - y) : 1;

                                    if (fill_x && fill_y) {
                                        gfx_draw_rectangle(&vm->gfx, x, y, width, height, rgb);
                                    } else if (fill_x) {
                                        gfx_draw_line(&vm->gfx, (V2){ .x = (f32)x, .y = (f32)y }, (V2){ .x = (f32)vm_screen_initial_width, .y = (f32)y }, 1.f, rgb);
                                    } else if (fill_y) {
                                        unreachable;
                                    } else gfx_set_pixel(&vm->gfx, x, y, rgb);
                                } break;
                                case vm_screen_sprite: {
                                    if (x >= vm_screen_initial_width) break;
                                    if (y >= vm_screen_initial_height) break;

                                    u8 two_bits_per_pixel = argument & 0x80;
                                    u8 use_background_layer = argument & 0x40;
                                    u8 flip_y = argument & 0x20;
                                    u8 flip_x = argument & 0x10;
                                    u32 colours[] = { rgb_from_colour(vm, argument & 0x03), rgb_from_colour(vm, (argument & 0x0c) >> 2) };

                                    // TODO(felix)
                                    assert(!two_bits_per_pixel);
                                    assert(!flip_y);
                                    assert(!flip_x);

                                    u16 column_count = min(8, vm_screen_initial_width - x);
                                    u16 row_count = min(8, vm_screen_initial_height - y);

                                    u8 *sprite = &vm->memory[get16(&vm->memory[vm_device_screen | vm_screen_data])];
                                    for (u16 row = 0; row < row_count; row += 1, sprite += 1) {
                                        for (u16 column = 0; column < column_count; column += 1) {
                                            u8 shift = 7 - (u8)column;
                                            u8 colour = (*sprite & (1 << shift)) >> shift;
                                            if (!use_background_layer && colour == 0) continue;
                                            gfx_set_pixel(&vm->gfx, x + column, y + row, colours[colour]);
                                        }
                                    }

                                    u8 auto_x = vm->memory[vm_device_screen | vm_screen_auto] & 0x01;
                                    u8 auto_y = vm->memory[vm_device_screen | vm_screen_auto] & 0x02;
                                    if (auto_x) x += 8;
                                    if (auto_y) y += 8;
                                } break;
                                case vm_screen_auto: break;
                                default: panic("[write.1] invalid action #% for screen device", fmt(u8, action, .base = 16));
                            }
                            set16(x_address, x);
                            set16(y_address, y);
                        } break;
                        case vm_device_keyboard: switch (action) {
                            case vm_keyboard_key_value: break;
                            default: panic("[write.1] invalid action #% for keyboard device", fmt(u8, action, .base = 16));
                        } break;
                        default: panic("[write.1] invalid device #%", fmt(u8, device, .base = 16));
                    }
                    vm->memory[vm_device_and_action] = argument;
                } break;
                default: panic("unreachable %{#%}", fmt(cstring, (char *)vm_opcode_name(instruction)), fmt(u64, instruction, .base = 16));
            } break;
            case VM_MODE_SHORT: switch (instruction & VM_OPCODE_MASK) {
                case vm_opcode_jmi: case vm_opcode_jei: case vm_opcode_jni: case vm_opcode_jsi: case vm_opcode_break:
                    panic("reached full-byte opcodes in generic switch case");
                case vm_opcode_push:   vm_push16(vm, vm_load16(vm, vm->program_counter + 1)); add = 2 + 1; break;
                case vm_opcode_drop:   discard(vm_pop16(vm)); break;
                case vm_opcode_swap:   { u16 c = vm_pop16(vm), b = vm_pop16(vm); vm_push16(vm, c); vm_push16(vm, b); } break;
                case vm_opcode_rot:    { u16 c = vm_pop16(vm), b = vm_pop16(vm), a = vm_pop16(vm); vm_push16(vm, b); vm_push16(vm, c); vm_push16(vm, a); } break;
                case vm_opcode_dup:    assert(!(vm->current_mode & VM_MODE_KEEP)); vm_push16(vm, vm_get16(vm, 1)); break;
                case vm_opcode_over:   assert(!(vm->current_mode & VM_MODE_KEEP)); vm_push16(vm, vm_get16(vm, 2)); break;
                case vm_opcode_eq:     vm_push8(vm, vm_pop16(vm) == vm_pop16(vm)); break;
                case vm_opcode_neq:    vm_push8(vm, vm_pop16(vm) != vm_pop16(vm)); break;
                case vm_opcode_gt:     { u16 right = vm_pop16(vm), left = vm_pop16(vm); vm_push8(vm, left > right); } break;
                case vm_opcode_lt:     { u16 right = vm_pop16(vm), left = vm_pop16(vm); vm_push8(vm, left < right); } break;
                case vm_opcode_add:    vm_push16(vm, vm_pop16(vm) + vm_pop16(vm)); break;
                case vm_opcode_sub:    { u16 right = vm_pop16(vm), left = vm_pop16(vm); vm_push16(vm, left - right); } break;
                case vm_opcode_mul:    vm_push16(vm, vm_pop16(vm) * vm_pop16(vm)); break;
                case vm_opcode_div:    { u16 right = vm_pop16(vm), left = vm_pop16(vm); vm_push16(vm, right == 0 ? 0 : (left / right)); } break;
                case vm_opcode_not:    vm_push16(vm, ~vm_pop16(vm)); break;
                case vm_opcode_and:    vm_push16(vm, vm_pop16(vm) & vm_pop16(vm)); break;
                case vm_opcode_or:     vm_push16(vm, vm_pop16(vm) | vm_pop16(vm)); break;
                case vm_opcode_xor:    vm_push16(vm, vm_pop16(vm) ^ vm_pop16(vm)); break;
                case vm_opcode_shift:  { u8 shift = vm_pop8(vm), r = shift & 0x0f, l = (shift & 0xf0) >> 4; vm_push16(vm, (u16)(vm_pop16(vm) << l >> r)); } break;
                case vm_opcode_jmp:    vm->program_counter = vm_pop16(vm); add = 0; break;
                case vm_opcode_jme:    { u16 addr = vm_pop16(vm); if (vm_pop16(vm)) { vm->program_counter = addr; add = 0; } } break;
                case vm_opcode_jst:    {
                    vm->active_stack = VM_MODE_RETURN_STACK;
                    vm_push16(vm, vm->program_counter + 1);
                    vm->active_stack = 0;
                    vm->program_counter = vm_pop16(vm);
                    add = 0;
                } break;
                case vm_opcode_stash:  { u16 val = vm_pop16(vm); vm->active_stack = VM_MODE_RETURN_STACK; vm_push16(vm, val); vm->active_stack = 0; } break;
                case vm_opcode_load:   { u16 addr = vm_pop16(vm); u16 val = vm_load16(vm, addr); vm_push16(vm, val); } break;
                case vm_opcode_store:  { u16 addr = vm_pop16(vm); u16 val = vm_pop16(vm); vm_store16(vm, addr, val); } break;
                case vm_opcode_read:   {
                    u8 vm_device_and_action = vm_pop8(vm);
                    Vm_Device device = vm_device_and_action & 0xf0;
                    u8 action = vm_device_and_action & 0x0f;
                    u16 to_push = get16(&vm->memory[vm_device_and_action]);
                    switch (device) {
                        case vm_device_screen: switch (action) {
                            case vm_screen_width: to_push = vm_screen_initial_width; break;
                            case vm_screen_height: to_push = vm_screen_initial_height; break;
                            default: panic("[read.2] invalid action #% for screen device", fmt(u64, action, .base = 16));
                        } break;
                        case vm_device_mouse: {
                            V2 virtual_over_real = v2_div(vm->gfx.frame_info.virtual_window_size, vm->gfx.frame_info.real_window_size);
                            V2 virtual_mouse_position = v2_mul(vm->gfx.frame_info.real_mouse_position, virtual_over_real);
                            switch (action) {
                                case vm_mouse_x: {
                                    to_push = (u16)virtual_mouse_position.x;
                                } break;
                                case vm_mouse_y: {
                                    to_push = (u16)virtual_mouse_position.y;
                                } break;
                                default: panic("[read.2] invalid action #% for mouse device", fmt(u8, action, .base = 16));
                            }
                        } break;
                        case vm_device_file: {
                            assert(get16(&vm->memory[vm_device_file | vm_file_name]) != 0);
                            switch (action) {
                                case vm_file_length: break;
                                case vm_file_read: {
                                    assert(get16(&vm->memory[vm_device_file | vm_file_cursor]) < get16(&vm->memory[vm_device_file | vm_file_length]));
                                    to_push = get16(&vm->file_bytes[get16(&vm->memory[vm_device_file | vm_file_cursor])]);
                                } break;
                                default: panic("[read.2] invalid action #% for file device", fmt(u8, action, .base = 16));
                            }
                        } break;
                        default: panic("[read.2] invalid device #%", fmt(u64, device, .base = 16));
                    }
                    set16(&vm->memory[vm_device_and_action], to_push);
                    vm_push16(vm, to_push);
                } break;
                case vm_opcode_write:  {
                    u8 vm_device_and_action = vm_pop8(vm);
                    Vm_Device device = vm_device_and_action & 0xf0;
                    u8 action = vm_device_and_action & 0x0f;
                    u16 argument = vm_pop16(vm);
                    switch (device) {
                        case vm_device_console: switch (action) {
                            case vm_console_print: {
                                u16 str_addr = argument;
                                u8 str_count = vm_load8(vm, str_addr);
                                String str = { .data = vm->memory + str_addr + 1, .count = str_count };
                                print("%", fmt(String, str));
                            } break;
                            default: panic("[write.2] invalid action #% for console device", fmt(u64, action, .base = 16));
                        } break;
                        case vm_device_screen: switch (action) {
                            case vm_screen_x: {
                                set16(&vm->memory[vm_device_screen | vm_screen_x], argument);
                            } break;
                            case vm_screen_y: {
                                set16(&vm->memory[vm_device_screen | vm_screen_y], argument);
                            } break;
                            case vm_screen_data: break;
                            default: panic("[write.2] invalid action #% for screen device", fmt(u8, action, .base = 16));
                        } break;
                        case vm_device_file: switch (action) {
                            case vm_file_name: {
                                u16 string_address = argument;
                                u8 string_length = vm_load8(vm, string_address);
                                String file_name = { .data = &vm->memory[string_address + 1], .count = string_length };

                                Slice_u8 bytes = os_read_entire_file(vm->arena, file_name, 0xffff);

                                vm->file_bytes = bytes.data;

                                assert(bytes.count <= 0xffff);
                                set16(&vm->memory[vm_device_file | vm_file_length], (u16)bytes.count);
                            } break;
                            case vm_file_length: {
                                assert(argument <= get16(&vm->memory[vm_device_file | vm_file_length]));
                            } break;
                            case vm_file_cursor: {
                                assert(get16(&vm->memory[vm_device_file | vm_file_name]) != 0);
                            } break;
                            case vm_file_copy: {
                                assert(get16(&vm->memory[vm_device_file | vm_file_name]) != 0);
                                assert(vm->file_bytes != 0);
                                assert(get16(&vm->memory[vm_device_file | vm_file_length]) != 0);
                                u16 address_to_copy_to = argument;
                                memcpy(vm->memory + address_to_copy_to, vm->file_bytes, get16(&vm->memory[vm_device_file | vm_file_length]));
                            } break;
                            default: panic("[write.2] invalid action #% for file device", fmt(u8, action, .base = 16));
                        } break;
                        default: panic("[write.2] invalid device #%", fmt(u8, device, .base = 16));
                    }
                    set16(&vm->memory[vm_device_and_action], argument);
                } break;
                default: panic("unreachable %{#%}", fmt(cstring, (char *)vm_opcode_name(instruction)), fmt(u8, instruction, .base = 16));
            } break;
            default: unreachable;
        }
    }
}

static bool instruction_takes_immediate(u8 instruction) {
    instruction &= VM_OPCODE_MASK;
    if (false);
    #define X(name, value, is_immediate) else if (instruction == value) return is_immediate;
    VM_OPCODE_TABLE
    #undef X
    else unreachable;
}

static void write_u16_swap_bytes(Array_u8 rom, u64 index, u16 value) {
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
    token_kind_number,
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
    u8 base;
};

structdef(Token_Id) { File_Id file_id; i32 index; };

structdef(Label) {
    Token_Id token_id;
    u16 address;
    Map_Label children;
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

structdef(Input_File) { String name; Slice_u8 bytes; Array_Token tokens; };

structdef(Assembler_Context) {
    Map_Input_File files;
    File_Id file_id;
    Map_Label labels;
    Array_Label_Reference label_references;
    Array_Scoped_Reference scoped_references;
    Insertion_Mode insertion_mode;
};

static Token_Id token_id_shift(Token_Id token_id, i32 shift) {
    Token_Id result = token_id;
    result.index += shift;
    return result;
}

static Input_File *token_get_file(Assembler_Context *assembler, Token_Id token_id) {
    File_Id file_id = token_id.file_id;
    Input_File *file = &assembler->files.values.data[file_id];
    return file;
}

static Token *token_get(Assembler_Context *assembler, Token_Id token_id) {
    Input_File file = *token_get_file(assembler, token_id);
    i32 index = token_id.index;
    if (index < 0 || (u64)index >= file.tokens.count) {
        static Token nil_token = {0};
        return &nil_token;
    }
    Token *token = &file.tokens.data[index];
    return token;
}

static String token_lexeme(Assembler_Context *assembler, Token_Id token_id) {
    Token token = *token_get(assembler, token_id);
    Input_File *file = token_get_file(assembler, token_id);
    String asm = string_from_bytes(file->bytes);
    String token_string = string_range(asm, token.start_index, token.start_index + token.length);
    return token_string;
}

structdef(Parse_Error_Arguments) {
    Assembler_Context *assembler;
    Token token;
    u32 start_index;
    u32 length;
    File_Id file_id;
};
#define parse_error(...) parse_error_argument_struct((Parse_Error_Arguments){ __VA_ARGS__ })
static void parse_error_argument_struct(Parse_Error_Arguments arguments) {
    Assembler_Context *assembler = arguments.assembler;

    File_Id file_id = assembler->file_id;
    if (arguments.file_id != 0) file_id = arguments.file_id;

    u32 start_index = arguments.start_index;
    u32 length = arguments.length;
    if (arguments.token.kind != 0) {
        assert(arguments.start_index == 0);
        assert(arguments.length == 0);
        start_index = arguments.token.start_index;
        length = arguments.token.length;
    } else if (length == 0) length = 1;

    String text = string_from_bytes(assembler->files.values.data[file_id].bytes);
    String file_name = assembler->files.values.data[file_id].name;
    String lexeme = string_range(text, start_index, start_index + length);

    u64 row = 1, column = 1;
    for (u64 i = 0; i < start_index; i += 1) {
        u8 c = text.data[i];
        column += 1;
        if (c == '\n') {
            row += 1;
            column = 1;
        }
    }

    print("note: '%' in %:%:%\n", fmt(String, lexeme), fmt(String, file_name), fmt(u64, row), fmt(u64, column));

    os_exit(1);
}

#define usage "usage: bici <compile|run|script> <file...>"

structdef(File_Cursor) { Token_Id token_id; u32 cursor; };

static void program(void) {
    Arena arena = arena_init(64 * 1024 * 1024);
    Slice_String arguments = os_get_arguments(&arena);

    if (arguments.count < 3) {
        log_error("%", fmt(cstring, usage));
        os_exit(1);
    }

    enumdef(Command, u8) {
        command_compile, command_run, command_script,
        command_count,
    };
    String command_string = arguments.data[1];

    Command command = 0;
    if (string_equal(command_string, string("compile"))) {
        if (arguments.count != 4) {
            log_error("usage: bici compile <file.asm> <file.rom>");
            os_exit(1);
        }
        command = command_compile;
    } else if (string_equal(command_string, string("run"))) {
        if (arguments.count != 3) {
            log_error("usage: bici run <file.rom>");
            os_exit(1);
        }
        command = command_run;
    } else if (string_equal(command_string, string("script"))) {
        if (arguments.count != 3) {
            log_error("usage: bici script <file.asm>");
            os_exit(1);
        }
        command = command_script;
    } else {
        log_error("no such command '%'\n%", fmt(String, command_string), fmt(cstring, usage));
        os_exit(1);
    }

    String input_filepath = arguments.data[2];
    u64 max_filesize = 0x10000;
    Slice_u8 input_bytes = os_read_entire_file(&arena, input_filepath, max_filesize);

    Array_u8 rom = array_from_c_array(u8, 0x10000);

    bool should_compile = command == command_compile || command == command_script;
    if (!should_compile) rom = (Array_u8)array_from_slice(input_bytes);
    else {
        // TODO(felix): fix
        // Map_u8 opcode_from_name_map = {0};
        // map_make(&arena, &opcode_from_name_map, 256);
        // u8 i;
        // #define X(name, byte, ...) i = byte; map_put(&opcode_from_name_map, string(#name), &i);
        // VM_OPCODE_TABLE
        // #undef X

        Assembler_Context assembler = {
            .label_references.arena = &arena,
            .scoped_references.arena = &arena,
        };
        map_make(&arena, &assembler.files, max_file_id);
        map_make(&arena, &assembler.labels, 1024);

        Input_File main_file = { .bytes = input_bytes, .name = input_filepath, .tokens.arena = &arena };
        File_Id main_file_id = (File_Id)map_put(&assembler.files, input_filepath, &main_file).index;

        Array_File_Cursor file_stack = { .arena = &arena };
        push(&file_stack, (File_Cursor){ .token_id.file_id = main_file_id });

        while (file_stack.count > 0) {
            File_Cursor file_cursor = pop(file_stack);
            assembler.file_id = file_cursor.token_id.file_id;
            Input_File *file = &assembler.files.values.data[assembler.file_id];
            String asm = string_from_bytes(file->bytes);

            for (u32 cursor = file_cursor.cursor; cursor < asm.count;) {
                while (cursor < asm.count && ascii_is_whitespace(asm.data[cursor])) cursor += 1;
                if (cursor == asm.count) break;

                u32 start_index = cursor;
                Token_Kind kind = 0;
                u16 value = 0;
                u8 base = 0;
                if (is_starting_symbol_character(asm.data[cursor])) {
                    kind = token_kind_symbol;
                    cursor += 1;

                    for (; cursor < asm.count; cursor += 1) {
                        u8 c = asm.data[cursor];
                        bool is_symbol_character = is_starting_symbol_character(c) || ascii_is_decimal(c);
                        if (!is_symbol_character) break;
                    }
                } else switch (asm.data[cursor]) {
                    case '0': {
                        kind = token_kind_number;

                        if (cursor + 1 == asm.count || (asm.data[cursor + 1] != 'x' && asm.data[cursor + 1] != 'b')) {
                            log_error("expected 'x' or 'b' after '0' to begin binary or hexadecimal literal");
                            parse_error(.start_index = cursor + 1, 1);
                        }
                        u8 base_char = asm.data[cursor + 1];
                        cursor += 2;
                        start_index = cursor;

                        if (cursor == asm.count) {
                            log_error("expected digits following '0%'", fmt(char, base_char));
                            parse_error(.start_index = cursor - 2, 2);
                        }

                        for (; cursor < asm.count; cursor += 1) {
                            u8 c = asm.data[cursor];
                            if (base_char == 'x' && ascii_is_hexadecimal(c)) continue;
                            if (base_char == 'b' && (c == '0' || c == '1')) continue;
                            if (ascii_is_whitespace(c)) break;
                            log_error("expected base_char % digit", fmt(char, base_char));
                            parse_error(.start_index = cursor);
                        }

                        base = base_char == 'b' ? 2 : 16;
                        String number_string = string_range(asm, start_index, cursor);
                        u64 value_unbounded = int_from_string_base(number_string, base);
                        if (value_unbounded >= 0x10000) {
                            log_error("0d% is too large to fit in 16 bits", fmt(u64, value_unbounded));
                            parse_error(.start_index = start_index, (u8)(cursor - start_index));
                        }
                        value = (u16)value_unbounded;
                    } break;
                    case ';': {
                        while (cursor < asm.count && asm.data[cursor] != '\n') cursor += 1;
                        cursor += 1;
                        continue;
                    } break;
                    case ':': case '$': case '{': case '}': case '[': case ']': case ',': case '/': {
                        kind = asm.data[cursor];
                        cursor += 1;
                    } break;
                    case '"': {
                        kind = token_kind_string;

                        cursor += 1;
                        start_index = cursor;
                        for (; cursor < asm.count; cursor += 1) {
                            u8 c = asm.data[cursor];
                            if (c == '"') break;
                            if (c == '\n') {
                                log_error("expected '\"' to close string before newline");
                                parse_error(.start_index = start_index, (u8)(cursor - start_index));
                            }
                        }

                        if (cursor == asm.count) {
                            log_error("expected '\"' to close string before end of file");
                            parse_error(.start_index = start_index, (u8)(cursor - start_index));
                        }

                        assert(asm.data[cursor] == '"');
                        cursor += 1;
                    } break;
                    case '.': {
                        kind = token_kind_opmode;

                        cursor += 1;
                        start_index = cursor;
                        for (; cursor < asm.count; cursor += 1) {
                            u8 c = asm.data[cursor];
                            if (ascii_is_whitespace(c)) break;
                            if (c != '2' && c != 'k' && c != 'r') {
                                log_error("only characters '2', 'k', and 'r' are valid op modes");
                                parse_error(.start_index = cursor);
                            }
                        }

                        u64 length = cursor - start_index;
                        if (length == 0 || length > 3) {
                            log_error("a valid op mode contains 1, 2, or 3 characters (as in op.2kr), but this one has %", fmt(u64, length));
                            parse_error(.start_index = start_index, (u8)length);
                        }
                    } break;
                    default: {
                        log_error("invalid syntax '%'", fmt(char, asm.data[cursor]));
                        parse_error(.start_index = cursor);
                    }
                }

                u64 length = cursor - start_index;
                if (kind == token_kind_string) length -= 1;
                assert(length <= 255);

                Token token = {
                    .start_index = start_index,
                    .length = (u8)length,
                    .kind = kind,
                    .value = value,
                    .base = base,
                };
                push(&file->tokens, token);
            }

            bool parsing_relative_label = false;
            for (Token_Id file_token_id = file_cursor.token_id; (u64)file_token_id.index < file->tokens.count; file_token_id.index += 1) {
                Token token = *token_get(&assembler, file_token_id);
                String token_string = token_lexeme(&assembler, file_token_id);

                if (assembler.insertion_mode.is_active) {
                    switch (token.kind) {
                        case token_kind_symbol: case ']': case '$': case '/': case token_kind_number: case token_kind_string: break;
                        default: {
                            log_error("insertion mode only supports addresses (e.g. label) and numeric literals");
                            parse_error(&assembler, token);
                        }
                    };
                }

                Token_Id next_id = token_id_shift(file_token_id, 1);
                Token next = *token_get(&assembler, next_id);
                String next_string = token_lexeme(&assembler, next_id);

                switch (token.kind) {
                    case '{': {
                        Scoped_Reference reference = { .address = (u16)rom.count };
                        push(&assembler.scoped_references, reference);
                        rom.count += 2;
                    } break;
                    case '}': {
                        if (assembler.scoped_references.count == 0) {
                            log_error("'}' has no matching '{'");
                            parse_error(&assembler, token);
                        }

                        Scoped_Reference reference = pop(assembler.scoped_references);
                        if (reference.is_relative) {
                            log_error("expected to resolve absolute reference (from '{') but found unresolved relative reference");
                            parse_error(&assembler, token);
                        }

                        write_u16_swap_bytes(rom, reference.address, (u16)rom.count);
                    } break;
                    case '[': {
                        if (assembler.insertion_mode.is_active) {
                            log_error("cannot enter insertion mode while already in insertion mode");
                            parse_error(&assembler, token);
                        }
                        assembler.insertion_mode.is_active = true;

                        bool compile_byte_count_to_closing_bracket = next.kind == '$';
                        if (compile_byte_count_to_closing_bracket) {
                            assembler.insertion_mode.has_relative_reference = true;
                            Scoped_Reference reference = { .address = (u16)rom.count, .is_relative = true };
                            push(&assembler.scoped_references, reference);
                            rom.count += 1;
                            file_token_id = token_id_shift(file_token_id, 1);
                        }
                    } break;
                    case ']': {
                        if (!assembler.insertion_mode.is_active) {
                            log_error("']' has no matching '['");
                            parse_error(&assembler, token);
                        }

                        if (assembler.insertion_mode.has_relative_reference) {
                            Scoped_Reference reference = pop(assembler.scoped_references);
                            if (!reference.is_relative) {
                                log_error("expected to resolve relative reference (from '[$') but found unresolved absolute reference; is there an unmatched '{'?");
                                parse_error(&assembler, token);
                            }

                            u64 relative_difference = rom.count - reference.address - 1;
                            if (relative_difference > 255) {
                                log_error("relative difference from '[$' is % bytes, but the maximum is 255", fmt(u64, relative_difference));
                                parse_error(&assembler, token);
                            }

                            u8 *location_to_patch = &rom.data[reference.address];
                            *location_to_patch = (u8)relative_difference;
                        }

                        assembler.insertion_mode = (Insertion_Mode){0};
                    } break;
                    case '/': {
                        if (next.kind != token_kind_symbol) {
                            log_error("expected relative label following '/'");
                            parse_error(&assembler, token);
                        }
                        parsing_relative_label = true;
                    } break;
                    case token_kind_symbol: {
                        if (parsing_relative_label) {
                            if (next.kind != ':') {
                                log_error("expected ':' ending relative label definition");
                                parse_error(&assembler, next);
                            }
                        }

                        bool is_label = next.kind == ':';
                        if (is_label) {
                            if (map_get(&assembler.labels, token_string).pointer != 0) {
                                log_error("redefinition of label '%'", fmt(String, token_string));
                                parse_error(&assembler, token);
                            }

                            Label new_label = { .token_id = file_token_id, .address = (u16)rom.count };
                            map_make(&arena, &new_label.children, 256);

                            Map_Label *labels = 0;
                            if (parsing_relative_label) {
                                Label *parent = slice_get_last(assembler.labels.values);
                                Map_Label *parent_labels = &parent->children;
                                labels = parent_labels;

                                if (map_get(parent_labels, token_string).index != 0) {
                                    log_error("redefinition of label '%'", fmt(String, token_string));
                                    parse_error(&assembler, token);
                                }

                                parsing_relative_label = false;
                            } else {
                                labels = &assembler.labels;
                            }

                            map_put(labels, token_string, &new_label);

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        }

                        // TODO(felix): fix
                        // u8 instruction = *map_get(&opcode_from_name_map, token_string).pointer;

                        u8 instruction = {0};
                        bool is_opcode = false;
                        if (false) {}
                        #define X(name, byte, ...) else if (string_equal(token_string, string(#name))) { instruction = byte; is_opcode = true; }
                        VM_OPCODE_TABLE
                        #undef X

                        if (is_opcode) {
                            bool explicit_mode = next.kind == token_kind_opmode;
                            if (explicit_mode) {
                                for_slice (u8 *, c, next_string) {
                                    switch (*c) {
                                        case '2': instruction |= VM_MODE_SHORT; break;
                                        case 'k': instruction |= VM_MODE_KEEP; break;
                                        case 'r': instruction |= VM_MODE_RETURN_STACK; break;
                                        default: unreachable;
                                    }
                                }
                                file_token_id = token_id_shift(file_token_id, 1);
                            }

                            u8 byte = instruction;
                            rom.data[rom.count] = byte;
                            rom.count += 1;

                            if (instruction_takes_immediate(instruction)) {
                                next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                                switch (next.kind) {
                                    case token_kind_symbol: case token_kind_number: case '{': break;
                                    default: {
                                        log_error("instruction '%' takes an immediate, but no label or numeric literal is given", fmt(cstring, (char *)vm_opcode_name(instruction)));
                                        parse_error(&assembler, next);
                                    }
                                }

                                if (next.kind == '{') continue;

                                u8 width = ((instruction & VM_MODE_SHORT) >> 5) + 1;

                                if (next.kind == token_kind_symbol) {
                                    Label_Reference reference = {
                                        .token_id = token_id_shift(file_token_id, 1),
                                        .destination_address = (u16)rom.count,
                                        .width = width,
                                        .parent_id = (Label_Id)(assembler.labels.values.count - 1),
                                    };
                                    push(&assembler.label_references, reference);
                                    rom.count += reference.width;
                                } else {
                                    assert(next.kind == token_kind_number);
                                    next_string = token_lexeme(&assembler, token_id_shift(file_token_id, 1));
                                    u16 value = next.value;

                                    if (width == 1) {
                                        if (value > 255) {
                                            log_error("attempt to supply 16-bit value to instruction taking 8-bit immediate (% is greater than 255); did you mean to use mode '.2'?", fmt(u16, value));
                                            parse_error(&assembler, next);
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

                        if (string_equal(token_string, string("org")) || string_equal(token_string, string("rorg")) || string_equal(token_string, string("aorg"))) {
                            if (next.kind != token_kind_number) {
                                log_error("expected numeric literal (byte offset) after directive '%'", fmt(String, token_string));
                                parse_error(&assembler, next);
                            }

                            u16 value = next.value;
                            bool is_relative = token_string.data[0] == 'r';
                            if (is_relative) value += (u16)rom.count;

                            if (value < rom.count) {
                                log_error("offset % is less than current offset %", fmt(u64, value), fmt(u64, rom.count));
                                parse_error(&assembler, next);
                            }

                            bool assert_only = token_string.data[0] == 'a';
                            if (!assert_only) rom.count = value;

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        } else if (string_equal(token_string, string("patch"))) {
                            if (next.kind != token_kind_symbol) {
                                log_error("expected label to indicate destination offset as first argument to directive 'patch'");
                                parse_error(&assembler, next);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            Label_Reference reference = { .is_patch = true, .destination_label_token_id = file_token_id, .parent_id = (Label_Id)(assembler.labels.values.count - 1) };

                            next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                            if (next.kind != ',') {
                                log_error("expected ',' after between first and second arguments to directive 'patch'");
                                parse_error(&assembler, next);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                            if (next.kind != token_kind_symbol && next.kind != token_kind_number) {
                                log_error("expected label or numeric literal to indicate address as second argument to directive 'patch'");
                                parse_error(&assembler, next);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            reference.token_id = file_token_id;
                            push(&assembler.label_references, reference);
                        } else if (string_equal(token_string, string("include"))) {
                            if (next.kind != token_kind_string) {
                                log_error("expected string following directive 'include'");
                                parse_error(&assembler, next);
                            }

                            String include_filepath = next_string;
                            if (string_equal(include_filepath, file->name)) {
                                log_error("cyclic inclusion of file '%'", fmt(String, file->name));
                                parse_error(&assembler, next);
                            }
                            Slice_u8 include_bytes = os_read_entire_file(&arena, include_filepath, 0xffff);
                            if (include_bytes.count == 0) os_exit(1);

                            Input_File new_file = { .bytes = include_bytes, .name = include_filepath, .tokens.arena = &arena };
                            File_Id new_file_id = (File_Id)map_put(&assembler.files, include_filepath, &new_file).index;

                            File_Cursor current = { .cursor = (u32)asm.count, .token_id = token_id_shift(file_token_id, 2) };
                            push(&file_stack, current);

                            File_Cursor next_file = { .cursor = 0, .token_id = { .file_id = new_file_id } };
                            push(&file_stack, next_file);
                            goto switch_file;
                        }

                        Label_Reference reference = { .token_id = file_token_id, .destination_address = (u16)rom.count, .width = 2, .parent_id = (Label_Id)(assembler.labels.values.count - 1) };
                        push(&assembler.label_references, reference);
                        rom.count += reference.width;
                    } break;
                    case token_kind_number: {
                        if (!assembler.insertion_mode.is_active) {
                            log_error("standalone number literals are only supported in insertion mode (e.g. in [ ... ])");
                            parse_error(&assembler, token);
                        }

                        u16 value = token.value;

                        bool is_byte = (token.base == 16 && token_string.count <= 2) || (token.base == 2 && token_string.count <= 8);
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
                        if (!assembler.insertion_mode.is_active) {
                            log_error("strings can only be used in insertion mode (e.g. inside [ ... ])");
                            parse_error(&assembler, token);
                        }

                        assert(rom.count + token_string.count <= 0x10000);
                        memcpy(rom.data + rom.count, token_string.data, token_string.count);
                        rom.count += token_string.count;
                    } break;
                    default: {
                        log_error("invalid syntax");
                        parse_error(&assembler, token);
                    }
                }
            }

            if (assembler.scoped_references.count != 0) {
                Scoped_Reference reference = assembler.scoped_references.data[0];
                Token token = *token_get(&assembler, reference.token_id);
                log_error("anonymous reference ('{') without matching '}'");
                parse_error(&assembler, token);
            }

            switch_file:;
        }

        for_slice (Label_Reference *, reference, assembler.label_references) {
            Label *source_label = 0, *destination_label = 0;
            Label **to_match[] = { &source_label, &destination_label };
            Token_Id match_against[] = { reference->token_id, reference->destination_label_token_id };
            u64 match_count = reference->is_patch ? 2 : 1;

            for (u64 i = 0; i < match_count; i += 1) {
                Label **match = to_match[i];

                Token_Id token_id = match_against[i];
                Token token = *token_get(&assembler, token_id);
                String reference_string = token_lexeme(&assembler, token_id);

                if (token.kind == token_kind_number) {
                    u16 value = token.value;
                    static Label dummy = {0};
                    dummy.address = value;
                    source_label = &dummy;
                    continue;
                }
                assert(token.kind == token_kind_symbol);

                Label parent = assembler.labels.values.data[reference->parent_id];
                Label *find = map_get(&parent.children, reference_string).pointer;
                if (find == 0) find = map_get(&assembler.labels, reference_string).pointer;
                if (find != 0) {
                    *match = find;
                    goto done;
                }

                done:
                if (*match == 0) {
                    log_error("no such label '%'", fmt(String, reference_string));
                    parse_error(&assembler, token, .file_id = token_id.file_id);
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
            assert(arguments.count == 4);
            String output_path = arguments.data[3];
            os_write_entire_file(arena, output_path, rom.slice);
        }
    }

    bool should_run = command == command_script || command == command_run;
    if (should_run) {
        if (rom.count == 0) os_exit(1);

        Vm vm = { .arena = &arena };
        memcpy(vm.memory, rom.data, rom.count);

        bool print_memory = false;
        if (print_memory) {
            Scratch scratch = scratch_begin(&arena);
            String_Builder builder = {0};
            {
                string_builder_print(&builder, "MEMORY ===\n");
                for (u16 token_id = 0x100; token_id < rom.count; token_id += 1) {
                    u8 byte = vm.memory[token_id];

                    String mode_string = string("");
                    switch ((byte & 0xe0) >> 5) {
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
            os_write(builder.string);
            scratch_end(scratch);
        }

        Gfx_Render_Context *gfx = &vm.gfx;
        *gfx = gfx_window_create(&arena, "bici", vm_screen_initial_width, vm_screen_initial_height);
        ShowCursor(false);
        gfx->font = gfx_font_default_3x5;

        // TODO(felix): program should control this
        gfx_clear(gfx, 0);

        u16 vm_on_system_start_pc = vm_load16(&vm, vm_device_system + vm_system_start);
        vm_run_to_break(&vm, vm_on_system_start_pc);

        while (!gfx_window_should_close(gfx)) {
            u16 vm_on_screen_update_pc = vm_load16(&vm, vm_device_screen | vm_screen_update);
            vm_run_to_break(&vm, vm_on_screen_update_pc);
        }

        u16 vm_on_system_quit_pc = vm_load16(&vm, vm_device_system + vm_system_quit);
        vm_run_to_break(&vm, vm_on_system_quit_pc);

        print("working stack (bottom->top): { ");
        for (u8 token_id = 0; token_id < vm.stacks[0].data; token_id += 1) print("% ", fmt(u64, vm.stacks[0].memory[token_id], .base = 16));
        print("}\nreturn  stack (bottom->top): { ");
        for (u8 token_id = 0; token_id < vm.stacks[VM_MODE_RETURN_STACK].data; token_id += 1) print("% ", fmt(u64, vm.stacks[VM_MODE_RETURN_STACK].memory[token_id], .base = 16));
        print("}\n");
    }

    arena_deinit(&arena);
}
