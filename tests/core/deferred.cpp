#include <railguard/core/engine.h>
#include <railguard/core/mesh.h>
#include <railguard/core/renderer/renderer.h>
#include <railguard/core/window.h>
#include <railguard/utils/array.h>
#include <railguard/utils/event_sender.h>
#include <railguard/utils/geometry/transform.h>

#include <SDL2/SDL_keycode.h>
#include <test_framework/test_framework.hpp>

TEST
{
    rg::Engine engine;

    ASSERT_NO_THROWS(engine = rg::Engine("My wonderful game", 500, 500));

    // Setup scene
    auto &renderer = engine.renderer();

    // Load shaders
    auto geom_vertex_shader   = renderer.load_shader_module("resources/shaders/deferred/geometry.vert.spv", rg::ShaderStage::VERTEX);
    auto geom_fragment_shader = renderer.load_shader_module("resources/shaders/deferred/geometry.frag.spv", rg::ShaderStage::FRAGMENT);
    auto light_vertex_shader  = renderer.load_shader_module("resources/shaders/deferred/light.vert.spv", rg::ShaderStage::VERTEX);
    auto light_fragment_shader = renderer.load_shader_module("resources/shaders/deferred/light.frag.spv", rg::ShaderStage::FRAGMENT);

    // Create shader effects
    auto geom_effect  = renderer.create_shader_effect({geom_vertex_shader, geom_fragment_shader},
                                                     rg::RenderStageKind::DEFERRED_GEOMETRY,
                                                     {{rg::ShaderStage::FRAGMENT}});
    auto light_effect = renderer.create_shader_effect({light_vertex_shader, light_fragment_shader},
                                                      rg::RenderStageKind::DEFERRED_LIGHTING,
                                                      {
                                                          // G-Buffer has 3 textures, we want them in the fragment shader
                                                          {rg::ShaderStage::FRAGMENT},
                                                          {rg::ShaderStage::FRAGMENT},
                                                          {rg::ShaderStage::FRAGMENT},
                                                      });

    // Set light effect as a global effect (it doesn't use material system)
    renderer.set_global_shader_effect(rg::RenderStageKind::DEFERRED_LIGHTING, light_effect);

    // Create material template
    auto material_template = renderer.create_material_template({geom_effect});

    // Load texture
    auto texture = renderer.load_texture("resources/textures/lost_empire-RGB.png", rg::FilterMode::NEAREST);

    // Create material
    auto material = renderer.create_material(material_template, {{texture}});

    // Create a scene mesh part
    auto scene = rg::MeshPart::load_from_obj("resources/meshes/lost_empire.obj", engine.renderer(), true);

    // Create a model
    auto  model           = renderer.create_model(scene, material);
    auto &model_transform = renderer.get_model_transform(model);

    // Create a camera
    auto  camera                = renderer.create_perspective_camera(0, glm::radians(70.f), 0.01f, 200.0f);
    auto &camera_transform      = renderer.get_camera_transform(camera);
    camera_transform.position.x = 4;
    camera_transform.position.y = 3;
    camera_transform.position.z = -10;

    engine.window().on_key_event()->subscribe(
        [&camera_transform](const rg::KeyEvent &event)
        {
            if (event.down)
            {
                if (event.key == SDLK_z)
                {
                    // Forward
                    camera_transform.position.z += 1;
                }
                else if (event.key == SDLK_s)
                {
                    // Backward
                    camera_transform.position.z -= 1;
                }
                else if (event.key == SDLK_q)
                {
                    // Left
                    camera_transform.position.x += 1;
                }
                else if (event.key == SDLK_d)
                {
                    // Right
                    camera_transform.position.x -= 1;
                }
                else if (event.key == SDLK_a)
                {
                    // Up
                    camera_transform.position.y -= 1;
                }
                else if (event.key == SDLK_e)
                {
                    // Down
                    camera_transform.position.y += 1;
                }
                // W and C to rotate camera left and right
                else if (event.key == SDLK_w)
                {
                    camera_transform.rotation = glm::rotate(camera_transform.rotation, glm::radians(1.f), glm::vec3(0, 1, 0));
                }
                else if (event.key == SDLK_c)
                {
                    camera_transform.rotation = glm::rotate(camera_transform.rotation, glm::radians(-1.f), glm::vec3(0, 1, 0));
                }
            }
            else
            {
            }
        });

    // Run engine
    EXPECT_NO_THROWS(engine.run_main_loop());
}