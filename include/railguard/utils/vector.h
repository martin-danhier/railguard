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
        explicit Vector(size_t initial_capacity = 1) : m_impl(initial_capacity, sizeof(T))
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
            // Don't do this if the vector is not valid (for example if its buffer was moved to another vector)
            if (m_impl.is_valid())
            {
                clear();
            }
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

        bool operator==(const Vector &other) const
        {
            return &m_impl == &other.m_impl;
        }

        bool operator!=(const Vector &other) const
        {
            return &m_impl != &other.m_impl;
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

        // Copy

        /**
         * Copies the value at index src_pos to the element at index dst_pos.
         * @param src_pos index of the copied element in the vector.
         * @param dst_pos index of the element where the src element will be copied
         * @return true if it worked, false otherwise
         * @example With an array comparison, it would look like: arr[dst_pos] = arr[src_pos]
         */
        bool copy(size_t src_pos, size_t dst_pos) {
            if (src_pos >= size() || dst_pos >= size())
            {
                // The indexes must be valid
                return false;
            }

            // Don't bother if the items are the same index, it's already done
            if (src_pos == dst_pos)
            {
                return true;
            }

            T& src_slot = *static_cast<T *>(m_impl.get_element(src_pos));
            T& dst_slot = *static_cast<T *>(m_impl.get_element(dst_pos));

            // Do the copy
            // The "copy" operator should be called
            dst_slot = src_slot;
            return true;
        }

        // iterator
        class iterator
        {
          private:
            T* m_ptr;
            size_t m_index;
            const _impl::VectorImpl *m_impl;
          public:

            // Traits
            using difference_type = ptrdiff_t;
            using value_type = T;
            using pointer = value_type *;
            using reference = value_type &;
            using iterator_category = std::bidirectional_iterator_tag;

            iterator(const Vector &vec, size_t startIndex): m_ptr(nullptr), m_index(startIndex), m_impl(&vec.m_impl) {
                if (m_impl->is_valid() && m_index < m_impl->size())
                {
                    m_ptr = static_cast<T*>(m_impl->get_element(m_index));
                }
            }

            // Operators
            bool operator==(const iterator &other) const
            {
                return m_impl == other.m_impl && m_index == other.m_index;
            }

            bool operator!=(const iterator &other) const
            {
                return m_impl != other.m_impl || m_index != other.m_index;
            }

            iterator &operator++()
            {
                if (m_ptr != nullptr)
                {
                    m_index++;
                    if (m_index < m_impl->size())
                    {
                        m_ptr++;
                    }
                    else
                    {
                        m_ptr = nullptr;
                    }
                }
                return *this;
            }

            iterator &operator--()
            {
                if (m_ptr != nullptr)
                {
                    m_index--;
                    if (m_index > 0)
                    {
                        m_ptr--;
                    }
                    else
                    {
                        m_ptr = nullptr;
                    }
                }

                return *this;
            }

            reference operator*()
            {
                return *m_ptr;
            }

            pointer operator->()
            {
                return m_ptr;
            }

        };

        iterator begin() const
        {
            return iterator(*this, 0);
        }

        iterator end() const
        {
            return iterator(*this, m_impl.size());
        }

        // Clear

        void clear()
        {
            // Call the destructor of each element
            for (const auto &elem : *this)
            {
                elem.~T();
            }

            // Clear the vector
            m_impl.clear();
        }

        // Erase

        void remove_at(size_t index) {
            if (index > size())
            {
                // The index is not in the vector, so it is already erased, in a way
                return;
            }
            else if (index == size() - 1)
            {
                // The last element is erased, so we can just pop_back
                pop_back();
            }
            else
            {
                // The element is not the last one, so we need to move the last element to the index and pop_back
                T& elem = *static_cast<T *>(m_impl.get_element(index));
                T& last_elem = *static_cast<T *>(m_impl.get_element(size() - 1));

                // We will destroy the last element, so we can move it
                elem = std::move(last_elem);

                // We can now pop_back
                pop_back();
            }
        }
    };

} // namespace rg
