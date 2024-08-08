#include "base/base_include.h"
#include "base/base_include.c"

#include "bici.c"
#include "compile.c"

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
        compile(&arena, max_asm_filesize, argv[2], argv[3]);
    } else if (string8_eql(cmd, string8("run"))) {
        run(argv[2]);
        return 0;
    } else if (string8_eql(cmd, string8("script"))) {
        if (argc != 4) {
            err("usage: bici script <file.biciasm> <file.bici>");
            return 1;
        }
        usize max_asm_filesize = 8 * 1024 * 1024;
        arena = arena_init(max_asm_filesize);
        if (compile(&arena, max_asm_filesize, argv[2], argv[3])) run(argv[3]);
    } else {
        errf("no such command '%.*s'\n%s", string_fmt(cmd), usage);
        return 1;
    }

    arena_deinit(&arena);
    return 0;
}
