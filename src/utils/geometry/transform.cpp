#include "railguard/utils/geometry/transform.h"

namespace rg
{

    Transform::Transform() : m_position(Vec3::zero()), m_rotation(Quat::identity()), m_scale(Vec3::one())
    {
    }

    Transform::Transform(const Vec3 &position, const Quat &rotation, const Vec3 &scale)
        : m_position(position),
          m_rotation(rotation),
          m_scale(scale)
    {
    }

} // namespace rg
