#ifdef WINDOW_SDL2

#include "railguard/core/window.h"
#include <railguard/utils/event_sender.h>

#include <iostream>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

// Vulkan-specific
#ifdef RENDERER_VULKAN
#include <railguard/utils/array.h>

#include <SDL2/SDL_vulkan.h>
#endif

using namespace rg;

// --==== TYPES ====--

// Define obscure struct that holds m_data
struct Window::Data
{
    SDL_Window *sdl_window = nullptr;
    Extent2D    extent     = {0, 0};
    // Events
    EventSender<Extent2D>       resize_event;
    EventSender<std::nullptr_t> close_event;
    EventSender<KeyEvent>       key_event;
};

// --==== WINDOW MANAGER ====--

// This class manages the main functions of SDL, which must be called at the beginning and end of the program
// We use a destructor of a global variable to ensure that the cleanup function is called at the end of the program
// For the init, we cannot just create a constructor, so we check at the creation of each window if the manager is initialized or not,
// and if not, we initialize it
class WindowManager
{
    bool m_initialized = false;

  public:
    [[nodiscard]] inline bool is_initialized() const
    {
        return m_initialized;
    }

    // Called when the first window is created
    void init()
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
            exit(1);
        }
        m_initialized = true;
    }

    // Destroyed at the end of the program
    ~WindowManager()
    {
        if (m_initialized)
        {
            SDL_Quit();
            m_initialized = false;
        }
    }
};

// Global variable that will allow the destructor to be called at the end of the program
WindowManager window_manager;

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
    if (!window_manager.is_initialized())
    {
        window_manager.init();
    }

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

    // It may fail (in WSL for example)
    if (m_data->sdl_window == nullptr)
    {
        delete m_data;
        m_data = nullptr;
        throw std::runtime_error("Failed to create window");
    }

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

Window &rg::Window::operator=(Window &&other) noexcept
{
    // Avoid self assignment
    if (this == &other)
    {
        return *this;
    }

    // Destroy old data
    if (m_data != nullptr)
    {
        SDL_DestroyWindow(m_data->sdl_window);
        delete m_data;
    }

    // Move data
    m_data       = other.m_data;
    other.m_data = nullptr;

    return *this;
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
        // Key press
        else if (event.type == SDL_KEYDOWN)
        {
            // TODO nice key handling
            int32_t key = event.key.keysym.sym;
            m_data->key_event.send(KeyEvent {key, true});
        }
        // Key release
        else if (event.type == SDL_KEYUP)
        {
            // TODO nice key handling
            int32_t key = event.key.keysym.sym;
            m_data->key_event.send(KeyEvent {key, false});
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

EventSender<KeyEvent> *rg::Window::on_key_event() const
{
    return &m_data->key_event;
}

// Vulkan-specific

#ifdef RENDERER_VULKAN
rg::Array<const char *> rg::Window::get_required_vulkan_extensions(uint32_t extra_array_size) const
{
    // Get the number of required extensions
    uint32_t required_extensions_count = 0;

    sdl_check(SDL_Vulkan_GetInstanceExtensions(m_data->sdl_window, &required_extensions_count, nullptr));

    // Create an array with that number and fetch said extensions
    // We add the extra_array_size to allow the caller to add its own extensions at the end of the array
    Array<const char *> required_extensions(required_extensions_count + extra_array_size);
    sdl_check(SDL_Vulkan_GetInstanceExtensions(m_data->sdl_window, &required_extensions_count, required_extensions.data()));

    return required_extensions;
}

VkSurfaceKHR rg::Window::get_vulkan_surface(VkInstance vulkan_instance) const
{
    VkSurfaceKHR surface = nullptr;
    sdl_check(SDL_Vulkan_CreateSurface(m_data->sdl_window, vulkan_instance, &surface));
    return surface;
}

#endif

#endif