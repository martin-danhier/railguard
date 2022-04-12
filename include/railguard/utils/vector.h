#pragma once

#include <railguard/utils/array_like.h>
#include <railguard/utils/impl/vector_impl.h>
#include <railguard/utils/optional.h>

namespace rg
{
    template<typename T>
    class Vector : public ArrayLike<T>
    {
        using Base = ArrayLike<T>;

        _impl::VectorImpl m_impl;

      public:
        Vector() = default;

        explicit Vector(size_t initial_capacity) : m_impl(initial_capacity, sizeof(T))
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

        // Initializer list
        Vector(std::initializer_list<T> list) : m_impl(list.size(), sizeof(T))
        {
            for (auto &elem : list)
            {
                push_back(elem);
            }
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

        inline T *data()
        {
            return static_cast<T *>(m_impl.data());
        }

        inline const T *data() const
        {
            return static_cast<const T *>(m_impl.data());
        }

        // Push back

        inline void push_back(const T &value)
        {
            auto *slot = static_cast<T *>(m_impl.push_slot());
            // Since this is a new slot, the value is not initialized so there is no need to call the constructor
            new (slot) T(value);
        }

        inline void push_back(T &&value)
        {
            // Same as above
            auto *slot = static_cast<T *>(m_impl.push_slot());
            new (slot) T(std::move(value));
        }

        // Extend

        inline void extend(const Vector<T> &other)
        {
            ensure_capacity(m_impl.size() + other.m_impl.size());

            for (size_t i = 0; i < other.m_impl.size(); i++)
            {
                push_back(other[i]);
            }
        }

        inline void extend(Vector<T> &&other)
        {
            ensure_capacity(m_impl.size() + other.m_impl.size());

            for (size_t i = 0; i < other.m_impl.size(); i++)
            {
                push_back(std::move(other[i]));
            }
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

        inline T &last()
        {
            if (m_impl.size() == 0)
            {
                throw std::out_of_range("Vector is empty");
            }

            return *static_cast<T *>(m_impl.last_element());
        }

        inline const T &last() const
        {
            if (m_impl.size() == 0)
            {
                throw std::out_of_range("Vector is empty");
            }

            return *static_cast<const T *>(m_impl.last_element());
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
        bool copy(size_t src_pos, size_t dst_pos)
        {
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

            T &src_slot = *static_cast<T *>(m_impl.get_element(src_pos));
            T &dst_slot = *static_cast<T *>(m_impl.get_element(dst_pos));

            // Do the copy
            // The "copy" operator should be called
            dst_slot = src_slot;
            return true;
        }

        Optional<size_t> index_of(const T &value) const
        {
            for (size_t i = 0; i < size(); i++)
            {
                const auto &val = *static_cast<T *>(m_impl.get_element(i));
                if (val == value)
                {
                    return some<size_t>(i);
                }
            }
            return none<size_t>();
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

        void remove_at(size_t index)
        {
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
                T &elem      = *static_cast<T *>(m_impl.get_element(index));
                T &last_elem = *static_cast<T *>(m_impl.get_element(size() - 1));

                // We will destroy the last element, so we can move it
                elem = std::move(last_elem);

                // We can now pop_back
                pop_back();
            }
        }

        void remove(const T &elem)
        {
            // Find the element
            auto res = index_of(elem);
            if (res.has_value())
            {
                remove_at(res.take());
            }
        }
    };

} // namespace rg
