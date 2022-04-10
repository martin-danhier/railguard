#include "railguard/utils/geometry/transform.h"
#include <railguard/utils/geometry/mat4.h>

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

    Mat4 Transform::view_matrix() const
    {
        auto view = Mat4::identity();
        view.scale(m_scale);
        view.translate(m_position);
        view.rotate(m_rotation);
        return view;
    }

} // namespace rg
