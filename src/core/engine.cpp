#include "railguard/core/engine.h"

#include <railguard/core/window.h>
#include <railguard/utils/event_sender.h>

#include <algorithm>

#ifdef NO_INTERACTIVE
#include <thread>
#endif

namespace rg
{
    // --==== Types ====--

    struct Engine::Data
    {
        Window window;

        explicit Data(Window &&window) : window(std::move(window))
        {
        }
    };

    // --==== Constructors ====--

    Engine::Engine()
    {
        const char *title = "My wonderful game";

        // Create window
        Extent2D window_extent = {500, 500};
        Window   window(window_extent, title);

        // Create renderer
        // TODO

        // Save data in engine
        m_data = new Data(std::move(window));
    }

    Engine::~Engine()
    {
        delete m_data;
    }

    // --==== Methods ====--

    void Engine::run_main_loop()
    {
        if (m_data == nullptr)
        {
            throw std::runtime_error("Engine not initialized");
        }

        // Init variables
        bool     should_quit        = false;
        uint64_t current_frame_time = 0;
        double   delta_time         = 0.0;



        // Register close event
        m_data->window.on_close()->subscribe([&should_quit](std::nullptr_t _) {
            should_quit = true;
        });

#ifdef NO_INTERACTIVE
        // In tests, we want a timeout
        uint64_t timeout = 20;

        // Run sleep in another thread, then send event
        std::thread([&timeout, &should_quit]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            should_quit = true;
        }).detach();
#endif

        // Main loop
        while (!should_quit) {


            // Update delta time
            delta_time = Window::compute_delta_time(&current_frame_time);

            // Handle events
            m_data->window.handle_events();

            // Run rendering
            // TODO
        }
    }
} // namespace rg