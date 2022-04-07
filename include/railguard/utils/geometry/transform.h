#pragma once

#include <railguard/utils/geometry/quat.h>
#include <railguard/utils/geometry/vec3.h>

namespace rg
{
    struct Transform
    {
      private:
        Vec3 m_position;
        Quat m_rotation;
        Vec3 m_scale;

      public:
        Transform();
        Transform(const Vec3& position, const Quat& rotation, const Vec3& scale);

        // Getters
        inline Vec3& position() {
            return m_position;
        }
        [[nodiscard]] inline const Vec3& position() const {
            return m_position;
        }

        inline Quat& rotation() {
            return m_rotation;
        }
        [[nodiscard]] inline const Quat& rotation() const {
            return m_rotation;
        }

        inline Vec3& scale() {
            return m_scale;
        }
        [[nodiscard]] inline const Vec3& scale() const {
            return m_scale;
        }

    };
} // namespace rg