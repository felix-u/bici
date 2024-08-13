static SDL_Window *screen_window;
static SDL_Renderer *screen_renderer;
static SDL_Texture *screen_texture;

#define screen_width 320
#define screen_height 180
u32 pixels[screen_width * screen_height];

static void screen_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) panic("SDL_Init failed");

    screen_window = SDL_CreateWindow(
        "bici", 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        screen_width, screen_height, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (screen_window == 0) panic("SDL_CreateWindow failed");

    screen_renderer = SDL_CreateRenderer(screen_window, -1, SDL_RENDERER_ACCELERATED);
    if (screen_renderer == 0) panic("SDL_CreateRenderer failed");

    screen_texture = SDL_CreateTexture(
        screen_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 
        screen_width, screen_height
    );
    if (screen_texture == 0) panic("SDL_CreateTexture failed");

    for (u32 y = 0; y < screen_height; y += 1) for (u32 x = 0; x < screen_width; x += 1) {
        pixels[y * screen_width + x] = 0xff0000ff;
    }
}

static void screen_quit(void) {
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(screen_renderer);
    SDL_DestroyWindow(screen_window);
    SDL_Quit();
}

static void screen_update(void) {
    SDL_Event event = {0};
    while (SDL_PollEvent(&event) != 0) switch (event.type) {
        case SDL_QUIT: screen_quit(); break;
        default: break;
    }

    SDL_UpdateTexture(screen_texture, 0, pixels, screen_width * sizeof(u32));
    SDL_RenderClear(screen_renderer);
    SDL_RenderCopy(screen_renderer, screen_texture, 0, 0);
    SDL_RenderPresent(screen_renderer);
}
