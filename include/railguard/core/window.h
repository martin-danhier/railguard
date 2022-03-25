#pragma once

#include <cstdint>

namespace rg
{

    struct Extend2D
    {
        uint32_t width;
        uint32_t height;
    };

    // Window handler. Points to a window object and provides functions to handle it.
    class Window
    {
      private:
        // Obscure struct to hold data - allows different implementations as in C
        struct Data;

        // Pointer to the data
        Data *data;

      public:
        Window(Extend2D extent, const char *title);
        ~Window();

        /**
         * Updates the frame time counter and computes the delta time.
         * @param current_frame_time counter used by the window to track the time elapsed since last frame. Mutated by a call to this
         * function.
         * @warning The frame time is not necessarily in a time unit. Avoid using it outside of a call to this function.
         * @return The delta time, which is the time elapsed since last call to this function, in seconds.
         */
        static double compute_delta_time(uint64_t *current_frame_time);

        // bool handle_events();

        // Getters
        // Extend2D get_current_extent();

        // Events
    };

} // namespace rg
