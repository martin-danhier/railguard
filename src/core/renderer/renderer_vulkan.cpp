#ifdef RENDERER_VULKAN
#include "railguard/core/renderer/renderer.h"
#include <railguard/core/mesh.h>
#include <railguard/core/renderer/gpu_structs.h>
#include <railguard/core/renderer/render_pipeline.h>
#include <railguard/core/window.h>
#include <railguard/utils/array.h>
#include <railguard/utils/event_sender.h>
#include <railguard/utils/geometry/transform.h>
#include <railguard/utils/io.h>
#include <railguard/utils/storage.h>

#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <iostream>
#include <stb_image.h>
#include <string>
#include <volk.h>

// Needs to be after volk.h
#include <railguard/utils/vulkan/descriptor_set_helpers.h>

#include <vk_mem_alloc.h>

// Check dependencies versions
#ifndef VK_API_VERSION_1_3
// We need Vulkan SDK 1.3 for vk_mem_alloc, because it uses VK_API_VERSION_MAJOR which was introduced in 1.3
// We need this even if we use a lower version of Vulkan in the instance
#error "Vulkan 1.3 is required"
#endif

// ---==== Defines ====---

#define NB_OVERLAPPING_FRAMES   3
#define VULKAN_API_VERSION      VK_API_VERSION_1_2
#define WAIT_FOR_FENCES_TIMEOUT 1000000000
#define SEMAPHORE_TIMEOUT       1000000000

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
        VmaAllocator m_allocator             = VK_NULL_HANDLE;
        VkDevice     m_device                = VK_NULL_HANDLE;
        uint32_t     m_graphics_queue_family = 0;
        uint32_t     m_transfer_queue_family = 0;

      public:
        Allocator() = default;
        Allocator(VkInstance       instance,
                  VkDevice         device,
                  VkPhysicalDevice physical_device,
                  uint32_t         graphics_queue_family,
                  uint32_t         transfer_queue_family);
        Allocator(Allocator &&other) noexcept;
        Allocator &operator=(Allocator &&other) noexcept;

        ~Allocator();

        [[nodiscard]] AllocatedImage create_image(VkFormat           image_format,
                                                  VkExtent3D         image_extent,
                                                  VkImageUsageFlags  image_usage,
                                                  VkImageAspectFlags image_aspect,
                                                  VmaMemoryUsage     memory_usage,
                                                  bool               concurrent = false) const;
        void                         destroy_image(AllocatedImage &image) const;

        [[nodiscard]] AllocatedBuffer create_buffer(size_t             allocation_size,
                                                    VkBufferUsageFlags buffer_usage,
                                                    VmaMemoryUsage     memory_usage,
                                                    bool               concurrent = false) const;
        void                          destroy_buffer(AllocatedBuffer &buffer) const;
        void                         *map_buffer(AllocatedBuffer &buffer) const;
        void                          unmap_buffer(AllocatedBuffer &buffer) const;
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
        /** Layout of the textures set.
         *
         * Each shader configuration is designed for a specific arrangement of textures (normal map, etc).
         * All materials that use this shader thus need to respect the same layout.
         */
        VkDescriptorSetLayout textures_set_layout = VK_NULL_HANDLE;
    };

    struct MaterialTemplate
    {
        /**
         * Array of shader effect IDs. Available effects for this template.
         * Given a render stages kind, the first corresponding effect will be the one used.
         * */
        Array<ShaderEffectId> shader_effects = {};
    };

    /** An attachment texture is a texture that is actually an output attachment of a render stage */
    struct AttachmentTexture
    {
        size_t    attachment_index = 0;
        VkSampler sampler          = VK_NULL_HANDLE;
    };

    struct Texture
    {
        AllocatedImage image   = {};
        VkSampler      sampler = VK_NULL_HANDLE;
    };

    struct Material
    {
        /** Template this material is based on. Defines the available shader effects for this material. */
        MaterialTemplateId template_id = NULL_ID;
        /** Models using this material */
        Vector<ModelId>         models_using_material = {};
        Array<Array<TextureId>> textures              = {};
        Array<VkDescriptorSet>  textures_sets         = {};
    };

    struct StoredMeshPart
    {
        MeshPart mesh_part;
        size_t   vertex_offset;
        size_t   index_offset;
        bool     is_uploaded;

        explicit StoredMeshPart(MeshPart &&part) : mesh_part(std::move(part)), vertex_offset(0), index_offset(0), is_uploaded(false)
        {
        }
    };

    struct Model
    {
        /** MeshPart used by this model */
        MeshPartId mesh_part_id = NULL_ID;
        /** Material used by this model */
        MaterialId material_id = NULL_ID;
        /** Nodes using this model */
        Vector<RenderNodeId> instances = {};
        /**
         * Root transform of the model.
         * All instances object matrix will be first multiplied by the model matrix.
         * It can for example be used to adjust the orientation of an imported model which may not use the same axis conventions.
         * */
        Transform transform = {};
    };

    struct RenderNode
    {
        /** Model used by this node */
        ModelId model_id = NULL_ID;
    };

    // Transfer

    struct TransferCommand
    {
        AllocatedBuffer staging_buffer = {};
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkFence         fence          = VK_NULL_HANDLE;

        // Begin command buffer
        void begin() const;
        void end_and_submit(VkQueue queue);
    };

    struct TransferContext
    {
        VkCommandPool           transfer_pool = VK_NULL_HANDLE;
        VkCommandPool           graphics_pool = VK_NULL_HANDLE;
        Vector<TransferCommand> commands {2};
    };

    // Main types

    struct VertexInputDescription
    {
        VkPipelineVertexInputStateCreateFlags    flags;
        uint32_t                                 binding_count;
        const VkVertexInputBindingDescription   *bindings;
        uint32_t                                 attribute_count;
        const VkVertexInputAttributeDescription *attributes;
    };

    struct Camera
    {
        bool       enabled                = false;
        size_t     target_swapchain_index = 0;
        Transform  transform              = {};
        CameraType type                   = CameraType::PERSPECTIVE;
        union CameraSpecs
        {
            struct Perspective
            {
                float fov          = 0.0f;
                float aspect_ratio = 0.0f;
                float near_plane   = 0.0f;
                float far_plane    = 0.0f;
            } as_perspective;
            struct Orthographic
            {
                float width      = 0.0f;
                float height     = 0.0f;
                float near_plane = 0.0f;
                float far_plane  = 0.0f;
            } as_orthographic;
        } specs;
    };

    struct RenderBatch
    {
        size_t           offset          = 0;
        size_t           count           = 0;
        VkPipeline       pipeline        = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VkDescriptorSet  textures_set    = VK_NULL_HANDLE;
    };

    /**
     * A render stage instance is a structure that contain swapchain-specific render stage data, such as the indirect buffer or the
     * render batches cache.
     */
    struct RenderStageInstance
    {
        /**
         * The actual attachments of this stage.
         * It is structured as an array storing, for each image index, an array of attachments.
         * There are as many image indices as swapchain images.
         */
        Array<Array<AllocatedImage>> attachments = {};
        /** Array storing, for each image index, the related framebuffer */
        Array<VkFramebuffer>      framebuffers    = {};
        AllocatedBuffer           indirect_buffer = {};
        Vector<RenderBatch>       batches {5};
        Vector<AttachmentTexture> output_textures {3};
        /** One per image */
        Array<VkDescriptorSet> output_textures_set = {};
    };

    /**
     * A render stage is a structure that contains the global render stage data, such as Vulkan render passes.
     */
    struct RenderStage
    {
        RenderStageKind kind           = RenderStageKind::INVALID;
        VkRenderPass    vk_render_pass = VK_NULL_HANDLE;
    };

    struct FrameData
    {
        VkCommandPool   command_pool      = VK_NULL_HANDLE;
        VkCommandBuffer command_buffer    = VK_NULL_HANDLE;
        VkSemaphore     present_semaphore = VK_NULL_HANDLE;
        VkSemaphore     render_semaphore  = VK_NULL_HANDLE;
        VkFence         render_fence      = VK_NULL_HANDLE;

        // Descriptor sets

        // One descriptor pool per frame: that way we can just reset the pool to free all sets of the frame
        DynamicDescriptorPool descriptor_pool = {};
        // Used to determine whether the sets should be rebuilt
        uint64_t built_buffers_config_version = 0;

        // Object data
        AllocatedBuffer object_info_buffer = {};
        VkDescriptorSet global_set         = VK_NULL_HANDLE;

        // Per-swapchain sets and buffers. Dynamic over swapchains
        AllocatedBuffer camera_info_buffer = {};
        VkDescriptorSet swapchain_set      = VK_NULL_HANDLE;
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
        uint32_t       window_index    = 0;

        // Swapchain images
        uint32_t           image_count  = 0;
        VkSurfaceFormatKHR image_format = {};

        // Present mode
        VkPresentModeKHR              present_mode  = VK_PRESENT_MODE_MAILBOX_KHR;
        VkSurfaceTransformFlagBitsKHR pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

        // Target window
        Window                   *target_window                  = nullptr;
        EventSender<Extent2D>::Id window_resize_event_handler_id = NULL_ID;
        VkSurfaceKHR              surface                        = VK_NULL_HANDLE;
        // Pipelines
        // Since vulkan handles are just pointers, a hash map is all we need
        HashMap  pipelines             = {};
        uint64_t built_effects_version = 0;

        // Internal textures
        /** Pool that is reset every time the swapchain is recreated. Useful for resources that don't require an update at each frame,
         * but need to be recreated with the swapchain. For example, attachment textures (like G-buffer)
         */
        DynamicDescriptorPool swapchain_static_descriptor_pool = {};

        // Render stages
        uint64_t                   built_draw_cache_version        = 0;
        uint32_t                   built_internal_textures_version = 0;
        Array<RenderStageInstance> render_stages                   = {};

        // Swapchain version (incremented at each recreation)
        uint32_t swapchain_version = 0;
    };

    struct Renderer::Data
    {
        VkInstance                 instance          = VK_NULL_HANDLE;
        VkDevice                   device            = VK_NULL_HANDLE;
        VkPhysicalDevice           physical_device   = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties device_properties = {};
        Allocator                  allocator         = {};
#ifdef USE_VK_VALIDATION_LAYERS
        VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
#endif
        /**
         * @brief Fixed-size array containing the swapchains.
         * We place them in a array to be able to efficiently iterate through them.
         */
        Array<Swapchain> swapchains         = {};
        size_t           swapchain_capacity = 0;

        // Render pipeline
        RenderPipelineDescription render_pipeline_description = {};
        Array<RenderStage>        render_stages               = {};
        /** Stores, for each render stage that doesn't use the material system, the id of the shader effect to use on the default quad.
         */
        HashMap global_shader_effects = {};

        // Queues
        Queue graphics_queue = {};
        Queue transfer_queue = {};

        // Counter of frame since the start of the renderer
        uint64_t  current_frame_number          = 1;
        FrameData frames[NB_OVERLAPPING_FRAMES] = {};

        // Transfer context
        TransferContext transfer_context = {};

        // Storages for the material system
        Storage<ShaderModule>     shader_modules     = {};
        Storage<ShaderEffect>     shader_effects     = {};
        Storage<MaterialTemplate> material_templates = {};
        Storage<Texture>          textures           = {};
        Storage<Material>         materials          = {};
        Storage<Model>            models             = {};
        Storage<RenderNode>       render_nodes       = {};
        Storage<Camera>           cameras            = {};
        Storage<StoredMeshPart>   mesh_parts         = {};

        // Vertex and index buffer for all the meshes
        AllocatedBuffer vertex_buffer = {};
        AllocatedBuffer index_buffer  = {};

        // Storage buffer sizes
        size_t object_data_capacity = 100;

        // Descriptor pool for sets that don't need to change per frame
        DynamicDescriptorPool static_descriptor_pool = {};

        // Descriptor layouts
        VkDescriptorSetLayout global_set_layout    = VK_NULL_HANDLE;
        VkDescriptorSetLayout swapchain_set_layout = VK_NULL_HANDLE;

        // Number incremented at each created shader effect
        // It is stored in the swapchain when effects are built
        // If the number in the swapchain is different, we need to rebuild the pipelines
        uint64_t effects_version = 0;
        // Same principle for descriptor sets
        // It is updated when buffer or texture combinations change
        // Frames that are out of date will be rebuilt
        uint64_t buffer_config_version = 0;
        // Same for draw cache
        uint64_t draw_cache_version = 0;
        // Idem for meshes, but since the buffers are global to the renderer, a bool is enough
        bool should_update_mesh_buffers = false;

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
        void                             recreate_swapchain(Swapchain &swapchain, const Extent2D &new_extent);
        uint32_t                         get_next_swapchain_image(Swapchain &swapchain) const;

        [[nodiscard]] VkPipeline build_shader_effect(const VkExtent2D &viewport_extent, const ShaderEffect &effect);
        void                     build_out_of_date_effects(Swapchain &swapchain);
        void                     clear_pipelines(Swapchain &swapchain) const;
        void                     recreate_pipelines(Swapchain &swapchain);
        void                     destroy_pipeline(Swapchain &swapchain, ShaderEffectId shader_effect_id) const;
        void                     update_storage_buffers(FrameData &frame);
        void                     update_descriptor_sets(FrameData &frame) const;
        void                     update_render_stages_output_sets(Swapchain &swapchain) const;

        void update_stage_cache(Swapchain &swapchain);

        void send_camera_data(size_t window_index, const Camera &camera, FrameData &current_frame);

        void draw_from_cache(const RenderStageInstance &stage,
                             VkCommandBuffer            cmd,
                             FrameData                 &current_frame,
                             size_t                     window_index) const;
        void draw_quad(const Swapchain &swapchain,
                       size_t           stage_index,
                       size_t           image_index,
                       VkCommandBuffer  cmd,
                       FrameData       &current_frame,
                       size_t           window_index) const;
        template<typename T>
        void                 copy_buffer_to_gpu(const T &src, AllocatedBuffer &dst, size_t offset = 0);
        [[nodiscard]] size_t pad_uniform_buffer_size(size_t original_size) const;

        [[nodiscard]] static VertexInputDescription get_vertex_description();
        void                                        update_mesh_buffers();

        // Transfer
        TransferCommand create_transfer_command(VkCommandPool pool) const;
        void            reset_transfer_context();
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
            case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
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

    // region Conversion to Vk types

    VkShaderStageFlags convert_shader_stages(ShaderStage stage, bool forceOne = false)
    {
        VkShaderStageFlags result = 0;

        if (forceOne)
        {
            check(stage == ShaderStage::VERTEX || stage == ShaderStage::FRAGMENT, "Expected a single shader stages, got multiple.");
        }

        // Stage can be a mask
        if (stage & ShaderStage::VERTEX)
        {
            result |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (stage & ShaderStage::FRAGMENT)
        {
            result |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        return result;
    }

    /**
     * Converts railguard format to a Vulkan format.
     * @param format Original format.
     * @param window_format The format that we need to use to replace WINDOW_FORMAT
     */
    VkFormat convert_format(Format format, VkFormat window_format = VK_FORMAT_UNDEFINED)
    {
        switch (format)
        {
            case Format::UNDEFINED: return VK_FORMAT_UNDEFINED;
            case Format::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
            case Format::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
            case Format::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
            case Format::R8G8B8A8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
            case Format::R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case Format::WINDOW_FORMAT: return window_format;
            default: return VK_FORMAT_UNDEFINED;
        }
    }

    VkImageLayout convert_layout(ImageLayout layout)
    {
        switch (layout)
        {
            case ImageLayout::UNDEFINED: return VK_IMAGE_LAYOUT_UNDEFINED;
            case ImageLayout::SHADER_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ImageLayout::PRESENT_SRC: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            case ImageLayout::DEPTH_STENCIL_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            default: return VK_IMAGE_LAYOUT_UNDEFINED;
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

        // Prefer something else than llvmpipe, which is testing use only
#ifdef __cpp_lib_starts_ends_with
        if (!std::string(device_properties.deviceName).starts_with("llvmpipe"))
        {
            score += 15000;
        }
#else
        if (!std::string(device_properties.deviceName).find("llvmpipe"))
        {
            score += 15000;
        }
#endif

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

    Allocator::Allocator(VkInstance       instance,
                         VkDevice         device,
                         VkPhysicalDevice physical_device,
                         uint32_t         graphics_queue_family,
                         uint32_t         transfer_queue_family)
        : m_device(device),
          m_graphics_queue_family(graphics_queue_family),
          m_transfer_queue_family(transfer_queue_family)
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

    Allocator::Allocator(Allocator &&other) noexcept
        : m_allocator(other.m_allocator),
          m_device(other.m_device),
          m_graphics_queue_family(other.m_graphics_queue_family),
          m_transfer_queue_family(other.m_transfer_queue_family)
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
            m_device          = other.m_device;
            other.m_allocator = VK_NULL_HANDLE;
        }
        return *this;
    }

    AllocatedImage Allocator::create_image(VkFormat           image_format,
                                           VkExtent3D         image_extent,
                                           VkImageUsageFlags  image_usage,
                                           VkImageAspectFlags image_aspect,
                                           VmaMemoryUsage     memory_usage,
                                           bool               concurrent) const
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

        // Sharing mode
        if (concurrent && m_graphics_queue_family != m_transfer_queue_family)
        {
            image_create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            Array<uint32_t> queue_indices = {
                m_graphics_queue_family,
                m_transfer_queue_family,
            };
            image_create_info.pQueueFamilyIndices   = queue_indices.data();
            image_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_indices.size());
        }

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
        vk_check(vkCreateImageView(m_device, &image_view_create_info, nullptr, &image.image_view), "Failed to create image view");

        return image;
    }

    void Allocator::destroy_image(AllocatedImage &image) const
    {
        vkDestroyImageView(m_device, image.image_view, nullptr);
        if (image.allocation != VK_NULL_HANDLE)
        {
            vmaDestroyImage(m_allocator, image.image, image.allocation);
        }
        image.image      = VK_NULL_HANDLE;
        image.allocation = VK_NULL_HANDLE;
        image.image_view = VK_NULL_HANDLE;
    }

    AllocatedBuffer Allocator::create_buffer(size_t             allocation_size,
                                             VkBufferUsageFlags buffer_usage,
                                             VmaMemoryUsage     memory_usage,
                                             bool               concurrent) const
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

        // Sharing mode
        if (concurrent && m_graphics_queue_family != m_transfer_queue_family)
        {
            buffer_create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            Array<uint32_t> queue_indices  = {
                 m_graphics_queue_family,
                 m_transfer_queue_family,
            };
            buffer_create_info.pQueueFamilyIndices   = queue_indices.data();
            buffer_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_indices.size());
        }

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

    void Allocator::destroy_buffer(AllocatedBuffer &buffer) const
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

    template<typename T>
    void Renderer::Data::copy_buffer_to_gpu(const T &src, AllocatedBuffer &dst, size_t offset)
    {
        // Copy the data to the GPU
        char *data = static_cast<char *>(allocator.map_buffer(dst));

        // Pad the data if needed
        if (offset != 0)
        {
            data += pad_uniform_buffer_size(sizeof(T)) * offset;
        }

        memcpy(data, &src, sizeof(T));
        allocator.unmap_buffer(dst);
    }

    size_t Renderer::Data::pad_uniform_buffer_size(size_t original_size) const
    {
        // Get the alignment requirement
        const size_t &min_alignment = device_properties.limits.minUniformBufferOffsetAlignment;
        size_t        aligned_size  = original_size;
        if (min_alignment > 0)
        {
            aligned_size = (aligned_size + min_alignment - 1) & ~(min_alignment - 1);
        }
        return aligned_size;
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
        // Reset descriptor pool to destroy all descriptor sets that pointed to this data
        vk_check(swapchain.swapchain_static_descriptor_pool.reset(), "Failed to reset swapchain-static descriptor pool");
        swapchain.built_internal_textures_version = 0;

        for (auto &stage : swapchain.render_stages)
        {
            for (auto &framebuffer : stage.framebuffers)
            {
                // Destroy framebuffers
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }

            for (auto &attachments : stage.attachments)
            {
                // Destroy images
                for (auto &attachment : attachments)
                {
                    allocator.destroy_image(attachment);
                }
            }

            for (auto &texture : stage.output_textures)
            {
                // Destroy samplers
                vkDestroySampler(device, texture.sampler, nullptr);
            }
            stage.output_textures.clear();
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

            // Destroy descriptor sets

            // Destroy render stages
            for (auto &stage : swapchain.render_stages)
            {
                if (stage.indirect_buffer.is_valid())
                {
                    allocator.destroy_buffer(stage.indirect_buffer);
                }
            }

            destroy_swapchain_inner(swapchain);

            // Destroy descriptor pool
            swapchain.swapchain_static_descriptor_pool.clear();

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
        // Increment version
        swapchain.swapchain_version++;

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

        vk_check(vkGetSwapchainImagesKHR(device, swapchain.vk_swapchain, &swapchain.image_count, nullptr));

        // Init stages

        bool swapchain_image_used = false;
        for (size_t stage_i = 0; stage_i < swapchain.render_stages.size(); stage_i++)
        {
            auto       &stage      = swapchain.render_stages[stage_i];
            const auto &stage_desc = render_pipeline_description.stages[stage_i];

            // Init attachment arrays
            stage.attachments = Array<Array<AllocatedImage>>(swapchain.image_count);
            for (size_t i = 0; i < stage.attachments.size(); i++)
            {
                stage.attachments[i] = Array<AllocatedImage>(stage_desc.attachments.size());
            }

            for (size_t attachment_i = 0; attachment_i < stage_desc.attachments.size(); attachment_i++)
            {
                const auto &attachment_desc = stage_desc.attachments[attachment_i];

                // Will be used for window => swapchain image
                if (attachment_desc.final_layout == ImageLayout::PRESENT_SRC)
                {
                    check(!swapchain_image_used, "Window image can only be used one time in a render pipeline.");

                    // Get the swapchain images
                    Array<VkImage> swapchain_images(swapchain.image_count);
                    vk_check(vkGetSwapchainImagesKHR(device, swapchain.vk_swapchain, &swapchain.image_count, swapchain_images.data()));

                    // Create image views for those images
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
                    for (uint32_t image_i = 0; image_i < swapchain.image_count; image_i++)
                    {
                        // Store the image data in the stage
                        stage.attachments[image_i][attachment_i].allocation = VK_NULL_HANDLE;
                        stage.attachments[image_i][attachment_i].image      = swapchain_images[image_i];

                        image_view_create_info.image = swapchain_images[image_i];
                        vk_check(vkCreateImageView(device,
                                                   &image_view_create_info,
                                                   nullptr,
                                                   &stage.attachments[image_i][attachment_i].image_view),
                                 "Failed to create image view for swapchain image");
                    }

                    swapchain_image_used = true;
                }
                // Depth image
                else if (attachment_desc.final_layout == ImageLayout::DEPTH_STENCIL_OPTIMAL)
                {
                    const VkExtent3D depth_image_extent = {extent.width, extent.height, 1};
                    for (uint32_t image_i = 0; image_i < swapchain.image_count; image_i++)
                    {
                        stage.attachments[image_i][attachment_i] =
                            allocator.create_image(convert_format(attachment_desc.format, swapchain.image_format.format),
                                                   depth_image_extent,
                                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                   VK_IMAGE_ASPECT_DEPTH_BIT,
                                                   VMA_MEMORY_USAGE_GPU_ONLY);
                    }
                }
                // Intermediate image
                else if (attachment_desc.final_layout == ImageLayout::SHADER_READ_ONLY_OPTIMAL)
                {
                    const VkExtent3D image_extent = {extent.width, extent.height, 1};
                    for (uint32_t image_i = 0; image_i < swapchain.image_count; image_i++)
                    {
                        stage.attachments[image_i][attachment_i] =
                            allocator.create_image(convert_format(attachment_desc.format, swapchain.image_format.format),
                                                   image_extent,
                                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                                   VMA_MEMORY_USAGE_GPU_ONLY);
                    }

                    // Create a sampler for the image (maybe don't need this many and a global one would be enough ?)
                    VkSamplerCreateInfo sampler_info = {
                        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        // Filter
                        .magFilter  = VK_FILTER_LINEAR,
                        .minFilter  = VK_FILTER_LINEAR,
                        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                        // Address mode
                        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                        .mipLodBias              = 0.0f,
                        .anisotropyEnable        = VK_FALSE,
                        .maxAnisotropy           = 1,
                        .compareEnable           = VK_FALSE,
                        .compareOp               = VK_COMPARE_OP_ALWAYS,
                        .minLod                  = 0.0f,
                        .maxLod                  = 0.0f,
                        .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                        .unnormalizedCoordinates = VK_FALSE,
                    };
                    VkSampler sampler = VK_NULL_HANDLE;
                    vk_check(vkCreateSampler(device, &sampler_info, nullptr, &sampler), "Failed to create sampler");

                    // Store texture
                    stage.output_textures.push_back(AttachmentTexture {
                        .attachment_index = attachment_i,
                        .sampler          = sampler,
                    });
                }
                // Other
                else
                {
                    const VkExtent3D image_extent = {extent.width, extent.height, 1};
                    for (uint32_t image_i = 0; image_i < swapchain.image_count; image_i++)
                    {
                        stage.attachments[image_i][attachment_i] =
                            allocator.create_image(convert_format(attachment_desc.format, swapchain.image_format.format),
                                                   image_extent,
                                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                                   VMA_MEMORY_USAGE_GPU_ONLY);
                    }
                }
            }

            // Init framebuffers

            // Create the final framebuffer array, and a temporary attachment array to store image views for their creation
            Array<VkImageView> attachments_views(stage_desc.attachments.size());
            stage.framebuffers = Array<VkFramebuffer>(swapchain.image_count);

            VkFramebufferCreateInfo framebuffer_create_info {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext           = nullptr,
                .flags           = 0,
                .renderPass      = render_stages[stage_i].vk_render_pass,
                .attachmentCount = static_cast<uint32_t>(attachments_views.size()),
                .pAttachments    = attachments_views.data(),
                .width           = extent.width,
                .height          = extent.height,
                .layers          = 1,
            };

            for (size_t image_i = 0; image_i < swapchain.image_count; image_i++)
            {
                // Get image views for this image index
                for (size_t attachment_i = 0; attachment_i < stage.attachments[image_i].size(); attachment_i++)
                {
                    attachments_views[attachment_i] = stage.attachments[image_i][attachment_i].image_view;
                }

                vk_check(vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &stage.framebuffers[image_i]),
                         "Failed to create framebuffer");
            }
        }
    }

    void Renderer::Data::recreate_swapchain(Swapchain &swapchain, const Extent2D &new_extent)
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

        // Update aspect ratio of cameras
        for (auto &res : cameras)
        {
            auto &camera = res.value();
            if (camera.target_swapchain_index == swapchain.window_index && camera.type == CameraType::PERSPECTIVE)
            {
                camera.specs.as_perspective.aspect_ratio =
                    static_cast<float>(new_extent.width) / static_cast<float>(new_extent.height);
            }
        }
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

    // region Descriptor sets functions

    void Renderer::Data::update_descriptor_sets(FrameData &frame) const
    {
        // If the built version is out of date, rebuild
        // Otherwise, we can just reuse the descriptor sets from the last frame
        if (frame.built_buffers_config_version < buffer_config_version)
        {
            // Reset pool
            vk_check(frame.descriptor_pool.reset());
            frame.swapchain_set = VK_NULL_HANDLE;
            frame.global_set    = VK_NULL_HANDLE;

            vk_check(DescriptorSetBuilder(device, frame.descriptor_pool)
                         // Create descriptor set for all swapchains
                         .add_dynamic_uniform_buffer(frame.camera_info_buffer.buffer, sizeof(GPUCameraData))
                         .save_descriptor_set(swapchain_set_layout, &frame.swapchain_set)
                         // Create descriptor set for global data (for example object matrices)
                         .add_storage_buffer(frame.object_info_buffer.buffer, sizeof(GPUObjectData) * object_data_capacity)
                         .save_descriptor_set(global_set_layout, &frame.global_set)
                         .build(),
                     "Couldn't build descriptor sets.");

            // Update built version
            frame.built_buffers_config_version = buffer_config_version;
        }
    }

    void Renderer::Data::update_render_stages_output_sets(Swapchain &swapchain) const
    {
        // Was the swapchain recreated since the last check ?
        // If so, swapchain images were recreated and descriptor sets deleted, and we
        // need to create them again.
        if (swapchain.built_internal_textures_version < swapchain.swapchain_version)
        {
            // Always ignore last stage for now: textures there will not be considered, because there is no next stage to use them.
            for (size_t stage_i = 0; stage_i < swapchain.render_stages.size() - 1; stage_i++)
            {
                auto &stage = swapchain.render_stages[stage_i];

                // If there are textures in that stage, we need to create descriptor sets
                if (!stage.output_textures.is_empty())
                {
                    // Find descriptor layout in the effect of the next stage (assuming it's a global one)
                    // If it is not a global one, too bad for now.
                    // TODO maybe later: if the next stage uses the material system, which layout to use ?
                    // Or move the layout in the current stage (and leave the effect's one to null)
                    auto effect_id =
                        global_shader_effects.get(static_cast<HashMap::Key>(render_pipeline_description.stages[stage_i + 1].kind));
                    check(effect_id.has_value(),
                          "Global shader effects need to be set for stages that don't use the material system.");
                    auto effect = shader_effects.get(effect_id.take()->as_size);
                    check(effect.has_value(), "");

                    // Create descriptor builder
                    DescriptorSetBuilder builder(device, swapchain.swapchain_static_descriptor_pool);

                    // Create output array if it is not already of the right size
                    // Array will init them to null
                    if (stage.output_textures_set.size() != swapchain.image_count)
                    {
                        stage.output_textures_set = Array<VkDescriptorSet>(swapchain.image_count);
                    }
                    // Otherwise, ensure we have null everywhere
                    else
                    {
                        stage.output_textures_set.fill(VK_NULL_HANDLE);
                    }

                    // For each swapchain image
                    for (size_t image_i = 0; image_i < swapchain.image_count; image_i++)
                    {
                        // For each texture
                        for (size_t texture_i = 0; texture_i < stage.output_textures.size(); texture_i++)
                        {
                            auto &texture = stage.output_textures[texture_i];

                            // Add it to the set
                            builder.add_combined_image_sampler(texture.sampler,
                                                               stage.attachments[image_i][texture.attachment_index].image_view);
                        }
                        // Store the set
                        builder.save_descriptor_set(effect->textures_set_layout, &stage.output_textures_set[image_i]);
                    }

                    // Save everything
                    vk_check(builder.build());
                }
            }

            // Update version to avoid running this again until necessary
            swapchain.built_internal_textures_version = swapchain.swapchain_version;
        }
    }

    // endregion

    // region Effect functions

    void Renderer::Data::build_out_of_date_effects(Swapchain &swapchain)
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
                    swapchain.pipelines.set(effect_id,
                                            HashMap::Value {.as_ptr = build_shader_effect(swapchain.viewport_extent, effect.value())});
                }
            }

            // Update the version
            swapchain.built_effects_version = effects_version;
        }
    }

    VkPipeline Renderer::Data::build_shader_effect(const VkExtent2D &viewport_extent, const ShaderEffect &effect)
    {
        // This function will take the m_data contained in the effect and build a pipeline with it
        // First, create all the structs we will need in the pipeline create info

        // region Create shader stages

        Array<VkPipelineShaderStageCreateInfo> stages(effect.shader_stages.size());
        for (auto i = 0; i < effect.shader_stages.size(); i++)
        {
            // Get shader module
            const auto &module = shader_modules.get(effect.shader_stages[i]);
            check(module.has_value(), "Couldn't get shader module required to build effect.");

            // Convert stages flag
            VkShaderStageFlagBits stage_flags = {};
            switch (module->stage)
            {
                case ShaderStage::VERTEX: stage_flags = VK_SHADER_STAGE_VERTEX_BIT; break;
                case ShaderStage::FRAGMENT: stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT; break;
                default: check(false, "Unknown shader stages");
            }

            // Create shader stages
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

        // region Get render stage

        size_t stage_index = 0;
        bool   stage_found = false;
        // Find the stage that has that stage kind
        // Since it takes the first one, there is no support for multiple stages with the same kind
        for (size_t i = 0; i < render_pipeline_description.stages.size(); i++)
        {
            if (render_pipeline_description.stages[i].kind == effect.render_stage_kind)
            {
                stage_index = i;
                stage_found = true;
                break;
            }
        }
        // Ensure that a render pass was found
        check(stage_found, "Invalid render stage kind: couldn't find related stage. Check your render pipeline.");

        // endregion

        // region Create vertex input state
        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 0,
            .pVertexBindingDescriptions      = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions    = nullptr,
        };

        const bool has_vertex_input = render_pipeline_description.stages[stage_index].uses_material_system;
        if (has_vertex_input)
        {
            const auto vertex_input_description = get_vertex_description();

            vertex_input_state_create_info.flags                           = vertex_input_description.flags;
            vertex_input_state_create_info.vertexBindingDescriptionCount   = vertex_input_description.binding_count;
            vertex_input_state_create_info.pVertexBindingDescriptions      = vertex_input_description.bindings;
            vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_input_description.attribute_count;
            vertex_input_state_create_info.pVertexAttributeDescriptions    = vertex_input_description.attributes;
        }

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
            .offset = {0, 0},
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
            // Keep the primitive in the rasterization stages
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

        auto                                       &stage = render_pipeline_description.stages[stage_index];
        Vector<VkPipelineColorBlendAttachmentState> color_blend_attachments(stage.attachments.size());
        for (uint32_t i = 0; i < stage.attachments.size(); i++)
        {
            // If color attachment
            if (stage.attachments[i].final_layout != ImageLayout::DEPTH_STENCIL_OPTIMAL)
            {
                color_blend_attachments.push_back(VkPipelineColorBlendAttachmentState {
                    .blendEnable = VK_FALSE,
                    .colorWriteMask =
                        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                });
            }
        }

        // No blending
        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
            .pAttachments    = color_blend_attachments.data(),
            .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
        };

        // endregion

        // region Create depth stencil state

        bool                                  do_depth_test = render_pipeline_description.stages[stage_index].do_depth_test;
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .depthTestEnable       = do_depth_test ? VK_TRUE : VK_FALSE,
            .depthWriteEnable      = do_depth_test ? VK_TRUE : VK_FALSE,
            .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
            .depthBoundsTestEnable = false,
            .stencilTestEnable     = false,
            .minDepthBounds        = 0.0f,
            .maxDepthBounds        = 1.0f,
        };

        // endregion

        // region Create pipeline

        VkGraphicsPipelineCreateInfo pipeline_create_info = {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = static_cast<uint32_t>(stages.size()),
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
            .renderPass          = render_stages[stage_index].vk_render_pass,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };
        VkPipeline pipeline = VK_NULL_HANDLE;
        vk_check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline),
                 "Failed to create pipeline");

        // endregion

        // This action has an implication on the draw cache, so we also need to mark it for update
        draw_cache_version++;

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

    void Renderer::Data::recreate_pipelines(Swapchain &swapchain)
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
        if (draw_cache_version > swapchain.built_draw_cache_version)
        {
            // For each stages
            for (size_t stage_i = 0; stage_i < render_pipeline_description.stages.size(); stage_i++)
            {
                auto       &stage      = swapchain.render_stages[stage_i];
                const auto &stage_desc = render_pipeline_description.stages[stage_i];

                // Only build cache for stages that use the material system
                if (stage_desc.uses_material_system)
                {
                    // Clear cache
                    stage.batches.clear();

                    // Find the model_ids using the materials using a template using an effect matching the stages
                    // = we want a list of model_ids, sorted by materials, which are sorted by templates, which are sorted by effects
                    // this will minimize the number of binds to do
                    Vector<ModelId> stage_models(10);

                    // For each effect
                    for (const auto &effect : shader_effects)
                    {
                        // If the effect supports that kind
                        if (effect.value().render_stage_kind == stage_desc.kind)
                        {
                            // Get the pipeline
                            auto pipeline = swapchain.pipelines.get(effect.key());
                            check(pipeline.has_value(), "Tried to draw a shader effect that was not built.");

                            // For each material template
                            for (const auto &mat_template : material_templates)
                            {
                                // If the template has that effect
                                auto effect_i_in_mat = mat_template.value().shader_effects.find_first_of(effect.key());
                                if (effect_i_in_mat.has_value())
                                {
                                    // For each material
                                    // Note: if this becomes to computationally intensive, we could register materials in the template
                                    // like we do with the models
                                    for (const auto &material : materials)
                                    {
                                        // Get the textures' descriptor set for this effect
                                        check(material.value().textures_sets.size() > effect_i_in_mat.value(),
                                              "The used texture is not present in the material.");
                                        VkDescriptorSet textures_set = material.value().textures_sets[effect_i_in_mat.value()];

                                        // If the material has that template and there is at least one model to render
                                        if (material.value().template_id == mat_template.key()
                                            && !material.value().models_using_material.is_empty())
                                        {
                                            // Add a render batch
                                            // Even though we could for now regroup them by shader effect instead of materials, this
                                            // may change when we will add descriptor sets So we split batches by materials However, we
                                            // won't bind a pipeline if it is the same as before, and batches are sorted by effect
                                            stage.batches.push_back(RenderBatch {
                                                stage_models.size(),
                                                material.value().models_using_material.size(),
                                                static_cast<VkPipeline>(pipeline.value()->as_ptr),
                                                effect.value().pipeline_layout,
                                                textures_set,
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
                        const VkBufferUsageFlags indirect_buffer_usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                                                                         | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                                         | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                        const VmaMemoryUsage indirect_buffer_memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
                        const size_t required_indirect_buffer_size        = stage_models.size() * sizeof(VkDrawIndexedIndirectCommand);

                        // If it does not exist, create it
                        if (stage.indirect_buffer.buffer == VK_NULL_HANDLE)
                        {
                            stage.indirect_buffer = allocator.create_buffer(required_indirect_buffer_size,
                                                                            indirect_buffer_usage,
                                                                            indirect_buffer_memory_usage);
                        }
                        // If it exists but isn't big enough, recreate it
                        else if (stage.indirect_buffer.size < required_indirect_buffer_size)
                        {
                            allocator.destroy_buffer(stage.indirect_buffer);
                            stage.indirect_buffer = allocator.create_buffer(required_indirect_buffer_size,
                                                                            indirect_buffer_usage,
                                                                            indirect_buffer_memory_usage);
                        }

                        // At this point, we have an indirect buffer big enough to hold the commands we want to register

                        // Register commands
                        auto *indirect_commands =
                            static_cast<VkDrawIndexedIndirectCommand *>(allocator.map_buffer(stage.indirect_buffer));

                        for (auto i = 0; i < stage_models.size(); ++i)
                        {
                            // Get model
                            const auto model_id  = stage_models[i];
                            const auto model_res = models.get(model_id);
                            check(model_res.has_value(), "Tried to draw a model that doesn't exist.");
                            const auto &model = model_res.value();

                            // Get mesh
                            const auto mesh_res = mesh_parts.get(model.mesh_part_id);
                            check(mesh_res.has_value(), "Tried to draw a mesh part that doesn't exist.");
                            const auto &part = mesh_res.value();
                            check(part.is_uploaded, "Tried to draw a mesh part that hasn't been uploaded.");

                            indirect_commands[i].vertexOffset  = static_cast<int32_t>(part.vertex_offset);
                            indirect_commands[i].indexCount    = part.mesh_part.triangle_count() * 3;
                            indirect_commands[i].firstIndex    = part.index_offset;
                            indirect_commands[i].instanceCount = 1; // TODO when instances are added
                            indirect_commands[i].firstInstance = 0;
                        }

                        allocator.unmap_buffer(stage.indirect_buffer);
                    }
                }
            }

            // The cache is now up-to-date
            swapchain.built_draw_cache_version = draw_cache_version;
        }
    }
    void Renderer::Data::draw_from_cache(const RenderStageInstance &stage,
                                         VkCommandBuffer            cmd,
                                         FrameData                 &current_frame,
                                         size_t                     window_index) const
    {
        constexpr uint32_t draw_stride        = sizeof(VkDrawIndexedIndirectCommand);
        VkPipeline         bound_pipeline     = VK_NULL_HANDLE;
        bool               global_sets_bound  = false;
        VkDescriptorSet    bound_textures_set = VK_NULL_HANDLE;

        if (stage.batches.is_empty())
        {
            // Nothing to draw
            return;
        }

        // For each batch
        for (const auto &batch : stage.batches)
        {
            // If the pipeline is different from the last one, bind it
            if (bound_pipeline != batch.pipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, batch.pipeline);
                bound_pipeline = batch.pipeline;
            }

            // The first iteration, bind global sets
            if (!global_sets_bound)
            {
                uint32_t                     camera_buffer_offset = pad_uniform_buffer_size(sizeof(GPUCameraData)) * window_index;
                const Array<uint32_t>        offsets              = {camera_buffer_offset};
                const Array<VkDescriptorSet> sets                 = {current_frame.swapchain_set, current_frame.global_set};
                vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        batch.pipeline_layout,
                                        0,
                                        sets.size(),
                                        sets.data(),
                                        offsets.size(),
                                        offsets.data());
                global_sets_bound = true;
            }

            // Rebind descriptor set if it is different
            if (bound_textures_set != batch.textures_set && batch.textures_set != VK_NULL_HANDLE)
            {
                vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        batch.pipeline_layout,
                                        2, // We have two global sets before, so this one is the third
                                        1,
                                        &batch.textures_set,
                                        0,
                                        nullptr);
                bound_textures_set = batch.textures_set;
            }

            // Draw the batch
            const uint32_t &&draw_offset = draw_stride * batch.offset;

            vkCmdDrawIndexedIndirect(cmd, stage.indirect_buffer.buffer, draw_offset, batch.count, draw_stride);
        }
    }

    void Renderer::Data::draw_quad(const Swapchain &swapchain,
                                   size_t           stage_index,
                                   size_t           image_index,
                                   VkCommandBuffer  cmd,
                                   FrameData       &current_frame,
                                   size_t           window_index) const
    {
        auto &stage_desc = render_pipeline_description.stages[stage_index];

        // Get global effect id for current stage
        auto id_result = global_shader_effects.get(static_cast<HashMap::Key>(stage_desc.kind));
        check(id_result.has_value() && id_result.value()->as_size != NULL_ID,
              "Missing global shader effect for stage \"" + std::string(stage_desc.name) + "\"");
        ShaderEffectId id = id_result.value()->as_size;

        // Using the id, find the effect
        auto effect_result = shader_effects.get(id);
        check(effect_result.has_value(),
              "Invalid global shader effect for stage \"" + std::string(stage_desc.name)
                  + "\". The stored id doesn't belong to any existing shader effect.");
        auto &effect = effect_result.value();

        // Bind global sets
        uint32_t                     camera_buffer_offset = pad_uniform_buffer_size(sizeof(GPUCameraData)) * window_index;
        const Array<uint32_t>        offsets              = {camera_buffer_offset};
        const Array<VkDescriptorSet> sets                 = {current_frame.swapchain_set, current_frame.global_set};
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                effect.pipeline_layout,
                                0,
                                sets.size(),
                                sets.data(),
                                offsets.size(),
                                offsets.data());

        // Bind attachment set if needed (to access output attachments of the previous stage, for example for lighting stage in
        // deferred rendering).
        if (stage_index != 0)
        {
            auto &previous_stage = swapchain.render_stages[stage_index - 1];
            if (!previous_stage.output_textures.is_empty())
            {
                VkDescriptorSet attachments_set = previous_stage.output_textures_set[image_index];
                vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        effect.pipeline_layout,
                                        2,
                                        1,
                                        &attachments_set,
                                        0,
                                        nullptr);
            }
        }

        // Get the pipeline
        auto pipeline = swapchain.pipelines.get(id);
        check(pipeline.has_value(), "Tried to draw a shader effect that was not built.");

        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VkPipeline>(pipeline.value()->as_ptr));

        // Draw
        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    // endregion

    // region Camera functions

    void Renderer::Data::send_camera_data(size_t window_index, const Camera &camera, FrameData &current_frame)
    {
        // Get camera infos and send them to the shader
        GPUCameraData camera_data = {};
        Swapchain    &swapchain   = swapchains[window_index];

        // Projection
        switch (camera.type)
        {
            case CameraType::PERSPECTIVE:
                camera_data.projection = glm::perspective(camera.specs.as_perspective.fov,
                                                          camera.specs.as_perspective.aspect_ratio,
                                                          camera.specs.as_perspective.near_plane,
                                                          camera.specs.as_perspective.far_plane);
                camera_data.projection[1][1] *= -1;
                break;
            case CameraType::ORTHOGRAPHIC:
                camera_data.projection = glm::ortho(-camera.specs.as_orthographic.width / 2.0f,
                                                    camera.specs.as_orthographic.width / 2.0f,
                                                    -camera.specs.as_orthographic.height / 2.0f,
                                                    camera.specs.as_orthographic.height / 2.0f,
                                                    camera.specs.as_orthographic.near_plane,
                                                    camera.specs.as_orthographic.far_plane);
                break;
        }

        // View
        camera_data.view = camera.transform.view_matrix();
        // View projection
        camera_data.view_projection = camera_data.projection * camera_data.view;

        // Copy it to buffer
        copy_buffer_to_gpu(camera_data, current_frame.camera_info_buffer, window_index);
    }
    // endregion

    // region Mesh part functions

    VertexInputDescription Renderer::Data::get_vertex_description()
    {
        static constexpr VkVertexInputBindingDescription bindings[1] = {
            VkVertexInputBindingDescription {
                .binding   = 0,
                .stride    = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
        };

        static const VkVertexInputAttributeDescription attributes[3] = {
            // Vertex position attribute: location 0
            VkVertexInputAttributeDescription {
                .location = 0,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32_SFLOAT,
                .offset   = static_cast<uint32_t>(offsetof(Vertex, position)),
            },
            // Vertex normal attribute: location 1
            VkVertexInputAttributeDescription {
                .location = 1,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32_SFLOAT,
                .offset   = static_cast<uint32_t>(offsetof(Vertex, normal)),
            },
            // Vertex color coordinates attribute: location 2
            VkVertexInputAttributeDescription {
                .location = 2,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = static_cast<uint32_t>(offsetof(Vertex, tex_coord)),
            },
        };

        static constexpr VertexInputDescription description {
            .flags           = 0,
            .binding_count   = 1,
            .bindings        = bindings,
            .attribute_count = 3,
            .attributes      = attributes,
        };

        return description;
    }

    void Renderer::Data::update_mesh_buffers()
    {
        // For now, keep it simple: we create a buffer big enough for every mesh in the storage and send it.
        // Later, we can try to introduce more local updates to the existing buffer (for example define it like a gpu vector) TODO

        if (should_update_mesh_buffers)
        {
            // Create transfer commands
            TransferCommand vb_transfer_command = create_transfer_command(transfer_context.transfer_pool);
            TransferCommand ib_transfer_command = create_transfer_command(transfer_context.transfer_pool);

            constexpr size_t vertex_size   = MeshPart::vertex_byte_size();
            constexpr size_t triangle_size = MeshPart::triangle_byte_size();
            constexpr size_t index_size    = MeshPart::index_byte_size();

            // Determine the size of all meshes
            size_t total_vb_size = 0;
            size_t total_ib_size = 0;

            for (auto &res : mesh_parts)
            {
                const auto &part = res.value();

                total_vb_size += vertex_size * part.mesh_part.vertex_count();
                total_ib_size += triangle_size * part.mesh_part.triangle_count();
            }

            // region Create GPU-side buffers

            // Vertex buffer
            auto vb_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if (!vertex_buffer.is_valid())
            {
                // Doesn't exist yet, create it
                vertex_buffer = allocator.create_buffer(total_vb_size, vb_usage, VMA_MEMORY_USAGE_GPU_ONLY, true);
            }
            else if (vertex_buffer.size < total_vb_size)
            {
                // Exists but too small, reallocate it
                allocator.destroy_buffer(vertex_buffer);
                vertex_buffer = allocator.create_buffer(total_vb_size, vb_usage, VMA_MEMORY_USAGE_GPU_ONLY, true);
            }
            // Index buffer
            auto ib_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            if (!index_buffer.is_valid())
            {
                // Doesn't exist yet, create it
                index_buffer = allocator.create_buffer(total_ib_size, ib_usage, VMA_MEMORY_USAGE_GPU_ONLY, true);
            }
            else if (index_buffer.size < total_ib_size)
            {
                // Exists but too small, reallocate it
                allocator.destroy_buffer(index_buffer);
                index_buffer = allocator.create_buffer(total_ib_size, ib_usage, VMA_MEMORY_USAGE_GPU_ONLY, true);
            }

            // endregion

            // region Setup transfer buffers

            vb_transfer_command.staging_buffer =
                allocator.create_buffer(total_vb_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            ib_transfer_command.staging_buffer =
                allocator.create_buffer(total_ib_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            // Copy data to the buffers
            size_t vb_offset = 0;
            size_t ib_offset = 0;

            // Map buffers
            auto vb = static_cast<Vertex *>(allocator.map_buffer(vb_transfer_command.staging_buffer));
            auto ib = static_cast<Triangle *>(allocator.map_buffer(ib_transfer_command.staging_buffer));

            for (auto &res : mesh_parts)
            {
                auto &part = res.value();

                // Copy vertex data
                part.vertex_offset = vb_offset;
                for (const auto &vertex : part.mesh_part.vertices())
                {
                    vb[vb_offset] = vertex;
                    vb_offset++;
                }

                // Copy index data
                part.index_offset = ib_offset;
                for (const auto &triangle : part.mesh_part.triangles())
                {
                    ib[ib_offset] = triangle;
                    ib_offset++;
                }

                part.is_uploaded = true;
            }

            // Unmap buffers
            allocator.unmap_buffer(vb_transfer_command.staging_buffer);
            allocator.unmap_buffer(ib_transfer_command.staging_buffer);

            // endregion

            // region Copy data to GPU

            // Vertex buffer
            vb_transfer_command.begin();
            VkBufferCopy vb_copy {
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = total_vb_size,
            };
            vkCmdCopyBuffer(vb_transfer_command.command_buffer,
                            vb_transfer_command.staging_buffer.buffer,
                            vertex_buffer.buffer,
                            1,
                            &vb_copy);
            vb_transfer_command.end_and_submit(transfer_queue.queue);

            // Index buffer
            ib_transfer_command.begin();
            VkBufferCopy ib_copy {
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = total_ib_size,
            };
            vkCmdCopyBuffer(ib_transfer_command.command_buffer,
                            ib_transfer_command.staging_buffer.buffer,
                            index_buffer.buffer,
                            1,
                            &ib_copy);
            ib_transfer_command.end_and_submit(transfer_queue.queue);

            // endregion

            // Store the commands
            transfer_context.commands.push_back(vb_transfer_command);
            transfer_context.commands.push_back(ib_transfer_command);

            // Mesh buffers will now be up-to-date
            should_update_mesh_buffers = false;

            // Though, we need to update draw cache because the buffers might have changed
            draw_cache_version++;
        }
    }

    // endregion

    // region Storage buffers functions

    void Renderer::Data::update_storage_buffers(FrameData &frame)
    {
        // region Object data

        // We do it each frame because we want to be able to move objects around

        // We need to store a GPUObjectData for each model
        // TODO, later, it will be for each render node

        // If the capacity is too small, we need to increase it
        if (object_data_capacity < models.count())
        {
            // For now, we take a fixed margin above the required amount to avoid frequent reallocation's
            // In the future, maybe a vector approach would be better (i.e. double the growth amount each time)
            object_data_capacity = models.count() + 50;

            // We also need to update the config version, so that the descriptor sets will be updated
            buffer_config_version++;

            const auto required_size = sizeof(GPUObjectData) * object_data_capacity;
            if (!frame.object_info_buffer.is_valid())
            {
                // Doesn't exist yet, create it
                frame.object_info_buffer =
                    allocator.create_buffer(required_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            }
            else if (frame.object_info_buffer.size < required_size)
            {
                // Exists but too small, reallocate it
                allocator.destroy_buffer(frame.object_info_buffer);
                frame.object_info_buffer =
                    allocator.create_buffer(required_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            }
        }
        // Copy model data to the GPU
        if (frame.object_info_buffer.is_valid())
        {
            auto buf = static_cast<GPUObjectData *>(allocator.map_buffer(frame.object_info_buffer));

            auto i = 0;
            for (const auto &model : models)
            {
                buf[i] = GPUObjectData {
                    .transform = model.value().transform.view_matrix(),
                };
                i++;
            }

            allocator.unmap_buffer(frame.object_info_buffer);
        }

        // endregion
    }

    // endregion

    // region Transfer functions

    void Renderer::Data::reset_transfer_context()
    {
        // Nothing to reset
        if (transfer_context.commands.is_empty())
        {
            return;
        }

        // Wait for the transfer queue to finish
        Array<VkFence> fences {transfer_context.commands.size()};
        for (size_t i = 0; i < transfer_context.commands.size(); i++)
        {
            fences[i] = transfer_context.commands[i].fence;
        }
        vkWaitForFences(device, fences.size(), fences.data(), VK_TRUE, WAIT_FOR_FENCES_TIMEOUT);

        for (auto &command : transfer_context.commands)
        {
            // Destroy the staging buffers
            if (command.staging_buffer.is_valid())
            {
                allocator.destroy_buffer(command.staging_buffer);
            }
            // Destroy fence
            vkDestroyFence(device, command.fence, nullptr);
        }

        // Reset command pool
        vkResetCommandPool(device, transfer_context.transfer_pool, 0);
        vkResetCommandPool(device, transfer_context.graphics_pool, 0);

        // Clear the commands
        transfer_context.commands.clear();
    }

    TransferCommand Renderer::Data::create_transfer_command(VkCommandPool pool) const
    {
        const VkCommandBufferAllocateInfo cmd_allocate_info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = pool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        constexpr VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = 0,
        };
        TransferCommand command = {};
        vk_check(vkAllocateCommandBuffers(device, &cmd_allocate_info, &command.command_buffer));
        vk_check(vkCreateFence(device, &fence_create_info, nullptr, &command.fence));
        return command;
    }

    void TransferCommand::begin() const
    {
        constexpr VkCommandBufferBeginInfo begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };
        vkBeginCommandBuffer(command_buffer, &begin_info);
    }

    void TransferCommand::end_and_submit(VkQueue queue)
    {
        vk_check(vkEndCommandBuffer(command_buffer));

        // Submit commands
        const VkSubmitInfo submit_info = {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &command_buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr,
        };
        vk_check(vkQueueSubmit(queue, 1, &submit_info, fence));
    }

    // endregion

    // ---==== Renderer ====---

    // region Base renderer functions

    Renderer::Renderer(const Window               &example_window,
                       const char                 *application_name,
                       const Version              &application_version,
                       uint32_t                    window_capacity,
                       RenderPipelineDescription &&render_pipeline_description)
        : m_data(new Data)
    {
        std::cout << "Using Vulkan backend, version " << VK_API_VERSION_MAJOR(VULKAN_API_VERSION) << "."
                  << VK_API_VERSION_MINOR(VULKAN_API_VERSION) << "\n";

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
            auto extra_ext_index = required_extensions.size() - extra_extension_count;
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
                .enabledExtensionCount   = static_cast<uint32_t>(required_extensions.size()),
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
            bool found_graphics_queue         = false;
            bool found_transfer_queue         = false;
            bool found_optimal_transfer_queue = false;

            for (uint32_t i = 0; i < queue_family_properties_count; i++)
            {
                const auto &family_properties = queue_family_properties[i];

                // Save the graphics queue family_index
                if (!found_graphics_queue && family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    m_data->graphics_queue.family_index = i;
                    found_graphics_queue                = true;
                }

                // Save the transfer queue family_index
                if (family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
                {
                    // If it's not the same as the graphics queue, we can use it
                    if (!(family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    {
                        m_data->transfer_queue.family_index = i;
                        found_transfer_queue                = true;
                        found_optimal_transfer_queue        = true;
                    }
                    // It's the same as the graphics one, but we don't have one yet, so we'll take it
                    // But we'll keep looking for another one
                    else if (!found_transfer_queue)
                    {
                        m_data->transfer_queue.family_index = i;
                        found_transfer_queue                = true;
                    }
                }

                // Stop searching if we found everything we need
                if (found_graphics_queue && found_optimal_transfer_queue)
                {
                    break;
                }
            }

            // If we didn't find a graphics queue, we can't continue
            check(found_graphics_queue, "Unable to find a graphics queue family_index.");

            // If we didn't find a transfer queue, we can't continue
            check(found_transfer_queue, "Unable to find a transfer queue family_index.");

            // Get GPU properties
            vkGetPhysicalDeviceProperties(m_data->physical_device, &m_data->device_properties);
        }

        // endregion

        // --=== Logical device and queues creation ===--

        // region Device and queues creation

        {
            Vector<VkDeviceQueueCreateInfo> queue_create_infos(2);

            // Define the parameters for the graphics queue
            Vector<float> priorities(2);
            priorities.push_back(1.0f);

            // Add the transfer queue if it is the same as the graphics one
            if (m_data->graphics_queue.family_index == m_data->transfer_queue.family_index)
            {
                priorities.push_back(0.7f);
            }
            queue_create_infos.push_back(VkDeviceQueueCreateInfo {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                // Queue info
                .queueFamilyIndex = m_data->graphics_queue.family_index,
                .queueCount       = static_cast<uint32_t>(priorities.size()),
                .pQueuePriorities = priorities.data(),
            });

            // Define the parameters for the transfer queue
            float transfer_queue_priority = 1.0f;
            if (m_data->graphics_queue.family_index != m_data->transfer_queue.family_index)
            {
                queue_create_infos.push_back(VkDeviceQueueCreateInfo {
                    // Struct infos
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = nullptr,
                    // Queue info
                    .queueFamilyIndex = m_data->transfer_queue.family_index,
                    .queueCount       = 1,
                    .pQueuePriorities = &transfer_queue_priority,
                });
            }

            Array<const char *> required_device_extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            };

            // Create the logical device
            VkDeviceCreateInfo device_create_info = {
                // Struct infos
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = nullptr,
                // Queue infos
                .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
                .pQueueCreateInfos    = queue_create_infos.data(),
                // Layers
                .enabledLayerCount   = 0,
                .ppEnabledLayerNames = nullptr,
                // Extensions
                .enabledExtensionCount   = static_cast<uint32_t>(required_device_extensions.size()),
                .ppEnabledExtensionNames = required_device_extensions.data(),
                .pEnabledFeatures        = nullptr,
            };
            vk_check(vkCreateDevice(m_data->physical_device, &device_create_info, nullptr, &m_data->device),
                     "Couldn't create logical device.");

            // Load device in volk
            volkLoadDevice(m_data->device);

            // Get created queues
            vkGetDeviceQueue(m_data->device, m_data->graphics_queue.family_index, 0, &m_data->graphics_queue.queue);
            // Get the transfer queue. If it is the same as the graphics one, it will be a second queue on the same family
            if (m_data->graphics_queue.family_index == m_data->transfer_queue.family_index)
            {
                vkGetDeviceQueue(m_data->device, m_data->transfer_queue.family_index, 1, &m_data->transfer_queue.queue);
            }
            // Otherwise, it is in a different family, so the index is 0
            else
            {
                vkGetDeviceQueue(m_data->device, m_data->transfer_queue.family_index, 0, &m_data->transfer_queue.queue);
            }
        }

        // endregion

        // --=== Allocator ===--

        m_data->allocator = std::move(Allocator(m_data->instance,
                                                m_data->device,
                                                m_data->physical_device,
                                                m_data->graphics_queue.family_index,
                                                m_data->transfer_queue.family_index));

        // --=== Swapchains ===--

        // We need to create an array big enough to hold all the swapchains.
        m_data->swapchains         = Array<Swapchain>(window_capacity);
        m_data->swapchain_capacity = window_capacity;

        // --=== Render stages ===--

        // region Render stages initialization

        {
            // Store pipeline description
            // The description stores every setting we need to create render passes
            m_data->render_pipeline_description = std::move(render_pipeline_description);
            const auto &pipeline_desc           = m_data->render_pipeline_description;

            // Choose a swapchain_image_format for the given example window
            // For now we will assume that all swapchain will use that swapchain_image_format
            VkSurfaceKHR       example_surface        = example_window.get_vulkan_surface(m_data->instance);
            VkSurfaceFormatKHR swapchain_image_format = m_data->select_surface_format(example_surface);
            // Destroy the surface - this was just an example window, we will create a new one later
            vkDestroySurfaceKHR(m_data->instance, example_surface, nullptr);

            // Init the array that will store the render passes
            m_data->render_stages = Array<RenderStage>(pipeline_desc.stages.size());

            for (uint32_t i = 0; i < m_data->render_stages.size(); i++)
            {
                const auto &stage_desc = pipeline_desc.stages[i];

                m_data->render_stages[i].kind = stage_desc.kind;

                // Create render pass based on the pipeline description
                Array<VkAttachmentDescription> attachments(stage_desc.attachments.size());
                // If we find a depth attachment, we will store it in the dedicated reference.
                Vector<VkAttachmentReference> color_references(attachments.size());
                bool                          depth_reference_set = false;
                VkAttachmentReference         depth_reference     = {};

                for (uint32_t att_i = 0; att_i < stage_desc.attachments.size(); att_i++)
                {
                    // Create refs to have cleaner code
                    auto &att      = attachments[att_i];
                    auto &att_desc = stage_desc.attachments[att_i];

                    // Use desc to create vk attachment
                    att.format = convert_format(att_desc.format, swapchain_image_format.format);
                    // No MSAA
                    att.samples = VK_SAMPLE_COUNT_1_BIT;
                    // Operators
                    att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
                    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
                    att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    // Layouts
                    att.initialLayout = convert_layout(att_desc.initial_layout);
                    att.finalLayout   = convert_layout(att_desc.final_layout);

                    // Create a reference for the attachment
                    bool                  is_depth_stencil_attachment = att_desc.final_layout == ImageLayout::DEPTH_STENCIL_OPTIMAL;
                    VkAttachmentReference reference {
                        .attachment = att_i,
                        .layout     = is_depth_stencil_attachment ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                                                  : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    };

                    // Store the reference
                    if (is_depth_stencil_attachment)
                    {
                        check(!depth_reference_set,
                              "There cannot be more than one depth stencil attachment reference in a render stage.");

                        depth_reference     = reference;
                        depth_reference_set = true;
                    }
                    else
                    {
                        color_references.push_back(reference);
                    }
                }

                // Create subpass and render pass
                auto subpass_description = VkSubpassDescription {
                    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                    // Color attachments
                    .colorAttachmentCount = static_cast<uint32_t>(color_references.size()),
                    .pColorAttachments    = color_references.data(),
                    // Depth attachment
                    .pDepthStencilAttachment = depth_reference_set ? &depth_reference : VK_NULL_HANDLE,
                };
                auto render_pass_create_info = VkRenderPassCreateInfo {
                    .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                    .pNext           = VK_NULL_HANDLE,
                    .attachmentCount = static_cast<uint32_t>(attachments.size()),
                    .pAttachments    = attachments.data(),
                    .subpassCount    = 1,
                    .pSubpasses      = &subpass_description,
                };
                vk_check(
                    vkCreateRenderPass(m_data->device, &render_pass_create_info, nullptr, &m_data->render_stages[i].vk_render_pass),
                    "Couldn't create \"" + std::string(stage_desc.name) + "\" (" + std::to_string(i) + ") render pass");
            }
        }
        // endregion

        // --=== Init global sets ===--

        // Init sets
        DescriptorSetLayoutBuilder(m_data->device)
            // Camera buffer
            .add_dynamic_uniform_buffer(VK_SHADER_STAGE_VERTEX_BIT)
            .save_descriptor_set_layout(&m_data->swapchain_set_layout)
            // Object data
            .add_storage_buffer(VK_SHADER_STAGE_VERTEX_BIT)
            .save_descriptor_set_layout(&m_data->global_set_layout);
        m_data->buffer_config_version++;

        // Create static descriptor pool
        m_data->static_descriptor_pool = DynamicDescriptorPool(m_data->device,
                                                               DescriptorBalance {
                                                                   0,
                                                                   0,
                                                                   0,
                                                                   // Lots of capacity for textures
                                                                   100,
                                                               });

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

                // Create buffers
                frame.camera_info_buffer = m_data->allocator.create_buffer(m_data->pad_uniform_buffer_size(sizeof(GPUCameraData))
                                                                               * m_data->swapchain_capacity,
                                                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                           VMA_MEMORY_USAGE_CPU_TO_GPU);
                frame.object_info_buffer = m_data->allocator.create_buffer(sizeof(GPUObjectData) * m_data->object_data_capacity,
                                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                           VMA_MEMORY_USAGE_CPU_TO_GPU);

                // Create descriptor pool
                frame.descriptor_pool = DynamicDescriptorPool(m_data->device,
                                                              DescriptorBalance {
                                                                  4,
                                                                  0,
                                                                  2,
                                                                  0,
                                                              });
            }
        }
        // endregion

        // --=== Init transfer context ===--

        // Create command pool for the do_transfer queue
        VkCommandPoolCreateInfo command_pool_create_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = VK_NULL_HANDLE,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_data->transfer_queue.family_index,
        };
        vk_check(vkCreateCommandPool(m_data->device, &command_pool_create_info, nullptr, &m_data->transfer_context.transfer_pool));
        command_pool_create_info.queueFamilyIndex = m_data->graphics_queue.family_index;
        vk_check(vkCreateCommandPool(m_data->device, &command_pool_create_info, nullptr, &m_data->transfer_context.graphics_pool));
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

        // Destroy do_transfer context
        m_data->reset_transfer_context();
        vkDestroyCommandPool(m_data->device, m_data->transfer_context.transfer_pool, nullptr);
        vkDestroyCommandPool(m_data->device, m_data->transfer_context.graphics_pool, nullptr);

        // Destroy vertex and index buffers
        if (m_data->vertex_buffer.is_valid())
        {
            m_data->allocator.destroy_buffer(m_data->vertex_buffer);
        }

        if (m_data->index_buffer.is_valid())
        {
            m_data->allocator.destroy_buffer(m_data->index_buffer);
        }

        // Destroy descriptor layouts
        vkDestroyDescriptorSetLayout(m_data->device, m_data->swapchain_set_layout, nullptr);
        vkDestroyDescriptorSetLayout(m_data->device, m_data->global_set_layout, nullptr);

        // Destroy pool
        m_data->static_descriptor_pool.clear();

        // Clear frames
        for (auto &frame : m_data->frames)
        {
            // Destroy pool
            frame.descriptor_pool.clear();
            // Destroy buffers
            m_data->allocator.destroy_buffer(frame.camera_info_buffer);
            m_data->allocator.destroy_buffer(frame.object_info_buffer);

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
        clear_mesh_parts();
        clear_textures();
        clear_materials();
        clear_material_templates();
        clear_shader_effects();
        clear_shader_modules();

        // Clear swapchains
        m_data->clear_swapchains();

        // Destroy render passes
        for (auto &stage : m_data->render_stages)
        {
            vkDestroyRenderPass(m_data->device, stage.vk_render_pass, nullptr);
            stage.vk_render_pass = VK_NULL_HANDLE;
        }

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
        check(window_slot_index < m_data->swapchains.size(), "Window index is out of bounds");

        // Get the swapchain in the renderer
        Swapchain &swapchain = m_data->swapchains[window_slot_index];

        // Ensure that there is not a live swapchain here already
        check(!swapchain.enabled,
              "Attempted to create a swapchain in a slot where there was already an active one."
              " To recreate a swapchain, see rg_renderer_recreate_swapchain.");

        // Reset versions
        swapchain.swapchain_version     = 0;
        swapchain.built_effects_version = 0;

        // region Window & Surface

        // Get the window's surface
        swapchain.target_window = &window;
        swapchain.surface       = window.get_vulkan_surface(m_data->instance);
        swapchain.window_index  = window_slot_index;

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
            for (const auto &desired_mode : desired_modes)
            {
                for (const auto &available_mode : available_present_modes)
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

        // region Image size selection

        VkSurfaceCapabilitiesKHR surface_capabilities = {};
        vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_data->physical_device, swapchain.surface, &surface_capabilities));

        // For the image size, take the minimum plus one. Or, if the minimum is equal to the maximum, take that value.
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

        // Init render stage instances
        // Using a dynamic array allows us to define a parameter to this function to define the stages
        swapchain.render_stages = Array<RenderStageInstance>(m_data->render_pipeline_description.stages.size());

        // Init descriptor pool for "swapchain-lived" sets
        swapchain.swapchain_static_descriptor_pool = DynamicDescriptorPool(m_data->device, {.combined_image_sampler_count = 10});

        m_data->init_swapchain_inner(swapchain, extent);

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

    // endregion

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

    ShaderEffectId Renderer::create_shader_effect(const Array<ShaderModuleId> &stages,
                                                  RenderStageKind              render_stage_kind,
                                                  const Array<TextureLayout>  &textures)
    {
        check(stages.size() > 0, "A shader effect must have at least one stages.");

        // Create shader effect
        ShaderEffect effect {render_stage_kind, stages, VK_NULL_HANDLE, VK_NULL_HANDLE};

        // Get descriptor sets for the pipeline
        Vector<VkDescriptorSetLayout> descriptor_set_layouts {3};
        descriptor_set_layouts.push_back(m_data->swapchain_set_layout);
        descriptor_set_layouts.push_back(m_data->global_set_layout);

        // Create texture set layout
        if (!textures.is_empty())
        {
            DescriptorSetLayoutBuilder builder(m_data->device);
            for (auto &texture_layout : textures)
            {
                // Convert stages to Vk stages
                VkShaderStageFlags textureStages = convert_shader_stages(texture_layout.stages);

                // Add binding
                // For now, we assume a combined image sampler binding
                builder.add_combined_image_sampler(textureStages);
            }
            // Save the layout
            builder.save_descriptor_set_layout(&effect.textures_set_layout);

            // Add the layout to the list
            descriptor_set_layouts.push_back(effect.textures_set_layout);
        }

        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = static_cast<uint32_t>(descriptor_set_layouts.size()),
            .pSetLayouts            = descriptor_set_layouts.data(),
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
            if (res.value().textures_set_layout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(m_data->device, res.value().textures_set_layout, nullptr);
            }

            m_data->shader_modules.remove(id);
        }
        // No value = already deleted, in a sense. This is not an error since the contract is respected
    }
    void Renderer::clear_shader_effects()
    {
        for (auto &effect : m_data->shader_effects)
        {
            vkDestroyPipelineLayout(m_data->device, effect.value().pipeline_layout, nullptr);
            if (effect.value().textures_set_layout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(m_data->device, effect.value().textures_set_layout, nullptr);
            }
        }
        m_data->shader_effects.clear();
    }

    void Renderer::set_global_shader_effect(RenderStageKind stage_kind, ShaderEffectId effect_id)
    {
        // Override the old value
        m_data->global_shader_effects.set(static_cast<HashMap::Key>(stage_kind), HashMap::Value {.as_size = effect_id});
    }

    // endregion

    // region Material template functions

    MaterialTemplateId Renderer::create_material_template(const Array<ShaderEffectId> &available_effects)
    {
        check(available_effects.size() > 0, "A material template must have at least one effect.");

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

    MaterialId Renderer::create_material(MaterialTemplateId material_template, const Array<Array<TextureId>> &textures)
    {
        auto textures_copy = textures;
        return create_material(material_template, std::move(textures_copy));
    }

    MaterialId Renderer::create_material(MaterialTemplateId material_template, Array<Array<TextureId>> &&textures)
    {
        check(material_template != NULL_ID, "A material must have a template.");

        // Get layout
        auto mat_template = m_data->material_templates[material_template];

        Array<VkDescriptorSet> descriptor_sets {textures.size()};
        DescriptorSetBuilder   builder(m_data->device, m_data->static_descriptor_pool);

        // For each supported effect that has textures
        for (size_t i = 0; i < textures.size(); i++)
        {
            auto effect_texture_ids = textures[i];

            if (!effect_texture_ids.is_empty())
            {
                auto shader_effect = m_data->shader_effects[mat_template.shader_effects[i]];

                // Create the descriptor set
                for (auto tex_id : effect_texture_ids)
                {
                    auto texture = m_data->textures[tex_id];
                    builder.add_combined_image_sampler(texture.sampler, texture.image.image_view);
                }
                builder.save_descriptor_set(shader_effect.textures_set_layout, &descriptor_sets[i]);
            }
            else
            {
                // No textures, set to null
                descriptor_sets[i] = VK_NULL_HANDLE;
            }
        }

        vk_check(builder.build());

        // Create material
        return m_data->materials.push({
            material_template,
            Vector<ModelId>(10),
            std::move(textures),
            std::move(descriptor_sets),
        });
    }

    void Renderer::destroy_material(MaterialId id)
    {
        // There is no special vulkan handle to destroy in the material, so we just let the storage do its job
        // The descriptor set will be freed when resetting the pool
        m_data->materials.remove(id);
    }

    void Renderer::clear_materials()
    {
        // Same here
        m_data->materials.clear();
    }

    // endregion

    // region Mesh parts

    MeshPartId Renderer::save_mesh_part(MeshPart &&mesh_part)
    {
        // New buffers to store
        m_data->should_update_mesh_buffers = true;

        // Store it in the storage
        return m_data->mesh_parts.push(StoredMeshPart {
            std::move(mesh_part),
        });
    }

    void Renderer::destroy_mesh_part(MeshPartId id)
    {
        // Maybe do not update buffers and let it there, and use a smarter way to update them
        m_data->should_update_mesh_buffers = true;

        // There is no special vulkan handle to destroy in the mesh part, so we just let the storage do its job
        m_data->mesh_parts.remove(id);
    }

    void Renderer::clear_mesh_parts()
    {
        // Same here
        m_data->should_update_mesh_buffers = true;

        m_data->mesh_parts.clear();
    }

    // endregion

    // region Model functions

    ModelId Renderer::create_model(MeshPartId mesh_part, MaterialId material)
    {
        check(mesh_part != NULL_ID, "A model must have a mesh part.");
        check(material != NULL_ID, "A model must have a material.");

        // Create model
        auto model_id = m_data->models.push({
            mesh_part,
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

    Transform &Renderer::get_model_transform(ModelId id)
    {
        return m_data->models[id].transform;
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

    // region Textures

    TextureId Renderer::load_texture(const char *path, FilterMode filter_mode)
    {
        // Load image
        int32_t  width, height, tex_channels;
        stbi_uc *pixels = stbi_load(path, &width, &height, &tex_channels, STBI_rgb_alpha);
        if (pixels == nullptr)
        {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return NULL_ID;
        }

        const VkDeviceSize image_size = width * height * 4;

        // Create staging buffer
        TransferCommand cmd = m_data->create_transfer_command(m_data->transfer_context.transfer_pool);
        cmd.staging_buffer  = m_data->allocator.create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        // Copy pixels to staging buffer
        void *data = m_data->allocator.map_buffer(cmd.staging_buffer);
        memcpy(data, pixels, static_cast<size_t>(image_size));
        m_data->allocator.unmap_buffer(cmd.staging_buffer);

        // Free imported image
        stbi_image_free(pixels);
        pixels = nullptr;

        constexpr VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;
        const VkExtent3D   extent       = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

        AllocatedImage image = m_data->allocator.create_image(image_format,
                                                              extent,
                                                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                              VK_IMAGE_ASPECT_COLOR_BIT,
                                                              VMA_MEMORY_USAGE_GPU_ONLY);

        // Do the transfer and conversion
        cmd.begin();

        VkImageSubresourceRange subresource_range   = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageMemoryBarrier    barrier_to_transfer = {
               .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
               .pNext = nullptr,
               // Access mask
               .srcAccessMask = 0,
               .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
               // Image layout
               .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
               .newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               .image            = image.image,
               .subresourceRange = subresource_range,
        };
        vkCmdPipelineBarrier(cmd.command_buffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier_to_transfer);

        // Copy image data from staging buffer to image
        VkBufferImageCopy copy_region = {
            .bufferOffset      = 0,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageOffset       = {0, 0, 0},
            .imageExtent       = extent,
        };
        vkCmdCopyBufferToImage(cmd.command_buffer,
                               cmd.staging_buffer.buffer,
                               image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &copy_region);

        if (m_data->graphics_queue.family_index == m_data->transfer_queue.family_index)
        {
            // Change its format again
            VkImageMemoryBarrier barrier_to_readable = barrier_to_transfer;
            barrier_to_readable.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier_to_readable.newLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier_to_readable.srcAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier_to_readable.dstAccessMask        = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd.command_buffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier_to_readable);

            cmd.end_and_submit(m_data->transfer_queue.queue);
        }
        else
        {
            // Change its format again
            VkImageMemoryBarrier barrier_to_readable = barrier_to_transfer;
            barrier_to_readable.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier_to_readable.newLayout            = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            barrier_to_readable.srcAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier_to_readable.dstAccessMask        = VK_ACCESS_TRANSFER_READ_BIT;
            barrier_to_readable.srcQueueFamilyIndex  = m_data->transfer_queue.family_index;
            barrier_to_readable.dstQueueFamilyIndex  = m_data->graphics_queue.family_index;
            vkCmdPipelineBarrier(cmd.command_buffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier_to_readable);
            cmd.end_and_submit(m_data->transfer_queue.queue);

            // Continue from the graphics queue
            TransferCommand cmd2 = m_data->create_transfer_command(m_data->transfer_context.graphics_pool);

            cmd2.begin();

            VkImageMemoryBarrier barrier_to_readable2 = barrier_to_transfer;
            barrier_to_readable2.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier_to_readable2.newLayout            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier_to_readable2.srcAccessMask        = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier_to_readable2.dstAccessMask        = VK_ACCESS_SHADER_READ_BIT;
            barrier_to_readable2.srcQueueFamilyIndex  = m_data->transfer_queue.family_index;
            barrier_to_readable2.dstQueueFamilyIndex  = m_data->graphics_queue.family_index;
            vkCmdPipelineBarrier(cmd2.command_buffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &barrier_to_readable2);

            cmd2.end_and_submit(m_data->graphics_queue.queue);
            m_data->transfer_context.commands.push_back(cmd2);
        }

        // Save command
        m_data->transfer_context.commands.push_back(cmd);

        // Get filter
        VkFilter filter = VK_FILTER_LINEAR;
        switch (filter_mode)
        {
            case FilterMode::NEAREST: filter = VK_FILTER_NEAREST; break;
            default: break;
        }

        // Create sampler
        VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            // Filter
            .magFilter  = filter,
            .minFilter  = filter,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            // Address mode
            .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 1,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_ALWAYS,
            .minLod                  = 0.0f,
            .maxLod                  = 0.0f,
            .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
        };
        VkSampler sampler = VK_NULL_HANDLE;
        vk_check(vkCreateSampler(m_data->device, &sampler_info, nullptr, &sampler), "Failed to create sampler");

        // Store the image
        return m_data->textures.push(Texture {
            .image   = image,
            .sampler = sampler,
        });
    }

    void Renderer::destroy_texture(TextureId id)
    {
        // Get the texture
        auto texture = m_data->textures.get(id);
        if (texture.has_value())
        {
            // Destroy the sampler
            vkDestroySampler(m_data->device, texture->sampler, nullptr);

            // Destroy the image
            m_data->allocator.destroy_image(texture->image);

            // Remove the texture
            m_data->textures.remove(id);
        }
        // No value = already destroyed
    }

    void Renderer::clear_textures()
    {
        // Destroy all textures
        for (auto &res : m_data->textures)
        {
            auto &texture = res.value();

            // Destroy the sampler
            vkDestroySampler(m_data->device, texture.sampler, nullptr);

            // Destroy the image
            m_data->allocator.destroy_image(texture.image);
        }

        // Clear the textures
        m_data->textures.clear();
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
        m_data->reset_transfer_context();

        // Update SSBOs if needed
        m_data->update_storage_buffers(current_frame);

        // Update the descriptor sets if needed
        m_data->update_descriptor_sets(current_frame);

        // Update meshes if needed
        m_data->update_mesh_buffers();

        // For each enabled camera
        for (auto &cam_entry : m_data->cameras)
        {
            const auto &camera = cam_entry.value();
            if (camera.enabled)
            {
                // Get its target swapchain
                auto &swapchain = m_data->swapchains[camera.target_swapchain_index];
                check(swapchain.enabled, "Active camera tries to render to a disabled swapchain.");

                // Update internal textures sets if needed
                m_data->update_render_stages_output_sets(swapchain);

                // Get camera infos and send them to the shader
                m_data->send_camera_data(camera.target_swapchain_index, camera, current_frame);

                // Update pipelines if needed
                m_data->build_out_of_date_effects(swapchain);

                // Update render stages cache if needed
                m_data->update_stage_cache(swapchain);

                // Begin recording
                m_data->begin_recording();

                // Get next image
                auto image_index = m_data->get_next_swapchain_image(swapchain);

                // For each stage
                for (size_t stage_i = 0; stage_i < m_data->render_pipeline_description.stages.size(); stage_i++)
                {
                    const auto &stage_desc = m_data->render_pipeline_description.stages[stage_i];
                    auto       &stage      = swapchain.render_stages[stage_i];

                    // Get clear values of each attachment
                    Array<VkClearValue> clear_values(stage_desc.attachments.size());
                    for (size_t i = 0; i < stage_desc.attachments.size(); i++)
                    {
                        // Depth image
                        if (stage_desc.attachments[i].final_layout == ImageLayout::DEPTH_STENCIL_OPTIMAL)
                        {
                            clear_values[i].depthStencil = VkClearDepthStencilValue {1.0f, 0};
                        }
                        // Color image
                        else
                        {
                            clear_values[i].color = VkClearColorValue {0.2f, 0.2f, 0.2f, 1.0f};
                        }
                    }

                    // Begin render pass
                    VkRenderPassBeginInfo render_pass_begin_info = {
                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                        .pNext = nullptr,
                        // Render pass
                        .renderPass = m_data->render_stages[stage_i].vk_render_pass,
                        // Link framebuffer
                        .framebuffer = stage.framebuffers[image_index],
                        // Render area
                        .renderArea = {.offset = {0, 0}, .extent = swapchain.viewport_extent},
                        // Clear values
                        .clearValueCount = static_cast<uint32_t>(clear_values.size()),
                        .pClearValues    = clear_values.data(),
                    };
                    vkCmdBeginRenderPass(current_frame.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

                    if (stage_desc.uses_material_system)
                    {
                        // Bind vertex and index buffers if needed
                        VkDeviceSize offset = 0;
                        vkCmdBindVertexBuffers(current_frame.command_buffer, 0, 1, &m_data->vertex_buffer.buffer, &offset);
                        vkCmdBindIndexBuffer(current_frame.command_buffer, m_data->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                        // Draw
                        m_data->draw_from_cache(stage, current_frame.command_buffer, current_frame, camera.target_swapchain_index);
                    }
                    else
                    {
                        // Just draw a quad if it doesn't use the material system
                        m_data->draw_quad(swapchain,
                                          stage_i,
                                          image_index,
                                          current_frame.command_buffer,
                                          current_frame,
                                          camera.target_swapchain_index);
                    }

                    vkCmdEndRenderPass(current_frame.command_buffer);
                }

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
        check(window_index < m_data->swapchains.size(), "Invalid window index");
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
        check(window_index < m_data->swapchains.size(), "Invalid window index");
        // Create camera
        return m_data->cameras.push(Camera {
            .enabled                = true,
            .target_swapchain_index = window_index,
            .transform              = transform,
            .type                   = CameraType::ORTHOGRAPHIC,
            .specs =
                {
                    .as_orthographic =
                        {
                            .width      = width,
                            .height     = height,
                            .near_plane = near,
                            .far_plane  = far,
                        },
                },
        });
    }

    CameraId Renderer::create_perspective_camera(uint32_t window_index, float fov, float near, float far)
    {
        check(window_index < m_data->swapchains.size(), "Invalid window index");
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
        check(window_index < m_data->swapchains.size(), "Invalid window index");
        // Create camera
        return m_data->cameras.push(Camera {
            .enabled                = true,
            .target_swapchain_index = window_index,
            .transform              = transform,
            .type                   = CameraType::PERSPECTIVE,
            .specs =
                {
                    .as_perspective =
                        {
                            .fov          = fov,
                            .aspect_ratio = aspect,
                            .near_plane   = near,
                            .far_plane    = far,
                        },
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