static void screen_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) panic("SDL_Init failed");

    SDL_Window *window = SDL_CreateWindow("bici", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320, 180, SDL_WINDOW_SHOWN);
    if (window == 0) panic("SDL_CreateWindow failed");

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == 0) panic("SDL_CreateRenderer failed");
}

static void screen_quit(void) {
    SDL_Quit();
}
