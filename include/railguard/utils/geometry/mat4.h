#pragma once

#include <cmath>
#ifdef PRETTY_PRINT_MAT4
#include <string>
#endif

namespace rg
{
    class Vec3;
    class Quat;

    float radians(float degrees);

    class Mat4
    {
      private:
        union Data
        {
            struct
            {
                float m00, m01, m02, m03;
                float m10, m11, m12, m13;
                float m20, m21, m22, m23;
                float m30, m31, m32, m33;
            };
            float m[4][4];
            float mm[16];
        } m_data {};

      public:
        Mat4() = default;

        // clang-format off
        constexpr explicit Mat4(float m00, float m01, float m02, float m03,
                                float m10, float m11, float m12, float m13,
                                float m20, float m21, float m22, float m23,
                                float m30, float m31, float m32, float m33)
            : m_data({m00, m01, m02, m03,
                      m10, m11, m12, m13,
                      m20, m21, m22, m23,
                      m30, m31, m32, m33})
        // clang-format on
        {
        }

        constexpr static inline Mat4 identity()
        {
            // clang-format off
            return Mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            );
            // clang-format on
        }

        constexpr static inline Mat4 perspective(float fov, float aspect, float near, float far)
        {
            // clang-format off
            return Mat4(
                1.0f / (aspect * std::tan(fov / 2.0f)),    0.0f,                          0.0f,                               0.0f,
                0.0f,                                      1.0f / std::tan(fov / 2.0f),   0.0f,                               0.0f,
                0.0f,                                      0.0f,                          far / (near - far),                   -1.0f,
                0.0f,                                      0.0f,                          -((far * near) / far - near),             0.0f
            );
            // clang-format on
        }

        constexpr static inline Mat4 orthographic(float left, float right, float bottom, float top, float near, float far)
        {
            // clang-format off
            return Mat4(
                2.0f / (right - left),           0.0f,                            0.0f,                               0.0f,
                0.0f,                            2.0f / (top - bottom),           0.0f,                               0.0f,
                0.0f,                            0.0f,                            2.0f / (far - near),                0.0f,
                (left + right) / (left - right), (bottom + top) / (bottom - top), (near + far) / (near - far),        1.0f
            );
            // clang-format on
        }

        // Getter
        [[nodiscard]] constexpr inline float *operator[](int row)
        {
            return m_data.m[row];
        }
        [[nodiscard]] constexpr inline const float *operator[](int row) const
        {
            return m_data.m[row];
        }

        [[nodiscard]] constexpr inline float *data()
        {
            return m_data.mm;
        }
        [[nodiscard]] constexpr inline const float *data() const
        {
            return m_data.mm;
        }

        // Operators

        Mat4 operator*(const Mat4 &other) const;

        bool operator==(const Mat4 &other) const;
        bool operator!=(const Mat4 &other) const;

        // Methods
        /** In-place translation */
        void translate(const Vec3 &other);
        /** const translation, returns a new matrix */
        static Mat4 translate(const Mat4 &matrix, const Vec3 &other);

        /** In-place rotation */
        void rotate(const Quat &rotation);
        /** const rotation, returns a new matrix */
        static Mat4 rotate(const Mat4 &matrix, const Quat &rotation);

        /** In-place scaling */
        void scale(const Vec3 &scale);
        /** const scaling, returns a new matrix */
        static Mat4 scale(const Mat4 &matrix, const Vec3 &scale);

#ifdef PRETTY_PRINT_MAT4
        // Overloads for pretty printing
        friend std::ostream &operator<<(std::ostream &os, const Mat4 &m);
        friend std::string   operator+(const std::string &str, const Mat4 &m);
        friend std::string  &operator+=(std::string &str, const Mat4 &m);
#endif
    };

} // namespace rg