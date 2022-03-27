#pragma once

#include <railguard/utils/impl/vector_impl.h>

#include <stdexcept>

namespace rg
{
    template<typename T>
    class Vector
    {
        _impl::VectorImpl m_impl;

      public:
        explicit Vector(size_t initial_capacity = 0) : m_impl(initial_capacity, sizeof(T))
        {
        }

        Vector(const Vector &other) : m_impl(other.m_impl)
        {
        }

        Vector(Vector &&other) noexcept : m_impl(std::move(other.m_impl))
        {
        }

        Vector &operator=(const Vector &other)
        {
            m_impl = _impl::VectorImpl(other.m_impl);
            return *this;
        }

        Vector &operator=(Vector &&other) noexcept
        {
            m_impl = std::move(other.m_impl);
            return *this;
        }

        ~Vector()
        {
            // Before the vector is destroyed, we need to manually call the destructor of each element
            clear();
        }

        inline void ensure_capacity(size_t required_minimum_capacity)
        {
            m_impl.ensure_capacity(required_minimum_capacity);
        }

        // Push back

        inline void push_back(T &value)
        {
            T& slot = *static_cast<T*>(m_impl.push_slot());
            slot = value;
        }

        inline void push_back(T &&value)
        {
            // Same as above
            T& slot = *static_cast<T*>(m_impl.push_slot());
            slot = std::move(value);
        }

        // Pop back
        void pop_back()
        {
            if (m_impl.size() > 0)
            {
                // Destroy the last element
                static_cast<T *>(m_impl.get_element(m_impl.size() - 1))->~T();

                m_impl.pop_back();
            }
        }

        // Operators

        T &operator[](size_t index)
        {
            auto ptr = static_cast<T *>(m_impl.get_element(index));
            if (ptr == nullptr)
            {
                throw std::out_of_range("Vector index out of range");
            }
            return *ptr;
        }

        const T &operator[](size_t index) const
        {
            auto ptr = static_cast<T *>(m_impl.get_element(index));
            if (ptr == nullptr)
            {
                throw std::out_of_range("Vector index out of range");
            }
            return *ptr;
        }

        // Getters

        [[nodiscard]] inline size_t size() const
        {
            return m_impl.size();
        }

        [[nodiscard]] inline size_t capacity() const
        {
            return m_impl.capacity();
        }

        T &last()
        {
            if (m_impl.size() == 0)
            {
                throw std::out_of_range("Vector is empty");
            }

            return m_impl.last_element();
        }

        const T &last() const
        {
            if (m_impl.size() == 0)
            {
                throw std::out_of_range("Vector is empty");
            }

            return m_impl.last_element();
        }

        [[nodiscard]] inline bool is_empty() const
        {
            return m_impl.is_empty();
        }

        // Clear

        void clear()
        {
            // Call the destructor of each element
            // TODO replace with iterator
            for (size_t i = 0; i < m_impl.size(); ++i)
            {
                static_cast<T *>(m_impl.get_element(i))->~T();
            }

            // Clear the vector
            m_impl.clear();
        }
    };

} // namespace rg
