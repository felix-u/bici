#define BASE_IMPLEMENTATION
#include "base/base.h"

#if BASE_OS == BASE_OS_WASM
    #include "base/base_platform_wasm.c"
#elif BASE_OS == BASE_OS_WINDOWS
    META(subsystem = console)
    #include "base/base_platform_wingles3.c"
#endif

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

typedef u16 Vm_Device;
enum {
    Vm_Device_SYSTEM   = 0x00,
    Vm_Device_CONSOLE  = 0x10,
    Vm_Device_SCREEN   = 0x20,
    Vm_Device_MOUSE    = 0x30,
    Vm_Device_KEYBOARD = 0x40,
    Vm_Device_FILE     = 0x50,
};

typedef u16 Vm_System_Action;
enum {
    Vm_System_RESERVED = 0x0,
    Vm_System_END      = 0x2,
    Vm_System_START    = 0x4,
    Vm_System_QUIT     = 0x6,
    Vm_System_COLOUR_0 = 0x8,
    Vm_System_COLOUR_1 = 0xa,
    Vm_System_COLOUR_2 = 0xc,
    Vm_System_COLOUR_3 = 0xe,
};

typedef u16 Vm_Console_Action;
enum {
    Vm_Console_PRINT = 0x0,
};

typedef u16 Vm_Screen_Action;
enum {
    Vm_Screen_UPDATE = 0x0,
    Vm_Screen_WIDTH  = 0x2,
    Vm_Screen_HEIGHT = 0x4,
    Vm_Screen_X      = 0x6,
    Vm_Screen_Y      = 0x8,
    Vm_Screen_PIXEL  = 0xa,
    Vm_Screen_SPRITE = 0xb,
    Vm_Screen_DATA   = 0xc,
    Vm_Screen_AUTO   = 0xe,
};

typedef u16 Vm_Mouse_Action;
enum {
    Vm_Mouse_X = 0x0,
    Vm_Mouse_Y = 0x2,
    Vm_Mouse_LEFT_BUTTON = 0x4,
    Vm_Mouse_RIGHT_BUTTON = 0x5,
};

typedef u16 Vm_Keyboard_Action;
enum {
    Vm_Keyboard_KEY_VALUE = 0x0,
    Vm_Keyboard_KEY_STATE = 0x1,
};

typedef u16 Vm_File_Action;
enum {
    Vm_File_NAME   = 0x0,
    Vm_File_LENGTH = 0x2,
    Vm_File_CURSOR = 0x4,
    Vm_File_READ   = 0x6,
    Vm_File_COPY   = 0x8,
};

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

typedef struct {
    u8 *pixels;
    i32 pixel_stride;
    V2 size;
    V2 mouse_position;
    f32 window_over_screen_scale;
    f32 window_over_screen_scale_target;
} Screen;
static Screen global_screen = {0};

enum {
    SCREEN_MAX_WIDTH = 1920,
    SCREEN_MAX_HEIGHT = 1200,
    SCREEN_INITIAL_SCALE = 3,
    SCREEN_LEAST_SCALE = 1,
    SCREEN_GREATEST_SCALE = 5,
};
#define SCREEN_SCALE_LOGARITHM_PER_SCROLL_NOTCH 0.1f
#define SCREEN_SCALE_APPROACH_RATE_PER_SECOND 20.f

static inline u16 get16(u8 *address) {
    u16 result = 0;
    result |= (u16)*address << 8;
    result |= (u16)*(address + 1);
    return result;
}

static inline void set16(u8 *address, u16 value) {
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
                                if (platform->frame.mouse_clicked[App_Mouse_Button_LEFT]) to_push |= 0xf0;
                                if (platform->frame.mouse_down[App_Mouse_Button_LEFT])    to_push |= 0x0f;
                            } break;
                            case Vm_Mouse_RIGHT_BUTTON: {
                                if (platform->frame.mouse_clicked[App_Mouse_Button_RIGHT]) to_push |= 0xf0;
                                if (platform->frame.mouse_down[App_Mouse_Button_RIGHT])    to_push |= 0x0f;
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
                            u16 screen_width = (u16)global_screen.size.x;
                            u16 screen_height = (u16)global_screen.size.y;
                            u8 *pixels = global_screen.pixels;
                            i32 stride = global_screen.pixel_stride;
                            switch (action) {
                                case Vm_Screen_PIXEL: {
                                    if (x >= screen_width) break;
                                    if (y >= screen_height) break;

                                    u8 fill_x = argument & 0x80;
                                    u8 fill_y = argument & 0x40;

                                    u8 colour = argument & 0x0f;
                                    if (colour > 3) panic("[write:screen/pixel] colour #%02x is invalid; there are only 4 palette colours", colour);

                                    if (fill_y && !fill_x) unreachable;
                                    u16 right = fill_x ? screen_width : x + 1;
                                    u16 bottom = fill_y ? screen_height : y + 1;
                                    for (u16 row = y; row < bottom; row += 1) {
                                        for (u16 column = x; column < right; column += 1) pixels[row * stride + column] = colour;
                                    }
                                } break;
                                case Vm_Screen_SPRITE: {
                                    if (x >= screen_width) break;
                                    if (y >= screen_height) break;

                                    u8 two_bits_per_pixel = argument & 0x80;
                                    u8 use_background_layer = argument & 0x40;
                                    u8 flip_y = argument & 0x20;
                                    u8 flip_x = argument & 0x10;
                                    u8 colours[] = { argument & 0x03, (u8)((argument & 0x0c) >> 2) };

                                    // TODO(felix)
                                    assert(!two_bits_per_pixel);
                                    assert(!flip_y);
                                    assert(!flip_x);

                                    u16 column_count = MIN(8, screen_width - x);
                                    u16 row_count = MIN(8, screen_height - y);

                                    u8 *sprite = &vm->memory[get16(&vm->memory[Vm_Device_SCREEN | (Vm_Device)Vm_Screen_DATA])];
                                    for (u16 row = 0; row < row_count; row += 1, sprite += 1) {
                                        for (u16 column = 0; column < column_count; column += 1) {
                                            u8 shift = 7 - (u8)column;
                                            u8 colour = (*sprite & (1 << shift)) >> shift;
                                            if (!use_background_layer && colour == 0) continue;

                                            pixels[(y + row) * stride + x + column] = colours[colour];
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
                            case Vm_Screen_WIDTH: to_push = (u16)global_screen.size.x; break;
                            case Vm_Screen_HEIGHT: to_push = (u16)global_screen.size.y; break;
                            default: panic("[read.2] invalid action #%02x for screen device", action);
                        } break;
                        case Vm_Device_MOUSE: {
                            V2 mouse_position = global_screen.mouse_position;
                            V2 window_size = global_screen.size;
                            switch (action) {
                                case Vm_Mouse_X: to_push = (u16)CLAMP(mouse_position.x, 0.f, MAX(window_size.x - 1.f, 0.f)); break;
                                case Vm_Mouse_Y: to_push = (u16)CLAMP(mouse_position.y, 0.f, MAX(window_size.y - 1.f, 0.f)); break;
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
                                #if BASE_OS == BASE_OS_WASM
                                    String bytes = base_embedded_files_get(file_name);
                                    read_ok = bytes.data != 0;
                                #else
                                    String bytes = os_read_entire_file(vm->arena, cstring_from_string(vm->arena, file_name), &read_ok);
                                #endif
                                if (!read_ok) log_error("file '%S' not found", file_name);

                                vm->file_bytes = read_ok ? (u8 *)bytes.data : 0;
                                u64 file_length = read_ok ? bytes.count : 0;

                                assert(file_length <= 0xffff);
                                set16(&vm->memory[Vm_Device_FILE | (Vm_Device)Vm_File_LENGTH], (u16)file_length);
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

static inline bool is_starting_symbol_character(char c) {
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

typedef struct {
    File_Id file_id;
    i32 index;
} Token_Id;

typedef u32 Label_Id;
#define MAX_LABELS 1024

typedef struct {
    Token_Id token_id;
    u16 address;
    Label_Id parent_id;
} Label;

typedef struct {
    Token_Id token_id;
    union { u16 destination_address; Token_Id destination_label_token_id; };
    u8 width;
    bool is_patch;
    Label_Id parent_id;
} Label_Reference;

typedef struct {
    u16 address;
    Token_Id token_id;
    bool is_relative;
} Scoped_Reference;

typedef struct {
    bool is_active;
    bool has_relative_reference;
} Insertion_Mode;

typedef struct {
    String name;
    String bytes;
    Token *tokens;
    u64 token_count, token_capacity;
} Input_File;

#define FILE_SLOT_EXPONENT 9
#define LABEL_SLOT_EXPONENT 11
_Static_assert((1 << FILE_SLOT_EXPONENT) >= 2 * MAX_FILES, "file slots must be at most half full");
_Static_assert((1 << LABEL_SLOT_EXPONENT) >= 2 * MAX_LABELS, "label slots must be at most half full");

#define OPCODE_SLOT_EXPONENT 8
#define OPCODE_SLOT_MASK ((1u << OPCODE_SLOT_EXPONENT) - 1)
_Static_assert((1 << OPCODE_SLOT_EXPONENT) >= VM_OPCODE_MAX_VALUE_PLUS_ONE, "every opcode needs its own slot");

#define SCOPED_REFERENCES_MAX 256

typedef struct {
    Input_File files[MAX_FILES];
    int files_count;
    ds_Hash_Slot file_slots[1 << FILE_SLOT_EXPONENT];
    Label labels[MAX_LABELS];
    int labels_count;
    ds_Hash_Slot label_slots[1 << LABEL_SLOT_EXPONENT];
    Label_Id global_label_id;
    File_Id file_id;
    Label_Reference *label_references;
    u64 label_reference_count, label_reference_capacity;
    Scoped_Reference scoped_references[SCOPED_REFERENCES_MAX];
    u32 scoped_reference_count;
    Insertion_Mode insertion_mode;
} Assembler_Context;

static Token_Id token_id_shift(Token_Id token_id, i32 shift) {
    Token_Id result = token_id;
    result.index += shift;
    return result;
}

static Input_File *token_get_file(Assembler_Context *assembler, Token_Id token_id) {
    Input_File *file = &assembler->files[token_id.file_id];
    return file;
}

static Token *token_get(Assembler_Context *assembler, Token_Id token_id) {
    Input_File file = *token_get_file(assembler, token_id);
    i32 index = token_id.index;
    if (index < 0 || (u64)index >= file.token_count) {
        static Token nil_token = {0};
        return &nil_token;
    }
    Token *token = &file.tokens[index];
    return token;
}

static String token_lexeme(Assembler_Context *assembler, Token_Id token_id) {
    Token token = *token_get(assembler, token_id);
    Input_File *file = token_get_file(assembler, token_id);
    String asm = file->bytes;
    String token_string = string_range(asm, token.start_index, token.start_index + token.length);
    return token_string;
}

static ds_Hash_Slot *assembler_get_file_slot(Assembler_Context *assembler, String name) {
    u32 hash = ds_hash_fnv1a(name.data, size_t_from_u64(name.count));
    ds_Hash_Slot *slot = 0;
    for (u32 probe_index = hash; slot == 0;) {
        probe_index = ds_hash_step_index(hash, FILE_SLOT_EXPONENT, probe_index);
        ds_Hash_Slot *candidate = &assembler->file_slots[probe_index];
        bool is_empty = candidate->index == 0;
        bool is_same_key = !is_empty && candidate->hash == hash && string_equals(assembler->files[candidate->index].name, name);
        if (is_empty) candidate->hash = hash;
        if (is_empty || is_same_key) slot = candidate;
    }
    return slot;
}

static ds_Hash_Slot *assembler_get_label_slot(Assembler_Context *assembler, Label_Id parent_id, String name) {
    u32 parent_hash = ds_hash_fnv1a(&parent_id, sizeof parent_id);
    u32 hash = ds_hash_fnv1a_update(parent_hash, name.data, size_t_from_u64(name.count));
    ds_Hash_Slot *slot = 0;
    for (u32 probe_index = hash; slot == 0;) {
        probe_index = ds_hash_step_index(hash, LABEL_SLOT_EXPONENT, probe_index);
        ds_Hash_Slot *candidate = &assembler->label_slots[probe_index];
        Label *label = &assembler->labels[candidate->index];
        bool is_empty = candidate->index == 0;
        bool is_same_key = !is_empty && candidate->hash == hash && label->parent_id == parent_id && string_equals(token_lexeme(assembler, label->token_id), name);
        if (is_empty) candidate->hash = hash;
        if (is_empty || is_same_key) slot = candidate;
    }
    return slot;
}

static void file_push_token(Input_File *file, Token token) {
    assert(file->token_count < file->token_capacity);
    file->tokens[file->token_count] = token;
    file->token_count += 1;
}

static void assembler_push_label_reference(Assembler_Context *assembler, Label_Reference reference) {
    assert(assembler->label_reference_count < assembler->label_reference_capacity);
    assembler->label_references[assembler->label_reference_count] = reference;
    assembler->label_reference_count += 1;
}

static void assembler_push_scoped_reference(Assembler_Context *assembler, Scoped_Reference reference) {
    if (assembler->scoped_reference_count == SCOPED_REFERENCES_MAX) {
        log_error("more than %d unresolved '{' and '[$' references", SCOPED_REFERENCES_MAX);
        os_exit(1);
    }
    assembler->scoped_references[assembler->scoped_reference_count] = reference;
    assembler->scoped_reference_count += 1;
}

typedef struct {
    Token token;
    u32 start_index;
    u32 length;
    File_Id file_id;
} Parse_Error_Arguments;
static void parse_error(Assembler_Context *assembler, Parse_Error_Arguments arguments) {
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

    String text = assembler->files[file_id].bytes;
    String file_name = assembler->files[file_id].name;
    String lexeme = string_range(text, start_index, start_index + length);

    u64 row = 1, column = 1;
    for (u64 i = 0; i < start_index; i += 1) {
        u8 c = (u8)text.data[i];
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

#define FILE_STACK_MAX 256
typedef struct {
    File_Cursor cursors[FILE_STACK_MAX];
    u32 count;
} File_Stack;

static void file_stack_push(File_Stack *stack, File_Cursor cursor) {
    if (stack->count == FILE_STACK_MAX) {
        log_error("includes nested deeper than %d", FILE_STACK_MAX - 1);
        os_exit(1);
    }
    stack->cursors[stack->count] = cursor;
    stack->count += 1;
}

static void app_start(Platform *platform) {
    Arena arena = {0};
    u64 arguments_count = 0;
    const char **arguments = os_get_arguments(&arena, &arguments_count);

    typedef enum {
        Command_COMPILE,
        Command_RUN,
        Command_SCRIPT,

        Command_COUNT,
    } Command;
    Command command = 0;
    const char *input_filepath = 0;
    String input_bytes = {0};

    #if BASE_OS == BASE_OS_WASM
    {
        // TODO(felix): Let the web build load any ROM, instead of only the ones embedded at build time.
        META(embed "os.rom")
        META(embed "hello.rom")
        META(embed "keyboard.rom")
        META(embed "mouse.rom")
        META(embed "screen.rom")

        command = Command_RUN;
        input_filepath = "os.rom";
        input_bytes = base_embedded_files_get(string_from_cstring(input_filepath));
        ensure(input_bytes.count > 0);
    }
    #else
    {
        if (arguments_count < 3) {
            log_error("%s", USAGE_HELP_TEXT);
            os_exit(1);
        }

        const char *command_string = arguments[1];

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

        input_filepath = arguments[2];
        bool read_ok = false;
        input_bytes = os_read_entire_file(&arena, input_filepath, &read_ok);
        ensure(read_ok);
    }
    #endif

    ensure(input_bytes.count <= 0x10000);

    u8 rom_bytes[0x10000] = {0};
    u8 *rom = rom_bytes;
    u16 rom_count = 0;

    bool should_compile = command == Command_COMPILE || command == Command_SCRIPT;
    if (!should_compile) rom = (u8 *)input_bytes.data;
    else {
        ds_Hash_Slot opcode_slots[1 << OPCODE_SLOT_EXPONENT] = {0};
        for (u8 opcode = 0; opcode < VM_OPCODE_MAX_VALUE_PLUS_ONE; opcode += 1) {
            String name = string_from_cstring(vm_opcode_name[opcode]);
            u32 hash = ds_hash_fnv1a(name.data, size_t_from_u64(name.count));
            ds_Hash_Slot *slot = &opcode_slots[hash & OPCODE_SLOT_MASK];
            assert(slot->index == 0);
            *slot = (ds_Hash_Slot){ .hash = hash, .index = (u32)opcode + 1 };
        }

        Assembler_Context assembler = {0};
        assembler.files_count = 1;
        assembler.labels_count = 1;

        String main_file_name = string_from_cstring(input_filepath);
        File_Id main_file_id = (File_Id)assembler.files_count;
        assembler.files_count += 1;
        assembler.files[main_file_id] = (Input_File){ .bytes = input_bytes, .name = main_file_name };
        assembler_get_file_slot(&assembler, main_file_name)->index = main_file_id;

        File_Stack file_stack = {0};
        file_stack_push(&file_stack, (File_Cursor){ .token_id.file_id = main_file_id });

        while (file_stack.count > 0) {
            file_stack.count -= 1;
            File_Cursor file_cursor = file_stack.cursors[file_stack.count];
            assembler.file_id = file_cursor.token_id.file_id;

            Input_File *file = &assembler.files[assembler.file_id];

            String asm = file->bytes;

            u64 token_minimum = file->token_count + (asm.count - file_cursor.cursor);
            file->tokens = arena_array_reserve(&arena, file->tokens, file->token_count, &file->token_capacity, token_minimum);

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
                        char c = asm.data[cursor];
                        bool is_symbol_character = is_starting_symbol_character(c) || ascii_is_decimal(c);
                        if (!is_symbol_character) break;
                    }
                } else switch (asm.data[cursor]) {
                    case '0': {
                        kind = Token_Kind_NUMBER;

                        if (cursor + 1 == asm.count || (asm.data[cursor + 1] != 'x' && asm.data[cursor + 1] != 'b')) {
                            log_error("expected 'x' or 'b' after '0' to begin binary or hexadecimal literal");
                            parse_error(&assembler, (Parse_Error_Arguments){ .start_index = cursor + 1, .length = 1 });
                        }
                        char base_char = asm.data[cursor + 1];
                        cursor += 2;
                        start_index = cursor;

                        if (cursor == asm.count) {
                            log_error("expected digits following '0%c'", base_char);
                            parse_error(&assembler, (Parse_Error_Arguments){ .start_index = cursor - 2, .length = 2 });
                        }

                        for (; cursor < asm.count; cursor += 1) {
                            char c = asm.data[cursor];
                            if (base_char == 'x' && ascii_is_hexadecimal(c)) continue;
                            if (base_char == 'b' && (c == '0' || c == '1')) continue;
                            if (ascii_is_whitespace(c)) break;
                            log_error("expected base_char %c digit", base_char);
                            parse_error(&assembler, (Parse_Error_Arguments){ .start_index = cursor });
                        }

                        base = base_char == 'b' ? 2 : 16;
                        String number_string = string_range(asm, start_index, cursor);
                        u64 value_unbounded = (u64)int_from_string_base(number_string, base);
                        if (value_unbounded >= 0x10000) {
                            log_error("0d%llu is too large to fit in 16 bits", value_unbounded);
                            parse_error(&assembler, (Parse_Error_Arguments){ .start_index = start_index, .length = (u8)(cursor - start_index) });
                        }
                        value = (u16)value_unbounded;
                    } break;
                    case ';': {
                        while (cursor < asm.count && asm.data[cursor] != '\n') cursor += 1;
                        cursor += 1;
                        continue;
                    } break;
                    case ':': case '$': case '{': case '}': case '[': case ']': case ',': case '/': {
                        kind = (Token_Kind)asm.data[cursor];
                        cursor += 1;
                    } break;
                    case '"': {
                        kind = Token_Kind_STRING;

                        cursor += 1;
                        start_index = cursor;
                        for (; cursor < asm.count; cursor += 1) {
                            char c = asm.data[cursor];
                            if (c == '"') break;
                            if (c == '\n') {
                                log_error("expected '\"' to close string before newline");
                                parse_error(&assembler, (Parse_Error_Arguments){ .start_index = start_index, .length = (u8)(cursor - start_index) });
                            }
                        }

                        if (cursor == asm.count) {
                            log_error("expected '\"' to close string before end of file");
                            parse_error(&assembler, (Parse_Error_Arguments){ .start_index = start_index, .length = (u8)(cursor - start_index) });
                        }

                        assert(asm.data[cursor] == '"');
                        cursor += 1;
                    } break;
                    case '.': {
                        kind = Token_Kind_OPMODE;

                        cursor += 1;
                        start_index = cursor;
                        for (; cursor < asm.count; cursor += 1) {
                            char c = asm.data[cursor];
                            if (ascii_is_whitespace(c)) break;
                            if (c != '2' && c != 'k' && c != 'r') {
                                log_error("only characters '2', 'k', and 'r' are valid op modes");
                                parse_error(&assembler, (Parse_Error_Arguments){ .start_index = cursor });
                            }
                        }

                        u64 length = cursor - start_index;
                        if (length == 0 || length > 3) {
                            log_error("a valid op mode contains 1, 2, or 3 characters (as in op.2kr), but this one has %llu", length);
                            parse_error(&assembler, (Parse_Error_Arguments){ .start_index = start_index, .length = (u8)length });
                        }
                    } break;
                    default: {
                        log_error("invalid syntax '%c'", asm.data[cursor]);
                        parse_error(&assembler, (Parse_Error_Arguments){ .start_index = cursor });
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
                file_push_token(file, token);
            }

            u64 label_reference_minimum = assembler.label_reference_count + (file->token_count - (u64)file_cursor.token_id.index);
            assembler.label_references = arena_array_reserve(&arena, assembler.label_references, assembler.label_reference_count, &assembler.label_reference_capacity, label_reference_minimum);

            bool parsing_relative_label = false;
            for (Token_Id file_token_id = file_cursor.token_id; (u64)file_token_id.index < file->token_count; file_token_id.index += 1) {
                Token token = *token_get(&assembler, file_token_id);
                String token_string = token_lexeme(&assembler, file_token_id);

                if (assembler.insertion_mode.is_active) {
                    switch (token.kind) {
                        case Token_Kind_SYMBOL: case ']': case '$': case '/': case Token_Kind_NUMBER: case Token_Kind_STRING: break;
                        default: {
                            log_error("insertion mode only supports addresses (e.g. label) and numeric literals");
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                        }
                    };
                }

                Token_Id next_id = token_id_shift(file_token_id, 1);
                Token next = *token_get(&assembler, next_id);
                String next_string = token_lexeme(&assembler, next_id);

                switch (token.kind) {
                    case '{': {
                        Scoped_Reference reference = { .address = rom_count };
                        assembler_push_scoped_reference(&assembler, reference);
                        rom_count += 2;
                    } break;
                    case '}': {
                        if (assembler.scoped_reference_count == 0) {
                            log_error("'}' has no matching '{'");
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                        }

                        assembler.scoped_reference_count -= 1;
                        Scoped_Reference reference = assembler.scoped_references[assembler.scoped_reference_count];
                        if (reference.is_relative) {
                            log_error("expected to resolve absolute reference (from '{') but found unresolved relative reference");
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                        }

                        write_u16_swap_bytes(rom, reference.address, rom_count);
                    } break;
                    case '[': {
                        if (assembler.insertion_mode.is_active) {
                            log_error("cannot enter insertion mode while already in insertion mode");
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                        }
                        assembler.insertion_mode.is_active = true;

                        bool compile_byte_count_to_closing_bracket = next.kind == '$';
                        if (compile_byte_count_to_closing_bracket) {
                            assembler.insertion_mode.has_relative_reference = true;
                            Scoped_Reference reference = { .address = rom_count, .is_relative = true };
                            assembler_push_scoped_reference(&assembler, reference);
                            rom_count += 1;
                            file_token_id = token_id_shift(file_token_id, 1);
                        }
                    } break;
                    case ']': {
                        if (!assembler.insertion_mode.is_active) {
                            log_error("']' has no matching '['");
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                        }

                        if (assembler.insertion_mode.has_relative_reference) {
                            if (assembler.scoped_reference_count == 0) {
                                log_error("']' has no matching '[$' reference to resolve");
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                            }
                            assembler.scoped_reference_count -= 1;
                            Scoped_Reference reference = assembler.scoped_references[assembler.scoped_reference_count];
                            if (!reference.is_relative) {
                                log_error("expected to resolve relative reference (from '[$') but found unresolved absolute reference; is there an unmatched '{'?");
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                            }

                            u64 relative_difference = rom_count - reference.address - 1;
                            if (relative_difference > 255) {
                                log_error("relative difference from '[$' is %llu bytes, but the maximum is 255", relative_difference);
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                            }

                            u8 *location_to_patch = &rom[reference.address];
                            *location_to_patch = (u8)relative_difference;
                        }

                        assembler.insertion_mode = (Insertion_Mode){0};
                    } break;
                    case '/': {
                        if (next.kind != Token_Kind_SYMBOL) {
                            log_error("expected relative label following '/'");
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                        }
                        parsing_relative_label = true;
                    } break;
                    case Token_Kind_SYMBOL: {
                        if (parsing_relative_label) {
                            if (next.kind != ':') {
                                log_error("expected ':' ending relative label definition");
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }
                        }

                        bool is_label = next.kind == ':';
                        if (is_label) {
                            ds_Hash_Slot *global_slot = assembler_get_label_slot(&assembler, 0, token_string);
                            bool is_redefinition = global_slot->index != 0;
                            if (is_redefinition) {
                                log_error("redefinition of label '%S'", token_string);
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                            }

                            Label new_label = { .token_id = file_token_id, .address = rom_count };
                            ds_Hash_Slot *slot = global_slot;
                            if (parsing_relative_label) {
                                if (assembler.global_label_id == 0) {
                                    log_error("relative label '%S' has no preceding global label", token_string);
                                    parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                                }

                                new_label.parent_id = assembler.global_label_id;
                                slot = assembler_get_label_slot(&assembler, new_label.parent_id, token_string);
                                is_redefinition = slot->index != 0;
                                if (is_redefinition) {
                                    log_error("redefinition of label '%S'", token_string);
                                    parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                                }

                                parsing_relative_label = false;
                            }

                            if (assembler.labels_count == MAX_LABELS) {
                                log_error("more than %d labels", MAX_LABELS - 1);
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                            }
                            Label_Id new_label_id = (Label_Id)assembler.labels_count;
                            assembler.labels_count += 1;
                            assembler.labels[new_label_id] = new_label;
                            slot->index = new_label_id;
                            if (new_label.parent_id == 0) assembler.global_label_id = new_label_id;

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        }

                        u32 opcode_hash = ds_hash_fnv1a(token_string.data, size_t_from_u64(token_string.count));
                        ds_Hash_Slot opcode = opcode_slots[opcode_hash & OPCODE_SLOT_MASK];
                        bool is_instruction = opcode.index != 0 && opcode.hash == opcode_hash && string_equals(string_from_cstring(vm_opcode_name[opcode.index - 1]), token_string);
                        u8 matched_instruction = (u8)(opcode.index - 1);
                        u8 *instruction_match = is_instruction ? &matched_instruction : 0;

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
                                        parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                                    }
                                }

                                if (next.kind == '{') continue;

                                u8 width = ((instruction & Vm_Mode_SHORT) >> 5) + 1;

                                if (next.kind == Token_Kind_SYMBOL) {
                                    Label_Reference reference = {
                                        .token_id = token_id_shift(file_token_id, 1),
                                        .destination_address = rom_count,
                                        .width = width,
                                        .parent_id = assembler.global_label_id,
                                    };
                                    assembler_push_label_reference(&assembler, reference);
                                    rom_count += reference.width;
                                } else {
                                    assert(next.kind == Token_Kind_NUMBER);
                                    next_string = token_lexeme(&assembler, token_id_shift(file_token_id, 1));
                                    u16 value = next.value;

                                    if (width == 1) {
                                        if (value > 255) {
                                            log_error("attempt to supply 16-bit value to instruction taking 8-bit immediate (%u is greater than 255); did you mean to use mode '.2'?", value);
                                            parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
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
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }

                            u16 value = next.value;
                            bool is_relative = token_string.data[0] == 'r';
                            if (is_relative) value += rom_count;

                            if (value < rom_count) {
                                log_error("offset %u is less than current offset %u", value, rom_count);
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }

                            bool assert_only = token_string.data[0] == 'a';
                            if (!assert_only) rom_count = value;

                            file_token_id = token_id_shift(file_token_id, 1);
                            break;
                        } else if (string_equals(token_string, S("patch"))) {
                            if (next.kind != Token_Kind_SYMBOL) {
                                log_error("expected label to indicate destination offset as first argument to directive 'patch'");
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            Label_Reference reference = {
                                .is_patch = true,
                                .destination_label_token_id = file_token_id,
                                .parent_id = assembler.global_label_id,
                            };

                            next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                            if (next.kind != ',') {
                                log_error("expected ',' after between first and second arguments to directive 'patch'");
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            next = *token_get(&assembler, token_id_shift(file_token_id, 1));
                            if (next.kind != Token_Kind_SYMBOL && next.kind != Token_Kind_NUMBER) {
                                log_error("expected label or numeric literal to indicate address as second argument to directive 'patch'");
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }

                            file_token_id = token_id_shift(file_token_id, 1);
                            reference.token_id = file_token_id;
                            assembler_push_label_reference(&assembler, reference);
                        } else if (string_equals(token_string, S("include"))) {
                            if (next.kind != Token_Kind_STRING) {
                                log_error("expected string following directive 'include'");
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }

                            String include_filepath = next_string;
                            if (string_equals(include_filepath, file->name)) {
                                log_error("cyclic inclusion of file '%S'", file->name);
                                parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                            }

                            bool include_read_ok = false;
                            String include_bytes = os_read_entire_file(&arena, cstring_from_string(&arena, include_filepath), &include_read_ok);
                            if (!include_read_ok || include_bytes.count > 0x10000) os_exit(1);

                            ds_Hash_Slot *included_file_slot = assembler_get_file_slot(&assembler, include_filepath);
                            if (included_file_slot->index == 0) {
                                if (assembler.files_count == MAX_FILES) {
                                    log_error("more than %d files", MAX_FILES - 1);
                                    parse_error(&assembler, (Parse_Error_Arguments){ .token = next });
                                }
                                included_file_slot->index = (u32)assembler.files_count;
                                assembler.files_count += 1;
                            }
                            File_Id new_file_id = (File_Id)included_file_slot->index;
                            assembler.files[new_file_id] = (Input_File){ .bytes = include_bytes, .name = include_filepath };

                            File_Cursor current = { .cursor = (u32)asm.count, .token_id = token_id_shift(file_token_id, 2) };
                            file_stack_push(&file_stack, current);

                            File_Cursor next_file = { .cursor = 0, .token_id = { .file_id = new_file_id } };
                            file_stack_push(&file_stack, next_file);
                            goto switch_file;
                        }

                        Label_Reference reference = {
                            .token_id = file_token_id,
                            .destination_address = rom_count,
                            .width = 2,
                            .parent_id = assembler.global_label_id,
                        };
                        assembler_push_label_reference(&assembler, reference);
                        rom_count += reference.width;
                    } break;
                    case Token_Kind_NUMBER: {
                        if (!assembler.insertion_mode.is_active) {
                            log_error("standalone number literals are only supported in insertion mode (e.g. in [ ... ])");
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
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
                            parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                        }

                        assert(rom_count + token_string.count <= 0x10000);
                        memcpy(rom + rom_count, token_string.data, token_string.count);
                        rom_count += (u16)token_string.count;
                    } break;
                    default: {
                        log_error("invalid syntax");
                        parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
                    }
                }
            }

            if (assembler.scoped_reference_count != 0) {
                Scoped_Reference reference = assembler.scoped_references[0];
                Token token = *token_get(&assembler, reference.token_id);
                log_error("anonymous reference ('{') without matching '}'");
                parse_error(&assembler, (Parse_Error_Arguments){ .token = token });
            }

            switch_file:;
        }

        for (u64 reference_index = 0; reference_index < assembler.label_reference_count; reference_index += 1) {
            Label_Reference *reference = &assembler.label_references[reference_index];
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

                Label *find = 0;
                if (reference->parent_id != 0) {
                    Label_Id relative_label_id = assembler_get_label_slot(&assembler, reference->parent_id, reference_string)->index;
                    if (relative_label_id != 0) find = &assembler.labels[relative_label_id];
                }

                if (find == 0) {
                    Label_Id global_label_id = assembler_get_label_slot(&assembler, 0, reference_string)->index;
                    if (global_label_id != 0) find = &assembler.labels[global_label_id];
                }

                if (find != 0) {
                    *match = find;
                    goto done;
                }

                done:
                if (*match == 0) {
                    log_error("no such label '%S'", reference_string);
                    parse_error(&assembler, (Parse_Error_Arguments){ .token = token, .file_id = token_id.file_id });
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

    u64 rom_size = should_compile ? rom_count : input_bytes.count;

    bool should_run = command == Command_SCRIPT || command == Command_RUN;
    if (!should_run) {
        platform->should_quit = true;
    } else {
        if (rom_size == 0) os_exit(1);

        Vm *vm = &global_vm;
        vm->arena = platform->persistent_arena;
        memcpy(vm->memory, rom, rom_size);

        bool print_memory = false;
        if (print_memory) {
            Scratch scratch = scratch_begin(&arena);
            String_Builder builder = {0};
            {
                string_builder_print(scratch.arena, &builder, "MEMORY ===\n");
                for (u32 token_id = 0x100; token_id < rom_size; token_id += 1) {
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
                        default: unreachable;
                    }

                    string_builder_print(scratch.arena, &builder, "[%x]\t'%c'\t#%x\t%s;%s\n", token_id, byte, byte, vm_opcode_name[byte & VM_OPCODE_MASK], mode_string);
                }
                string_builder_print(scratch.arena, &builder, "\nRUN ===\n");
            }
            os_write_console(builder.data, builder.count);
            scratch_end(scratch);
        }

        platform->window_name = "bici";
        platform->frame_arena_startup_reservation_in_bytes = 4 * 1024 * 1024;
        static u8 screen_colour_indices[SCREEN_MAX_WIDTH * SCREEN_MAX_HEIGHT];
        global_screen.pixels = screen_colour_indices;
        global_screen.pixel_stride = SCREEN_MAX_WIDTH;
        global_screen.window_over_screen_scale = SCREEN_INITIAL_SCALE;
        global_screen.window_over_screen_scale_target = SCREEN_INITIAL_SCALE;
        platform->hide_cursor = true;
        // TODO(felix): program should control this
        platform->clear_color = 0x000000ff;

        u16 vm_on_system_start_pc = vm_load16(vm, Vm_Device_SYSTEM + Vm_System_START);
        vm_run_to_break(platform, vm, vm_on_system_start_pc);
    }
}

static void app_update_and_render(Platform *platform) {
    Vm *vm = &global_vm;

    App_Frame_Info *frame = &platform->frame;
    bool is_control_down = frame->key_down[App_Key_CONTROL];
    if (is_control_down) {
        f32 scaled_target = global_screen.window_over_screen_scale_target * expf(frame->scroll * SCREEN_SCALE_LOGARITHM_PER_SCROLL_NOTCH);
        global_screen.window_over_screen_scale_target = CLAMP(scaled_target, (f32)SCREEN_LEAST_SCALE, (f32)SCREEN_GREATEST_SCALE);
    }

    f32 approach_fraction = 1.f - expf(-frame->seconds_since_last_frame * SCREEN_SCALE_APPROACH_RATE_PER_SECOND);
    f32 scale_difference = global_screen.window_over_screen_scale_target - global_screen.window_over_screen_scale;
    f32 approached_scale = global_screen.window_over_screen_scale + scale_difference * approach_fraction;

    V2 window_size = frame->window_size;
    f32 scale_that_fits_screen_memory = MAX(window_size.x / (f32)SCREEN_MAX_WIDTH, window_size.y / (f32)SCREEN_MAX_HEIGHT);
    f32 window_over_screen_scale = MAX(approached_scale, scale_that_fits_screen_memory);
    global_screen.window_over_screen_scale = window_over_screen_scale;
    global_screen.size = (V2){ .x = ceilf(window_size.x / window_over_screen_scale), .y = ceilf(window_size.y / window_over_screen_scale) };
    global_screen.mouse_position = v2_scale(platform->frame.mouse_position, 1.f / window_over_screen_scale);

    u16 vm_on_screen_update_pc = vm_load16(vm, Vm_Device_SCREEN | (Vm_Device)Vm_Screen_UPDATE);
    vm_run_to_break(platform, vm, vm_on_screen_update_pc);

    u16 vm_on_system_quit_pc = vm_load16(vm, Vm_Device_SYSTEM | (Vm_Device)Vm_System_QUIT);
    vm_run_to_break(platform, vm, vm_on_system_quit_pc);

    u32 screen_width = (u32)global_screen.size.x;
    u32 screen_height = (u32)global_screen.size.y;
    bool has_screen = screen_width > 0 && screen_height > 0;
    if (has_screen) {
        String screen_fragment_source = S(STRINGIFY(
            layout(std140) uniform gpu_arguments { vec4 palette[4]; };

            vec3 palette_colour_at(ivec2 texel, ivec2 last_texel) {
                ivec2 clamped_texel = clamp(texel, ivec2(0), last_texel);
                int colour_index = int(texelFetch(gpu_texture_0, clamped_texel, 0).r * 255.0 + 0.5);
                return palette[colour_index].rgb;
            }

            void main() {
                ivec2 texture_size = textureSize(gpu_texture_0, 0);
                vec2 texel_position = gpu_uv * vec2(texture_size);
                vec2 antialiased_texel_position = floor(texel_position) + min(fract(texel_position) / fwidth(texel_position), 1.0) - 0.5;

                vec2 sample_position = antialiased_texel_position - 0.5;
                vec2 first_texel = floor(sample_position);
                vec2 weight = sample_position - first_texel;
                ivec2 texel = ivec2(first_texel);
                ivec2 last_texel = texture_size - 1;

                vec3 top = mix(palette_colour_at(texel, last_texel), palette_colour_at(texel + ivec2(1, 0), last_texel), weight.x);
                vec3 bottom = mix(palette_colour_at(texel + ivec2(0, 1), last_texel), palette_colour_at(texel + ivec2(1, 1), last_texel), weight.x);
                gpu_color_out = vec4(mix(top, bottom, weight.y), 1.0);
            }
        ));
        bool shader_ok = true;
        GPU_Shader_Id screen_shader = gpu_shader_id(platform, &(GPU_Shader){ .fragment_source = screen_fragment_source }, &shader_ok);
        ensure(shader_ok);

        enum { SCREEN_TEXTURE_KEY = 1 };
        u64 screen_pixels_size = ((u64)(screen_height - 1) * SCREEN_MAX_WIDTH + screen_width) * sizeof *global_screen.pixels;
        GPU_Texture_Id screen_texture = gpu_texture_id(platform, &(GPU_Texture){
            .key = SCREEN_TEXTURE_KEY,
            .width = screen_width,
            .height = screen_height,
            .format = GPU_Texture_Format_R8,
            .filter = GPU_Texture_Filter_NEAREST,
            .pixels = { .data = global_screen.pixels, .size = screen_pixels_size },
            .pixels_per_row = SCREEN_MAX_WIDTH,
        });

        typedef struct { V4 palette[4]; } Screen_Arguments;
        Screen_Arguments screen_arguments = {0};
        for (u8 colour_index = 0; colour_index < 4; colour_index += 1) {
            u16 colour = get16(&vm->memory[Vm_Device_SYSTEM | (Vm_Device)(Vm_System_COLOUR_0 + colour_index * 2)]);
            screen_arguments.palette[colour_index] = (V4){
                .x = (f32)((colour & 0x0f00) >> 8) / 15.f,
                .y = (f32)((colour & 0x00f0) >> 4) / 15.f,
                .z = (f32)(colour & 0x000f) / 15.f,
                .w = 1.f,
            };
        }

        f32 scale = global_screen.window_over_screen_scale;
        gpu_draw(platform, &(GPU_Draw){
            .shader = screen_shader,
            .rectangle = { .x = 0, .y = 0, .z = (f32)screen_width * scale, .w = (f32)screen_height * scale },
            .textures = { screen_texture },
            .arguments = gpu_arguments(screen_arguments),
            .blend_mode = GPU_Blend_Mode_NONE,
        });
    }

    if (platform->should_quit) {
        print("working stack (bottom->top): { ");
        for (u8 token_id = 0; token_id < vm->stacks[0].data; token_id += 1) print("%llx ", vm->stacks[0].memory[token_id]);
        print("}\nreturn  stack (bottom->top): { ");
        for (u8 token_id = 0; token_id < vm->stacks[1].data; token_id += 1) print("%llx ", vm->stacks[1].memory[token_id]);
        print("}\n");
    }
}
