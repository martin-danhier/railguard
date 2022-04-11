#pragma once

#include <glm/mat4x4.hpp>

namespace rg
{
    struct GPUCameraData
    {
        glm::mat4                  view            = {};
        glm::mat4                  projection      = {};
        [[maybe_unused]] glm::mat4 view_projection = {};
    };
} // namespace rg