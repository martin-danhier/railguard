#include "test_framework/test_framework.hpp"
#include <railguard/core/engine.h>
#include <railguard/utils/array.h>
#include <railguard/core/renderer.h>

TEST
{
    rg::Engine engine;

    // Setup scene

    auto &renderer = engine.renderer();

    // Load shaders
    auto vertex_shader   = renderer.load_shader_module("resources/shaders/hello/test.vert.spv", rg::ShaderStage::VERTEX);
    auto fragment_shader = renderer.load_shader_module("resources/shaders/hello/test.frag.spv", rg::ShaderStage::FRAGMENT);

    // Create a shader effect
    auto hello_effect = renderer.create_shader_effect({vertex_shader, fragment_shader}, rg::RenderStageKind::LIGHTING);

    // Create a material template
    auto material_template = renderer.create_material_template({hello_effect});

    // Create a material
    auto material = renderer.create_material(material_template);

    // Create a model
    auto model = renderer.create_model(material);

    // Create a render node
    auto render_node = renderer.create_render_node(model);

    // Create a camera
    auto camera = renderer.create_orthographic_camera(0, 500, 500, 0, 100);

    // Run engine
    EXPECT_NO_THROWS(engine.run_main_loop());
}