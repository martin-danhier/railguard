#pragma once

#include <cstdint>

namespace rg
{
    class Renderer;
    class Window;

    class Engine
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        Engine() = default;
        Engine(const char *title, uint32_t width, uint32_t height);
        Engine(Engine &&other) noexcept;
        ~Engine();
        Engine &operator=(Engine &&other) noexcept;

        // Getters
        [[nodiscard]] Renderer &renderer() const;
        [[nodiscard]] Window &window() const;

        void run_main_loop();
    };

} // namespace rg
