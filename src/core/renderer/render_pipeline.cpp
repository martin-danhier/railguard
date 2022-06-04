#include "railguard/core/renderer/render_pipeline.h"

namespace rg
{

    [[maybe_unused]] RenderPipelineDescription deferred_render_pipeline()
    {
        // Init stage array
        Array<RenderStageDescription> stages(2);

        // Geometry stage
        stages[0].name        = "geometry";
        stages[0].kind        = RenderStageKind::DEFERRED_GEOMETRY;
        stages[0].uses_material_system = true;
        stages[0].do_depth_test = false;
        stages[0].attachments = {
            // Position color buffer
            RenderStageAttachmentDescription {
                .format       = Format::R16G16B16A16_SFLOAT,
                .final_layout = ImageLayout::SHADER_READ_ONLY_OPTIMAL,
            },
            // Normal color buffer
            RenderStageAttachmentDescription {
                .format       = Format::R16G16B16A16_SFLOAT,
                .final_layout = ImageLayout::SHADER_READ_ONLY_OPTIMAL,
            },
            // Albedo + specular buffer
            RenderStageAttachmentDescription {
                .format       = Format::R8G8B8A8_SRGB,
                .final_layout = ImageLayout::SHADER_READ_ONLY_OPTIMAL,
            },
        };

        // Lighting stage
        stages[1].name        = "lighting";
        stages[1].kind        = RenderStageKind::DEFERRED_LIGHTING;
        stages[1].attachments = {
            // Output
            RenderStageAttachmentDescription {
                // WINDOW_FORMAT will be replaced by the actual window format, which will be inferred by the renderer based on the
                // window
                .format       = Format::WINDOW_FORMAT,
                .final_layout = ImageLayout::PRESENT_SRC,
            },
            // Depth stencil
//            RenderStageAttachmentDescription {
//                .format       = Format::D32_SFLOAT,
//                .final_layout = ImageLayout::DEPTH_STENCIL_OPTIMAL,
//            },
        };

        return RenderPipelineDescription {
            .stages = stages,
        };
    }

} // namespace rg