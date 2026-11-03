#define BASE_IMPLEMENTATION
#include "base/base.h"

#include "base/base_platform_wingdi.c"

#define Vm_Screen_INITIAL_WIDTH 640
#define Vm_Screen_INITIAL_HEIGHT 360

typedef enum {
    Vm_Mode_SHORT        = 1 << 5,
    Vm_Mode_RETURN_STACK = 1 << 6,
    Vm_Mode_KEEP         = 1 << 7,
    VM_MODE_MASK         = Vm_Mode_SHORT | Vm_Mode_RETURN_STACK | Vm_Mode_KEEP,
} Vm_Mode;
#define VM_OPCODE_MASK (~(VM_MODE_MASK))

#define VM_OPCODE_TABLE \
    /* name,  byte, is_immediate */\
    X(break,  0x00, 0)/* NO MODE */\
    X(push,   0x01, 1)/* NO .k MODE */\
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

typedef enum {
    Vm_Device_SYSTEM   = 0x00,
    Vm_Device_CONSOLE  = 0x10,
    Vm_Device_SCREEN   = 0x20,
    Vm_Device_MOUSE    = 0x30,
    Vm_Device_KEYBOARD = 0x40,
    Vm_Device_FILE     = 0x50,
} Vm_Device;

typedef enum {
    Vm_System_RESERVED = 0x0,
    Vm_System_END      = 0x2,
    Vm_System_START    = 0x4,
    Vm_System_QUIT     = 0x6,
    Vm_System_COLOUR_0 = 0x8,
    Vm_System_COLOUR_1 = 0xa,
    Vm_System_COLOUR_2 = 0xc,
    Vm_System_COLOUR_3 = 0xe,
} Vm_System_Action;

typedef enum {
    Vm_Console_PRINT = 0x0,
} Vm_Console_Action;

typedef enum {
    Vm_Screen_UPDATE = 0x0,
    Vm_Screen_WIDTH  = 0x2,
    Vm_Screen_HEIGHT = 0x4,
    Vm_Screen_X      = 0x6,
    Vm_Screen_Y      = 0x8,
    Vm_Screen_PIXEL  = 0xa,
    Vm_Screen_SPRITE = 0xb,
    Vm_Screen_DATA   = 0xc,
    Vm_Screen_AUTO   = 0xe,
} Vm_Screen_Action;

typedef enum {
    Vm_Mouse_X = 0x0,
    Vm_Mouse_Y = 0x2,
    Vm_Mouse_LEFT_BUTTON = 0x4,
    Vm_Mouse_RIGHT_BUTTON = 0x5,
} Vm_Mouse_Action;

typedef enum {
    Vm_Keyboard_KEY_VALUE = 0x0,
    Vm_Keyboard_KEY_STATE = 0x1,
} Vm_Keyboard_Action;

typedef enum {
    Vm_File_NAME   = 0x0,
    Vm_File_LENGTH = 0x2,
    Vm_File_CURSOR = 0x4,
    Vm_File_READ   = 0x6,
    Vm_File_COPY   = 0x8,
} Vm_File_Action;

enum {
    #define X(name, val, ...) vm_opcode_##name = val,
    VM_OPCODE_TABLE
    #undef X
    VM_OPCODE_MAX_VALUE_PLUS_ONE,
    VM_OPCODE_MAX_VALUE = VM_OPCODE_MAX_VALUE_PLUS_ONE - 1,
};

static const char *vm_opcode_name[VM_OPCODE_MAX_VALUE_PLUS_ONE] = {
    #define X(name, val, ...) [val] = #name,
    VM_OPCODE_TABLE
    #undef X
};

typedef struct {
    u8 memory[0x100];
    u8 data;
} Vm_Stack;

typedef struct {
    Arena *arena;

    u8 memory[0x10000];
    Vm_Stack stacks[2];
    u8 active_stack;
    u16 program_counter;
    u8 current_mode;

    u8 *file_bytes;
} Vm;
static Vm global_vm = {0};

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

#define vm_stack vm->stacks[vm->active_stack >> 6].memory
#define vm_sp vm->stacks[vm->active_stack >> 6].data

#define vm_s(data) vm_stack[(u8)(data)]
#define vm_mem(data) vm->memory[(u16)(data)]

static u8   vm_get8(Vm *vm, u8 i_back) { return *(vm_stack + (u8)(vm_sp - i_back)); }
static void vm_push8(Vm *vm, u8 byte) { vm_s(vm_sp) = byte; vm_sp += 1; }
static u8   vm_pop8(Vm *vm) { u8 val = vm_s(vm_sp - 1); if (!(vm->current_mode & Vm_Mode_KEEP)) vm_sp -= 1; return val; }
static u8   vm_load8(Vm *vm, u16 addr) { return vm_mem(addr); }
static void vm_store8(Vm *vm, u16 addr, u8 val) { vm_mem(addr) = val; }

static u16  vm_get16(Vm *vm, u8 i_back) { return (u16)vm_s(vm_sp - 2 * i_back + 1) | (u16)(vm_s(vm_sp - 2 * i_back) << 8); }
static void vm_push16(Vm *vm, u16 byte2) { vm_push8(vm, byte2 >> 8); vm_push8(vm, (u8)byte2); }
static u16  vm_pop16(Vm *vm) { u16 val = (u16)vm_s(vm_sp - 1) | (u16)(vm_s(vm_sp - 2) << 8); if (!(vm->current_mode & Vm_Mode_KEEP)) vm_sp -= 2; return val; }
static u16  vm_load16(Vm *vm, u16 addr) { return (u16)(((u16)vm_mem(addr) << 8) | (u16)vm_mem(addr + 1)); }
static void vm_store16(Vm *vm, u16 addr, u16 val) { vm_mem(addr) = (u8)(val >> 8); vm_mem(addr + 1) = (u8)val; } // TODO(felix): ensure correct

static inline u32 rgb_from_colour(Vm *vm, u8 colour_index) {
    assert(colour_index < 4);
    Vm_System_Action colour_offset = Vm_System_COLOUR_0 + colour_index * 2;
    u16 colour = get16(&vm->memory[Vm_Device_SYSTEM | (Vm_Device)colour_offset]);
    u32 r = ((u32)colour & 0x0f00) >> 8;
    u32 g = ((u32)colour & 0x00f0) >> 4;
    u32 b = ((u32)colour & 0x000f) >> 0;
    u32 rgb = (r << 20) | (r << 16) | (g << 12) | (g << 8) | (b << 4) | b;
    return rgb;
}

static void vm_run_to_break(Platform *platform, Vm *vm, u16 program_counter) {
    vm->program_counter = program_counter;
    u16 add = 1;
    for (; true; vm->program_counter += add) {
        add = 1;
        u8 instruction = vm_mem(vm->program_counter);

        vm->current_mode = instruction & VM_MODE_MASK;
        vm->active_stack = instruction & Vm_Mode_RETURN_STACK;

        switch (instruction & VM_OPCODE_MASK) {
            case vm_opcode_break: return;
            case vm_opcode_jmi: vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; continue;
            case vm_opcode_jei: if ( vm_pop8(vm)) { vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; } else add = 3; continue;
            case vm_opcode_jni: if (!vm_pop8(vm)) { vm->program_counter = vm_load16(vm, vm->program_counter + 1); add = 0; } else add = 3; continue;
            case vm_opcode_jsi: {
                vm->active_stack = Vm_Mode_RETURN_STACK;
                vm_push16(vm, vm->program_counter + 3);
                vm->active_stack = 0;
                vm->program_counter = vm_load16(vm, vm->program_counter + 1);
                add = 0;
                continue;
            } break;
            default: break;
        }

        switch (instruction & Vm_Mode_SHORT) {
            case 0: switch (instruction & VM_OPCODE_MASK) {
                case vm_opcode_jmi: case vm_opcode_jei: case vm_opcode_jni: case vm_opcode_jsi: case vm_opcode_break:
                    panic("reached full-byte opcodes in generic switch case");
                case vm_opcode_push:   vm_push8(vm, vm_load8(vm, vm->program_counter + 1)); add = 1 + 1; break;
                case vm_opcode_drop:   (void)vm_pop8(vm); break;
                case vm_opcode_swap:   { u8 c = vm_pop8(vm), b = vm_pop8(vm); vm_push8(vm, c); vm_push8(vm, b); } break;
                case vm_opcode_rot:    { u8 c = vm_pop8(vm), b = vm_pop8(vm), a = vm_pop8(vm); vm_push8(vm, b); vm_push8(vm, c); vm_push8(vm, a); } break;
                case vm_opcode_dup:    assert(!(vm->current_mode & Vm_Mode_KEEP)); vm_push8(vm, vm_get8(vm, 1)); break;
                case vm_opcode_over:   assert(!(vm->current_mode & Vm_Mode_KEEP)); vm_push8(vm, vm_get8(vm, 2)); break;
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
                    vm->active_stack = Vm_Mode_RETURN_STACK;
                    vm_push16(vm, vm->program_counter + 1);
                    vm->active_stack = 0;
                    vm->program_counter = vm_pop16(vm);
                    add = 0;
                } break;
                case vm_opcode_stash:  { // TODO(felix): fix
                    u8 val = vm_pop8(vm);
                    vm->active_stack = vm->active_stack == 0 ? Vm_Mode_RETURN_STACK : 0;
                    vm_push8(vm, val);
                    vm->active_stack = vm->active_stack == 0 ? Vm_Mode_RETURN_STACK : 0;
                } break;
                case vm_opcode_load:   { u16 addr = vm_pop16(vm); u8 val = vm_load8(vm, addr); vm_push8(vm, val); } break;
                case vm_opcode_store:  { u16 addr = vm_pop16(vm); u8 val = vm_pop8(vm); vm_store8(vm, addr, val); } break;
                case vm_opcode_read:   {
                    u8 Vm_Device_AND_ACTION = vm_pop8(vm);
                    Vm_Device device = Vm_Device_AND_ACTION & 0xf0;
                    u8 action = Vm_Device_AND_ACTION & 0x0f;
                    u8 to_push = 0;
                    switch (device) {
                        case Vm_Device_MOUSE: switch (action) {
                            case Vm_Mouse_LEFT_BUTTON: {
                                if (platform->frame.mouse_clicked[App_Mouse_LEFT]) to_push |= 0xf0;
                                if (platform->frame.mouse_down[App_Mouse_LEFT])    to_push |= 0x0f;
                            } break;
                            case Vm_Mouse_RIGHT_BUTTON: {
                                if (platform->frame.mouse_clicked[App_Mouse_RIGHT]) to_push |= 0xf0;
                                if (platform->frame.mouse_down[App_Mouse_RIGHT])    to_push |= 0x0f;
                            } break;
                            default: panic("[read.1] invalid action #%02x for mouse device", action);
                        } break;
                        case Vm_Device_KEYBOARD: switch (action) {
                            case Vm_Keyboard_KEY_STATE: {
                                u8 key = vm->memory[Vm_Device_KEYBOARD | (Vm_Device)Vm_Keyboard_KEY_VALUE];
                                bool key_down = platform->frame.key_down[key];
                                if (key_down) to_push |= 0x0f;
                                bool key_pressed = platform->frame.key_pressed[key];
                                if (key_pressed) to_push |= 0xf0;
                            } break;
                            default: panic("[read.1] invalid action #%02x for keyboard device", action);
                        } break;
                        default: panic("[read.1] invalid device #%02x", device);
                    }
                    vm_push8(vm, to_push);
                    // // TODO(felix): not sure about this. do I use the same address for different purposes depending on whether I'm reading or writing?
                    // vm->memory[Vm_Device_AND_ACTION] = to_push;
                } break;
                case vm_opcode_write:  {
                    u8 Vm_Device_AND_ACTION = vm_pop8(vm);
                    Vm_Device device = Vm_Device_AND_ACTION & 0xf0;
                    u8 action = Vm_Device_AND_ACTION & 0x0f;
                    u8 argument = vm_pop8(vm);
                    switch (device) {
                        case Vm_Device_SCREEN: {
                            u8 *x_address = &vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_X];
                            u8 *y_address = &vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_Y];
                            u16 x = get16(x_address);
                            u16 y = get16(y_address);
                            switch (action) {
                                case Vm_Screen_PIXEL: {
                                    if (x >= Vm_Screen_INITIAL_WIDTH) break;
                                    if (y >= Vm_Screen_INITIAL_HEIGHT) break;

                                    u8 fill_x = argument & 0x80;
                                    u8 fill_y = argument & 0x40;

                                    u8 colour = argument & 0x0f;
                                    if (colour > 3) panic("[write:screen/pixel] colour #%02x is invalid; there are only 4 palette colours", colour);

                                    u32 rgb = rgb_from_colour(vm, colour);

                                    u16 width = fill_x ? (Vm_Screen_INITIAL_WIDTH - x) : 1;
                                    u16 height = fill_y ? (Vm_Screen_INITIAL_HEIGHT - y) : 1;

                                    if (fill_x && fill_y) {
                                        panic("TODO(felix)");
                                        (void)width;
                                        (void)height;
                                        (void)rgb;
                                        // gfx_draw_rectangle(&vm->gfx, x, y, width, height, rgb);
                                    } else if (fill_x) {
                                        panic("TODO(felix)");
                                        // gfx_draw_line(&vm->gfx, (V2){ .x = (f32)x, .y = (f32)y }, (V2){ .x = (f32)Vm_Screen_INITIAL_WIDTH, .y = (f32)y }, 1.f, rgb);
                                    } else if (fill_y) {
                                        unreachable;
                                    } else {
                                        panic("TODO(felix)");
                                        // gfx_set_pixel(&vm->gfx, x, y, rgb);
                                    }
                                } break;
                                case Vm_Screen_SPRITE: {
                                    if (x >= Vm_Screen_INITIAL_WIDTH) break;
                                    if (y >= Vm_Screen_INITIAL_HEIGHT) break;

                                    u8 two_bits_per_pixel = argument & 0x80;
                                    u8 use_background_layer = argument & 0x40;
                                    u8 flip_y = argument & 0x20;
                                    u8 flip_x = argument & 0x10;
                                    u32 colours[] = { rgb_from_colour(vm, argument & 0x03), rgb_from_colour(vm, (argument & 0x0c) >> 2) };

                                    // TODO(felix)
                                    assert(!two_bits_per_pixel);
                                    assert(!flip_y);
                                    assert(!flip_x);

                                    u16 column_count = MIN(8, Vm_Screen_INITIAL_WIDTH - x);
                                    u16 row_count = MIN(8, Vm_Screen_INITIAL_HEIGHT - y);

                                    u8 *sprite = &vm->memory[get16(&vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_DATA])];
                                    for (u16 row = 0; row < row_count; row += 1, sprite += 1) {
                                        for (u16 column = 0; column < column_count; column += 1) {
                                            u8 shift = 7 - (u8)column;
                                            u8 colour = (*sprite & (1 << shift)) >> shift;
                                            if (!use_background_layer && colour == 0) continue;

                                            panic("TODO(felix)");
                                            (void)colours;
                                            // gfx_set_pixel(&vm->gfx, x + column, y + row, colours[colour]);
                                        }
                                    }

                                    u8 auto_x = vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_AUTO] & 0x01;
                                    u8 auto_y = vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_AUTO] & 0x02;
                                    if (auto_x) x += 8;
                                    if (auto_y) y += 8;
                                } break;
                                case Vm_Screen_AUTO: break;
                                default: panic("[write.1] invalid action #%02x for screen device", action);
                            }
                            set16(x_address, x);
                            set16(y_address, y);
                        } break;
                        case Vm_Device_KEYBOARD: switch (action) {
                            case Vm_Keyboard_KEY_VALUE: break;
                            default: panic("[write.1] invalid action #%02x for keyboard device", action);
                        } break;
                        default: panic("[write.1] invalid device #%02x", device);
                    }
                    vm->memory[Vm_Device_AND_ACTION] = argument;
                } break;
                default: panic("unreachable %s{#%02x}", vm_opcode_name[instruction & VM_OPCODE_MASK], instruction);
            } break;
            case Vm_Mode_SHORT: switch (instruction & VM_OPCODE_MASK) {
                case vm_opcode_jmi: case vm_opcode_jei: case vm_opcode_jni: case vm_opcode_jsi: case vm_opcode_break:
                    panic("reached full-byte opcodes in generic switch case");
                case vm_opcode_push:   vm_push16(vm, vm_load16(vm, vm->program_counter + 1)); add = 2 + 1; break;
                case vm_opcode_drop:   (void)vm_pop16(vm); break;
                case vm_opcode_swap:   { u16 c = vm_pop16(vm), b = vm_pop16(vm); vm_push16(vm, c); vm_push16(vm, b); } break;
                case vm_opcode_rot:    { u16 c = vm_pop16(vm), b = vm_pop16(vm), a = vm_pop16(vm); vm_push16(vm, b); vm_push16(vm, c); vm_push16(vm, a); } break;
                case vm_opcode_dup:    assert(!(vm->current_mode & Vm_Mode_KEEP)); vm_push16(vm, vm_get16(vm, 1)); break;
                case vm_opcode_over:   assert(!(vm->current_mode & Vm_Mode_KEEP)); vm_push16(vm, vm_get16(vm, 2)); break;
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
                    vm->active_stack = Vm_Mode_RETURN_STACK;
                    vm_push16(vm, vm->program_counter + 1);
                    vm->active_stack = 0;
                    vm->program_counter = vm_pop16(vm);
                    add = 0;
                } break;
                case vm_opcode_stash:  { u16 val = vm_pop16(vm); vm->active_stack = Vm_Mode_RETURN_STACK; vm_push16(vm, val); vm->active_stack = 0; } break;
                case vm_opcode_load:   { u16 addr = vm_pop16(vm); u16 val = vm_load16(vm, addr); vm_push16(vm, val); } break;
                case vm_opcode_store:  { u16 addr = vm_pop16(vm); u16 val = vm_pop16(vm); vm_store16(vm, addr, val); } break;
                case vm_opcode_read:   {
                    u8 Vm_Device_AND_ACTION = vm_pop8(vm);
                    Vm_Device device = Vm_Device_AND_ACTION & 0xf0;
                    u8 action = Vm_Device_AND_ACTION & 0x0f;
                    u16 to_push = get16(&vm->memory[Vm_Device_AND_ACTION]);
                    switch (device) {
                        case Vm_Device_SCREEN: switch (action) {
                            case Vm_Screen_WIDTH: to_push = Vm_Screen_INITIAL_WIDTH; break;
                            case Vm_Screen_HEIGHT: to_push = Vm_Screen_INITIAL_HEIGHT; break;
                            default: panic("[read.2] invalid action #%02x for screen device", action);
                        } break;
                        case Vm_Device_MOUSE: {
                            panic("TODO(felix)");
                            // V2 virtual_over_real = v2_div(platform->frame.virtual_window_size, platform->frame.real_window_size);
                            // V2 virtual_mouse_position = v2_mul(platform->frame.real_mouse_position, virtual_over_real);
                            switch (action) {
                                case Vm_Mouse_X: {
                                    panic("TODO(felix)");
                                    // to_push = (u16)virtual_mouse_position.x;
                                } break;
                                case Vm_Mouse_Y: {
                                    panic("TODO(felix)");
                                    // to_push = (u16)virtual_mouse_position.y;
                                } break;
                                default: panic("[read.2] invalid action #%02x for mouse device", action);
                            }
                        } break;
                        case Vm_Device_FILE: {
                            assert(get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_NAME]) != 0);
                            switch (action) {
                                case Vm_File_LENGTH: break;
                                case Vm_File_READ: {
                                    assert(get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_CURSOR]) < get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_LENGTH]));
                                    to_push = get16(&vm->file_bytes[get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_CURSOR])]);
                                } break;
                                default: panic("[read.2] invalid action #%02x for file device", action);
                            }
                        } break;
                        default: panic("[read.2] invalid device #%02x", device);
                    }
                    set16(&vm->memory[Vm_Device_AND_ACTION], to_push);
                    vm_push16(vm, to_push);
                } break;
                case vm_opcode_write:  {
                    u8 Vm_Device_AND_ACTION = vm_pop8(vm);
                    Vm_Device device = Vm_Device_AND_ACTION & 0xf0;
                    u8 action = Vm_Device_AND_ACTION & 0x0f;
                    u16 argument = vm_pop16(vm);
                    switch (device) {
                        case Vm_Device_CONSOLE: switch (action) {
                            case Vm_Console_PRINT: {
                                u16 str_addr = argument;
                                u8 str_count = vm_load8(vm, str_addr);
                                String str = { .data = (const char *)(vm->memory + str_addr + 1), .count = str_count };
                                print("%S", str);
                            } break;
                            default: panic("[write.2] invalid action #%02x for console device", action);
                        } break;
                        case Vm_Device_SCREEN: switch (action) {
                            case Vm_Screen_X: {
                                set16(&vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_X], argument);
                            } break;
                            case Vm_Screen_Y: {
                                set16(&vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_Y], argument);
                            } break;
                            case Vm_Screen_DATA: break;
                            default: panic("[write.2] invalid action #%02x for screen device", action);
                        } break;
                        case Vm_Device_FILE: switch (action) {
                            case Vm_File_NAME: {
                                u16 string_address = argument;
                                u8 string_length = vm_load8(vm, string_address);
                                String file_name = { .data = (const char *)&vm->memory[string_address + 1], .count = string_length };

                                bool read_ok = false;
                                String bytes = os_read_entire_file(vm->arena, cstring_from_string(vm->arena, file_name), &read_ok);
                                ensure(read_ok);

                                vm->file_bytes = (u8 *)bytes.data;

                                assert(bytes.count <= 0xffff);
                                set16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_LENGTH], (u16)bytes.count);
                            } break;
                            case Vm_File_LENGTH: {
                                assert(argument <= get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_LENGTH]));
                            } break;
                            case Vm_File_CURSOR: {
                                assert(get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_NAME]) != 0);
                            } break;
                            case Vm_File_COPY: {
                                assert(get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_NAME]) != 0);
                                assert(vm->file_bytes != 0);
                                assert(get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_LENGTH]) != 0);
                                u16 address_to_copy_to = argument;
                                memcpy(vm->memory + address_to_copy_to, vm->file_bytes, get16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_LENGTH]));
                            } break;
                            default: panic("[write.2] invalid action #%02x for file device", action);
                        } break;
                        default: panic("[write.2] invalid device #%02x", device);
                    }
                    set16(&vm->memory[Vm_Device_AND_ACTION], argument);
                } break;
                default: panic("unreachable %s{#%02x}", vm_opcode_name[instruction & VM_OPCODE_MASK], instruction);
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
    return false;
}

static void write_u16_swap_bytes(u8 rom[0x10000], u64 index, u16 value) {
    assert(index + 1 < 0x10000);
    u8 byte2 = (u8)(value);
    u8 byte1 = (u8)(value >> 8);
    rom[index] = byte1;
    rom[index + 1] = byte2;
}

static force_inline bool is_starting_symbol_character(u8 c) {
    return ascii_is_alpha(c) || c == '_';
}

typedef u8 Token_Kind;
enum {
    Token_Kind_ASCII_DELIMITER = 128,

    Token_Kind_SYMBOL,
    Token_Kind_NUMBER,
    Token_Kind_STRING,
    Token_Kind_OPMODE,

    Token_Kind_COUNT,
};

typedef u8 File_Id;
#define MAX_FILES 255

typedef struct {
    u32 start_index;
    u8 length;
    Token_Kind kind;
    u16 value;
    u8 base;
} Token;
typedef Array(Token) Array_Token;

typedef struct {
    File_Id file_id;
    i32 index;
} Token_Id;

typedef struct {
    Token_Id token_id;
    u16 address;
    // TODO(felix)
    int children;
    // Map_Label children;
} Label;
typedef u32 Label_Id;
#define MAX_LABELS 1024

typedef struct {
    Token_Id token_id;
    union { u16 destination_address; Token_Id destination_label_token_id; };
    u8 width;
    bool is_patch;
    Label_Id parent_id;
} Label_Reference;
typedef Array(Label_Reference) Array_Label_Reference;

typedef struct {
    u16 address;
    Token_Id token_id;
    bool is_relative;
} Scoped_Reference;
typedef Array(Scoped_Reference) Array_Scoped_Reference;

typedef struct {
    bool is_active;
    bool has_relative_reference;
} Insertion_Mode;

typedef struct {
    String name;
    String bytes;
    Array_Token tokens;
} Input_File;

typedef struct {
    Input_File files[MAX_FILES];
    int files_count;
    Label labels[MAX_LABELS];
    int labels_count;
    File_Id file_id;
    Array_Label_Reference label_references;
    Array_Scoped_Reference scoped_references;
    Insertion_Mode insertion_mode;
} Assembler_Context;

static Token_Id token_id_shift(Token_Id token_id, i32 shift) {
    Token_Id result = token_id;
    result.index += shift;
    return result;
}

static Input_File *token_get_file(Assembler_Context *assembler, Token_Id token_id) {
    File_Id file_id = token_id.file_id;
    panic("TODO(felix)");
    Input_File *file = 0;
    (void)assembler;
    (void)file_id;
    // Input_File *file = &assembler->files.values.data[file_id];
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
    String asm = file->bytes;
    String token_string = string_range(asm, token.start_index, token.start_index + token.length);
    return token_string;
}

typedef struct {
    Assembler_Context *assembler;
    Token token;
    u32 start_index;
    u32 length;
    File_Id file_id;
} Parse_Error_Arguments;
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

    String text = {0};
    panic("TODO(felix)");
    // String text = assembler->files.values.data[file_id].bytes;
    String file_name = {0};
    panic("TODO(felix)");
    // String file_name = assembler->files.values.data[file_id].name;
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

    print("note: '%S' in %S:%llu:%llu\n", lexeme, file_name, row, column);

    os_exit(1);
}

#define USAGE_HELP_TEXT "usage: bici <compile|run|script> <file...>"

typedef struct {
    Token_Id token_id;
    u32 cursor;
} File_Cursor;
typedef Array(File_Cursor) Array_File_Cursor;

static void app_start(Platform *platform) {
    Arena arena = arena_init(64 * 1024 * 1024);
    u64 arguments_count = 0;
    const char **arguments = os_get_arguments(&arena, &arguments_count);

    if (arguments_count < 3) {
        log_error("%s", USAGE_HELP_TEXT);
        os_exit(1);
    }

    typedef enum {
        Command_COMPILE,
        Command_RUN,
        Command_SCRIPT,

        Command_COUNT,
    } Command;
    const char *command_string = arguments[1];

    Command command = 0;
    if (cstring_equals(command_string, "compile")) {
        if (arguments_count != 4) {
            log_error("usage: bici compile <file.asm> <file.rom>");
            os_exit(1);
        }
        command = Command_COMPILE;
    } else if (cstring_equals(command_string, "run")) {
        if (arguments_count != 3) {
            log_error("usage: bici run <file.rom>");
            os_exit(1);
        }
        command = Command_RUN;
    } else if (cstring_equals(command_string, "script")) {
        if (arguments_count != 3) {
            log_error("usage: bici script <file.asm>");
            os_exit(1);
        }
        command = Command_SCRIPT;
    } else {
        log_error("no such command '%s'\n%s", command_string, USAGE_HELP_TEXT);
        os_exit(1);
    }

    const char *input_filepath = arguments[2];
    bool read_ok = false;
    String input_bytes = os_read_entire_file(&arena, input_filepath, &read_ok);
    assert(read_ok);
    assert(input_bytes.count <= 0x10000);

    u8 rom_bytes[0x10000] = {0};
    u8 *rom = rom_bytes;
    u16 rom_count = 0;

    bool should_compile = command == Command_COMPILE || command == Command_SCRIPT;
    if (!should_compile) rom = (u8 *)input_bytes.data;
    else {
        const char *opcode_from_name[VM_OPCODE_MAX_VALUE_PLUS_ONE * 16] = {0};
        for (u8 opcode = 0; opcode < VM_OPCODE_MAX_VALUE_PLUS_ONE; opcode += 1) {
            const char *name = vm_opcode_name[opcode];
            u64 hash = hash_djb2(name, cstring_length(name));

            assert(is_power_of_2(sizeof opcode_from_name / sizeof *opcode_from_name));
            u64 index = hash & ((sizeof opcode_from_name / sizeof *opcode_from_name) - 1);

            const char **name_in_map = &opcode_from_name[index];
            assert(*name_in_map == 0);
            *name_in_map = vm_opcode_name[opcode];
        }

        Assembler_Context assembler = {0};

        // Input_File main_file = { .bytes = input_bytes, .name = input_filepath };
        File_Id main_file_id = 0;
        panic("TODO(felix)");
        // File_Id main_file_id = (File_Id)map_put(&assembler.files, input_filepath, &main_file).index;

        Array_File_Cursor file_stack = {0};
        push(&arena, &file_stack, (File_Cursor){ .token_id.file_id = main_file_id });

        while (file_stack.count > 0) {
            // File_Cursor file_cursor = pop(file_stack);
            File_Cursor file_cursor = {0};
            panic("TODO(felix)");
            assembler.file_id = file_cursor.token_id.file_id;

            // Input_File *file = &assembler.files.values.data[assembler.file_id];
            Input_File *file = 0;
            panic("TODO(felix)");

            String asm = file->bytes;

            for (u32 cursor = file_cursor.cursor; cursor < asm.count;) {
                while (cursor < asm.count && ascii_is_whitespace(asm.data[cursor])) cursor += 1;
                if (cursor == asm.count) break;

                u32 start_index = cursor;
                Token_Kind kind = 0;
                u16 value = 0;
                u8 base = 0;
                if (is_starting_symbol_character(asm.data[cursor])) {
                    kind = Token_Kind_SYMBOL;
                    cursor += 1;

                    for (; cursor < asm.count; cursor += 1) {
                        u8 c = asm.data[cursor];
                        bool is_symbol_character = is_starting_symbol_character(c) || ascii_is_decimal(c);
                        if (!is_symbol_character) break;
                    }
                } else switch (asm.data[cursor]) {
                    case '0': {
                        kind = Token_Kind_NUMBER;

                        if (cursor + 1 == asm.count || (asm.data[cursor + 1] != 'x' && asm.data[cursor + 1] != 'b')) {
                            log_error("expected 'x' or 'b' after '0' to begin binary or hexadecimal literal");
                            parse_error(.start_index = cursor + 1, 1);
                        }
                        u8 base_char = asm.data[cursor + 1];
                        cursor += 2;
                        start_index = cursor;

                        if (cursor == asm.count) {
                            log_error("expected digits following '0%c'", base_char);
                            parse_error(.start_index = cursor - 2, 2);
                        }

                        for (; cursor < asm.count; cursor += 1) {
                            u8 c = asm.data[cursor];
                            if (base_char == 'x' && ascii_is_hexadecimal(c)) continue;
                            if (base_char == 'b' && (c == '0' || c == '1')) continue;
                            if (ascii_is_whitespace(c)) break;
                            log_error("expected base_char %c digit", base_char);
                            parse_error(.start_index = cursor);
                        }

                        base = base_char == 'b' ? 2 : 16;
                        String number_string = string_range(asm, start_index, cursor);
                        u64 value_unbounded = int_from_string_base(number_string, base);
                        if (value_unbounded >= 0x10000) {
                            log_error("0d%llu is too large to fit in 16 bits", value_unbounded);
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
                        kind = Token_Kind_STRING;

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
                        kind = Token_Kind_OPMODE;

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
                            log_error("a valid op mode contains 1, 2, or 3 characters (as in op.2kr), but this one has %llu", length);
                            parse_error(.start_index = start_index, (u8)length);
                        }
                    } break;
                    default: {
                        log_error("invalid syntax '%c'", asm.data[cursor]);
                        parse_error(.start_index = cursor);
                    }
                }

                u64 length = cursor - start_index;
                if (kind == Token_Kind_STRING) length -= 1;
                assert(length <= 255);

                Token token = {
                    .start_index = start_index,
                    .length = (u8)length,
                    .kind = kind,
                    .value = value,
                    .base = base,
                };
                push(&arena, &file->tokens, token);
            }

            bool parsing_relative_label = false;
            for (Token_Id file_token_id = file_cursor.token_id; (u64)file_token_id.index < file->tokens.count; file_token_id.index += 1) {
                Token token = *token_get(&assembler, file_token_id);
                String token_string = token_lexeme(&assembler, file_token_id);

                if (assembler.insertion_mode.is_active) {
                    switch (token.kind) {
                        case Token_Kind_SYMBOL: case ']': case '$': case '/': case Token_Kind_NUMBER: case Token_Kind_STRING: break;
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
                        Scoped_Reference reference = { .address = rom_count };
                        push(&arena, &assembler.scoped_references, reference);
                        rom_count += 2;
                    } break;
                    case '}': {
                        if (assembler.scoped_references.count == 0) {
                            log_error("'}' has no matching '{'");
                            parse_error(&assembler, token);
                        }

                        // Scoped_Reference reference = pop(assembler.scoped_references);
                        Scoped_Reference reference = {0};
                        panic("TODO(felix)");
                        if (reference.is_relative) {
                            log_error("expected to resolve absolute reference (from '{') but found unresolved relative reference");
                            parse_error(&assembler, token);
                        }

                        write_u16_swap_bytes(rom, reference.address, rom_count);
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
                            Scoped_Reference reference = { .address = rom_count, .is_relative = true };
                            push(&arena, &assembler.scoped_references, reference);
                            rom_count += 1;
                            file_token_id = token_id_shift(file_token_id, 1);
                        }
                    } break;
                    case ']': {
                        if (!assembler.insertion_mode.is_active) {
                            log_error("']' has no matching '['");
                            parse_error(&assembler, token);
                        }

                        if (assembler.insertion_mode.has_relative_reference) {
                            // Scoped_Reference reference = pop(assembler.scoped_references);
                            Scoped_Reference reference = {0};
                            panic("TODO(felix)");
                            if (!reference.is_relative) {
                                log_error("expected to resolve relative reference (from '[$') but found unresolved absolute reference; is there an unmatched '{'?");
                                parse_error(&assembler, token);
                            }

                            u64 relative_difference = rom_count - reference.address - 1;
                            if (relative_difference > 255) {
                                log_error("relative difference from '[$' is %llu bytes, but the maximum is 255", relative_difference);
                                parse_error(&assembler, token);
                            }

                            u8 *location_to_patch = &rom[reference.address];
                            *location_to_patch = (u8)relative_difference;
                        }

                        assembler.insertion_mode = (Insertion_Mode){0};
                    } break;
                    case '/': {
                        if (next.kind != Token_Kind_SYMBOL) {
                            log_error("expected relative label following '/'");
                            parse_error(&assembler, token);
                        }
                        parsing_relative_label = true;
                    } break;
                    case Token_Kind_SYMBOL: {
                        if (parsing_relative_label) {
                            if (next.kind != ':') {
                                log_error("expected ':' ending relative label definition");
                                parse_error(&assembler, next);
                            }
                        }

                        bool is_label = next.kind == ':';
                        if (is_label) {
                            // bool is_redefinition = map_get(&assembler.labels, token_string).pointer != 0;
                            bool is_redefinition = true;
                            panic("TODO(felix)");
                            if (is_redefinition) {
                                log_error("redefinition of label '%S'", token_string);
                                parse_error(&assembler, token);
                            }

                            // Label new_label = { .token_id = file_token_id, .address = rom_count };
                            // map_make(&arena, &new_label.children, 256);
                            panic("TODO(felix)");

                            // Map_Label *labels = 0;
                            int *labels = 0;
                            panic("TODO(felix)");
                            if (parsing_relative_label) {
                                // Label *parent = slice_get_last(assembler.labels.values);
                                panic("TODO(felix)");
                                // Map_Label *parent_labels = &parent->children;
                                int *parent_labels = 0;
                                panic("TODO(felix)");
                                labels = parent_labels;

                                // bool is_redefinition = map_get(parent_labels, token_string).index != 0;
                                is_redefinition = true;
                                panic("TODO(felix)");
                                if (is_redefinition) {
                                    log_error("redefinition of label '%S'", token_string);
                                    parse_error(&assembler, token);
                                }

                                parsing_relative_label = false;
                            } else {
                                labels = &assembler.labels;
                            }

                            // map_put(labels, token_string, &new_label);
                            panic("TODO(felix)");

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        }

                        // u8 *instruction_match = map_get(&opcode_from_name_map, token_string).pointer;
                        u8 *instruction_match = 0;
                        panic("TODO(felix)");

                        if (instruction_match != 0) {
                            u8 instruction = *instruction_match;

                            bool explicit_mode = next.kind == Token_Kind_OPMODE;
                            if (explicit_mode) {
                                for (u64 next_string_index = 0; next_string_index < next_string.count; next_string_index += 1) {
                                    const char *c = &next_string.data[next_string_index];
                                    switch (*c) {
                                        case '2': instruction |= Vm_Mode_SHORT; break;
                                        case 'k': instruction |= Vm_Mode_KEEP; break;
                                        case 'r': instruction |= Vm_Mode_RETURN_STACK; break;
                                        default: unreachable;
                                    }
                                }
                                file_token_id = token_id_shift(file_token_id, 1);
                            }

                            u8 byte = instruction;
                            rom[rom_count] = byte;
                            rom_count += 1;

                            if (instruction_takes_immediate(instruction)) {
                                next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                                switch (next.kind) {
                                    case Token_Kind_SYMBOL: case Token_Kind_NUMBER: case '{': break;
                                    default: {
                                        log_error("instruction '%s' takes an immediate, but no label or numeric literal is given", vm_opcode_name[instruction & VM_OPCODE_MASK]);
                                        parse_error(&assembler, next);
                                    }
                                }

                                if (next.kind == '{') continue;

                                u8 width = ((instruction & Vm_Mode_SHORT) >> 5) + 1;

                                if (next.kind == Token_Kind_SYMBOL) {
                                    Label_Reference reference = {
                                        .token_id = token_id_shift(file_token_id, 1),
                                        .destination_address = rom_count,
                                        .width = width,
                                        // .parent_id = (Label_Id)(assembler.labels.values.count - 1),
                                        // TODO(felix)
                                    };
                                    push(&arena, &assembler.label_references, reference);
                                    rom_count += reference.width;
                                } else {
                                    assert(next.kind == Token_Kind_NUMBER);
                                    next_string = token_lexeme(&assembler, token_id_shift(file_token_id, 1));
                                    u16 value = next.value;

                                    if (width == 1) {
                                        if (value > 255) {
                                            log_error("attempt to supply 16-bit value to instruction taking 8-bit immediate (%u is greater than 255); did you mean to use mode '.2'?", value);
                                            parse_error(&assembler, next);
                                        }
                                        rom[rom_count] = (u8)value;
                                        rom_count += 1;
                                    } else {
                                        assert(width == 2);
                                        write_u16_swap_bytes(rom, rom_count, value);
                                        rom_count += 2;
                                    }
                                }
                                file_token_id = token_id_shift(file_token_id, 1);
                            }

                            break;
                        }

                        if (string_equals(token_string, S("org")) || string_equals(token_string, S("rorg")) || string_equals(token_string, S("aorg"))) {
                            if (next.kind != Token_Kind_NUMBER) {
                                log_error("expected numeric literal (byte offset) after directive '%S'", token_string);
                                parse_error(&assembler, next);
                            }

                            u16 value = next.value;
                            bool is_relative = token_string.data[0] == 'r';
                            if (is_relative) value += rom_count;

                            if (value < rom_count) {
                                log_error("offset %u is less than current offset %u", value, rom_count);
                                parse_error(&assembler, next);
                            }

                            bool assert_only = token_string.data[0] == 'a';
                            if (!assert_only) rom_count = value;

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        } else if (string_equals(token_string, S("patch"))) {
                            if (next.kind != Token_Kind_SYMBOL) {
                                log_error("expected label to indicate destination offset as first argument to directive 'patch'");
                                parse_error(&assembler, next);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            Label_Reference reference = {
                                .is_patch = true,
                                .destination_label_token_id = file_token_id,
                                // .parent_id = (Label_Id)(assembler.labels.values.count - 1),
                                // TODO(felix)
                            };

                            next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                            if (next.kind != ',') {
                                log_error("expected ',' after between first and second arguments to directive 'patch'");
                                parse_error(&assembler, next);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                            if (next.kind != Token_Kind_SYMBOL && next.kind != Token_Kind_NUMBER) {
                                log_error("expected label or numeric literal to indicate address as second argument to directive 'patch'");
                                parse_error(&assembler, next);
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            reference.token_id = file_token_id;
                            push(&arena, &assembler.label_references, reference);
                        } else if (string_equals(token_string, S("include"))) {
                            if (next.kind != Token_Kind_STRING) {
                                log_error("expected string following directive 'include'");
                                parse_error(&assembler, next);
                            }

                            String include_filepath = next_string;
                            if (string_equals(include_filepath, file->name)) {
                                log_error("cyclic inclusion of file '%S'", file->name);
                                parse_error(&assembler, next);
                            }

                            read_ok = false;
                            String include_bytes = os_read_entire_file(&arena, cstring_from_string(&arena, include_filepath), &read_ok);
                            if (!read_ok || include_bytes.count > 0x10000) os_exit(1);

                            // Input_File new_file = { .bytes = include_bytes, .name = include_filepath };
                            // File_Id new_file_id = (File_Id)map_put(&assembler.files, include_filepath, &new_file).index;
                            File_Id new_file_id = {0};
                            panic("TODO(felix)");

                            File_Cursor current = { .cursor = (u32)asm.count, .token_id = token_id_shift(file_token_id, 2) };
                            push(&arena, &file_stack, current);

                            File_Cursor next_file = { .cursor = 0, .token_id = { .file_id = new_file_id } };
                            push(&arena, &file_stack, next_file);
                            goto switch_file;
                        }

                        Label_Reference reference = {
                            .token_id = file_token_id,
                            .destination_address = rom_count,
                            .width = 2,
                            // .parent_id = (Label_Id)(assembler.labels.values.count - 1),
                            // TODO(felix)
                        };
                        push(&arena, &assembler.label_references, reference);
                        rom_count += reference.width;
                    } break;
                    case Token_Kind_NUMBER: {
                        if (!assembler.insertion_mode.is_active) {
                            log_error("standalone number literals are only supported in insertion mode (e.g. in [ ... ])");
                            parse_error(&assembler, token);
                        }

                        u16 value = token.value;

                        bool is_byte = (token.base == 16 && token_string.count <= 2) || (token.base == 2 && token_string.count <= 8);
                        if (is_byte) {
                            assert(value < 256);
                            rom[rom_count] = (u8)value;
                            rom_count += 1;
                            break;
                        }

                        bool is_short = true;
                        if (is_short) {
                            write_u16_swap_bytes(rom, rom_count, value);
                            rom_count += 2;
                            break;
                        }

                        unreachable;
                    } break;
                    case Token_Kind_STRING: {
                        if (!assembler.insertion_mode.is_active) {
                            log_error("strings can only be used in insertion mode (e.g. inside [ ... ])");
                            parse_error(&assembler, token);
                        }

                        assert(rom_count + token_string.count <= 0x10000);
                        memcpy(rom + rom_count, token_string.data, token_string.count);
                        rom_count += (u16)token_string.count;
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

        for (u64 reference_index = 0; reference_index < assembler.label_references.count; reference_index += 1) {
            Label_Reference *reference = &assembler.label_references.data[reference_index];
            Label *source_label = 0, *destination_label = 0;
            Label **to_match[] = { &source_label, &destination_label };
            Token_Id match_against[] = { reference->token_id, reference->destination_label_token_id };
            u64 match_count = reference->is_patch ? 2 : 1;

            for (u64 i = 0; i < match_count; i += 1) {
                Label **match = to_match[i];

                Token_Id token_id = match_against[i];
                Token token = *token_get(&assembler, token_id);
                String reference_string = token_lexeme(&assembler, token_id);

                if (token.kind == Token_Kind_NUMBER) {
                    u16 value = token.value;
                    static Label dummy = {0};
                    dummy.address = value;
                    source_label = &dummy;
                    continue;
                }
                assert(token.kind == Token_Kind_SYMBOL);

                // Label parent = assembler.labels.values.data[reference->parent_id];
                panic("TODO(felix)");
                // Label *find = map_get(&parent.children, reference_string).pointer;
                Label *find = 0;
                panic("TODO(felix)");

                if (find == 0) {
                    // find = map_get(&assembler.labels, reference_string).pointer;
                    panic("TODO(felix)");
                }

                if (find != 0) {
                    *match = find;
                    goto done;
                }

                done:
                if (*match == 0) {
                    log_error("no such label '%S'", reference_string);
                    parse_error(&assembler, token, .file_id = token_id.file_id);
                }
            }

            if (reference->is_patch) {
                write_u16_swap_bytes(rom, destination_label->address, source_label->address);
            } else switch (reference->width) {
                case 1: {
                    u8 *location_to_patch = &rom[reference->destination_address];
                    assert(source_label->address <= 255);
                    *location_to_patch = (u8)source_label->address;
                } break;
                case 2: {
                    write_u16_swap_bytes(rom, reference->destination_address, source_label->address);
                } break;
                default: unreachable;
            }
        }

        if (command == Command_COMPILE) {
            assert(arguments_count == 4);
            const char *output_path = arguments[3];
            os_write_entire_file(output_path, rom, rom_count);
        }
    }

    bool should_run = command == Command_SCRIPT || command == Command_RUN;
    if (!should_run) {
        platform->should_quit = true;
    } else {
        if (rom_count == 0) os_exit(1);

        Vm *vm = &global_vm;
        vm->arena = &arena;
        memcpy(vm->memory, rom, rom_count);

        bool print_memory = false;
        if (print_memory) {
            Arena scratch = arena;
            String_Builder builder = {0};
            {
                string_builder_print(&scratch, &builder, "MEMORY ===\n");
                for (u16 token_id = 0x100; token_id < rom_count; token_id += 1) {
                    u8 byte = vm->memory[token_id];

                    const char *mode_string = "";
                    switch ((byte & 0xe0) >> 5) {
                        case 0x1: mode_string = "2"; break;
                        case 0x2: mode_string = "r"; break;
                        case 0x3: mode_string = "r2"; break;
                        case 0x4: mode_string = "k"; break;
                        case 0x5: mode_string = "k2"; break;
                        case 0x6: mode_string = "kr"; break;
                        case 0x7: mode_string = "kr2"; break;
                    }

                    string_builder_print(&scratch, &builder, "[%x]\t'%c'\t#%x\t%s;%s\n", token_id, byte, byte, vm_opcode_name[byte & VM_OPCODE_MASK], mode_string);
                }
                string_builder_print(&scratch, &builder, "\nRUN ===\n");
            }
            os_write_console(builder.data, builder.count);
        }

        panic("TODO(felix)");
        // Gfx_Render_Context *gfx = &vm.gfx;
        // *gfx = gfx_window_create(&arena, "bici", Vm_Screen_INITIAL_WIDTH, Vm_Screen_INITIAL_HEIGHT);
        // ShowCursor(false);
        // gfx->font = gfx_font_default_3x5;

        // TODO(felix): program should control this
        panic("TODO(felix)");
        // gfx_clear(gfx, 0);

        u16 vm_on_system_start_pc = vm_load16(vm, Vm_Device_SYSTEM + Vm_System_START);
        vm_run_to_break(platform, vm, vm_on_system_start_pc);
    }
}

static void app_update_and_render(Platform *platform) {
    Vm *vm = &global_vm;

    u16 vm_on_screen_update_pc = vm_load16(vm, Vm_Device_SCREEN | (Vm_Device)Vm_Screen_UPDATE);
    vm_run_to_break(platform, vm, vm_on_screen_update_pc);

    u16 vm_on_system_quit_pc = vm_load16(vm, Vm_Device_SYSTEM | (Vm_Device)Vm_System_QUIT);
    vm_run_to_break(platform, vm, vm_on_system_quit_pc);

    print("working stack (bottom->top): { ");
    for (u8 token_id = 0; token_id < vm->stacks[0].data; token_id += 1) print("%llx ", vm->stacks[0].memory[token_id]);
    print("}\nreturn  stack (bottom->top): { ");
    for (u8 token_id = 0; token_id < vm->stacks[1].data; token_id += 1) print("%llx ", vm->stacks[Vm_Mode_RETURN_STACK].memory[token_id]);
    print("}\n");
}
