#include "railguard/utils/geometry/quat.h"

#include <railguard/utils/geometry/mat4.h>

#include <cmath>

namespace rg
{

    float Quat::norm() const
    {
        // sqrt(r^2 + i^2 + j^2 + k^2)
        return std::sqrt(m_data.r * m_data.r + m_data.i * m_data.i + m_data.j * m_data.j + m_data.k * m_data.k);
    }

    Mat4 Quat::to_mat4() const
    {
        // Normalize quaternion
        auto v = normalize();

        // Precalculate values
        const float ii = v.i() * v.i();
        const float jj = v.j() * v.j();
        const float kk = v.k() * v.k();
        const float ij = v.i() * v.j();
        const float ik = v.i() * v.k();
        const float jk = v.j() * v.k();
        const float kr = v.k() * v.r();
        const float ir = v.i() * v.r();
        const float jr = v.j() * v.r();

        // Create matrix
        auto result = Mat4::identity();

        // Save in matrix
        result[0][0] = 1 - 2 * (jj - kk);
        result[0][1] = 2 * (ij + kr);
        result[0][2] = 2 * (ik - jr);

        result[1][0] = 2 * (ij - kr);
        result[1][1] = 1 - 2 * (ii - kk);
        result[1][2] = 2 * (jk + ir);

        result[2][0] = 2 * (ik + jr);
        result[2][1] = 2 * (jk - ir);
        result[2][2] = 1 - 2 * (ii - jj);

        return result;
    }

} // namespace rg