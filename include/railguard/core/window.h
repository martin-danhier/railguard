#pragma once

#include <cstdint>

// Forward declarations for vulkan
#ifdef RENDERER_VULKAN
typedef struct VkInstance_T   *VkInstance;
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
#endif

namespace rg
{
    // Forward declare templates to avoid includes

    template<typename EventData>
    class EventSender;

    template<typename T>
    class Array;

    struct Extent2D
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
        Data *m_data = nullptr;

      public:
        // Constructor
        Window(Extent2D extent, const char *title);
        // No copy constructor: we want only 1 handler per window because we don't have a reference count to handle deletion properly
        Window(Window &&other) noexcept;
        ~Window();

        /**
         * Updates the frame time counter and computes the delta time.
         * @param current_frame_time counter used by the window to track the time elapsed since last frame. Mutated by a call to this
         * function.
         * @warning The frame time is not necessarily in a time unit. Avoid using it outside of a call to this function.
         * @return The delta time, which is the time elapsed since last call to this function, in seconds.
         */
        static double compute_delta_time(uint64_t *current_frame_time);

        void handle_events();

        // Getters
        Extent2D get_current_extent();

        // Events
        [[nodiscard]] EventSender<Extent2D>       *on_resize() const;
        [[nodiscard]] EventSender<std::nullptr_t> *on_close() const;

        // Vulkan specific

#ifdef RENDERER_VULKAN
        [[nodiscard]] rg::Array<const char *> get_required_vulkan_extensions(uint32_t extra_array_size) const;
        VkSurfaceKHR  get_vulkan_surface(VkInstance vulkan_instance);
#endif
    };

} // namespace rg
