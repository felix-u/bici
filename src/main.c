#include "SDL.h"

#define BASE_GRAPHICS 0
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

#define screen_width 320
#define screen_height 240

structdef(Screen) {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    u32 pixels[screen_width * screen_height];
};

static Screen screen_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) panic("SDL_Init failed");

    Screen screen = {0};

    screen.window = SDL_CreateWindow(
        "bici",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        screen_width, screen_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );
    if (screen.window == 0) panic("SDL_CreateWindow failed: %", fmt(cstring, (char *)SDL_GetError()));

    // TODO: SDL_CreateRenderer crashes with asan
    screen.renderer = SDL_CreateRenderer(screen.window, -1, SDL_RENDERER_ACCELERATED);
    if (screen.renderer == 0) panic("SDL_CreateRenderer failed: %", fmt(cstring, (char *)SDL_GetError()));

    screen.texture = SDL_CreateTexture(
        screen.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        screen_width, screen_height
    );
    if (screen.texture == 0) panic("SDL_CreateTexture failed: %", fmt(cstring, (char *)SDL_GetError()));

    return screen;
}

static void screen_quit(Screen *screen) {
    SDL_DestroyTexture(screen->texture);
    SDL_DestroyRenderer(screen->renderer);
    SDL_DestroyWindow(screen->window);
    SDL_Quit();
}

static bool screen_update(Screen *screen) {
    static bool fullscreen = false;

    SDL_Event event = {0};
    while (SDL_PollEvent(&event) != 0) switch (event.type) {
        case SDL_KEYDOWN: {
            if (event.key.keysym.sym != SDLK_F11) break;
            fullscreen = !fullscreen;
            SDL_SetWindowFullscreen(screen->window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        } break;
        case SDL_QUIT: return false;
        default: break;
    }

    SDL_UpdateTexture(screen->texture, 0, screen->pixels, screen_width * sizeof(u32));
    SDL_RenderClear(screen->renderer);
    SDL_RenderCopy(screen->renderer, screen->texture, 0, 0);
    SDL_RenderPresent(screen->renderer);

    return true;
}

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

enum Vm_Screen_Action {
    vm_screen_init   = 0x0,
    vm_screen_pixel  = 0x1,
    vm_screen_update = 0x2,
    vm_screen_quit   = 0x4,
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
    case vm_op_dup:    assert(!vm->op_mode.keep); vm_push##bi(vm, vm_get##bi(vm, 1)); break;\
    case vm_op_over:   assert(!vm->op_mode.keep); vm_push##bi(vm, vm_get##bi(vm, 2)); break;\
    case vm_op_eq:     vm_push8(vm, vm_pop##bi(vm) == vm_pop##bi(vm)); break;\
    case vm_op_neq:    vm_push8(vm, vm_pop##bi(vm) != vm_pop##bi(vm)); break;\
    case vm_op_gt:     { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push8(vm, left > right); } break;\
    case vm_op_lt:     { u##bi right = vm_pop##bi(vm), left = vm_pop##bi(vm); vm_push8(vm, left < right); } break;\
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
    case vm_op_load:   { u16 addr = vm_pop16(vm); u##bi val = vm_load##bi(vm, addr); vm_push##bi(vm, val); } break;\
    case vm_op_store:  { u16 addr = vm_pop16(vm); u##bi val = vm_pop##bi(vm); vm_store##bi(vm, addr, val); } break;\
    case vm_op_read:   {\
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
    case vm_op_write:  {\
        u8 vm_device_and_action = vm_pop8(vm);\
        Vm_Device device = vm_device_and_action & 0xf0;\
        u8 action = vm_device_and_action & 0x0f;\
        switch (device) {\
            case vm_device_console: switch (action) {\
                case 0x0: {\
                    assert(vm->op_mode.size == vm_op_size_byte);\
                    u16 str_addr = vm_pop16(vm);\
                    u8 str_len = vm_load8(vm, str_addr);\
                    String str = { .ptr = vm->memory + str_addr + 1, .len = str_len };\
                    /* TODO(felix): check if this is print from SDL */ print("%", fmt(String, str));\
                } break;\
                default: panic("[write] invalid action #% for console device", fmt(u64, action, .base = 16));\
            } break;\
            case vm_device_screen: switch (action) {\
                case vm_screen_pixel: {\
                    Screen_Colour colour = vm_pop8(vm);\
                    if (colour >= screen_colour_count) panic("[write:screen/pixel] colour #% is invalid; there are only #% palette colours", fmt(u64, colour, .base = 16), fmt(u64, screen_colour_count));\
                    u16 y = vm_pop16(vm), x = vm_pop16(vm);\
                    if (x >= screen_width || y >= screen_height) panic("[write:screen/pixel] coordinate #%x#% is outside screen bounds #%x#%", fmt(u64, x, .base = 16), fmt(u64, y, .base = 16), fmt(u64, screen_width, .base = 16), fmt(u64, screen_height, .base = 16));\
                    vm->screen.pixels[y * screen_width + x] = screen_palette[colour];\
                } break;\
                default: panic("[write] invalid action #% for screen device", fmt(u64, action, .base = 16));\
            } break;\
            default: panic("invalid device #% for operation 'write'", fmt(u64, device, .base = 16));\
        }\
    } break;\
    default: panic("unreachable %{#%}", fmt(cstring, (char *)vm_op_name(byte)), fmt(u64, byte, .base = 16));

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

// TODO(felix): this is only used once, so inline
static char *mode_name(u8 instruction) {
    if (vm_instruction_is_special(instruction)) return "";
    switch ((instruction & 0xe0) >> 5) {
        case 0x1: return "2";
        case 0x2: return "r";
        case 0x3: return "r2";
        case 0x4: return "k";
        case 0x5: return "k2";
        case 0x6: return "kr";
        case 0x7: return "kr2";
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
            default: panic("unreachable %{#%}", fmt(cstring, (char *)vm_op_name(byte)), fmt(u64, byte, .base = 16));
        }
    }
}

static void vm_run(String rom) {
    if (rom.len == 0) return;

    Vm vm = {0};
    memcpy(vm.memory, rom.ptr, rom.len);

    print("MEMORY ===\n");
    for (u16 i = 0x100; i < rom.len; i += 1) {
        u8 byte = vm.memory[i];
        print("[%]\t'%'\t#%\t%;%\n", fmt(u64, i, .base = 16), fmt(char, byte), fmt(u64, byte, .base = 16), fmt(cstring, (char *)vm_op_name(byte)), fmt(cstring, mode_name(byte)));
    }
    print("\nRUN ===\n");

    vm.screen = screen_init();
    u16 vm_on_screen_init_pc = vm_load16(&vm, vm_device_screen);
    vm_run_to_break(&vm, vm_on_screen_init_pc);

    u16 vm_on_screen_update_pc = vm_load16(&vm, vm_device_screen | vm_screen_update);
    do {
        vm_run_to_break(&vm, vm_on_screen_update_pc);
    } while (screen_update(&vm.screen));

    u16 vm_on_screen_quit_pc = vm_load16(&vm, vm_device_screen | vm_screen_quit);
    vm_run_to_break(&vm, vm_on_screen_quit_pc);
    screen_quit(&vm.screen);

    print("param  stack (bot->top): { ");
    for (u8 i = 0; i < vm.stacks[stack_param].ptr; i += 1) print("% ", fmt(u64, vm.stacks[stack_param].memory[i], .base = 16));
    print("}\nreturn stack (bot->top): { ");
    for (u8 i = 0; i < vm.stacks[stack_ret].ptr; i += 1) print("% ", fmt(u64, vm.stacks[stack_ret].memory[i], .base = 16));
    print("}\n");
}

structdef(Asm_Label) { String name; u16 addr; };

structdef(Asm_Label_Ref) { String name; u16 loc; Vm_Op_Size size; };

enumdef(Asm_Res_Mode, u8) { res_num, res_addr };
structdef(Asm_Res) { u16 pc; Asm_Res_Mode mode; };

structdef(Asm) {
    char *path_biciasm;
    String infile;
    u16 i;

    Array_Asm_Label labels;
    Array_Asm_Label_Ref label_refs;
    Array_Asm_Res forward_res;

    Array_u8 rom;
};

static void asm_label_push(Asm *ctx, String name, u16 addr) {
    if (ctx->labels.len == ctx->labels.cap) {
        panic("cannot add label '%': maximum number of labels reached", fmt(String, name));
    }
    ctx->labels.ptr[ctx->labels.len] = (Asm_Label){ .name = name, .addr = addr };
    ctx->labels.len += 1;
}

static u16 asm_label_get_addr_of(Asm *ctx, String name) {
    for (u8 i = 0; i < ctx->labels.len; i += 1) {
        if (!string_equal(ctx->labels.ptr[i].name, name)) continue;
        return ctx->labels.ptr[i].addr;
    }
    err("no such label '%'", fmt(String, name));
    return 0;
}

structdef(Asm_Hex) {
    bool ok;
    u8 num_digits;
    union { u8 int8; u16 int16; } result;
};

static Asm_Hex asm_parse_hex(Asm *ctx) {
    u16 beg_i = ctx->i;
    while (ctx->i < ctx->infile.len && is_hex_digit_table[ctx->infile.ptr[ctx->i]]) ctx->i += 1;
    u16 end_i = ctx->i;
    ctx->i -= 1;
    u8 num_digits = (u8)(end_i - beg_i);
    String hex_string = { .ptr = ctx->infile.ptr + beg_i, .len = num_digits };

    switch (num_digits) {
        case 2: return (Asm_Hex){
            .ok = true,
            .num_digits = num_digits,
            .result.int8 = (u8)decimal_from_hex_string(hex_string),
        };
        case 4: return (Asm_Hex){
            .ok = true,
            .num_digits = num_digits,
            .result.int16 = (u16)decimal_from_hex_string(hex_string),
        };
        default: {
            err("expected 2 digits (byte) or 4 digits (short), found % digits in number at '%'[%..%]", fmt(u64, num_digits), fmt(cstring, ctx->path_biciasm), fmt(u64, beg_i), fmt(u64, end_i));
            return (Asm_Hex){0};
        } break;
    }
}

static bool asm_is_whitespace(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool asm_is_alpha(u8 c) { return ('0' <= c && c <= '9') || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || (c == '_'); }

static String asm_parse_alpha(Asm *ctx) {
    u16 beg_i = ctx->i;
    u8 first_char = ctx->infile.ptr[ctx->i];
    if (!asm_is_alpha(first_char)) {
        err(
            "expected alphabetic character or underscore, found byte '%'/%/#% at '%'[%]",
            fmt(char, first_char), fmt(u64, first_char), fmt(u64, first_char, .base = 16), fmt(cstring, ctx->path_biciasm), fmt(u64, ctx->i)
        );
        return (String){0};
    }

    ctx->i += 1;
    while (ctx->i < ctx->infile.len && (asm_is_alpha(ctx->infile.ptr[ctx->i]))) ctx->i += 1;
    u16 end_i = ctx->i;
    ctx->i -= 1;
    return (String){ .ptr = ctx->infile.ptr + beg_i, .len = end_i - beg_i };
}

static void asm_rom_write(Asm *ctx, u8 byte) {
    if (ctx->rom.len == ctx->rom.cap) {
        err("reached maximum rom size");
        abort();
    }
    array_push_assume_capacity(&ctx->rom, &byte);
}

static void asm_rom_write2(Asm *ctx, u16 byte2) {
    asm_rom_write(ctx, (byte2 & 0xff00) >> 8);
    asm_rom_write(ctx, byte2 & 0x00ff);
}

static void asm_forward_res_push(Asm *ctx, Asm_Res_Mode res_mode) {
    if (ctx->forward_res.len == 0x0ff) {
        err("cannot resolve address at '%'[%]: maximum number of resolutions reached", fmt(cstring, ctx->path_biciasm), fmt(u64, ctx->i));
        ctx->rom.len = 0;
        return;
    }
    ctx->forward_res.ptr[ctx->forward_res.len] = (Asm_Res){ .pc = (u16)ctx->rom.len, .mode = res_mode };
    ctx->forward_res.len += 1;
}
static void asm_forward_res_resolve_last(Asm *ctx) {
    if (ctx->forward_res.len == 0) {
        err("forward resolution marker at '%'[%] matches no opening marker", fmt(cstring, ctx->path_biciasm), fmt(u64, ctx->i));
        ctx->rom.len = 0;
        return;
    }
    ctx->forward_res.len -= 1;
    Asm_Res res = ctx->forward_res.ptr[ctx->forward_res.len];
    switch (res.mode) {
        case res_num: ctx->rom.ptr[res.pc] = (u8)(ctx->rom.len - 1 - res.pc); break;
        case res_addr: {
            u16 pc_bak = (u16)ctx->rom.len;
            ctx->rom.len = res.pc;
            asm_rom_write2(ctx, pc_bak);
            ctx->rom.len = pc_bak;
        } break;
        default: unreachable;
    }
}

static void asm_compile_op(Asm *ctx, Vm_Op_Mode mode, u8 op) {
    if (vm_instruction_is_special(op)) { asm_rom_write(ctx, op); return; }
    asm_rom_write(ctx, op | (u8)(mode.size << 5) | (u8)(mode.stack << 6) | (u8)(mode.keep << 7));
}

static String asm_compile(Arena *arena, usize max_asm_filesize, char *_path_biciasm) {
    static u8 rom_mem[0x10000];
    static Asm_Label labels_mem[0x100];
    static Asm_Label_Ref label_refs_mem[0x100];
    static Asm_Res forward_res_mem[0x100];

    Asm ctx = {
        .path_biciasm = _path_biciasm,
        .infile = file_read_bytes_relative_path(arena, _path_biciasm, max_asm_filesize),
        .labels = { .ptr = labels_mem, .cap = 0x100 },
        .label_refs = { .ptr = label_refs_mem, .cap = 0x100 },
        .forward_res = { .ptr = forward_res_mem, .cap = 0x100 },
        .rom = { .ptr = rom_mem, .len = 0x100, .cap = 0x10000 },
    };
    if (ctx.infile.len == 0) return (String){0};

    u16 add = 1;
    for (ctx.i = 0; ctx.i < ctx.infile.len; ctx.i += add) {
        add = 1;
        if (asm_is_whitespace(ctx.infile.ptr[ctx.i])) continue;

        Vm_Op_Mode mode = {0};

        switch (ctx.infile.ptr[ctx.i]) {
            case '/': if (ctx.i + 1 < ctx.infile.len && ctx.infile.ptr[ctx.i + 1] == '/') {
                for (ctx.i += 2; ctx.i < ctx.infile.len && ctx.infile.ptr[ctx.i] != '\n';) ctx.i += 1;
            } continue;
            case '|': {
                ctx.i += 1;
                Asm_Hex new_pc = asm_parse_hex(&ctx);
                if (!new_pc.ok) return (String){0};
                ctx.rom.len = new_pc.result.int16;
            } continue;
            case '@': {
                ctx.i += 1;
                String name = asm_parse_alpha(&ctx);
                ctx.label_refs.ptr[ctx.label_refs.len] = (Asm_Label_Ref){ .name = name, .loc = (u16)ctx.rom.len, .size = vm_op_size_short };
                ctx.label_refs.len += 1;
                ctx.rom.len += 2;
            } continue;
            case '#': {
                ctx.i += 1;
                Asm_Hex num = asm_parse_hex(&ctx);
                if (!num.ok) return (String){0};
                switch (num.num_digits) {
                    case 2: asm_compile_op(&ctx, (Vm_Op_Mode){ .size = vm_op_size_byte }, vm_op_push); asm_rom_write(&ctx, num.result.int8); break;
                    case 4: asm_compile_op(&ctx, (Vm_Op_Mode){ .size = vm_op_size_short }, vm_op_push); asm_rom_write2(&ctx, num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '*': {
                asm_compile_op(&ctx, (Vm_Op_Mode){ .size = vm_op_size_byte }, vm_op_push);
                ctx.i += 1;
                String name = asm_parse_alpha(&ctx);
                ctx.label_refs.ptr[ctx.label_refs.len] = (Asm_Label_Ref){ .name = name, .loc = (u16)ctx.rom.len, .size = vm_op_size_byte };
                ctx.label_refs.len += 1;
                ctx.rom.len += 1;
            } continue;
            case '&': {
                asm_compile_op(&ctx, (Vm_Op_Mode){ .size = vm_op_size_short }, vm_op_push);
                ctx.i += 1;
                String name = asm_parse_alpha(&ctx);
                ctx.label_refs.ptr[ctx.label_refs.len] = (Asm_Label_Ref){ .name = name, .loc = (u16)ctx.rom.len, .size = vm_op_size_short };
                ctx.label_refs.len += 1;
                ctx.rom.len += 2;
            } continue;
            case ':': {
                ctx.i += 1;
                String name = asm_parse_alpha(&ctx);
                asm_label_push(&ctx, name, (u16)ctx.rom.len);
            } continue;
            case '{': {
                ctx.i += 1;
                switch (ctx.infile.ptr[ctx.i]) {
                    case '#': {
                        asm_forward_res_push(&ctx, res_num);
                        ctx.rom.len += 1;
                    } continue;
                    default: {
                        asm_forward_res_push(&ctx, res_addr);
                        ctx.rom.len += 2;
                        add = 0;
                    } continue;
                }
            } continue;
            case '}': asm_forward_res_resolve_last(&ctx); continue;
            case '_': {
                ctx.i += 1;
                Asm_Hex num = asm_parse_hex(&ctx);
                if (!num.ok) return (String){0};
                switch (num.num_digits) {
                    case 2: asm_rom_write(&ctx, num.result.int8); break;
                    case 4: asm_rom_write2(&ctx, num.result.int16); break;
                    default: unreachable;
                }
            } continue;
            case '"': {
                for (ctx.i += 1; ctx.i < ctx.infile.len && !asm_is_whitespace(ctx.infile.ptr[ctx.i]); ctx.i += 1) {
                    asm_rom_write(&ctx, ctx.infile.ptr[ctx.i]);
                }
                add = 0;
            } continue;
            default: break;
        }

        u8 first_char = ctx.infile.ptr[ctx.i];
        if (!asm_is_alpha(first_char)) {
            err("invalid byte '%'/%/#% at '%'[%]", fmt(char, first_char), fmt(u64, first_char), fmt(u64, first_char, .base = 16), fmt(cstring, ctx.path_biciasm), fmt(u64, ctx.i));
            return (String){0};
        }

        usize beg_i = ctx.i, end_i = beg_i;
        for (; ctx.i < ctx.infile.len && !asm_is_whitespace(ctx.infile.ptr[ctx.i]); ctx.i += 1) {
            if ('a' <= ctx.infile.ptr[ctx.i] && ctx.infile.ptr[ctx.i] <= 'z') continue;
            end_i = ctx.i;
            if (ctx.infile.ptr[ctx.i] == ';') for (ctx.i += 1; ctx.i < ctx.infile.len;) {
                u8 c = ctx.infile.ptr[ctx.i];
                if (asm_is_whitespace(c)) goto done;
                switch (c) {
                    case 'k': mode.keep = true; break;
                    case 'r': mode.stack = stack_ret; break;
                    case '2': mode.size = vm_op_size_short; break;
                    default: {
                        err("expected one of ['k', 'r', '2'], found byte '%'/%/#% at '%'[%]", fmt(char, c), fmt(u64, c), fmt(u64, c, .base = 16), fmt(cstring, ctx.path_biciasm), fmt(u64, ctx.i));
                        return (String){0};
                    } break;
                }
                ctx.i += 1;
            }
        }
        done:
        if (end_i == beg_i) end_i = ctx.i;
        String lexeme = string_range(ctx.infile, beg_i, end_i);

        if (false) {}
        #define compile_if_op_string(name, val)\
            else if (string_equal(lexeme, string(#name))) asm_compile_op(&ctx, mode, val);
        vm_for_op(compile_if_op_string)
        else {
            err("invalid token '%' at '%'[%..%]", fmt(String, lexeme), fmt(cstring, ctx.path_biciasm), fmt(u64, beg_i), fmt(u64, end_i));
            return (String){0};
        };

        continue;
    }
    if (ctx.rom.len == 0) return (String){0};
    String rom = slice_from_array(ctx.rom);

    for (u8 i = 0; i < ctx.label_refs.len; i += 1) {
        Asm_Label_Ref ref = ctx.label_refs.ptr[i];
        if (ref.name.len == 0) return (String){0};
        ctx.rom.len = ref.loc;
        switch (ref.size) {
            case vm_op_size_byte: asm_rom_write(&ctx, (u8)asm_label_get_addr_of(&ctx, ref.name)); break;
            case vm_op_size_short: asm_rom_write2(&ctx, asm_label_get_addr_of(&ctx, ref.name)); break;
            default: unreachable;
        }
    }

    print("ASM ===\n");
    for (usize i = 0x100; i < rom.len; i += 1) print("% ", fmt(u64, rom.ptr[i], .base = 16));
    putchar('\n');

    return rom;
}

#define usage "usage: bici <com|run|script> <file...>"

int main(int argc, char **argv) {
    if (argc < 3) {
        err("%", fmt(cstring, usage));
        return 1;
    }

    Arena arena = {0};

    String cmd = string_from_cstring(argv[1]);
    if (string_equal(cmd, string("com"))) {
        if (argc != 4) {
            err("usage: bici com <file.biciasm> <file.bici>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        String rom = asm_compile(&arena, max_asm_filesize, argv[2]);
        file_write_bytes_to_relative_path(argv[3], rom);
    } else if (string_equal(cmd, string("run"))) {
        arena = arena_init(0x10000);
        vm_run(file_read_bytes_relative_path(&arena, argv[2], 0x10000));
    } else if (string_equal(cmd, string("script"))) {
        if (argc != 3) {
            err("usage: bici script <file.biciasm>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        vm_run(asm_compile(&arena, max_asm_filesize, argv[2]));
    } else {
        err("no such command '%'\n%", fmt(String, cmd), fmt(cstring, usage));
        return 1;
    }

    arena_deinit(&arena);
    return 0;
}
