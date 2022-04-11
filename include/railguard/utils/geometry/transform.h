#pragma once

// Import forward declaration of glm vec and quat
#include <glm/gtc/quaternion.hpp>

namespace rg
{
    class Transform
    {
      private:
        glm::vec3 m_position = {};
        glm::quat m_rotation = {};
        glm::vec3 m_scale    = {};

      public:
        Transform();
        Transform(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale);
        explicit Transform(const glm::quat &mRotation);

        // Getters
        inline glm::vec3 &position()
        {
            return m_position;
        }
        [[nodiscard]] inline const glm::vec3 &position() const
        {
            return m_position;
        }

        inline glm::quat &rotation()
        {
            return m_rotation;
        }
        [[nodiscard]] inline const glm::quat &rotation() const
        {
            return m_rotation;
        }

        inline glm::vec3 &scale()
        {
            return m_scale;
        }
        [[nodiscard]] inline const glm::vec3 &scale() const
        {
            return m_scale;
        }

        // Methods
        [[nodiscard]] glm::mat4 view_matrix() const;
    };
} // namespace rg