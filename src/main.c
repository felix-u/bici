#include "SDL.h"

#define BASE_GRAPHICS 0
#include "base/base.c"

#include "screen.c"
#include "vm.c"
#include "asm.c"

#define usage "usage: bici <com|run|script> <file...>"

int main(int argc, char **argv) {
    if (argc < 3) {
        err("%", fmt(cstring, usage));
        return 1;
    }

    Arena arena = {0};

    Str8 cmd = str8_from_strc(argv[1]);
    if (str8_eql(cmd, str8("com"))) {
        if (argc != 4) {
            err("usage: bici com <file.biciasm> <file.bici>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        Str8 rom = asm_compile(&arena, max_asm_filesize, argv[2]);
        file_write_bytes_to_relative_path(argv[3], rom);
    } else if (str8_eql(cmd, str8("run"))) {
        arena = arena_init(0x10000);
        vm_run(file_read_bytes_relative_path(&arena, argv[2], 0x10000));
    } else if (str8_eql(cmd, str8("script"))) {
        if (argc != 3) {
            err("usage: bici script <file.biciasm>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        vm_run(asm_compile(&arena, max_asm_filesize, argv[2]));
    } else {
        err("no such command '%'\n%", fmt(Str8, cmd), fmt(cstring, usage));
        return 1;
    }

    arena_deinit(&arena);
    return 0;
}
