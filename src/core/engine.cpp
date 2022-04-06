#include "railguard/core/engine.h"

#include <railguard/core/renderer.h>
#include <railguard/core/window.h>
#include <railguard/utils/event_sender.h>
#include <railguard/utils/array.h>

#include <algorithm>

#ifdef NO_INTERACTIVE
#include <thread>
#endif

namespace rg
{
    // --==== Types ====--

    struct Engine::Data
    {
        Window   window;
        Renderer renderer;

        explicit Data(Window &&window, Renderer &&renderer) : window(std::move(window)), renderer(std::move(renderer))
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
        Renderer renderer(window, title, {0, 1, 0}, 2);

        // Link window
        renderer.connect_window(0, window);

        // Load shaders
        auto vertex_shader   = renderer.load_shader_module("resources/shaders/hello/test.vert.spv", ShaderStage::VERTEX);
        auto fragment_shader = renderer.load_shader_module("resources/shaders/hello/test.frag.spv", ShaderStage::FRAGMENT);

        // Create a shader effect
        auto hello_effect = renderer.create_shader_effect({vertex_shader, fragment_shader}, RenderStageKind::LIGHTING);

        // Create a material template
        auto material_template = renderer.create_material_template({hello_effect});

        // Create a material
        auto material = renderer.create_material(material_template);

        // Create a model
        auto model = renderer.create_model(material);

        // Create a render node
        auto render_node = renderer.create_render_node(model);

        // Save data in engine
        m_data = new Data(std::move(window), std::move(renderer));
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
        m_data->window.on_close()->subscribe([&should_quit](std::nullptr_t _) { should_quit = true; });

#ifdef NO_INTERACTIVE
        // In tests, we want a timeout
        uint64_t timeout = 20;

        // Run sleep in another thread, then send event
        std::thread(
            [&timeout, &should_quit]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
                should_quit = true;
            })
            .detach();
#endif

        // Main loop
        while (!should_quit)
        {
            // Update delta time
            delta_time = Window::compute_delta_time(&current_frame_time);

            // Handle events
            m_data->window.handle_events();

            // Run rendering
            // TODO
        }
    }
} // namespace rg