static void screen_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) == -1) panic("SDL_Init failed");
}

static void screen_quit(void) {
    SDL_Quit();
}
