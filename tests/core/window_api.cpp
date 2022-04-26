#include <test_framework/test_framework.h>

#ifdef WINDOW_SDL2
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

TEST
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags window_flags;

// We need to know which renderer to use
#ifdef RENDERER_VULKAN
    window_flags = SDL_WINDOW_VULKAN;
#endif

    SDL_Window *window = SDL_CreateWindow("Test window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 500, 500, window_flags);
    ASSERT_NOT_NULL(window);

    SDL_DestroyWindow(window);
    SDL_Quit();
}
#endif