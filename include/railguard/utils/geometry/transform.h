#pragma once

// Import forward declaration of glm vec and quat
#include <glm/gtc/quaternion.hpp>

namespace rg
{
    struct Transform
    {
        glm::vec3 position   = glm::vec3(0.0f);
        glm::quat rotation   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale      = glm::vec3(1.0f);

        // Methods
        [[nodiscard]] glm::mat4 view_matrix() const;
    };
} // namespace rg