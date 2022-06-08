#pragma once

#include <railguard/core/renderer/types.h>
#include <railguard/utils/array.h>

#include <cstdint>

namespace rg
{

    /** Describes a single attachment of a render stage. */
    struct RenderStageAttachmentDescription
    {
        Format      format         = Format::UNDEFINED;
        ImageLayout initial_layout = ImageLayout::UNDEFINED;
        ImageLayout final_layout   = ImageLayout::UNDEFINED;
    };

    /**
     * A render stage description tells the rendering engine about a single render stage (geometry, lighting, etc.)
     * */
    struct RenderStageDescription
    {
        /** Readable name to identify the stage in logs */
        const char                             *name        = "";
        /** The kind of a stage is used in shader effects to specify for which stage the shader is used. */
        RenderStageKind kind = RenderStageKind::INVALID;
        Array<RenderStageAttachmentDescription> attachments = {};
        /** If true, shader effects will use the material system (the render nodes that use materials with effects of this stage kind
         * will be rendered). Otherwise, a single quad (actually 2 triangles) is rendered. */
        bool uses_material_system = true;
        /**
         * If uses_material_system is false, this is the number of vertices that will be rendered. The actual vertex data is supposed hardcoded in the vertex shader.
         * The default is 6, which are 2 triangles to form a quad over the screen.
         */
        uint8_t vertex_count = 6;
        bool do_depth_test        = false;
    };

    /**
     * A render pipeline is a collection of render stages.
     * It specifies how any image is rendered.
     * */
    struct RenderPipelineDescription
    {
        Array<RenderStageDescription> stages;
    };

    // ---=== Presets ===---

    /**
     * A render pipeline suitable for deferred rendering.
     */
    RenderPipelineDescription deferred_render_pipeline();

    /** Most basic render pipeline, 1 stage that directly renders to the window. */
    [[maybe_unused]] RenderPipelineDescription basic_forward_render_pipeline();

} // namespace rg