#pragma once

#include <railguard/utils/map.h>

namespace rg
{
    template<typename T>
    class Storage
    {
      public:
        typedef uint64_t    Id;
        constexpr static Id NULL_ID = 0;

      private:
        Id     m_id_counter = 0;
        Map<T> m_map;

      public:
        Id push(const T &value)
        {
            // Increment counter
            m_id_counter++;

            // Set value
            m_map.set(m_id_counter, value);
            return m_id_counter;
        }

        Id push(T &&value)
        {
            // Increment counter
            m_id_counter++;

            // Set value
            m_map.set(m_id_counter, std::move(value));
            return m_id_counter;
        }

        OptionalPtr<T> get(Id id)
        {
            return m_map.get(id);
        }

        OptionalPtr<const T> get(Id id) const
        {
            return m_map.get(id);
        }

        T &operator[](Id id)
        {
            return get(id).expect("No element with id " + std::to_string(id) + ".");
        }

        void remove(Id id)
        {
            m_map.remove(id);
        }

        void clear()
        {
            m_map.clear();
        }

        [[nodiscard]] bool is_empty() const
        {
            return m_map.is_empty();
        }

        [[nodiscard]] size_t count() const
        {
            return m_map.count();
        }

        [[nodiscard]] bool exists(Id id) const
        {
            return m_map.exists(id);
        }

        // Iterator
        class Iterator
        {
          public:
            // Traits
            using iterator_category = std::forward_iterator_tag;
            using value_type        = typename Map<T>::Entry;
            using difference_type   = std::ptrdiff_t;
            using pointer           = value_type *;
            using reference         = value_type &;

          private:
            typename Map<T>::Iterator m_it;

          public:
            // Contents

            explicit Iterator(typename Map<T>::Iterator &&it) : m_it(std::move(it))
            {
            }

            Iterator &operator++()
            {
                ++m_it;
                return *this;
            }

            [[nodiscard]] bool operator!=(const Iterator &other) const
            {
                return m_it != other.m_it;
            }

            // Access

            [[nodiscard]] reference operator*()
            {
                return *m_it;
            }

            [[nodiscard]] pointer operator->() const
            {
                return &*m_it;
            }
        };

        [[nodiscard]] Iterator begin() const
        {
            return Iterator(m_map.begin());
        }

        [[nodiscard]] Iterator end() const
        {
            return Iterator(m_map.end());
        }
    };

} // namespace rg
