#ifdef RENDERER_VULKAN
#include "railguard/core/renderer.h"
#include <railguard/core/window.h>
#include <railguard/utils/array.h>
#include <railguard/utils/event_sender.h>
#include <railguard/utils/geometry/transform.h>
#include <railguard/utils/io.h>
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

    // region Types

    // Allocator

    struct AllocatedBuffer
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkBuffer      buffer     = VK_NULL_HANDLE;
        uint32_t      size       = 0;

        [[nodiscard]] inline bool is_valid() const
        {
            return allocation != VK_NULL_HANDLE;
        }
    };

    struct AllocatedImage
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImage       image      = VK_NULL_HANDLE;
        VkImageView   image_view = VK_NULL_HANDLE;
    };

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

        AllocatedImage create_image(VkDevice              device,
                                    VkFormat              image_format,
                                    VkExtent3D            image_extent,
                                    VkImageUsageFlags     image_usage,
                                    VkImageAspectFlagBits image_aspect,
                                    VmaMemoryUsage        memory_usage) const;
        void           destroy_image(VkDevice device, AllocatedImage &image) const;

        [[nodiscard]] AllocatedBuffer
              create_buffer(size_t allocation_size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage) const;
        void  destroy_buffer(VkDevice device, AllocatedBuffer &buffer) const;
        void *map_buffer(AllocatedBuffer &buffer) const;
        void  unmap_buffer(AllocatedBuffer &buffer) const;
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

    struct Camera
    {
        bool       enabled                = false;
        size_t     target_swapchain_index = 0;
        Transform  transform              = {};
        CameraType type                   = CameraType::PERSPECTIVE;
        union
        {
            struct
            {
                float fov          = 0.0f;
                float aspect_ratio = 0.0f;
                float near_plane   = 0.0f;
                float far_plane    = 0.0f;
            } perspective;
            struct
            {
                float width      = 0.0f;
                float height     = 0.0f;
                float near_plane = 0.0f;
                float far_plane  = 0.0f;
            } orthographic;
        };
    };

    struct RenderBatch
    {
        size_t     offset   = 0;
        size_t     count    = 0;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };

    struct RenderStage
    {
        RenderStageKind     kind            = RenderStageKind::INVALID;
        AllocatedBuffer     indirect_buffer = {};
        Vector<RenderBatch> batches {5};
    };

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
        bool           enabled         = false;
        VkSwapchainKHR vk_swapchain    = VK_NULL_HANDLE;
        VkExtent2D     viewport_extent = {};
        // Swapchain images
        uint32_t           image_count  = 0;
        VkSurfaceFormatKHR image_format = {};
        // Store images
        Array<VkImage>       images       = {};
        Array<VkImageView>   image_views  = {};
        Array<VkFramebuffer> framebuffers = {};
        // Present mode
        VkPresentModeKHR              present_mode  = VK_PRESENT_MODE_MAILBOX_KHR;
        VkSurfaceTransformFlagBitsKHR pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        // Depth image
        VkFormat       depth_image_format = VK_FORMAT_UNDEFINED;
        AllocatedImage depth_image        = {};
        // Target window
        Window                   *target_window                  = nullptr;
        EventSender<Extent2D>::Id window_resize_event_handler_id = NULL_ID;
        VkSurfaceKHR              surface                        = VK_NULL_HANDLE;
        // Pipelines
        // Since vulkan handles are just pointers, a hash map is all we need
        HashMap  pipelines             = {};
        uint64_t built_effects_version = 0;
        // Render stages
        Array<RenderStage> render_stages = {};
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
        uint64_t  current_frame_number          = 1;
        FrameData frames[NB_OVERLAPPING_FRAMES] = {};

        // Storages for the material system
        Storage<ShaderModule>     shader_modules     = {};
        Storage<ShaderEffect>     shader_effects     = {};
        Storage<MaterialTemplate> material_templates = {};
        Storage<Material>         materials          = {};
        Storage<Model>            models             = {};
        Storage<RenderNode>       render_nodes       = {};
        Storage<Camera>           cameras            = {};

        // Number incremented at each created shader effect
        // It is stored in the swapchain when effects are built
        // If the number in the swapchain is different, we need to rebuild the pipelines
        uint64_t effects_version = 0;

        // ------------ Methods ------------

        inline void                           wait_for_fence(VkFence fence) const;
        inline void                           wait_for_all_fences() const;
        [[nodiscard]] inline uint64_t         get_current_frame_index() const;
        inline FrameData                     &get_current_frame();
        [[nodiscard]] inline const FrameData &get_current_frame() const;
        VkCommandBuffer                       begin_recording();
        void                                  end_recording_and_submit();

        [[nodiscard]] VkSurfaceFormatKHR select_surface_format(const VkSurfaceKHR &surface) const;
        void                             destroy_swapchain_inner(Swapchain &swapchain) const;
        void                             destroy_swapchain(Swapchain &swapchain) const;
        void                             clear_swapchains();
        void                             init_swapchain_inner(Swapchain &swapchain, const Extent2D &extent) const;
        void                             recreate_swapchain(Swapchain &swapchain, const Extent2D &new_extent) const;
        uint32_t                         get_next_swapchain_image(Swapchain &swapchain) const;

        [[nodiscard]] VkPipeline build_shader_effect(const VkExtent2D &viewport_extent, const ShaderEffect &effect) const;
        void                     build_out_of_date_effects(Swapchain &swapchain) const;
        void                     clear_pipelines(Swapchain &swapchain) const;
        void                     recreate_pipelines(Swapchain &swapchain) const;
        void                     destroy_pipeline(Swapchain &swapchain, ShaderEffectId shader_effect_id) const;

        void update_stage_cache(Swapchain &swapchain);
    };

    // endregion

    // ---==== Utilities ====---

    // region Checks

    std::string vk_result_to_string(VkResult result)
    {
        switch (result)
        {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            default: return std::to_string(result);
        }
    }

    std::string vk_present_mode_to_string(VkPresentModeKHR present_mode)
    {
        switch (present_mode)
        {
            case VK_PRESENT_MODE_IMMEDIATE_KHR: return "Immediate";
            case VK_PRESENT_MODE_MAILBOX_KHR: return "Mailbox";
            case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO Relaxed";
            default: return std::to_string(present_mode);
        }
    }

    void vk_check(VkResult result, const std::string &error_message = "")
    {
        // Warnings
        if (result == VK_SUBOPTIMAL_KHR)
        {
            std::cout << "[Vulkan Warning] A Vulkan function call returned VkResult = " << vk_result_to_string(result) << "\n";
        }
        // Errors
        else if (result != VK_SUCCESS)
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
     * @param callback_data Additional m_data concerning the message
     * @param user_data User m_data passed to the debug messenger
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

    AllocatedImage Allocator::create_image(VkDevice              device,
                                           VkFormat              image_format,
                                           VkExtent3D            image_extent,
                                           VkImageUsageFlags     image_usage,
                                           VkImageAspectFlagBits image_aspect,
                                           VmaMemoryUsage        memory_usage) const
    {
        // We use VMA for now. We can always switch to a custom allocator later if we want to.
        AllocatedImage image;

        check(image_extent.width >= 1 && image_extent.height >= 1 && image_extent.depth >= 1,
              "Tried to create an image with an invalid extent. The extent must be at least 1 in each dimension.");

        // Create the image using VMA
        VkImageCreateInfo image_create_info = {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = image_format,
            .extent                = image_extent,
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = image_usage,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VmaAllocationCreateInfo alloc_create_info = {
            .usage          = memory_usage,
            .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };

        // Create the image
        vk_check(vmaCreateImage(m_allocator, &image_create_info, &alloc_create_info, &image.image, &image.allocation, nullptr),
                 "Failed to create image");

        // Create image view
        VkImageViewCreateInfo image_view_create_info = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .image    = image.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = image_format,
            .components =
                {
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                    VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    image_aspect,
                    0,
                    1,
                    0,
                    1,
                },
        };
        vk_check(vkCreateImageView(device, &image_view_create_info, nullptr, &image.image_view), "Failed to create image view");

        return image;
    }

    void Allocator::destroy_image(VkDevice device, AllocatedImage &image) const
    {
        vkDestroyImageView(device, image.image_view, nullptr);
        vmaDestroyImage(m_allocator, image.image, image.allocation);
        image.image      = VK_NULL_HANDLE;
        image.allocation = VK_NULL_HANDLE;
        image.image_view = VK_NULL_HANDLE;
    }

    AllocatedBuffer
        Allocator::create_buffer(size_t allocation_size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage) const
    {
        // We use VMA for now. We can always switch to a custom allocator later if we want to.
        AllocatedBuffer buffer = {
            .size = static_cast<uint32_t>(allocation_size),
        };

        // Create the buffer using VMA
        VkBufferCreateInfo buffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            // Buffer info
            .size  = allocation_size,
            .usage = buffer_usage,
        };

        // Create an allocation info
        VmaAllocationCreateInfo allocation_create_info = {
            .usage = memory_usage,
        };

        // Create the buffer
        vk_check(vmaCreateBuffer(m_allocator,
                                 &buffer_create_info,
                                 &allocation_create_info,
                                 &buffer.buffer,
                                 &buffer.allocation,
                                 VK_NULL_HANDLE),
                 "Couldn't allocate buffer");

        return buffer;
    }

    void Allocator::destroy_buffer(VkDevice device, AllocatedBuffer &buffer) const
    {
        if (buffer.buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
            buffer.buffer     = VK_NULL_HANDLE;
            buffer.allocation = VK_NULL_HANDLE;
            buffer.size       = 0;
        }
    }
    void *Allocator::map_buffer(AllocatedBuffer &buffer) const
    {
        void *data = nullptr;
        vk_check(vmaMapMemory(m_allocator, buffer.allocation, &data), "Failed to map buffer");
        return data;
    }
    void Allocator::unmap_buffer(AllocatedBuffer &buffer) const
    {
        vmaUnmapMemory(m_allocator, buffer.allocation);
    }

    // endregion

    // region Format functions

    VkSurfaceFormatKHR Renderer::Data::select_surface_format(const VkSurfaceKHR &surface) const
    {
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
        VkFence fences[NB_OVERLAPPING_FRAMES] = {};

        for (uint32_t i = 0; i < NB_OVERLAPPING_FRAMES; i++)
        {
            fences[i] = frames[i].render_fence;
        }

        // Wait for them
        vk_check(vkWaitForFences(device, NB_OVERLAPPING_FRAMES, fences, VK_TRUE, WAIT_FOR_FENCES_TIMEOUT),
                 "Failed to wait for fences");
    }

    uint64_t Renderer::Data::get_current_frame_index() const
    {
        return current_frame_number % NB_OVERLAPPING_FRAMES;
    }

    FrameData &Renderer::Data::get_current_frame()
    {
        return frames[get_current_frame_index()];
    }

    const FrameData &Renderer::Data::get_current_frame() const
    {
        return frames[get_current_frame_index()];
    }

    VkCommandBuffer Renderer::Data::begin_recording()
    {
        // Get current frame
        auto &frame = get_current_frame();

        // Reset command buffer
        vk_check(vkResetCommandBuffer(frame.command_buffer, 0));

        // Begin command buffer
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vk_check(vkBeginCommandBuffer(frame.command_buffer, &begin_info));

        return frame.command_buffer;
    }

    void Renderer::Data::end_recording_and_submit()
    {
        // Get current frame
        auto &frame = get_current_frame();

        // End command buffer
        vk_check(vkEndCommandBuffer(frame.command_buffer));

        // Submit command buffer
        VkPipelineStageFlags wait_stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo         submit_info = {};
        submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &frame.command_buffer;
        submit_info.pWaitDstStageMask    = &wait_stage;
        // Wait until the image to render is ready
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores    = &frame.present_semaphore;
        // Signal the render semaphore when the rendering is done
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &frame.render_semaphore;

        // Submit
        vk_check(vkQueueSubmit(graphics_queue.queue, 1, &submit_info, frame.render_fence), "Failed to submit command buffer");
    }

    // endregion

    // region Swapchain functions

    void Renderer::Data::destroy_swapchain_inner(Swapchain &swapchain) const
    {
        // Destroy framebuffers
        for (auto &framebuffer : swapchain.framebuffers)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        // Destroy depth image
        allocator.destroy_image(device, swapchain.depth_image);

        // Destroy images
        for (uint32_t i = 0; i < swapchain.image_count; i++)
        {
            vkDestroyImageView(device, swapchain.image_views[i], nullptr);
            swapchain.image_views[i] = VK_NULL_HANDLE;
        }

        // Destroy swapchain
        vkDestroySwapchainKHR(device, swapchain.vk_swapchain, nullptr);
        swapchain.vk_swapchain = VK_NULL_HANDLE;
    }

    void Renderer::Data::destroy_swapchain(Swapchain &swapchain) const
    {
        // If swapchain is disabled, then it is already destroyed and the contract is satisfied
        if (swapchain.enabled)
        {
            // Unregister window events
            swapchain.target_window->on_resize()->unsubscribe(swapchain.window_resize_event_handler_id);

            // Destroy render stages
            for (auto &stage : swapchain.render_stages)
            {
                if (stage.indirect_buffer.is_valid())
                {
                    allocator.destroy_buffer(device, stage.indirect_buffer);
                }
            }

            destroy_swapchain_inner(swapchain);

            // Destroy surface
            vkDestroySurfaceKHR(instance, swapchain.surface, nullptr);
            swapchain.surface = VK_NULL_HANDLE;

            // Destroy pipelines
            clear_pipelines(swapchain);

            // Disable it
            swapchain.enabled = false;
        }
    }

    void Renderer::Data::clear_swapchains()
    {
        for (auto &swapchain : swapchains)
        {
            destroy_swapchain(swapchain);
        }
    }

    void Renderer::Data::init_swapchain_inner(Swapchain &swapchain, const Extent2D &extent) const
    {
        // region Swapchain creation

        // Save extent
        swapchain.viewport_extent = VkExtent2D {extent.width, extent.height};

        // Create the swapchain
        VkSwapchainCreateInfoKHR create_info = {
            // Struct info
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            // Image options
            .surface          = swapchain.surface,
            .minImageCount    = swapchain.image_count,
            .imageFormat      = swapchain.image_format.format,
            .imageColorSpace  = swapchain.image_format.colorSpace,
            .imageExtent      = swapchain.viewport_extent,
            .imageArrayLayers = 1,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            // For now, we use the same queue for rendering and presenting. Maybe in the future, we will want to change that.
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform     = swapchain.pre_transform,
            .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            // Present options
            .presentMode  = swapchain.present_mode,
            .clipped      = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };
        vk_check(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain.vk_swapchain), "Failed to create swapchain");

        // endregion

        // region Images and image views

        // Get the images
        uint32_t effective_image_count;
        vk_check(vkGetSwapchainImagesKHR(device, swapchain.vk_swapchain, &effective_image_count, nullptr));
        swapchain.images = Array<VkImage>(effective_image_count);
        vk_check(vkGetSwapchainImagesKHR(device, swapchain.vk_swapchain, &effective_image_count, swapchain.images.data()));

        // Update the image count based on how many were effectively created
        swapchain.image_count = effective_image_count;

        // Create image views for those images
        swapchain.image_views = Array<VkImageView>(effective_image_count);
        VkImageViewCreateInfo image_view_create_info {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .image    = VK_NULL_HANDLE,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = swapchain.image_format.format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
        };
        for (uint32_t i = 0; i < effective_image_count; i++)
        {
            image_view_create_info.image = swapchain.images[i];
            vk_check(vkCreateImageView(device, &image_view_create_info, nullptr, &swapchain.image_views[i]),
                     "Failed to create image view for swapchain image");
        }

        // endregion

        // region Depth image creation

        VkExtent3D depth_image_extent = {extent.width, extent.height, 1};
        swapchain.depth_image_format  = VK_FORMAT_D32_SFLOAT;
        swapchain.depth_image         = allocator.create_image(device,
                                                       swapchain.depth_image_format,
                                                       depth_image_extent,
                                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                       VK_IMAGE_ASPECT_DEPTH_BIT,
                                                       VMA_MEMORY_USAGE_GPU_ONLY);

        // endregion

        // region Framebuffer creation

        swapchain.framebuffers = Array<VkFramebuffer>(effective_image_count);
        VkFramebufferCreateInfo framebuffer_create_info {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .renderPass      = passes.lighting_pass,
            .attachmentCount = 1,
            .pAttachments    = nullptr,
            .width           = extent.width,
            .height          = extent.height,
            .layers          = 1,
        };

        for (uint32_t i = 0; i < effective_image_count; i++)
        {
            framebuffer_create_info.pAttachments = &swapchain.image_views[i];
            vk_check(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &swapchain.framebuffers[i]),
                     "Failed to create framebuffer");
        }

        // endregion
    }

    void Renderer::Data::recreate_swapchain(Swapchain &swapchain, const Extent2D &new_extent) const
    {
        check(swapchain.enabled,
              "Attempted to recreate an non-existing swapchain. "
              "Use renderer.connect_window to create a new one instead.");

        // Wait for fences before recreating
        wait_for_all_fences();

        // Destroy the old swapchain
        destroy_swapchain_inner(swapchain);

        // Create the new swapchain
        init_swapchain_inner(swapchain, new_extent);

        // Recreate pipelines
        recreate_pipelines(swapchain);
    }

    uint32_t Renderer::Data::get_next_swapchain_image(Swapchain &swapchain) const
    {
        const auto &frame = get_current_frame();

        // Get next image
        uint32_t image_index = 0;
        auto     result      = vkAcquireNextImageKHR(device,
                                            swapchain.vk_swapchain,
                                            SEMAPHORE_TIMEOUT,
                                            frame.present_semaphore,
                                            VK_NULL_HANDLE,
                                            &image_index);

        // When the window resizes, the function waits for fences before recreating the swapchain
        // The last few frames may thus be suboptimal
        // We decide to still render them (suboptimal still works),
        // so we ignore the warning (we know it will be handled in one or two frames)
        // If this is an issue in the future, maybe the function could return an optional, which would be none in this case
        // And skip frames that returned none.
        if (result != VK_SUBOPTIMAL_KHR)
        {
            vk_check(result);
        }

        return image_index;
    }

    // endregion

    // region Effect functions

    void Renderer::Data::build_out_of_date_effects(Swapchain &swapchain) const
    {
        // The version is incremented when new effects are created.
        if (swapchain.built_effects_version < effects_version)
        {
            // Build all effects
            for (const auto &effect : shader_effects)
            {
                // Don't build a pipeline that is already built
                const ShaderEffectId &effect_id = effect.key();
                auto                  pipeline  = swapchain.pipelines.get(effect_id);

                if (!pipeline.has_value())
                {
                    // Store the pipeline with the same id as the effect
                    // That way, we can easily find the pipeline of a given effect
                    swapchain.pipelines.set(effect_id, {.as_ptr = build_shader_effect(swapchain.viewport_extent, effect.value())});
                }
            }

            // Update the version
            swapchain.built_effects_version = effects_version;
        }
    }

    VkPipeline Renderer::Data::build_shader_effect(const VkExtent2D &viewport_extent, const ShaderEffect &effect) const
    {
        // This function will take the m_data contained in the effect and build a pipeline with it
        // First, create all the structs we will need in the pipeline create info

        // region Create shader stages

        Array<VkPipelineShaderStageCreateInfo> stages(effect.shader_stages.count());
        for (auto i = 0; i < effect.shader_stages.count(); i++)
        {
            // Get shader module
            const auto &module = shader_modules.get(effect.shader_stages[i]);
            check(module.has_value(), "Couldn't get shader module required to build effect.");

            // Convert stage flag
            VkShaderStageFlagBits stage_flags = {};
            switch (module->stage)
            {
                case ShaderStage::VERTEX: stage_flags = VK_SHADER_STAGE_VERTEX_BIT; break;
                case ShaderStage::FRAGMENT: stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT; break;
                default: check(false, "Unknown shader stage");
            }

            // Create shader stage
            stages[i] = {
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = stage_flags,
                .module              = module->module,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            };
        }

        // endregion

        // region Create vertex input state

        // Default for now because we don't have vertex input
        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 0,
            .pVertexBindingDescriptions      = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions    = nullptr,
        };

        // endregion

        // region Create input assembly state

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        // endregion

        // region Create viewport state

        VkViewport viewport = {
            // Start in the corner
            .x = 0.0f,
            .y = 0.0f,
            // Scale to the size of the window
            .width  = static_cast<float>(viewport_extent.width),
            .height = static_cast<float>(viewport_extent.height),
            // Depth range is 0.0f to 1.0f
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor = {
            // Start in the corner
            .offset = (VkOffset2D) {0, 0},
            // Scale to the size of the window
            .extent = viewport_extent,
        };

        VkPipelineViewportStateCreateInfo viewport_state_create_info = {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 1,
            .pViewports    = &viewport,
            .scissorCount  = 1,
            .pScissors     = &scissor,
        };

        // endregion

        // region Create rasterization state

        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
            .sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .depthClampEnable = VK_FALSE,
            // Keep the primitive in the rasterization stage
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            // No backface culling
            .cullMode  = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            // No depth bias
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp          = 0.0f,
            .depthBiasSlopeFactor    = 0.0f,
            // Width of the line
            .lineWidth = 1.0f,
        };

        // endregion

        // region Create multisample state

        VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 1.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
        };

        // endregion

        // region Create color blend state

        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        // No blending
        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments    = &color_blend_attachment_state,
            .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
        };

        // endregion

        // region Create depth stencil state

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .depthTestEnable       = VK_FALSE,
            .depthWriteEnable      = VK_FALSE,
            .depthCompareOp        = VK_COMPARE_OP_ALWAYS,
            .depthBoundsTestEnable = false,
            .stencilTestEnable     = false,
            .minDepthBounds        = 0.0f,
            .maxDepthBounds        = 1.0f,
        };

        // endregion

        // region Get render pass

        VkRenderPass render_pass = VK_NULL_HANDLE;
        switch (effect.render_stage_kind)
        {
            case RenderStageKind::GEOMETRY: render_pass = passes.geometry_pass; break;
            case RenderStageKind::LIGHTING: render_pass = passes.lighting_pass; break;
            default: check(false, "Invalid render stage kind");
        }

        // endregion

        // region Create pipeline

        VkGraphicsPipelineCreateInfo pipeline_create_info = {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = static_cast<uint32_t>(stages.count()),
            .pStages             = stages.data(),
            .pVertexInputState   = &vertex_input_state_create_info,
            .pInputAssemblyState = &input_assembly_state_create_info,
            .pTessellationState  = nullptr,
            .pViewportState      = &viewport_state_create_info,
            .pRasterizationState = &rasterization_state_create_info,
            .pMultisampleState   = &multisample_state_create_info,
            .pDepthStencilState  = &depth_stencil_state_create_info,
            .pColorBlendState    = &color_blend_state_create_info,
            .pDynamicState       = nullptr,
            .layout              = effect.pipeline_layout,
            .renderPass          = render_pass,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };
        VkPipeline pipeline = VK_NULL_HANDLE;
        vk_check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline),
                 "Failed to create pipeline");

        // endregion

        return pipeline;
    }

    void Renderer::Data::clear_pipelines(Swapchain &swapchain) const
    {
        // Destroy all pipelines
        for (auto pipeline : swapchain.pipelines)
        {
            vkDestroyPipeline(device, static_cast<VkPipeline>(pipeline.value.as_ptr), nullptr);
        }

        // Clear the map
        swapchain.pipelines.clear();

        // Reset the version
        swapchain.built_effects_version = 0;
    }

    void Renderer::Data::recreate_pipelines(Swapchain &swapchain) const
    {
        // Destroy all pipelines
        clear_pipelines(swapchain);

        // Build effects
        build_out_of_date_effects(swapchain);
    }

    void Renderer::Data::destroy_pipeline(Swapchain &swapchain, ShaderEffectId effect_id) const
    {
        if (swapchain.enabled)
        {
            // Get the pipeline
            auto pipeline = swapchain.pipelines.get(effect_id);
            if (pipeline.has_value())
            {
                // Destroy the pipeline
                vkDestroyPipeline(device, static_cast<VkPipeline>(pipeline.value()->as_ptr), nullptr);

                // Remove the pipeline from the map
                swapchain.pipelines.remove(effect_id);
            }
        }
    }

    // endregion

    // region Stage functions

    void Renderer::Data::update_stage_cache(Swapchain &swapchain)
    {
        // For each stage
        for (auto &stage : swapchain.render_stages)
        {
            // Clear cache
            stage.batches.clear();

            // Find the model_ids using the materials using a template using an effect matching the stage
            // = we want a list of model_ids, sorted by materials, which are sorted by templates, which are sorted by effects
            // this will minimize the number of binds to do
            Vector<ModelId> stage_models(10);

            // For each effect
            for (const auto &effect : shader_effects)
            {
                // If the effect supports that kind
                if (effect.value().render_stage_kind == stage.kind)
                {
                    // Get the pipeline
                    auto pipeline = swapchain.pipelines.get(effect.key());
                    check(pipeline.has_value(), "Tried to draw a shader effect that was not built.");

                    // For each material template
                    for (const auto &mat_template : material_templates)
                    {
                        // If the template has that effect
                        if (mat_template.value().shader_effects.includes(effect.key()))
                        {
                            // For each material
                            // Note: if this becomes to computationally intensive, we could register materials in the template
                            // like we do with the models
                            for (const auto &material : materials)
                            {
                                // If the material has that template and there is at least one model to render
                                if (material.value().template_id == mat_template.key()
                                    && !material.value().models_using_material.is_empty())
                                {
                                    // Add a render batch
                                    // Even though we could for now regroup them by shader effect instead of materials, this may
                                    // change when we will add descriptor sets
                                    // So we split batches by materials
                                    // However, we won't bind a pipeline if it is the same as before, and batches are sorted by effect
                                    stage.batches.push_back(RenderBatch {
                                        stage_models.size(),
                                        material.value().models_using_material.size(),
                                        static_cast<VkPipeline>(pipeline.value()->as_ptr),
                                    });

                                    // Add the models using that material
                                    stage_models.extend(material.value().models_using_material);
                                }
                            }
                        }
                    }
                }
            }

            // If there is something to render
            if (!stage_models.is_empty())
            {
                // Prepare draw indirect commands
                const VkBufferUsageFlags indirect_buffer_usage =
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                const VmaMemoryUsage indirect_buffer_memory_usage  = VMA_MEMORY_USAGE_CPU_TO_GPU;
                const size_t         required_indirect_buffer_size = stage_models.size() * sizeof(VkDrawIndirectCommand);

                // If it does not exist, create it
                if (stage.indirect_buffer.buffer == VK_NULL_HANDLE)
                {
                    stage.indirect_buffer =
                        allocator.create_buffer(required_indirect_buffer_size, indirect_buffer_usage, indirect_buffer_memory_usage);
                }
                // If it exists but isn't big enough, recreate it
                else if (stage.indirect_buffer.size < required_indirect_buffer_size)
                {
                    allocator.destroy_buffer(device, stage.indirect_buffer);
                    stage.indirect_buffer =
                        allocator.create_buffer(required_indirect_buffer_size, indirect_buffer_usage, indirect_buffer_memory_usage);
                }

                // At this point, we have an indirect buffer big enough to hold the commands we want to register

                // Register commands
                auto *indirect_commands = static_cast<VkDrawIndirectCommand *>(allocator.map_buffer(stage.indirect_buffer));

                for (auto i = 0; i < stage_models.size(); ++i)
                {
                    // Get model
                    const auto model_id = stage_models[i];
                    const auto model    = models.get(model_id);
                    check(model.has_value(), "Tried to draw a model that doesn't exist.");

                    indirect_commands[i].vertexCount   = 3; // TODO when mesh is added
                    indirect_commands[i].firstVertex   = 0;
                    indirect_commands[i].instanceCount = 1; // TODO when instances are added
                    indirect_commands[i].firstInstance = 0;
                }

                allocator.unmap_buffer(stage.indirect_buffer);
            }
        }
    }

    void draw_from_cache(const RenderStage &stage, VkCommandBuffer cmd)
    {
        constexpr uint32_t draw_stride    = sizeof(VkDrawIndirectCommand);
        VkPipeline         bound_pipeline = VK_NULL_HANDLE;

        // For each batch
        for (const auto &batch : stage.batches)
        {
            // If the pipeline is different from the last one, bind it
            if (bound_pipeline != batch.pipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, batch.pipeline);
                bound_pipeline = batch.pipeline;
            }

            // Draw the batch
            const uint32_t &&draw_offset = draw_stride * batch.offset;

            vkCmdDrawIndirect(cmd, stage.indirect_buffer.buffer, draw_offset, batch.count, draw_stride);
        }
    }

    // endregion

    // region Camera functions

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
            VkSurfaceKHR       example_surface        = example_window.get_vulkan_surface(m_data->instance);
            VkSurfaceFormatKHR swapchain_image_format = m_data->select_surface_format(example_surface);
            // Destroy the surface - this was just an example window, we will create a new one later
            vkDestroySurfaceKHR(m_data->instance, example_surface, nullptr);

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
        // Just take the other's m_data and set the original to null, so it can't access it anymore
        other.m_data = nullptr;
    }

    Renderer::~Renderer()
    {
        // If the m_data is null, then the renderer was never initialized
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
        clear_render_nodes();
        clear_models();
        clear_materials();
        clear_material_templates();
        clear_shader_effects();
        clear_shader_modules();

        // Clear swapchains
        m_data->clear_swapchains();

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

    void Renderer::connect_window(uint32_t window_slot_index, Window &window)
    {
        // Prevent access to the positions that are outside the swapchain array
        check(window_slot_index < m_data->swapchains.count(), "Window index is out of bounds");

        // Get the swapchain in the renderer
        Swapchain &swapchain = m_data->swapchains[window_slot_index];

        // Ensure that there is not a live swapchain here already
        check(!swapchain.enabled,
              "Attempted to create a swapchain in a slot where there was already an active one."
              " To recreate a swapchain, see rg_renderer_recreate_swapchain.");

        // region Window & Surface

        // Get the window's surface
        swapchain.target_window = &window;
        swapchain.surface       = window.get_vulkan_surface(m_data->instance);

        // Check that the surface is supported
        VkBool32 surface_supported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_data->physical_device,
                                             m_data->graphics_queue.family_index,
                                             swapchain.surface,
                                             &surface_supported);
        check(surface_supported, "The chosen GPU is unable to render to the given surface.");

        // endregion

        // region Choose a present mode

        {
            // Get the list of supported present modes
            uint32_t present_mode_count = 0;
            vk_check(
                vkGetPhysicalDeviceSurfacePresentModesKHR(m_data->physical_device, swapchain.surface, &present_mode_count, nullptr));
            Array<VkPresentModeKHR> available_present_modes(present_mode_count);
            vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(m_data->physical_device,
                                                               swapchain.surface,
                                                               &present_mode_count,
                                                               available_present_modes.data()));

            bool                       present_mode_found = false;
            constexpr VkPresentModeKHR desired_modes[]    = {
                VK_PRESENT_MODE_MAILBOX_KHR,
                VK_PRESENT_MODE_FIFO_KHR,
            };
            for (const auto &available_mode : available_present_modes)
            {
                for (const auto &desired_mode : desired_modes)
                {
                    if (available_mode == desired_mode)
                    {
                        swapchain.present_mode = desired_mode;
                        present_mode_found     = true;
                        break;
                    }
                }
                if (present_mode_found)
                {
                    break;
                }
            }
            check(present_mode_found, "Could not find a suitable present mode for this surface.");

            // Log choice
            std::cout << "Chosen present mode: " << vk_present_mode_to_string(swapchain.present_mode) << std::endl;
        }

        // endregion

        // region Image count selection

        VkSurfaceCapabilitiesKHR surface_capabilities = {};
        vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_data->physical_device, swapchain.surface, &surface_capabilities));

        // For the image count, take the minimum plus one. Or, if the minimum is equal to the maximum, take that value.
        uint32_t image_count = surface_capabilities.minImageCount + 1;
        if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount)
        {
            image_count = surface_capabilities.maxImageCount;
        }
        swapchain.image_count   = image_count;
        swapchain.pre_transform = surface_capabilities.currentTransform;

        // endregion

        swapchain.image_format = m_data->select_surface_format(swapchain.surface);

        // Get window extent
        auto extent = window.get_current_extent();

        m_data->init_swapchain_inner(swapchain, extent);

        // region Init render stages

        // Using a dynamic array allows us to define a parameter to this function to define the stages
        swapchain.render_stages = Array<RenderStage>(RENDER_STAGE_COUNT);

        // Init stages
        // They are hardcoded for now
        swapchain.render_stages[0].kind = RenderStageKind::GEOMETRY;
        swapchain.render_stages[1].kind = RenderStageKind::LIGHTING;

        // endregion

        // region Register to window events

        // "this" is moved after the creation of the renderer
        // Thus the pointer doesn't point to the right place anymore when the callback is called
        // To ensure that the callback uses the real m_data, we need to copy the pointer to the renderer Data
        // Also, we fetch the swapchain with the window index again because the swapchain variable here is local
        auto data = m_data;
        swapchain.window_resize_event_handler_id =
            window.on_resize()->subscribe([data, window_slot_index](const Extent2D &new_extent) mutable
                                          { data->recreate_swapchain(data->swapchains[window_slot_index], new_extent); });

        // endregion

        // Enable it
        swapchain.enabled = true;
    }

    // region Shader modules functions

    ShaderModuleId Renderer::load_shader_module(const char *shader_path, ShaderStage kind)
    {
        // Load SPIR-V binary from file
        size_t    code_size = 0;
        uint32_t *code      = nullptr;
        try
        {
            code = static_cast<uint32_t *>(load_binary_file(shader_path, &code_size));
        }
        catch (const std::exception &e)
        {
            // Always fail for now, later we can simply return invalid id (or let the exception propagate) to let the caller try to
            // recover
            check(false, "Could not load shader module: " + std::string(e.what()));
        }

        // Create shader vk module
        VkShaderModuleCreateInfo shader_module_create_info {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = code_size,
            .pCode    = code,
        };
        VkShaderModule vk_shader_module;
        vk_check(vkCreateShaderModule(m_data->device, &shader_module_create_info, nullptr, &vk_shader_module));

        // Free code
        delete[] code;

        // Create the module
        ShaderModule module {
            .module = vk_shader_module,
            .stage  = kind,
        };

        // Add it in the storage
        auto id = m_data->shader_modules.push(module);

        // Log
        std::cout << "Loaded shader module " << id << ": " << shader_path << "\n";

        // Return the id
        return id;
    }

    void Renderer::destroy_shader_module(ShaderModuleId id)
    {
        // Lookup the module
        auto res = m_data->shader_modules.get(id);
        if (res.has_value())
        {
            vkDestroyShaderModule(m_data->device, res.value().module, nullptr);
            m_data->shader_modules.remove(id);
        }
        // No value = already deleted, in a sense. This is not an error since the contract is respected
    }

    void Renderer::clear_shader_modules()
    {
        for (auto &module : m_data->shader_modules)
        {
            vkDestroyShaderModule(m_data->device, module.value().module, nullptr);
        }
        m_data->shader_modules.clear();
    }

    // endregion

    // region Shader effect functions

    ShaderEffectId Renderer::create_shader_effect(const Array<ShaderModuleId> &stages, RenderStageKind render_stage_kind)
    {
        check(stages.count() > 0, "A shader effect must have at least one stage.");

        // Create shader effect
        ShaderEffect effect {render_stage_kind, stages, VK_NULL_HANDLE};

        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 0,
            .pSetLayouts            = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = nullptr,
        };
        vk_check(vkCreatePipelineLayout(m_data->device, &pipeline_layout_create_info, nullptr, &effect.pipeline_layout),
                 "Couldn't create pipeline layout");

        // Store effect
        auto id = m_data->shader_effects.push(std::move(effect));

        // Increment version in the renderer so that swapchains know they have to rebuild pipelines
        m_data->effects_version++;

        return id;
    }

    void Renderer::destroy_shader_effect(ShaderEffectId id)
    {
        // Lookup the effect
        auto res = m_data->shader_effects.get(id);
        if (res.has_value())
        {
            vkDestroyPipelineLayout(m_data->device, res.value().pipeline_layout, nullptr);
            m_data->shader_modules.remove(id);
        }
        // No value = already deleted, in a sense. This is not an error since the contract is respected
    }
    void Renderer::clear_shader_effects()
    {
        for (auto &effect : m_data->shader_effects)
        {
            vkDestroyPipelineLayout(m_data->device, effect.value().pipeline_layout, nullptr);
        }
        m_data->shader_effects.clear();
    }

    // endregion

    // region Material template functions

    MaterialTemplateId Renderer::create_material_template(const Array<ShaderEffectId> &available_effects)
    {
        check(available_effects.count() > 0, "A material template must have at least one effect.");

        // Create material template
        return m_data->material_templates.push({
            available_effects,
        });
    }

    void Renderer::destroy_material_template(MaterialTemplateId id)
    {
        // There is no special vulkan handle to destroy in the template, so we just let the storage do its job
        m_data->material_templates.remove(id);
    }

    void Renderer::clear_material_templates()
    {
        // Same here
        m_data->material_templates.clear();
    }

    // endregion

    // region Material functions

    MaterialId Renderer::create_material(MaterialTemplateId material_template)
    {
        check(material_template != NULL_ID, "A material must have a template.");

        // Create material
        return m_data->materials.push({
            material_template,
            Vector<ModelId>(10),
        });
    }

    void Renderer::destroy_material(MaterialId id)
    {
        // There is no special vulkan handle to destroy in the material, so we just let the storage do its job
        m_data->materials.remove(id);
    }

    void Renderer::clear_materials()
    {
        // Same here
        m_data->materials.clear();
    }

    // endregion

    // region Model functions

    ModelId Renderer::create_model(MaterialId material)
    {
        check(material != NULL_ID, "A model must have a material.");

        // Create model
        auto model_id = m_data->models.push({
            material,
            Vector<RenderNodeId>(10),
        });

        // Register it in the material
        auto mat = m_data->materials.get(material);
        check(mat.has_value(), "Material doesn't exist.");
        mat->models_using_material.push_back(model_id);

        return model_id;
    }

    void Renderer::destroy_model(ModelId id)
    {
        // Lookup the model
        auto res = m_data->models.get(id);
        if (res.has_value())
        {
            // Remove it from the material
            auto mat = m_data->materials.get(res.value().material_id);
            check(mat.has_value(), "Consistency error: material referenced in model " + std::to_string(id) + " doesn't exist.");
            mat->models_using_material.remove(id);

            // Remove it from the renderer
            m_data->models.remove(id);
        }
        // No value = already deleted, in a sense. This is not an error since the contract is respected
    }

    void Renderer::clear_models()
    {
        // Since all models are going to be destroyed, none of them should still exist in the materials
        // We can then just clear the vectors for the same result, and it will be quicker
        for (auto &mat : m_data->materials)
        {
            mat.value().models_using_material.clear();
        }

        m_data->models.clear();
    }

    // endregion

    // region Render nodes functions

    RenderNodeId Renderer::create_render_node(ModelId model)
    {
        check(model != NULL_ID, "A render node must have a model.");

        // Create render node
        auto node_id = m_data->render_nodes.push({model});

        // Register it in the model
        auto model_res = m_data->models.get(model);
        check(model_res.has_value(), "Model doesn't exist.");
        model_res->instances.push_back(node_id);

        return node_id;
    }

    void Renderer::destroy_render_node(RenderNodeId id)
    {
        // Lookup the render node
        auto res = m_data->render_nodes.get(id);
        if (res.has_value())
        {
            // Remove it from the model
            auto model_res = m_data->models.get(res.value().model_id);
            check(model_res.has_value(),
                  "Consistency error: model referenced in render node " + std::to_string(id) + " doesn't exist.");
            model_res->instances.remove(id);

            // Remove it from the renderer
            m_data->render_nodes.remove(id);
        }
        // No value = already deleted, in a sense. This is not an error since the contract is respected
    }

    void Renderer::clear_render_nodes()
    {
        // Since all render nodes will be destroyed, none of them should still exist in the models after the operation
        // This means that we can just clear the instance vectors, which will be faster
        for (auto &model : m_data->models)
        {
            model.value().instances.clear();
        }

        m_data->render_nodes.clear();
    }
    // endregion

    // region Drawing

    void Renderer::draw()
    {
        // If there are no cameras, there is nothing to draw
        if (m_data->cameras.is_empty())
        {
            return;
        }

        // Get current frame
        const uint64_t current_frame_index = m_data->get_current_frame_index();
        FrameData     &current_frame       = m_data->frames[current_frame_index];

        // Wait for the fence
        m_data->wait_for_fence(current_frame.render_fence);

        // For each enabled camera
        for (auto &cam_entry : m_data->cameras)
        {
            const auto &camera = cam_entry.value();
            if (camera.enabled)
            {
                // Get its target swapchain
                auto &swapchain = m_data->swapchains[camera.target_swapchain_index];
                check(swapchain.enabled, "Active camera tries to render to a disabled swapchain.");

                // Update pipelines if needed
                m_data->build_out_of_date_effects(swapchain);

                // Update render stage cache
                m_data->update_stage_cache(swapchain);

                // Begin recording
                m_data->begin_recording();

                // Get next image
                auto image_index = m_data->get_next_swapchain_image(swapchain);

                // Geometric pass TODO

                // Lighting pass
                VkClearValue clear_value = {
                    .color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}},
                };
                VkRenderPassBeginInfo render_pass_begin_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    .pNext = nullptr,
                    // Render pass
                    .renderPass = m_data->passes.lighting_pass,
                    // Link framebuffer
                    .framebuffer = swapchain.framebuffers[image_index],
                    // Render area
                    .renderArea = {.offset = {0, 0}, .extent = swapchain.viewport_extent},
                    // Clear values
                    .clearValueCount = 1,
                    .pClearValues    = &clear_value,
                };
                vkCmdBeginRenderPass(current_frame.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

                // Draw
                draw_from_cache(swapchain.render_stages[1], current_frame.command_buffer);

                vkCmdEndRenderPass(current_frame.command_buffer);

                // End recording and submit
                m_data->end_recording_and_submit();

                // Present
                VkPresentInfoKHR present_info = {
                    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                    .pNext              = nullptr,
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores    = &current_frame.render_semaphore,
                    .swapchainCount     = 1,
                    .pSwapchains        = &swapchain.vk_swapchain,
                    .pImageIndices      = &image_index,
                };
                vkQueuePresentKHR(m_data->graphics_queue.queue, &present_info);
            }
        }

        // Increment frame number
        m_data->current_frame_number++;
    }

    // endregion

    // region Cameras

    CameraId Renderer::create_orthographic_camera(uint32_t window_index, float near, float far)
    {
        check(window_index < m_data->swapchains.count(), "Invalid window index");
        const auto &extent = m_data->swapchains[window_index].viewport_extent;
        return create_orthographic_camera(window_index,
                                          static_cast<float>(extent.width),
                                          static_cast<float>(extent.height),
                                          near,
                                          far,
                                          Transform());
    }
    CameraId Renderer::create_orthographic_camera(uint32_t window_index, float width, float height, float near, float far)
    {
        return create_orthographic_camera(window_index, width, height, near, far, Transform());
    }
    CameraId Renderer::create_orthographic_camera(uint32_t         window_index,
                                                  float            width,
                                                  float            height,
                                                  float            near,
                                                  float            far,
                                                  const Transform &transform)
    {
        check(window_index < m_data->swapchains.count(), "Invalid window index");
        // Create camera
        return m_data->cameras.push(Camera {
            .enabled                = true,
            .target_swapchain_index = window_index,
            .transform              = transform,
            .type                   = CameraType::ORTHOGRAPHIC,
            .orthographic =
                {
                    .width      = width,
                    .height     = height,
                    .near_plane = near,
                    .far_plane  = far,
                },
        });
    }

    CameraId Renderer::create_perspective_camera(uint32_t window_index, float fov, float near, float far)
    {
        check(window_index < m_data->swapchains.count(), "Invalid window index");
        const auto &extent = m_data->swapchains[window_index].viewport_extent;
        return create_perspective_camera(window_index,
                                         fov,
                                         static_cast<float>(extent.width) / static_cast<float>(extent.height),
                                         near,
                                         far,
                                         Transform());
    }
    CameraId Renderer::create_perspective_camera(uint32_t window_index, float fov, float aspect, float near, float far)
    {
        return create_perspective_camera(window_index, fov, aspect, near, far, Transform());
    }
    CameraId Renderer::create_perspective_camera(uint32_t         window_index,
                                                 float            fov,
                                                 float            aspect,
                                                 float            near,
                                                 float            far,
                                                 const Transform &transform)
    {
        check(window_index < m_data->swapchains.count(), "Invalid window index");
        // Create camera
        return m_data->cameras.push(Camera {
            .enabled                = true,
            .target_swapchain_index = window_index,
            .transform              = transform,
            .type                   = CameraType::PERSPECTIVE,
            .perspective =
                {
                    .fov          = fov,
                    .aspect_ratio = aspect,
                    .near_plane   = near,
                    .far_plane    = far,
                },
        });
    }

    void Renderer::remove_camera(CameraId id)
    {
        m_data->cameras.remove(id);
    }

    const Transform &Renderer::get_camera_transform(CameraId id) const
    {
        return m_data->cameras[id].transform;
    }

    Transform &Renderer::get_camera_transform(CameraId id)
    {
        return m_data->cameras[id].transform;
    }

    void Renderer::disable_camera(CameraId id)
    {
        m_data->cameras[id].enabled = false;
    }

    void Renderer::enable_camera(CameraId id)
    {
        m_data->cameras[id].enabled = true;
    }

    bool Renderer::is_camera_enabled(CameraId id) const
    {
        return m_data->cameras[id].enabled;
    }

    CameraType Renderer::get_camera_type(CameraId id) const
    {
        return m_data->cameras[id].type;
    }

    // endregion

} // namespace rg
#endif