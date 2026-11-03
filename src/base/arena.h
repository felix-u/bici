// https://github.com/felix-u 2026-11-02
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(ARENA_H)
#define ARENA_H


#if !defined(ARENA_NO_SYSTEM_INCLUDE)
    #include <stddef.h> // size_t
#endif

typedef struct {
    unsigned char *memory;
    size_t capacity;
    size_t committed;
    size_t offset;
    size_t last_offset;
} Arena;

typedef struct {
    Arena *arena;
    size_t offset;
} arena_Scratch;

#if !defined(ARENA_FUNCTION)
    #define ARENA_FUNCTION
#endif

#define arena_make(arena, item_count, T) (T *)arena_make_((arena), (item_count), sizeof(T), 1)

ARENA_FUNCTION void          arena_free(Arena *arena);
ARENA_FUNCTION void         *arena_make_(Arena *arena, size_t item_count, size_t item_size, _Bool zero);
ARENA_FUNCTION void         *arena_push(Arena *arena, const void *data, size_t byte_count);
ARENA_FUNCTION Arena         arena_reserve(size_t byte_count);
ARENA_FUNCTION arena_Scratch scratch_begin(Arena *arena);
ARENA_FUNCTION void          scratch_end(arena_Scratch scratch);


#endif // ARENA_H


#if defined(ARENA_IMPLEMENTATION)


#if defined(_WIN32)
    #define ARENA_OS_WINDOWS
#else
    #error "UNIMPLEMENTED"
#endif

#if !defined(ARENA_NO_SYSTEM_INCLUDE)
    #if defined(_MSC_VER)
        #pragma comment(lib, "Kernel32.lib")
    #endif
    #include <windows.h>
#endif

#if !defined(ARENA_ASSERT)
    #include <assert.h>
    #define ARENA_ASSERT assert
#endif

#if !defined(ARENA_PANIC)
    #define ARENA_PANIC(message) ARENA_ASSERT(0 && (message))
#endif

#if !defined(ARENA_ASAN_POISON)
    #define ARENA_ASAN_POISON(address, byte_count)
#endif

#if !defined(ARENA_ASAN_UNPOISON)
    #define ARENA_ASAN_UNPOISON(address, byte_count)
#endif

// NOTE(felix): Targets with 32-bit addresses must define a smaller value.
#if !defined(ARENA_DEFAULT_RESERVE_SIZE)
    #define ARENA_DEFAULT_RESERVE_SIZE ((size_t)4 * 1024 * 1024 * 1024)
#endif

// NOTE(felix): Must be a power of 2 and a multiple of the page size.
#if !defined(ARENA_COMMIT_SIZE)
    #define ARENA_COMMIT_SIZE ((size_t)256 * 1024)
#endif

#if !defined(ARENA_MEMCPY)
    #define ARENA_MEMCPY arena__memcpy
    static void *arena__memcpy(void *destination, const void *source, size_t byte_count) {
        for (size_t i = 0; i < byte_count; i += 1) ((unsigned char *)destination)[i] = ((const unsigned char *)source)[i];
        return destination;
    }
#endif

#if !defined(ARENA_MEMSET)
    #define ARENA_MEMSET arena__memset
    static void *arena__memset(void *destination, int byte, size_t byte_count) {
        for (size_t i = 0; i < byte_count; i += 1) ((unsigned char *)destination)[i] = (unsigned char)byte;
        return destination;
    }
#endif

#define ARENA__ALIGNMENT (2 * sizeof(void *))

typedef enum {
    Arena_Win32_Memory_COMMIT  = 0x1000,
    Arena_Win32_Memory_RESERVE = 0x2000,
    Arena_Win32_Memory_RELEASE = 0x8000,
} Arena_Win32_Memory;

typedef enum {
    Arena_Win32_Protection_READ_WRITE = 0x04,
} Arena_Win32_Protection;

ARENA_FUNCTION void arena_free(Arena *arena) {
    if (arena->memory != 0) {
        ARENA_ASAN_UNPOISON(arena->memory, arena->committed);
        VirtualFree(arena->memory, 0, Arena_Win32_Memory_RELEASE);
    }
    *arena = (Arena){0};
}

ARENA_FUNCTION void *arena_make_(Arena *arena, size_t item_count, size_t item_size, _Bool zero) {
    ARENA_ASSERT(arena != 0);
    ARENA_ASSERT(item_size > 0);
    if (item_count > (size_t)-1 / item_size) {
        ARENA_PANIC("arena_make_: size overflow");
        return 0;
    }
    size_t byte_count = item_count * item_size;
    if (byte_count == 0) return 0;

    if (arena->memory == 0) *arena = arena_reserve(ARENA_DEFAULT_RESERVE_SIZE);
    if (arena->memory == 0) return 0;

    size_t pad = (ARENA__ALIGNMENT - (arena->offset & (ARENA__ALIGNMENT - 1))) & (ARENA__ALIGNMENT - 1);
    size_t available = arena->capacity - arena->offset;
    if (pad > available || byte_count > available - pad) {
        ARENA_PANIC("arena_make_: out of reserved memory");
        return 0;
    }

    size_t end = arena->offset + pad + byte_count;
    if (end > arena->committed) {
        size_t new_committed = (end + ARENA_COMMIT_SIZE - 1) & ~(ARENA_COMMIT_SIZE - 1);
        ARENA_ASSERT(new_committed <= arena->capacity);

        void *memory = VirtualAlloc(arena->memory + arena->committed, new_committed - arena->committed, Arena_Win32_Memory_COMMIT, Arena_Win32_Protection_READ_WRITE);
        if (memory == 0) {
            ARENA_PANIC("arena_make_: commit failure");
            return 0;
        }

        ARENA_ASAN_POISON(arena->memory + arena->committed, new_committed - arena->committed);
        arena->committed = new_committed;
    }

    #if defined(ARENA_POISON_BYTE)
        ARENA_ASAN_UNPOISON(arena->memory + arena->offset, pad);
        ARENA_MEMSET(arena->memory + arena->offset, ARENA_POISON_BYTE, pad);
    #endif
    ARENA_ASAN_POISON(arena->memory + arena->offset, pad);
    arena->offset += pad;

    unsigned char *result = arena->memory + arena->offset;
    arena->last_offset = arena->offset;
    arena->offset += byte_count;

    ARENA_ASAN_UNPOISON(result, byte_count);
    ARENA_ASAN_POISON(arena->memory + arena->offset, arena->committed - arena->offset);

    if (zero) ARENA_MEMSET(result, 0, byte_count);
    #if defined(ARENA_POISON_BYTE)
        else ARENA_MEMSET(result, ARENA_POISON_BYTE, byte_count);
    #endif

    return result;
}

ARENA_FUNCTION void *arena_push(Arena *arena, const void *data, size_t byte_count) {
    void *result = arena_make_(arena, byte_count, 1, 0);
    if (result != 0) ARENA_MEMCPY(result, data, byte_count);
    return result;
}

ARENA_FUNCTION Arena arena_reserve(size_t byte_count) {
    ARENA_ASSERT((ARENA_COMMIT_SIZE & (ARENA_COMMIT_SIZE - 1)) == 0);
    ARENA_ASSERT(ARENA_COMMIT_SIZE >= 4096);
    Arena arena = {0};

    size_t commit_mask = ARENA_COMMIT_SIZE - 1;
    _Bool size_can_round_up_to_commit_size = byte_count <= (size_t)-1 - commit_mask;
    if (byte_count == 0 || !size_can_round_up_to_commit_size) {
        ARENA_PANIC("arena_reserve: invalid size");
        return arena;
    }
    size_t capacity = (byte_count + commit_mask) & ~commit_mask;

    unsigned char *memory = VirtualAlloc(0, capacity, Arena_Win32_Memory_RESERVE, Arena_Win32_Protection_READ_WRITE);
    if (memory == 0) {
        ARENA_PANIC("arena_reserve: reservation failure");
        capacity = 0;
    }

    arena.memory = memory;
    arena.capacity = capacity;
    return arena;
}

ARENA_FUNCTION arena_Scratch scratch_begin(Arena *arena) {
    arena_Scratch scratch = { .arena = arena, .offset = arena->offset };
    return scratch;
}

ARENA_FUNCTION void scratch_end(arena_Scratch scratch) {
    Arena *arena = scratch.arena;
    _Bool is_reverse_order = scratch.offset <= arena->offset;
    ARENA_ASSERT(is_reverse_order);

    if (scratch.offset < arena->offset) {
        #if defined(ARENA_POISON_BYTE)
            ARENA_ASAN_UNPOISON(arena->memory + scratch.offset, arena->offset - scratch.offset);
            ARENA_MEMSET(arena->memory + scratch.offset, ARENA_POISON_BYTE, arena->offset - scratch.offset);
        #endif
        ARENA_ASAN_POISON(arena->memory + scratch.offset, arena->offset - scratch.offset);

        arena->offset = scratch.offset;
        if (arena->last_offset > scratch.offset) arena->last_offset = scratch.offset;
    }
}


#endif // ARENA_IMPLEMENTATION
