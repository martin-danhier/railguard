#pragma once

#include <railguard/utils/array_like.h>
#include <railguard/utils/optional.h>

#include <cstddef>
#include <iostream>
#include <iterator>

namespace rg
{
    /** Simple array of dynamic size. */
    template<typename T>
    class Array : public ArrayLike<T>
    {
      private:
        size_t m_count = 0;
        T     *m_data  = nullptr;

      public:
        // Constructor

        constexpr Array() : m_count(0), m_data(nullptr)
        {
        }

        explicit Array(size_t count) : m_count(count), m_data(nullptr)
        {
            if (count > 0)
            {
                m_data = new T[count];

                // Set all elements to zero
                for (size_t i = 0; i < m_count; ++i)
                {
                    T *slot = &m_data[i];
                    new (slot) T();
                }
            }
        }

        Array(const Array &other)
        {
            // Deep copy
            m_count = other.m_count;
            m_data  = new T[m_count];
            for (size_t i = 0; i < m_count; ++i)
            {
                T *slot = &m_data[i];
                new (slot) T(other.m_data[i]);
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
                T *slot = &m_data[i];
                new (slot) T(elem);
                ++i;
            }
        }
        Array(std::initializer_list<T> &&list) : m_count(list.size()), m_data(new T[m_count])
        {
            // Copy elements
            size_t i = 0;
            for (auto &elem : list)
            {
                T *slot = &m_data[i];
                new (slot) T(std::move(elem));
                ++i;
            }
        }

        ~Array()
        {
            if (m_data != nullptr)
            {
                // Call destructor on all elements
                for (const auto &elem : *this)
                {
                    elem.~T();
                }

                delete[] m_data;
                m_data  = nullptr;
                m_count = 0;
            }
        }

        // Operators
        T &operator[](size_t index)
        {
            if (index >= m_count)
            {
                throw std::out_of_range("Array index out of range");
            }
            return m_data[index];
        }

        const T &operator[](size_t index) const
        {
            if (index >= m_count)
            {
                throw std::out_of_range("Array index out of range");
            }
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

        void fill(const T &value)
        {
            for (size_t i = 0; i < m_count; ++i)
            {
                // They all exist so we use the update operator
                m_data[i] = value;
            }
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

        Optional<size_t> find_first_of(const T &value) const
        {
            for (size_t i = 0; i < m_count; ++i)
            {
                if (m_data[i] == value)
                {
                    return Optional {i};
                }
            }
            return {};
        }
    };

} // namespace rg