#include "base/base_include.h"
#include "base/base_include.c"

#include "SDL.h"

#include "screen.c"
#include "vm.c"
#include "asm.c"

#define usage "usage: bici <com|run|script> <file...>"

int main(int argc, char **argv) {
    if (argc < 3) {
        errf("%s", usage);
        return 1;
    }

    Arena arena = {0};

    String8 cmd = string8_from_cstring(argv[1]);
    if (string8_eql(cmd, string8("com"))) {
        if (argc != 4) {
            err("usage: bici com <file.biciasm> <file.bici>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        String8 rom = asm_compile(&arena, max_asm_filesize, argv[2]);
        file_open_write_close(argv[3], "wb", rom);
    } else if (string8_eql(cmd, string8("run"))) {
        arena = arena_init(0x10000);
        vm_run(&(Vm){ .pc = 0x100 }, file_read(&arena, argv[2], "rb", 0x10000));
        return 0;
    } else if (string8_eql(cmd, string8("script"))) {
        if (argc != 3) {
            err("usage: bici script <file.biciasm>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        vm_run(&(Vm){ .pc = 0x100 }, asm_compile(&arena, max_asm_filesize, argv[2]));
    } else {
        errf("no such command '%.*s'\n%s", string_fmt(cmd), usage);
        return 1;
    }

    arena_deinit(&arena);
    return 0;
}
