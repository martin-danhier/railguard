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

    struct KeyEvent {
        // TODO do that better
        int32_t key;
        bool down;
    };

    // Some windowing APIs require specific functions to be called once at the beginning and end of the program
    // This class provides an API-independent way to do that
    // When using the library, you must always call `WindowManager::init()` before using any other function, and call
    // `WindowManager::quit()` when you're done
//    class WindowManager {
//      private:
//        static WindowManager *m_instance;
//
//      protected:
//        // Define init statements here
//        WindowManager();
//        // Define quit statements here
//        ~WindowManager();
//      public:
//        [[nodiscard]] static bool is_initialized() { return m_instance != nullptr; }
//
//        static void init() {
//            if (!is_initialized())
//            {
//                m_instance = new WindowManager();
//            }
//        }
//    };

    // Window handler. Points to a window object and provides functions to handle it.
    class Window
    {
      private:
        // Obscure struct to hold m_data - allows different implementations as in C
        struct Data;

        // Pointer to the m_data
        Data *m_data = nullptr;

      public:
        Window() = default;
        // Constructor
        Window(Extent2D extent, const char *title);
        // No copy constructor: we want only 1 handler per window because we don't have a reference size to handle deletion properly
        Window(Window &&other) noexcept;
        ~Window();

        // Move assignment operator
        Window &operator=(Window &&other) noexcept;

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
        [[nodiscard]] EventSender<KeyEvent>       *on_key_event() const;

        // Vulkan specific

#ifdef RENDERER_VULKAN
        [[nodiscard]] rg::Array<const char *> get_required_vulkan_extensions(uint32_t extra_array_size) const;
        VkSurfaceKHR  get_vulkan_surface(VkInstance vulkan_instance) const;
#endif
    };

} // namespace rg
