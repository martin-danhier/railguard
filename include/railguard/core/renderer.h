#pragma once

#include <cstdint>

namespace rg
{
    // ---==== Forward declarations ====---
    class Window;

    // ---==== Structs ====---

    struct Version
    {
        uint32_t major = 0;
        uint32_t minor = 0;
        uint32_t patch = 0;
    };

    // ---==== Definitions ====---

    constexpr Version ENGINE_VERSION = {0, 1, 0};

    // ---==== Main classes ====---

    class Renderer
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        /**
         * Creates a new Renderer, the core class that manages the whole rendering system.
         * @param example_window Window that will be used to configure the renderer. It will not be linked to it - that will be done
         * when creating a swap chain. Understand it like an example window that we show the renderer so that it can see how it needs
         * to initialize.
         * @param application_name Name of the application / game.
         * @param application_version Version of the application / game.
         * @param window_capacity The renderer can hold a constant number of different swapchains.
         * This number needs to be determined early on (e.g. nb of windows, nb of swapchains needed for XR...).
         */
        Renderer(const Window  &example_window,
                 const char    *application_name,
                 const Version &application_version,
                 uint32_t       window_capacity);

        Renderer(Renderer &&other) noexcept;
    };
} // namespace rg