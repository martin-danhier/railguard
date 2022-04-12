#include "railguard/core/mesh.h"
#include <railguard/core/engine.h>
#include <railguard/core/renderer.h>
#include <railguard/core/window.h>
#include <railguard/utils/array.h>
#include <railguard/utils/event_sender.h>
#include <railguard/utils/geometry/transform.h>

#include <SDL2/SDL_keycode.h>
#include <test_framework/test_framework.hpp>

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

    // Create a cube mesh part
    auto cube = renderer.save_mesh_part(rg::MeshPart(
        {
            rg::Vertex {.position = {-1.0f, -1.0f, 1.0f}, .normal = {-1.0f, -1.0f, -1.0f}, .color = {1.0f, 0.0f, 0.0f}},
            rg::Vertex {.position = {1.0f, -1.0f, 1.0f}, .normal = {1.0f, -1.0f, -1.0f}, .color = {1.0f, 0.0f, 0.0f}},
            rg::Vertex {.position = {-1.0f, 1.0f, 1.0f}, .normal = {-1.0f, 1.0f, -1.0f}, .color = {1.0f, 0.0f, 0.0f}},
            rg::Vertex {.position = {1.0f, 1.0f, 1.0f}, .normal = {1.0f, 1.0f, -1.0f}, .color = {1.0f, 0.0f, 0.0f}},
            rg::Vertex {.position = {-1.0f, -1.0f, -1.0f}, .normal = {-1.0f, -1.0f, 1.0f}, .color = {1.0f, 1.0f, 0.0f}},
            rg::Vertex {.position = {1.0f, -1.0f, -1.0f}, .normal = {1.0f, -1.0f, 1.0f}, .color = {1.0f, 1.0f, 0.0f}},
            rg::Vertex {.position = {-1.0f, 1.0f, -1.0f}, .normal = {-1.0f, 1.0f, 1.0f}, .color = {1.0f, 1.0f, 0.0f}},
            rg::Vertex {.position = {1.0f, 1.0f, -1.0f}, .normal = {1.0f, 1.0f, 1.0f}, .color = {1.0f, 1.0f, 0.0f}},
        },
        {
            // Top
            rg::Triangle {2, 6, 7},
            rg::Triangle {2, 3, 7},

            // Bottom
            rg::Triangle {0, 4, 5},
            rg::Triangle {0, 1, 5},

            // Left
            rg::Triangle {0, 2, 6},
            rg::Triangle {0, 4, 6},

            // Right
            rg::Triangle {1, 3, 7},
            rg::Triangle {1, 5, 7},

            // Front
            rg::Triangle {0, 2, 3},
            rg::Triangle {0, 1, 3},

            // Back
            rg::Triangle {4, 6, 7},
            rg::Triangle {4, 5, 7},
        }));

    // Create a model
    auto model = renderer.create_model(cube, material);

    // Create a render node
    auto render_node = renderer.create_render_node(model);

    // Create a camera
    auto  camera                  = renderer.create_perspective_camera(0, glm::radians(70.f), 0.01f, 200.0f);
    auto &camera_transform        = renderer.get_camera_transform(camera);
    camera_transform.position.x = 4;
    camera_transform.position.y = 3;
    camera_transform.position.z = -10;

    glm::vec3 velocity(0, 0, 0);

    engine.window().on_key_event()->subscribe(
        [&camera_transform, &velocity](const rg::KeyEvent &event)
        {
            if (event.down)
            {
                if (event.key == SDLK_z)
                {
                    camera_transform.position.z += 1;
                }
                else if (event.key == SDLK_s)
                {
                    camera_transform.position.z -= 1;
                }
                else if (event.key == SDLK_q)
                {
                    camera_transform.position.x -= 1;
                }
                else if (event.key == SDLK_d)
                {
                    camera_transform.position.x += 1;
                }
                else if (event.key == SDLK_a)
                {
                    camera_transform.position.y -= 1;
                }
                else if (event.key == SDLK_e)
                {
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