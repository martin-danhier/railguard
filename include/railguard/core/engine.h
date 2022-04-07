#pragma once

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
        Engine();
        ~Engine();

        // Getters
        [[nodiscard]] Renderer &renderer() const;
        [[nodiscard]] Window &window() const;

        void run_main_loop();
    };

} // namespace rg
