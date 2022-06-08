#include <railguard/core/engine.h>
#include <railguard/core/renderer/render_pipeline.h>
#include <railguard/core/renderer/renderer.h>

#include <test_framework/test_framework.hpp>

TEST
{
    rg::Engine engine;

    // Define custom render pipeline for triangle
    rg::RenderPipelineDescription pipeline_description {
        .stages =
            {
                rg::RenderStageDescription {
                    .name = "triangle",
                    .kind = rg::RenderStageKind::FORWARD,
                    .attachments =
                        {
                            // Window
                            rg::RenderStageAttachmentDescription {
                                .format       = rg::Format::WINDOW_FORMAT,
                                .final_layout = rg::ImageLayout::PRESENT_SRC,
                            },
                        },
                    .uses_material_system = false,
                    .vertex_count         = 3,
                    .do_depth_test        = false,
                },
            },
    };
    ASSERT_NO_THROWS(engine = rg::Engine("My wonderful game", 500, 500, std::move(pipeline_description)));

    // Get renderer
    auto &renderer = engine.renderer();

    // Load shader modules
    auto vertex_shader   = renderer.load_shader_module("resources/shaders/triangle/triangle.vert.spv", rg::ShaderStage::VERTEX);
    auto fragment_shader = renderer.load_shader_module("resources/shaders/triangle/triangle.frag.spv", rg::ShaderStage::FRAGMENT);

    // Create shader effect
    auto triangle_effect = renderer.create_shader_effect({vertex_shader, fragment_shader}, rg::RenderStageKind::FORWARD, {});

    // Save global shader effect
    renderer.set_global_shader_effect(rg::RenderStageKind::FORWARD, triangle_effect);

    // Create a camera
    auto camera = renderer.create_orthographic_camera(0, 0, 1);

    // Run engine
    EXPECT_NO_THROWS(engine.run_main_loop());
}