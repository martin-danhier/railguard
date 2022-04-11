#include "test_framework/test_framework.hpp"
#include "railguard/utils/geometry/mat4.h"
#include <railguard/core/engine.h>
#include <railguard/core/window.h>
#include <railguard/utils/event_sender.h>
#include <railguard/utils/array.h>
#include <railguard/core/renderer.h>
#include <railguard/utils/geometry/transform.h>
#include <railguard/utils/geometry/vec3.h>
#include <SDL2/SDL_keycode.h>

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
    auto camera = renderer.create_perspective_camera(0, rg::radians(70.f), 0.01f, 200.0f);
    auto &camera_transform = renderer.get_camera_transform(camera);
    camera_transform.position().x() = 4;
    camera_transform.position().y() = 3;
    camera_transform.position().z() = -10;

    rg::Vec3 velocity(0, 0, 0);

    engine.window().on_key_event()->subscribe([&camera_transform, &velocity](const rg::KeyEvent &event)
    {
        if (event.down) {
            if (event.key == SDLK_z) {
                camera_transform.position().z() += 1;
            } else if (event.key == SDLK_s) {
                camera_transform.position().z() -= 1;
            } else if (event.key == SDLK_q) {
                camera_transform.position().x() -= 1;
            } else if (event.key == SDLK_d) {
                camera_transform.position().x() += 1;
            } else if (event.key == SDLK_a) {
                camera_transform.position().y() -= 1;
            } else if (event.key == SDLK_e) {
                camera_transform.position().y() += 1;
            }
        } else {

        }
    });

    // Run engine
    EXPECT_NO_THROWS(engine.run_main_loop());
}