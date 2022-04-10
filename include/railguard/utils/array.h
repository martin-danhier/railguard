#pragma once

#include <railguard/utils/array_like.h>

#include <cstddef>
#include <iterator>

namespace rg
{
    /** Simple array of dynamic size. */
    template<typename T>
    class Array : public ArrayLike<T>
    {
      private:
        using Base = ArrayLike<T>;

        size_t m_count = 0;
        T     *m_data  = nullptr;

      public:
        // Constructor

        Array() = default;

        explicit Array(size_t count) : m_count(count), m_data(new T[count])
        {
            // Set all elements to zero
            for (size_t i = 0; i < m_count; ++i)
            {
                m_data[i] = T();
            }
        }

        Array(const Array &other)
        {
            // Deep copy
            m_count = other.m_count;
            m_data  = new T[m_count];
            for (size_t i = 0; i < m_count; ++i)
            {
                m_data[i] = other.m_data[i];
            }
        }

        Array(Array &&other) noexcept
        {
            // Move
            m_count       = other.m_count;
            m_data        = other.m_data;
            other.m_count = 0;
            other.m_data  = nullptr;
        }

        Array &operator=(const Array &other)
        {
            if (this == &other)
                return *this;

            // Call destructor to clean this array before copying
            this->~Array();

            // Deep copy
            m_count = other.m_count;
            m_data  = new T[m_count];
            for (size_t i = 0; i < m_count; ++i)
            {
                m_data[i] = other.m_data[i];
            }
            return *this;
        }

        Array &operator=(Array &&other) noexcept
        {
            if (this == &other)
                return *this;

            // Call destructor to clean this array before moving
            this->~Array();

            // Move
            m_count       = other.m_count;
            m_data        = other.m_data;
            other.m_count = 0;
            other.m_data  = nullptr;
            return *this;
        }

        // List initializer
        Array(const std::initializer_list<T> &list) : m_count(list.size()), m_data(new T[m_count])
        {
            // Copy elements
            size_t i = 0;
            for (auto &elem : list)
            {
                m_data[i] = std::move(elem);
                ++i;
            }
        }

        ~Array()
        {
            if (m_data != nullptr)
            {
                // Call destructor on all elements
                for (size_t i = 0; i < m_count; ++i)
                {
                    m_data[i].~T();
                }

                delete[] m_data;
                m_data  = nullptr;
                m_count = 0;
            }
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
        [[nodiscard]] size_t size() const
        {
            return m_count;
        }

        [[nodiscard]] T *data()
        {
            return m_data;
        }

        [[nodiscard]] const T *data() const
        {
            return m_data;
        }

        // Other methods
        bool includes(const T &value) const
        {
            for (const auto &v : *this)
            {
                if (v == value)
                {
                    return true;
                }
            }
            return false;
        }

    };

} // namespace rg