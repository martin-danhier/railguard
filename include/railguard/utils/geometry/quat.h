#pragma once

namespace rg
{
    class Mat4;

    class Quat
    {
      private:
        union Data
        {
            struct
            {
                float r, i, j, k;
            };
            float data[4];
        };

        Data m_data;

      public:
        // Constructors
        constexpr inline Quat(float r, float i, float j, float k) : m_data {r, i, j, k}
        {
        }
        constexpr static inline Quat identity()
        {
            return {1, 0, 0, 0};
        }
        // Getters
        constexpr inline float &r()
        {
            return m_data.r;
        }
        [[nodiscard]] constexpr inline const float &r() const
        {
            return m_data.r;
        }

        constexpr inline float &i()
        {
            return m_data.i;
        }
        [[nodiscard]] constexpr inline const float &i() const
        {
            return m_data.i;
        }

        constexpr inline float &j()
        {
            return m_data.j;
        }
        [[nodiscard]] constexpr inline const float &j() const
        {
            return m_data.j;
        }

        constexpr inline float &k()
        {
            return m_data.k;
        }
        [[nodiscard]] constexpr inline const float &k() const
        {
            return m_data.k;
        }

        constexpr inline float &operator[](int index)
        {
            return m_data.data[index];
        }
        [[nodiscard]] constexpr inline const float &operator[](int index) const
        {
            return m_data.data[index];
        }

        // Operators

        [[nodiscard]] constexpr inline Quat operator*(float scalar) const {
            return {m_data.r * scalar, m_data.i * scalar, m_data.j * scalar, m_data.k * scalar};
        }

        [[nodiscard]] constexpr inline Quat operator/(float scalar) const {
            return {m_data.r / scalar, m_data.i / scalar, m_data.j / scalar, m_data.k / scalar};
        }

        // Methods

        [[nodiscard]] constexpr inline Quat conjugate() const
        {
            return {m_data.r, -m_data.i, -m_data.j, -m_data.k};
        }

        [[nodiscard]] float norm() const;

        [[nodiscard]] constexpr inline Quat normalize() const {
            return *this / norm();
        }

        [[nodiscard]] Mat4 to_mat4() const;

    };
} // namespace rg