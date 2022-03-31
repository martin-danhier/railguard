#pragma once

#include <cstddef>
#include <iterator>

namespace rg
{
    /** Simple array of dynamic size. */
    template<typename T>
    class Array
    {
      private:
        size_t m_count {};
        T     *m_data = nullptr;

      public:
        // Constructor
        explicit Array(size_t count) : m_count(count), m_data(new T[count])
        {
            // Set all elements to zero
            for (size_t i = 0; i < m_count; ++i)
            {
                m_data[i] = T();
            }
        }

        Array(const Array &other) {
            // Deep copy
            m_count = other.m_count;
            m_data = new T[m_count];
            for (size_t i = 0; i < m_count; ++i)
            {
                m_data[i] = other.m_data[i];
            }
        }

        Array(Array &&other)  noexcept {
            // Move
            m_count = other.m_count;
            m_data = other.m_data;
            other.m_count = 0;
            other.m_data = nullptr;
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

        // Iterator
        class Iterator {
          public:
            // Traits
            using difference_type = ptrdiff_t;
            using value_type = T;
            using pointer = value_type *;
            using reference = value_type &;
            using iterator_category = std::bidirectional_iterator_tag;
          private:
            T *m_data;
            size_t m_index;
          public:
            Iterator(T *data, size_t index) : m_data(data), m_index(index) {}

            Iterator &operator++() {
                ++m_index;
                return *this;
            }

            Iterator &operator--() {
                --m_index;
                return *this;
            }

            bool operator!=(const Iterator &other) const {
                return m_index != other.m_index;
            }

            T &operator*() {
                return m_data[m_index];
            }

            T *operator->() {
                return &m_data[m_index];
            }
        };

        Iterator begin() const {
            return Iterator(m_data, 0);
        }

        Iterator end() const {
            return Iterator(m_data, m_count);
        }
    };

} // namespace rg