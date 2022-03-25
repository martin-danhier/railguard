#ifdef WINDOW_SDL2

#include "railguard/core/window.h"

#include <iostream>
#include <SDL2/SDL.h>

using namespace rg;

// --==== TYPES ====--

// Define obscure struct that holds data
struct Window::Data
{
    SDL_Window *sdl_window;
    Extend2D    extent;
};

// --==== UTILS FUNCTIONS ====--

void sdl_check(SDL_bool result)
{
    if (result != SDL_TRUE)
    {
        std::cerr << "[SDL Error] Got SDL_FALSE !\n";
        exit(1);
    }
}

// --==== WINDOW ====--

Window::Window(Extend2D extent, const char *title)
{
    // Initialize window
    data = new Data;

    // Save other info
    data->extent = extent;

    // Init SDL2
    SDL_WindowFlags window_flags;

// We need to know which renderer to use
#ifdef RENDERER_VULKAN
    window_flags = SDL_WINDOW_VULKAN;
#endif

    data->sdl_window = SDL_CreateWindow(title,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        static_cast<int32_t>(extent.width),
                                        static_cast<int32_t>(extent.height),
                                        window_flags);

    // Apply more settings to the window
    SDL_SetWindowResizable(data->sdl_window, SDL_TRUE);

    std::cout << "Window created\n";
}

Window::~Window()
{
    SDL_DestroyWindow(data->sdl_window);
    delete data;

    std::cout << "Window destroyed\n";
}

double Window::compute_delta_time(uint64_t *current_frame_time)
{
    // Update frame time counter
    uint64_t previous_frame_time = *current_frame_time;
    *current_frame_time          = SDL_GetPerformanceCounter();

    // Delta time = counter since last frame (tics) / counter frequency (tics / second)
    //            = tics / (tics / second)
    //            = tics * second / tics
    //            = second
    // -> time in second elapsed since last frame
    return static_cast<double>(*current_frame_time - previous_frame_time) / static_cast<double>(SDL_GetPerformanceFrequency());
}

#endif