#pragma once

namespace rg
{
    class Engine
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        Engine();
        ~Engine();

        void run_main_loop();
    };

} // namespace rg
