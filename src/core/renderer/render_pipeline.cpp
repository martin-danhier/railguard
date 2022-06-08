#include "railguard/core/renderer/render_pipeline.h"

namespace rg
{

    [[maybe_unused]] RenderPipelineDescription deferred_render_pipeline()
    {
        // Init stage array
        return RenderPipelineDescription {
            .stages =
                {
                    // Geometry stage
                    RenderStageDescription {
                        .name = "geometry",
                        .kind = RenderStageKind::DEFERRED_GEOMETRY,
                        .attachments =
                            {
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
                                // Depth stencil
                                RenderStageAttachmentDescription {
                                    .format       = Format::D32_SFLOAT,
                                    .final_layout = ImageLayout::DEPTH_STENCIL_OPTIMAL,
                                },
                            },
                        .uses_material_system = true,
                        .do_depth_test        = true,
                    },
                    // Lighting stage
                    RenderStageDescription {
                        .name = "lighting",
                        .kind = RenderStageKind::DEFERRED_LIGHTING,
                        .attachments =
                            {
                                // Output
                                RenderStageAttachmentDescription {
                                    // WINDOW_FORMAT will be replaced by the actual window format, which will be inferred by
                                    // the renderer based on the window
                                    .format       = Format::WINDOW_FORMAT,
                                    .final_layout = ImageLayout::PRESENT_SRC,
                                },
                            },
                        .uses_material_system = false,
                        .do_depth_test        = false,
                    },
                },
        };
    }

    [[maybe_unused]] RenderPipelineDescription basic_forward_render_pipeline()
    {
        return RenderPipelineDescription {
            .stages =
                {
                    RenderStageDescription {
                        .name = "forward",
                        .kind = RenderStageKind::FORWARD,
                        .attachments =
                            {
                                // Output
                                RenderStageAttachmentDescription {
                                    .format       = Format::WINDOW_FORMAT,
                                    .final_layout = ImageLayout::PRESENT_SRC,
                                },
                                // Depth
                                RenderStageAttachmentDescription {
                                    .format       = Format::D32_SFLOAT,
                                    .final_layout = ImageLayout::DEPTH_STENCIL_OPTIMAL,
                                },
                            },
                        .uses_material_system = true,
                        .do_depth_test        = true,
                    },
                },
        };
    }

} // namespace rg