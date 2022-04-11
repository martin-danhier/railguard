#include "railguard/utils/geometry/transform.h"

namespace rg
{

    Transform::Transform() : m_position(glm::vec3(0.0f)), m_rotation(glm::vec3(0.0f)), m_scale(glm::vec3(1.0f))
    {
    }

    Transform::Transform(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale)
        : m_position(position),
          m_rotation(rotation),
          m_scale(scale)
    {
    }

    glm::mat4 Transform::view_matrix() const
    {
        auto view = glm::mat4(1.0f);
        view      = glm::translate(view, m_position);
        view      = glm::mat4_cast(m_rotation) * view;
        view      = glm::scale(view, m_scale);
        return view;
    }

    Transform::Transform(const glm::quat &mRotation) : m_rotation(mRotation)
    {
    }

} // namespace rg
