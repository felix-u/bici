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
