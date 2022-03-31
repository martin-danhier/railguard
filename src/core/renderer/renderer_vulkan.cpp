#ifdef RENDERER_VULKAN
#include "railguard/core/renderer.h"
#include <railguard/core/window.h>
#include <railguard/utils/array.h>
#include <railguard/utils/storage.h>

#include <iostream>
#include <string>
#include <volk.h>
// Needs to be after volk.h
#include <vk_mem_alloc.h>

// ---==== Defines ====---

#define NB_OVERLAPPING_FRAMES   3
#define VULKAN_API_VERSION      VK_API_VERSION_1_1
#define WAIT_FOR_FENCES_TIMEOUT 1000000000
#define SEMAPHORE_TIMEOUT       1000000000
#define RENDER_STAGE_COUNT      2

namespace rg
{
    // ---==== Types ====---

    // Allocator

    class Allocator
    {
      private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;

      public:
        Allocator() = default;
        Allocator(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device);
        Allocator(Allocator &&other) noexcept;
        Allocator &operator=(Allocator &&other) noexcept;

        ~Allocator();
    };

    struct AllocatedBuffer
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkBuffer      buffer     = VK_NULL_HANDLE;
        uint32_t      size       = 0;
    };

    struct AllocatedImage
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImage       image      = VK_NULL_HANDLE;
        VkImageView   image_view = VK_NULL_HANDLE;
    };

    // Material system

    struct ShaderModule
    {
        VkShaderModule module = VK_NULL_HANDLE;
        ShaderStage    stage  = ShaderStage::INVALID;
    };

    struct ShaderEffect
    {
        /** Render stages in which this effect can be used */
        RenderStageKind render_stage_kind = RenderStageKind::INVALID;
        /** Array of shader module ID. Shader stages of the pipeline, in order. */
        Array<ShaderModuleId> shader_stages   = {};
        VkPipelineLayout      pipeline_layout = VK_NULL_HANDLE;
    };

    struct MaterialTemplate
    {
        /**
         * Array of shader effect IDs. Available effects for this template.
         * Given a render stage kind, the first corresponding effect will be the one used.
         * */
        Array<ShaderEffectId> shader_effects = {};
    };

    struct Material
    {
        /** Template this material is based on. Defines the available shader effects for this material. */
        MaterialTemplateId template_id = NULL_ID;
        /** Models using this material */
        Vector<ModelId> models_using_material = {};
    };

    struct Model
    {
        /** Material used by this model */
        MaterialId material_id = NULL_ID;
        /** Nodes using this model */
        Vector<RenderNodeId> instances = {};
    };

    struct RenderNode
    {
        /** Model used by this node */
        ModelId model_id = NULL_ID;
    };

    // Main types

    struct Passes
    {
        VkRenderPass geometry_pass = VK_NULL_HANDLE;
        VkRenderPass lighting_pass = VK_NULL_HANDLE;
    };

    struct FrameData
    {
        VkCommandPool   command_pool      = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer    = VK_NULL_HANDLE;
        VkSemaphore     present_semaphore = VK_NULL_HANDLE;
        VkSemaphore     render_semaphore  = VK_NULL_HANDLE;
        VkFence         render_fence      = VK_NULL_HANDLE;
    };

    struct Queue
    {
        uint32_t family_index = 0;
        VkQueue  queue        = VK_NULL_HANDLE;
    };

    struct Swapchain
    {
        bool enabled = false;
    };

    struct Renderer::Data
    {
        VkInstance       instance        = VK_NULL_HANDLE;
        VkDevice         device          = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        Queue            graphics_queue  = {};
        Allocator        allocator       = {};
        Passes           passes          = {};
#ifdef USE_VK_VALIDATION_LAYERS
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
#endif
        /**
         * @brief Fixed-count array containing the swapchains.
         * We place them in a array to be able to efficiently iterate through them.
         */
        Array<Swapchain> swapchains = {};

        // Counter of frame since the start of the renderer
        uint64_t  current_frame_number = 1;
        FrameData frames[NB_OVERLAPPING_FRAMES];

        // Storages for the material system
        Storage<ShaderModule>     shader_modules     = {};
        Storage<ShaderEffect>     shader_effects     = {};
        Storage<MaterialTemplate> material_templates = {};
        Storage<Material>         materials          = {};
        Storage<Model>            models             = {};
        Storage<RenderNode>       render_nodes       = {};

        // Number incremented at each created shader effect
        // It is stored in the swapchain when effects are built
        // If the number in the swapchain is different, we need to rebuild the pipelines
        uint64_t effects_version = 0;

        // ------------ Methods ------------

        [[nodiscard]] VkSurfaceFormatKHR select_surface_format(const Window &example_window) const;
        inline void                      wait_for_fence(VkFence fence) const;
        inline void                      wait_for_all_fences() const;
    };

    // ---==== Utilities ====---

    // region Checks

    std::string vk_result_to_string(VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            default: return std::to_string(result);
        }
    }

    void vk_check(VkResult result, const std::string &error_message = "")
    {
        if (result != VK_SUCCESS)
        {
            // Pretty print error
            std::cerr << "[Vulkan Error] A Vulkan function call returned VkResult = " << vk_result_to_string(result) << "\n";

            // Optional custom error message precision
            if (!error_message.empty())
            {
                std::cerr << "Precision: " << error_message << "\n";
            }
        }
    }

    void check(bool result, const std::string &error_message)
    {
        if (!result)
        {
            std::cerr << "[Error] " << error_message << " Aborting.\n";
            // Completely halt the program
            // TODO maybe recoverable ?
            exit(1);
        }
    }

    // endregion

    // region Extensions and layers functions

    bool check_instance_extension_support(const Array<const char *> &desired_extensions)
    {
        // Get the number of available desired_extensions
        uint32_t available_extensions_count = 0;
        vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, VK_NULL_HANDLE));
        // Create an array with enough room and fetch the available desired_extensions
        Array<VkExtensionProperties> available_extensions(available_extensions_count);
        vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &available_extensions_count, available_extensions.data()));

        // For each desired extension, rg_renderer_check if it is available
        bool valid = true;
        for (const auto &desired_extension : desired_extensions)
        {
            bool       found = false;
            const auto ext   = std::string(desired_extension);

            // Search available extensions until the desired one is found or not
            for (const auto &available_extension : available_extensions)
            {
                if (ext == std::string(available_extension.extensionName))
                {
                    found = true;
                    break;
                }
            }

            // Stop looking if nothing was found
            if (!found)
            {
                valid = false;
                std::cerr << "[Error] The extension \"" << ext << "\" is not available.\n";
                break;
            }
        }

        return valid;
    }

    bool check_device_extension_support(const VkPhysicalDevice &physical_device, const Array<const char *> &desired_extensions)
    {
        // Get the number of available desired_extensions
        uint32_t available_extensions_count = 0;
        vk_check(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extensions_count, VK_NULL_HANDLE));
        // Create an array with enough room and fetch the available desired_extensions
        Array<VkExtensionProperties> available_extensions(available_extensions_count);
        vk_check(
            vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extensions_count, available_extensions.data()));

        // For each desired extension, rg_renderer_check if it is available
        bool valid = true;
        for (const auto &desired_extension : desired_extensions)
        {
            bool       found = false;
            const auto ext   = std::string(desired_extension);

            // Search available extensions until the desired one is found or not
            for (const auto &available_extension : available_extensions)
            {
                if (ext == std::string(available_extension.extensionName))
                {
                    found = true;
                    break;
                }
            }

            // Stop looking if nothing was found
            if (!found)
            {
                valid = false;
                std::cerr << "[Error] The extension \"" << ext << "\" is not available.\n";
                break;
            }
        }

        return valid;
    }

    bool check_layer_support(const Array<const char *> &desired_layers)
    {
        // Get the number of available desired_layers
        uint32_t available_layers_count = 0;
        vk_check(vkEnumerateInstanceLayerProperties(&available_layers_count, nullptr));
        // Create an array with enough room and fetch the available desired_layers
        Array<VkLayerProperties> available_layers(available_layers_count);
        vk_check(vkEnumerateInstanceLayerProperties(&available_layers_count, available_layers.data()));

        // For each desired layer, rg_renderer_check if it is available
        bool valid = true;
        for (const auto &desired_layer : desired_layers)
        {
            bool       found = false;
            const auto layer = std::string(desired_layer);

            // Search available layers until the desired one is found or not
            for (const auto &available_layer : available_layers)
            {
                if (layer == std::string(available_layer.layerName))
                {
                    found = true;
                    break;
                }
            }

            // Stop looking if nothing was found
            if (!found)
            {
                valid = false;
                std::cerr << "[Error] The layer \"" << layer << "\" is not available.\n";
                break;
            }
        }

        return valid;
    }

    /**
     * Callback for the vulkan debug messenger
     * @param message_severity Severity of the message
     * @param message_types Type of the message
     * @param callback_data Additional data concerning the message
     * @param user_data User data passed to the debug messenger
     */
    VkBool32 debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                      VkDebugUtilsMessageTypeFlagsEXT             message_types,
                                      const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                      void                                       *_)
    {
        // Inspired by VkBootstrap's default debug messenger. (Made by Charles Giessen)
        // Get severity
        const char *str_severity;
        switch (message_severity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: str_severity = "VERBOSE"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: str_severity = "ERROR"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: str_severity = "WARNING"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: str_severity = "INFO"; break;
            default: str_severity = "UNKNOWN"; break;
        }

        // Get type
        const char *str_type;
        switch (message_types)
        {
            case 7: str_type = "General | Validation | Performance"; break;
            case 6: str_type = "Validation | Performance"; break;
            case 5: str_type = "General | Performance"; break;
            case 4: str_type = "Performance"; break;
            case 3: str_type = "General | Validation"; break;
            case 2: str_type = "Validation"; break;
            case 1: str_type = "General"; break;
            default: str_type = "Unknown"; break;
        }

        // Print the message to stderr if it is an error.
        auto &output = message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? std::cerr : std::cout;
        output << "[" << str_severity << ": " << str_type << "]\n" << callback_data->pMessage << "\n";

        return VK_FALSE;
    }

    // endregion

    // region Physical device functions

    /**
     * @brief Computes a score for the given physical device.
     * @param device is the device to evaluate.
     * @return the score of that device. A bigger score means that the device is better suited.
     */
    uint32_t rg_renderer_rate_physical_device(const VkPhysicalDevice &device)
    {
        uint32_t score = 0;

        // Get properties and features of that device
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures   device_features;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceFeatures(device, &device_features);

        // Prefer discrete gpu when available
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += 10000;
        }

        // The bigger, the better
        score += device_properties.limits.maxImageDimension2D;

        // The device needs to support the following device extensions, otherwise it is unusable
        Array<const char *> required_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        bool extensions_are_supported = check_device_extension_support(device, required_device_extensions);

        // Reset score if the extension are not supported because it is mandatory
        if (!extensions_are_supported)
        {
            score = 0;
        }

        std::cout << "GPU: " << device_properties.deviceName << " | Score: " << score << "\n";

        return score;
    }

    // endregion

    // region Allocator

    Allocator::Allocator(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device)
    {
        VmaVulkanFunctions vulkan_functions = {
            .vkGetPhysicalDeviceProperties           = vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties     = vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory                        = vkAllocateMemory,
            .vkFreeMemory                            = vkFreeMemory,
            .vkMapMemory                             = vkMapMemory,
            .vkUnmapMemory                           = vkUnmapMemory,
            .vkFlushMappedMemoryRanges               = vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges          = vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory                      = vkBindBufferMemory,
            .vkBindImageMemory                       = vkBindImageMemory,
            .vkGetBufferMemoryRequirements           = vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements            = vkGetImageMemoryRequirements,
            .vkCreateBuffer                          = vkCreateBuffer,
            .vkDestroyBuffer                         = vkDestroyBuffer,
            .vkCreateImage                           = vkCreateImage,
            .vkDestroyImage                          = vkDestroyImage,
            .vkCmdCopyBuffer                         = vkCmdCopyBuffer,
            .vkGetBufferMemoryRequirements2KHR       = vkGetBufferMemoryRequirements2KHR,
            .vkGetImageMemoryRequirements2KHR        = vkGetImageMemoryRequirements2KHR,
            .vkBindBufferMemory2KHR                  = vkBindBufferMemory2KHR,
            .vkBindImageMemory2KHR                   = vkBindImageMemory2KHR,
            .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR,
        };
        VmaAllocatorCreateInfo allocator_create_info = {
            .physicalDevice   = physical_device,
            .device           = device,
            .pVulkanFunctions = &vulkan_functions,
            .instance         = instance,
        };

        vk_check(vmaCreateAllocator(&allocator_create_info, &m_allocator), "Failed to create allocator");
    }

    Allocator::~Allocator()
    {
        if (m_allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }
    }

    Allocator::Allocator(Allocator &&other) noexcept : m_allocator(other.m_allocator)
    {
        other.m_allocator = VK_NULL_HANDLE;
    }

    Allocator &Allocator::operator=(Allocator &&other) noexcept
    {
        if (this != &other)
        {
            if (m_allocator != VK_NULL_HANDLE)
            {
                vmaDestroyAllocator(m_allocator);
                m_allocator = VK_NULL_HANDLE;
            }
            m_allocator       = other.m_allocator;
            other.m_allocator = VK_NULL_HANDLE;
        }
        return *this;
    }
    // endregion

    // region Format functions

    VkSurfaceFormatKHR Renderer::Data::select_surface_format(const Window &example_window) const
    {
        // Get surface
        VkSurfaceKHR       surface        = example_window.get_vulkan_surface(instance);
        VkSurfaceFormatKHR surface_format = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

        // Get the available formats
        uint32_t available_format_count = 0;
        vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &available_format_count, nullptr));
        Array<VkSurfaceFormatKHR> available_formats(available_format_count);
        vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &available_format_count, available_formats.data()));

        // Desired formats, by order of preference
        constexpr uint32_t           DESIRED_FORMAT_COUNT                  = 2;
        constexpr VkSurfaceFormatKHR desired_formats[DESIRED_FORMAT_COUNT] = {
            {
                VK_FORMAT_B8G8R8A8_SRGB,
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            },
            {
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            },
        };

        // Get the first desired format available
        bool found = false;
        for (const auto &available_format : available_formats)
        {
            for (const auto &desired_format : desired_formats)
            {
                if (available_format.format == desired_format.format && available_format.colorSpace == desired_format.colorSpace)
                {
                    surface_format = available_format;
                    found          = true;
                    break;
                }
            }
            if (found)
            {
                break;
            }
        }
        check(found, "Couldn't find an appropriate format for the surface.");

        // Destroy the surface - this was just an example window, we will create a new one later
        vkDestroySurfaceKHR(instance, surface, nullptr);

        return surface_format;
    }

    // endregion

    // region Frame functions

    void Renderer::Data::wait_for_fence(VkFence fence) const
    {
        // Wait for it
        vk_check(vkWaitForFences(device, 1, &fence, VK_TRUE, WAIT_FOR_FENCES_TIMEOUT), "Failed to wait for fence");
        // Reset it
        vk_check(vkResetFences(device, 1, &fence), "Failed to reset fence");
    }

    void Renderer::Data::wait_for_all_fences() const
    {
        // Get fences in array
        VkFence fences[NB_OVERLAPPING_FRAMES];
        for (uint32_t i = 0; i < NB_OVERLAPPING_FRAMES; i++)
        {
            fences[i] = frames[i].render_fence;
        }

        // Wait for them
        vk_check(vkWaitForFences(device, NB_OVERLAPPING_FRAMES, fences, VK_TRUE, WAIT_FOR_FENCES_TIMEOUT),
                 "Failed to wait for fences");
    }

    // endregion

    // ---==== Renderer ====---

    Renderer::Renderer(const Window  &example_window,
                       const char    *application_name,
                       const Version &application_version,
                       uint32_t       window_capacity)
        : m_data(new Data)
    {
        // Initialize volk
        vk_check(volkInitialize(), "Couldn't initialize Volk.");

        // --=== Instance creation ===--

        // region Instance creation
        // Do it in a sub scope to call destructors earlier
        {
            // Set required extensions
            uint32_t extra_extension_count = 0;
#ifdef USE_VK_VALIDATION_LAYERS
            extra_extension_count += 1;
#endif
            // Get the extensions that the window manager needs
            auto required_extensions = example_window.get_required_vulkan_extensions(extra_extension_count);

            // Add other extensions in the extra slots
            auto extra_ext_index = required_extensions.count() - extra_extension_count;
#ifdef USE_VK_VALIDATION_LAYERS
            required_extensions[extra_ext_index++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

            check(check_instance_extension_support(required_extensions), "Not all required Vulkan extensions are supported.");

            // Get the validation layers if needed
#ifdef USE_VK_VALIDATION_LAYERS
#define ENABLED_LAYERS_COUNT 1
            Array<const char *> enabled_layers(ENABLED_LAYERS_COUNT);
            enabled_layers[0] = "VK_LAYER_KHRONOS_validation";
            check(check_layer_support(enabled_layers), "Vulkan validation layers requested, but not available.");
#endif

            VkApplicationInfo applicationInfo = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = VK_NULL_HANDLE,
                // Application infos
                .pApplicationName   = application_name,
                .applicationVersion = VK_MAKE_VERSION(application_version.major, application_version.minor, application_version.patch),
                // Engine infos
                .pEngineName   = "Railguard",
                .engineVersion = VK_MAKE_VERSION(ENGINE_VERSION.major, ENGINE_VERSION.minor, ENGINE_VERSION.patch),
                .apiVersion    = VULKAN_API_VERSION,
            };

            VkInstanceCreateInfo instanceCreateInfo {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = VK_NULL_HANDLE,
                // App info
                .pApplicationInfo = &applicationInfo,
            // Validation layers
#ifdef USE_VK_VALIDATION_LAYERS
                .enabledLayerCount   = ENABLED_LAYERS_COUNT,
                .ppEnabledLayerNames = enabled_layers.data(),
#else
                .enabledLayerCount   = 0,
                .ppEnabledLayerNames = nullptr,
#endif
                // Extensions
                .enabledExtensionCount   = static_cast<uint32_t>(required_extensions.count()),
                .ppEnabledExtensionNames = required_extensions.data(),
            };

            vk_check(vkCreateInstance(&instanceCreateInfo, nullptr, &m_data->instance), "Couldn't create instance.");

            // Register instance in Volk
            volkLoadInstance(m_data->instance);

            // Create debug messenger
#ifdef USE_VK_VALIDATION_LAYERS
            VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
                // Struct info
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext = VK_NULL_HANDLE,
                // Message settings
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                // Callback
                .pfnUserCallback = static_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debug_messenger_callback),
            };
            vk_check(vkCreateDebugUtilsMessengerEXT(m_data->instance, &debug_messenger_create_info, nullptr, &m_data->debug_messenger),
                     "Couldn't create debug messenger");
#endif
        }
        // endregion

        // --=== Physical device and queue families selection ===--

        // region Physical device and queue families selection

        {
            // Get the number of available devices
            uint32_t available_physical_devices_count = 0;
            vkEnumeratePhysicalDevices(m_data->instance, &available_physical_devices_count, nullptr);

            // Create an array big enough to hold everything and get the devices themselves
            Array<VkPhysicalDevice> available_physical_devices(available_physical_devices_count);
            vkEnumeratePhysicalDevices(m_data->instance, &available_physical_devices_count, available_physical_devices.data());

            // Find the best physical device
            // For that, we will assign each device a score and keep the best one
            uint32_t current_max_score = 0;
            for (uint32_t i = 0; i < available_physical_devices_count; i++)
            {
                const VkPhysicalDevice &checked_device = available_physical_devices[i];
                uint32_t                score          = rg_renderer_rate_physical_device(checked_device);

                if (score > current_max_score)
                {
                    // New best device found, save it.
                    // We don't need to keep the previous one, since we definitely won't choose it.
                    current_max_score       = score;
                    m_data->physical_device = checked_device;
                }
            }

            // There is a problem if the device is still null: it means none was found.
            check(m_data->physical_device != VK_NULL_HANDLE, "No suitable GPU was found.");

            // Log chosen GPU
            VkPhysicalDeviceProperties physical_device_properties;
            vkGetPhysicalDeviceProperties(m_data->physical_device, &physical_device_properties);
            printf("Suitable GPU found: %s\n", physical_device_properties.deviceName);

            // Get queue families
            uint32_t queue_family_properties_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(m_data->physical_device, &queue_family_properties_count, VK_NULL_HANDLE);
            Array<VkQueueFamilyProperties> queue_family_properties(queue_family_properties_count);
            vkGetPhysicalDeviceQueueFamilyProperties(m_data->physical_device,
                                                     &queue_family_properties_count,
                                                     queue_family_properties.data());

            // Find the queue families that we need
            bool found_graphics_queue = false;

            for (uint32_t i = 0; i < queue_family_properties_count; i++)
            {
                const auto &family_properties = queue_family_properties[i];

                // Save the graphics queue family_index
                if (family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    m_data->graphics_queue.family_index = i;
                    found_graphics_queue                = true;

                    break;
                }
            }

            // If we didn't find a graphics queue, we can't continue
            check(found_graphics_queue, "Unable to find a graphics queue family_index.");
        }

        // endregion

        // --=== Logical device and queues creation ===--

        // region Device and queues creation

        {
            // Define the parameters for the graphics queue
            float                   graphics_queue_priority    = 1.0f;
            VkDeviceQueueCreateInfo graphics_queue_create_info = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                // Queue info
                .queueFamilyIndex = m_data->graphics_queue.family_index,
                .queueCount       = 1,
                .pQueuePriorities = &graphics_queue_priority,
            };
            Array<const char *> required_device_extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            };

            // Create the logical device
            VkDeviceCreateInfo device_create_info = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                // Queue infos
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos    = &graphics_queue_create_info,
                // Layers
                .enabledLayerCount   = 0,
                .ppEnabledLayerNames = nullptr,
                // Extensions
                .enabledExtensionCount   = static_cast<uint32_t>(required_device_extensions.count()),
                .ppEnabledExtensionNames = required_device_extensions.data(),
                .pEnabledFeatures        = nullptr,
            };
            vk_check(vkCreateDevice(m_data->physical_device, &device_create_info, nullptr, &m_data->device),
                     "Couldn't create logical device.");

            // Load device in volk
            volkLoadDevice(m_data->device);

            // Get created queues
            vkGetDeviceQueue(m_data->device, m_data->graphics_queue.family_index, 0, &m_data->graphics_queue.queue);
        }

        // endregion

        // --=== Allocator ===--

        m_data->allocator = std::move(Allocator(m_data->instance, m_data->device, m_data->physical_device));

        // --=== Swapchains ===--

        // We need to create an array big enough to hold all the swapchains.
        m_data->swapchains = Array<Swapchain>(window_capacity);

        // --=== Render passes ===--

        // region Render passes creation

        {
            // Choose a swapchain_image_format for the given example window
            // For now we will assume that all swapchain will use that swapchain_image_format
            VkSurfaceFormatKHR swapchain_image_format = m_data->select_surface_format(example_window);

            // Create geometric render pass

            VkAttachmentReference   attachment_references[3];
            VkAttachmentDescription attachments[3];

            // Position color buffer
            attachments[0] = (VkAttachmentDescription) {
                // Format. We need high precision for position
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                // No MSAA
                .samples = VK_SAMPLE_COUNT_1_BIT,
                // Operators
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                // Layout
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            attachment_references[0] = (VkAttachmentReference) {
                .attachment = 0,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            // Normal color buffer. Same format as position
            attachments[1]           = attachments[0];
            attachment_references[1] = (VkAttachmentReference) {
                .attachment = 1,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            // Color + specular buffers
            attachments[2]           = attachments[0];
            attachments[2].format    = VK_FORMAT_R8G8B8A8_UINT;
            attachment_references[2] = (VkAttachmentReference) {
                .attachment = 2,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            // Group attachments in an array
            VkSubpassDescription subpass_description = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                // Color attachments
                .colorAttachmentCount = 3,
                .pColorAttachments    = attachment_references,
            };
            VkRenderPassCreateInfo render_pass_create_info = {
                .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .pNext           = VK_NULL_HANDLE,
                .attachmentCount = 3,
                .pAttachments    = attachments,
                .subpassCount    = 1,
                .pSubpasses      = &subpass_description,
            };
            vk_check(vkCreateRenderPass(m_data->device, &render_pass_create_info, nullptr, &m_data->passes.geometry_pass),
                     "Couldn't create geometry render pass");

            // Create lighting render pass
            VkAttachmentDescription lighting_attachment = {
                // Format. We need high precision for position
                .format = swapchain_image_format.format,
                // No MSAA
                .samples = VK_SAMPLE_COUNT_1_BIT,
                // Operators
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                // Layout
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            };
            VkAttachmentReference lighting_attachment_reference = {
                .attachment = 0,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            subpass_description.colorAttachmentCount = 1;
            subpass_description.pColorAttachments    = &lighting_attachment_reference;
            render_pass_create_info.attachmentCount  = 1;
            render_pass_create_info.pAttachments     = &lighting_attachment;
            vk_check(vkCreateRenderPass(m_data->device, &render_pass_create_info, nullptr, &m_data->passes.lighting_pass),
                     "Couldn't create lighting render pass");
        }
        // endregion

        // --=== Init frames ===--

        // region Init frames
        {
            // Define create infos
            VkCommandPoolCreateInfo command_pool_create_info = {
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = VK_NULL_HANDLE,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = m_data->graphics_queue.family_index,
            };

            VkFenceCreateInfo fence_create_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = VK_NULL_HANDLE,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            };

            VkSemaphoreCreateInfo semaphore_create_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = VK_NULL_HANDLE,
                .flags = 0,
            };

            // For each frame
            for (auto &frame : m_data->frames)
            {
                // Create command pool
                vk_check(vkCreateCommandPool(m_data->device, &command_pool_create_info, nullptr, &frame.command_pool),
                         "Couldn't create command pool");

                // Create command buffers
                VkCommandBufferAllocateInfo command_buffer_allocate_info = {
                    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .pNext              = VK_NULL_HANDLE,
                    .commandPool        = frame.command_pool,
                    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1,
                };
                vk_check(vkAllocateCommandBuffers(m_data->device, &command_buffer_allocate_info, &frame.command_buffer),
                         "Couldn't allocate command buffer");

                // Create fence
                vk_check(vkCreateFence(m_data->device, &fence_create_info, nullptr, &frame.render_fence), "Couldn't create fence");

                // Create semaphores
                vk_check(vkCreateSemaphore(m_data->device, &semaphore_create_info, nullptr, &frame.present_semaphore),
                         "Couldn't create image available semaphore");
                vk_check(vkCreateSemaphore(m_data->device, &semaphore_create_info, nullptr, &frame.render_semaphore),
                         "Couldn't create render semaphore");
            }
        }
        // endregion
    }

    Renderer::Renderer(Renderer &&other) noexcept : m_data(other.m_data)
    {
        // Just take the other's data and set the original to null, so it can't access it anymore
        other.m_data = nullptr;
    }

    Renderer::~Renderer()
    {
        // If the data is null, then the renderer was never initialized
        if (m_data == nullptr)
            return;

        // Wait for all frames to finish rendering
        m_data->wait_for_all_fences();

        // Clear frames
        for (const auto &frame : m_data->frames)
        {
            // Destroy semaphores
            vkDestroySemaphore(m_data->device, frame.present_semaphore, nullptr);
            vkDestroySemaphore(m_data->device, frame.render_semaphore, nullptr);
            // Destroy fence
            vkDestroyFence(m_data->device, frame.render_fence, nullptr);
            // Destroy command buffer
            vkFreeCommandBuffers(m_data->device, frame.command_pool, 1, &frame.command_buffer);
            // Destroy command pool
            vkDestroyCommandPool(m_data->device, frame.command_pool, nullptr);
        }

        // Clear storages
        m_data->render_nodes.clear();
        m_data->models.clear();
        m_data->materials.clear();
        m_data->material_templates.clear();
        m_data->shader_effects.clear();
        m_data->shader_modules.clear();

        // Destroy swapchains
        m_data->swapchains.~Array();

        // Destroy render passes
        vkDestroyRenderPass(m_data->device, m_data->passes.geometry_pass, nullptr);
        vkDestroyRenderPass(m_data->device, m_data->passes.lighting_pass, nullptr);

        // Destroy allocator
        m_data->allocator.~Allocator();

        // Destroy device
        vkDestroyDevice(m_data->device, nullptr);

#ifdef USE_VK_VALIDATION_LAYERS
        // Destroy debug messenger
        vkDestroyDebugUtilsMessengerEXT(m_data->instance, m_data->debug_messenger, nullptr);
#endif

        // Destroy instance
        vkDestroyInstance(m_data->instance, nullptr);

        // Free memory
        delete m_data;
        m_data = nullptr;
    }

} // namespace rg
#endif