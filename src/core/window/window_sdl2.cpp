#ifdef WINDOW_SDL2

#include "railguard/core/window.h"
#include <railguard/utils/event_sender.h>

#include <iostream>
#include <SDL2/SDL.h>

// Vulkan-specific
#ifdef RENDERER_VULKAN
#include <railguard/utils/array.h>

#include <SDL2/SDL_vulkan.h>
#endif

using namespace rg;

// --==== TYPES ====--

// Define obscure struct that holds data
struct Window::Data
{
    SDL_Window *sdl_window = nullptr;
    Extent2D    extent     = {0, 0};
    // Events
    EventSender<Extent2D>       resize_event;
    EventSender<std::nullptr_t> close_event;
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

Window::Window(Extent2D extent, const char *title)
{
    // Initialize window
    m_data = new Data;

    // Save other info
    m_data->extent = extent;

    // Init SDL2
    SDL_WindowFlags window_flags;

// We need to know which renderer to use
#ifdef RENDERER_VULKAN
    window_flags = SDL_WINDOW_VULKAN;
#endif

    m_data->sdl_window = SDL_CreateWindow(title,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          static_cast<int32_t>(extent.width),
                                          static_cast<int32_t>(extent.height),
                                          window_flags);

    // Apply more settings to the window
    SDL_SetWindowResizable(m_data->sdl_window, SDL_TRUE);
}

rg::Window::Window(Window &&other) noexcept : m_data(other.m_data)
{
    other.m_data = nullptr;
}

Window::~Window()
{
    if (m_data != nullptr)
    {
        SDL_DestroyWindow(m_data->sdl_window);
        delete m_data;
    }
}

// Main functions

void rg::Window::handle_events()
{
    SDL_Event event;

    // Handle all events in a queue
    while (SDL_PollEvent(&event) != 0)
    {
        // Window event
        if (event.type == SDL_WINDOWEVENT)
        {
            // Window resized
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                // Send a resize event with new extent
                Extent2D new_extent = {
                    static_cast<uint32_t>(event.window.data1),
                    static_cast<uint32_t>(event.window.data2),
                };
                m_data->resize_event.send(new_extent);

                // Update the extent
                m_data->extent = new_extent;
            }
        }
        // Quit
        else if (event.type == SDL_QUIT)
        {
            // Send a close event
            m_data->close_event.send(nullptr);
        }
    }
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

// Getters

Extent2D rg::Window::get_current_extent()
{
    return m_data->extent;
}

// Events

EventSender<Extent2D> *Window::on_resize() const
{
    return &m_data->resize_event;
}

EventSender<std::nullptr_t> *rg::Window::on_close() const
{
    return &m_data->close_event;
}

// Vulkan-specific

#ifdef RENDERER_VULKAN
rg::Array<const char *> &&rg::Window::get_required_vulkan_extensions(uint32_t extra_array_size)
{
    // Get the number of required extensions
    uint32_t required_extensions_count = 0;

    sdl_check(SDL_Vulkan_GetInstanceExtensions(m_data->sdl_window, &required_extensions_count, nullptr));

    // Create an array with that number and fetch said extensions
    // We add the extra_array_size to allow the caller to add its own extensions at the end of the array
    Array<const char*> required_extensions(required_extensions_count + extra_array_size);
    sdl_check(SDL_Vulkan_GetInstanceExtensions(m_data->sdl_window, &required_extensions_count, required_extensions.data()));

    return std::move(required_extensions);
}

VkSurfaceKHR rg::Window::get_vulkan_surface(VkInstance vulkan_instance)
{
    VkSurfaceKHR surface = nullptr;
    sdl_check(SDL_Vulkan_CreateSurface(m_data->sdl_window, vulkan_instance, &surface));
    return surface;
}
#endif

#endif