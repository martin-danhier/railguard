#pragma once

#include <railguard/core/renderer/types.h>

#include <cstdint>

namespace rg
{
    // ---==== Forward declarations ====---
    class Window;
    template<typename T>
    class Array;
    class MeshPart;
    struct RenderPipelineDescription;

    struct Transform;

    // ---==== Definitions ====---

    constexpr Version ENGINE_VERSION = {0, 1, 0};

    enum class CameraType
    {
        PERSPECTIVE  = 0,
        ORTHOGRAPHIC = 1,
    };

    // Define aliases for the storage id, that way it is more intuitive to know what the id is referring to.
    constexpr static uint64_t NULL_ID = 0;
    /** An association of the shader and its kind. */
    using ShaderModuleId = uint64_t;
    /**
     * A shader effect defines the whole shader pipeline (what shader_modules are used, in what order, for what render stages...)
     */
    using ShaderEffectId = uint64_t;
    /** A material template groups the common base between similar materials. It can be used to create new materials. */
    using MaterialTemplateId = uint64_t;
    /** Defines the appearance of a model (shader effect, texture...) */
    using MaterialId = uint64_t;
    using MeshPartId = uint64_t;
    /** Abstract representation of a model that can be instantiated in the world. */
    using ModelId = uint64_t;
    /** Instance of a model */
    using RenderNodeId = uint64_t;
    /**
     * A camera symbolizes the view of the world from which the scene is rendered.
     * It is the camera which defines the projection type (as_orthographic, as_perspective, etc.), and the viewport.
     * A camera can either render to a window or to a texture.
     * */
    using CameraId = uint64_t;
    /** A texture that can be used in a material. */
    using TextureId = uint64_t;

    /** Description of the characteristics of a texture */
    struct TextureLayout
    {
        /** Shader stages in which the texture will be accessible. Defaults to FRAGMENT. */
        ShaderStage stages = ShaderStage::FRAGMENT;
    };

    // ---==== Main classes ====---

    /**
     * The renderer is an opaque struct that contains all of the m_data used for rendering.
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
        Renderer(const Window               &example_window,
                 const char                 *application_name,
                 const Version              &application_version,
                 uint32_t                    window_capacity,
                 RenderPipelineDescription &&render_pipeline_description);

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
        void           destroy_shader_module(ShaderModuleId id);
        void           clear_shader_modules();

        // Shader effects

        /**
         * Creates a new shader effect, which defines the configuration of the rendering pipeline.
         * @param stages Shaders to be executed, in order.
         * @param render_stage_kind Render stages concerned by this pipeline.
         * @param textures Layouts of the textures of this shader configuration, in order.
         * @return the id of the new shader effect, or NULL_ID if it failed.
         */
        ShaderEffectId create_shader_effect(const Array<ShaderModuleId> &stages,
                                            RenderStageKind              render_stage_kind,
                                            const Array<TextureLayout>  &textures);
        void           destroy_shader_effect(ShaderEffectId id);
        void           clear_shader_effects();

        /**
         * Sets a shader effect as a global effect for a specific render stage. Global shader effects are used when a stage doesn't use
         * the material system. They are ignored otherwise. One, and exactly one global effect can be used for each stage. It will be
         * applied over a quad (6 vertices, 2 triangles), but no vertex input is provided. It is assumed that the vertex shader
         * hardcodes the four vertices in an array.
         */
        void set_global_shader_effect(RenderStageKind stage_kind, ShaderEffectId effect_id);

        // Material templates

        MaterialTemplateId create_material_template(const Array<ShaderEffectId> &available_effects);
        void               destroy_material_template(MaterialTemplateId id);
        void               clear_material_templates();

        // Materials

        /**
         * Creates a new material.
         * @param material_template template containing default parameters and common logic between similar materials.
         * @param textures For each effect (in the same order as the ones defined in the template), the textures that are used. Must
         * match the shader effect layout.
         * @return the id of the new material.
         */
        MaterialId create_material(MaterialTemplateId material_template, const Array<Array<TextureId>> &textures);
        MaterialId create_material(MaterialTemplateId material_template, Array<Array<TextureId>> &&textures);
        void       destroy_material(MaterialId id);
        void       clear_materials();

        // Meshes

        MeshPartId save_mesh_part(MeshPart &&mesh_part);
        void       destroy_mesh_part(MeshPartId id);
        void       clear_mesh_parts();

        // Models

        ModelId    create_model(MeshPartId mesh_part, MaterialId material);
        void       destroy_model(ModelId id);
        Transform &get_model_transform(ModelId id);
        void       clear_models();

        // Render node

        RenderNodeId create_render_node(ModelId model);
        void         destroy_render_node(RenderNodeId id);
        void         clear_render_nodes();

        // Textures

        TextureId load_texture(const char *path, FilterMode filter_mode);
        void      destroy_texture(TextureId id);
        void      clear_textures();

        // Cameras
        CameraId create_orthographic_camera(uint32_t window_index, float near, float far);
        CameraId create_orthographic_camera(uint32_t window_index, float width, float height, float near, float far);
        CameraId create_orthographic_camera(uint32_t         window_index,
                                            float            width,
                                            float            height,
                                            float            near,
                                            float            far,
                                            const Transform &transform);

        CameraId create_perspective_camera(uint32_t window_index, float fov, float near, float far);
        CameraId create_perspective_camera(uint32_t window_index, float fov, float aspect, float near, float far);
        CameraId create_perspective_camera(uint32_t         window_index,
                                           float            fov,
                                           float            aspect,
                                           float            near,
                                           float            far,
                                           const Transform &transform);

        void remove_camera(CameraId id);

        [[nodiscard]] const Transform &get_camera_transform(CameraId id) const;
        Transform                     &get_camera_transform(CameraId id);

        [[nodiscard]] CameraType get_camera_type(CameraId id) const;

        void               disable_camera(CameraId id);
        void               enable_camera(CameraId id);
        [[nodiscard]] bool is_camera_enabled(CameraId id) const;

        // Rendering
        void draw();

        ~Renderer();
    };
} // namespace rg