#pragma once

#include <cstdint>

namespace rg
{
    class Renderer;
    class Window;
    template<typename EventData>
    class EventSender;
    struct RenderPipelineDescription;

    class Engine
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        Engine() = default;
        Engine(const char *title, uint32_t width, uint32_t height, RenderPipelineDescription &&pipeline_description);
        Engine(Engine &&other) noexcept;
        ~Engine();
        Engine &operator=(Engine &&other) noexcept;

        // Getters
        [[nodiscard]] Renderer &renderer() const;
        [[nodiscard]] Window &window() const;
        [[nodiscard]] EventSender<double> *on_update() const;

        void run_main_loop();
    };

} // namespace rg
