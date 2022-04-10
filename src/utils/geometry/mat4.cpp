#define PRETTY_PRINT_MAT4
#include "railguard/utils/geometry/mat4.h"

#include <railguard/utils/geometry/quat.h>
#include <railguard/utils/geometry/vec3.h>

#include <sstream>
#include <string>

namespace rg
{

    Mat4 Mat4::operator*(const Mat4 &other) const
    {
        Mat4 result;

        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.m_data.m[i][j] = 0;
                for (int k = 0; k < 4; k++)
                {
                    result.m_data.m[i][j] += m_data.m[i][k] * other.m_data.m[k][j];
                }
            }
        }

        return result;
    }

    bool Mat4::operator==(const Mat4 &other) const
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                if (m_data.m[i][j] != other.m_data.m[i][j])
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool Mat4::operator!=(const Mat4 &other) const
    {
        return !(*this == other);
    }

    // Methods

    void Mat4::translate(const Vec3 &other)
    {
        m_data.m[0][3] += other.x();
        m_data.m[1][3] += other.y();
        m_data.m[2][3] += other.z();
    }

    Mat4 Mat4::translate(const Mat4 &matrix, const Vec3 &other)
    {
        Mat4 result = matrix;
        result.translate(other);
        return result;
    }

    void Mat4::rotate(const Quat &rotation)
    {
        Mat4 rotation_mat = rotation.to_mat4();
        *this             = rotation_mat * (*this);
    }

    Mat4 Mat4::rotate(const Mat4 &matrix, const Quat &rotation)
    {
        Mat4 result = matrix;
        result.rotate(rotation);
        return result;
    }

    void Mat4::scale(const Vec3 &scale)
    {
        m_data.m[0][0] *= scale.x();
        m_data.m[1][1] *= scale.y();
        m_data.m[2][2] *= scale.z();
    }

    Mat4 Mat4::scale(const Mat4 &matrix, const Vec3 &scale)
    {
        Mat4 result = matrix;
        result.scale(scale);
        return result;
    }

    // Overloads for pretty printing
    std::ostream &operator<<(std::ostream &os, const Mat4 &m)
    {
        os << "[";

        for (int i = 0; i < 4; i++)
        {
            os << "[";
            for (int j = 0; j < 3; j++)
            {
                os << m.m_data.m[i][j] << ", ";
            }
            os << m.m_data.m[i][3] << "]";

            if (i != 3)
            {
                os << ", ";
            }
        }

        os << "]";

        return os;
    }
    std::string operator+(const std::string &str, const Mat4 &m)
    {
        std::stringstream ss;
        ss << str << m;
        return ss.str();
    }
    std::string &operator+=(std::string &str, const Mat4 &m)
    {
        str = str + m;
        return str;
    }
} // namespace rg