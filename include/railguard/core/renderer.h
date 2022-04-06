#pragma once

#include <cstdint>

namespace rg
{
    // ---==== Forward declarations ====---
    class Window;
    template<typename T> class Array;

    // ---==== Structs ====---

    struct Version
    {
        uint32_t major = 0;
        uint32_t minor = 0;
        uint32_t patch = 0;
    };

    // ---==== Definitions ====---

    constexpr Version ENGINE_VERSION = {0, 1, 0};

    // ---==== Material system ====---

    enum class RenderStageKind
    {
        INVALID  = 0,
        GEOMETRY = 1,
        LIGHTING = 2,
    };

    enum class ShaderStage
    {
        INVALID  = 0,
        VERTEX   = 1,
        FRAGMENT = 2,
    };

    /** An association of the shader and its kind. */
    struct ShaderModule;

    /**
     * A shader effect defines the whole shader pipeline (what shader_modules are used, in what order, for what render stage...)
     */
    struct ShaderEffect;

    /** A material template groups the common base between similar materials. It can be used to create new materials. */
    struct MaterialTemplate;

    /** Defines the appearance of a model (shader effect, texture...) */
    struct Material;

    /** Abstract representation of a model that can be instantiated in the world. */
    struct Model;

    /** Instance of a model */
    struct RenderNode;

    // Define aliases for the storage id, that way it is more intuitive to know what the id is referring to.
    constexpr static uint64_t NULL_ID = 0;
    using ShaderModuleId              = uint64_t;
    using ShaderEffectId              = uint64_t;
    using MaterialTemplateId          = uint64_t;
    using MaterialId                  = uint64_t;
    using ModelId                     = uint64_t;
    using RenderNodeId                = uint64_t;

    // ---==== Main classes ====---

    /**
     * The renderer is an opaque struct that contains all of the data used for rendering.
     * It exact contents depend on the used graphics API, which is why it is opaque an only handled with pointers.
     *
     * The renderer is used in all functions that concern the rendering.
     */
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

        /**
         * Links the window to the renderer in the given slot.
         * @param window_slot_index Index of the slot where this window will be linked. Must be empty and smaller than the
         * window capacity.
         * @param window Window that will be linked to the renderer.
         */
        void connect_window(uint32_t window_slot_index, Window &window);

        // Shader modules

        /**
         * Loads a shader from the given file. The language of the shader depends on the used backend.
         * @param shader_path Path of the shader file.
         * @param kind Kind of the shader.
         * @return The id of the created shader.
         */
         ShaderModuleId load_shader_module(const char *shader_path, ShaderStage kind);
         void destroy_shader_module(ShaderModuleId id);
         void clear_shader_modules();

         // Shader effects

         ShaderEffectId create_shader_effect(const Array<ShaderModuleId>& stages, RenderStageKind render_stage_kind);
         void destroy_shader_effect(ShaderEffectId id);
         void clear_shader_effects();

         // Material templates

         MaterialTemplateId  create_material_template(const Array<ShaderEffectId> &available_effects);
         void destroy_material_template(MaterialTemplateId id);
         void clear_material_templates();

         // Materials

         MaterialId create_material(MaterialTemplateId material_template);
         void destroy_material(MaterialId id);
         void clear_materials();

         // Models

         ModelId create_model(MaterialId material);
         void destroy_model(ModelId id);
         void clear_models();

         // Render node

         RenderNodeId create_render_node(ModelId model);
         void destroy_render_node(RenderNodeId id);
         void clear_render_nodes();

        ~Renderer();
    };
} // namespace rg