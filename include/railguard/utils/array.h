#pragma once

#include <cstddef>

namespace rg
{
    /** Simple array of dynamic size. */
    template<typename T>
    class Array
    {
      private:
        size_t m_count {};
        T     *m_data;

      public:
        // Constructor
        explicit Array(size_t count) : m_count(count), m_data(new T[count])
        {
        }

        ~Array()
        {
            delete[] m_data;
        }

        // Operators
        T &operator[](size_t index)
        {
            return m_data[index];
        }

        const T &operator[](size_t index) const
        {
            return m_data[index];
        }

        // Getters
        [[nodiscard]] size_t count() const
        {
            return m_count;
        }

        [[nodiscard]] T *data() const
        {
            return m_data;
        }
    };

} // namespace rg