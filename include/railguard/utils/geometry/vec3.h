#pragma once

namespace rg
{
    struct Vec3
    {
      private:
        union
        {
            struct
            {
                float x, y, z;
            };
            float data[3];
        } m_data;

      public:
        // Constructor
        inline constexpr Vec3(float x, float y, float z) : m_data{x, y, z}
        {
        }
        constexpr static inline Vec3 zero()
        {
            return {0.0f, 0.0f, 0.0f};
        }
        constexpr static inline Vec3 one()
        {
            return {1.0f, 1.0f, 1.0f};
        }
        // Getters
        [[nodiscard]] inline float &x()
        {
            return m_data.x;
        }
        [[nodiscard]] inline const float &x() const
        {
            return m_data.x;
        }

        [[nodiscard]] inline float &y()
        {
            return m_data.y;
        }
        [[nodiscard]] inline const float &y() const
        {
            return m_data.y;
        }

        [[nodiscard]] inline float &z()
        {
            return m_data.z;
        }
        [[nodiscard]] inline const float &z() const
        {
            return m_data.z;
        }

        [[nodiscard]] inline float &operator[](int i)
        {
            return m_data.data[i];
        }
        [[nodiscard]] inline const float &operator[](int i) const
        {
            return m_data.data[i];
        }

    };
} // namespace rg