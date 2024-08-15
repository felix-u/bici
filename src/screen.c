typedef Array(u32) Array_u32;

enumdef(Screen_State, u8) {
    screen_state_ok = 0,
    screen_state_resized,
};

// TODO palette

structdef(Screen) {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    u16 width, height;
    Array_u32 pixels;
    Screen_State state;
};

static Screen screen_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) panic("SDL_Init failed");

    Screen screen = {0};

    int default_width = 640, default_height = 360;
    screen.window = SDL_CreateWindow(
        "bici", 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        default_width, default_height, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (screen.window == 0) panicf("SDL_CreateWindow failed: %s", SDL_GetError());

    screen.renderer = SDL_CreateRenderer(screen.window, -1, SDL_RENDERER_ACCELERATED);
    if (screen.renderer == 0) panicf("SDL_CreateRenderer failed: %s", SDL_GetError());

    SDL_DisplayMode display_mode;
    int display_idx = 0;
    if (SDL_GetCurrentDisplayMode(display_idx, &display_mode) != 0) panicf("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
    u16 max_screen_width = (u16)display_mode.w;
    u16 max_screen_height = (u16)display_mode.h;
    usize max_screen_bytes = sizeof(u32) * max_screen_width * max_screen_height;
    Arena screen_arena = arena_init(max_screen_bytes);
    arena_alloc_array(&screen_arena, &screen.pixels, max_screen_width * max_screen_height);

    SDL_GetWindowSize(screen.window, (int *)&screen.width, (int *)&screen.height);

    screen.texture = SDL_CreateTexture(
        screen.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 
        screen.width, screen.height
    );
    if (screen.texture == 0) panicf("SDL_CreateTexture failed", SDL_GetError());

    return screen;
}

static void screen_quit(Screen *screen) {
    SDL_DestroyTexture(screen->texture);
    SDL_DestroyRenderer(screen->renderer);
    SDL_DestroyWindow(screen->window);
    SDL_Quit();
}

static bool screen_update(Screen *screen) {
    screen->state = screen_state_ok;
    static bool fullscreen = false;

    SDL_Event event = {0};
    while (SDL_PollEvent(&event) != 0) switch (event.type) {
        case SDL_KEYDOWN: {
            if (event.key.keysym.sym != SDLK_F11) break;
            fullscreen = !fullscreen;
            SDL_SetWindowFullscreen(screen->window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        } break;
        case SDL_WINDOWEVENT: switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED: {
                SDL_GetWindowSize(screen->window, (int *)&screen->width, (int *)&screen->height);
                screen->state = screen_state_resized;
            } break;
            default: break; 
        } break;
        case SDL_QUIT: return false;
        default: break;
    }

    SDL_UpdateTexture(screen->texture, 0, screen->pixels.ptr, screen->width * sizeof(u32));
    SDL_RenderClear(screen->renderer);
    SDL_RenderCopy(screen->renderer, screen->texture, 0, 0);
    SDL_RenderPresent(screen->renderer);

    return true;
}

static void screen_get_width_height(Screen *screen) { SDL_GetWindowSize(screen->window, (int *)&screen->width, (int *)&screen->height); }
