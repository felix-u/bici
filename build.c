#define PLATFORM_NONE 1
#define BASE_IMPLEMENTATION
#include "src/base/base.h"

#define APP_NAME "bici"

static void program(void) {
    Arena arena = {0};
    u8 target_os = BASE_OS;

    u64 argument_count = 0;
    const char **arguments = os_get_arguments(&arena, &argument_count);
    for (u64 i = 0; i < argument_count; i += 1) {
        if (cstring_equals(arguments[i], "web")) target_os = BASE_OS_WASM;
    }

    u32 exit_code = build_default_everything(&arena, APP_NAME, &(Build_Options){ .target_os = target_os });
    os_exit(exit_code);
}
