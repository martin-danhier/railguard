#pragma once

#include <railguard/utils/hash_map.h>
#include <railguard/utils/optional.h>
#include <railguard/utils/vector.h>

namespace rg
{
    template<typename T>
    class Map
    {
      public:
        // Types
        using Key                     = typename HashMap::Key;
        constexpr static Key NULL_KEY = HashMap::NULL_KEY;

        class Entry
        {
          private:
            T   m_value;
            Key m_key;

          public:
            Entry(const Key &key, const T &value) : m_key(key), m_value(value)
            {
            }
            explicit Entry(const Key &key) : m_key(key)
            {
            }
            Entry(const Key &key, T &&value) : m_key(key), m_value(std::move(value))
            {
            }
            [[nodiscard]] inline Key key() const
            {
                return m_key;
            }
            [[nodiscard]] inline T &value()
            {
                return m_value;
            }
            [[nodiscard]] inline const T &value() const
            {
                return m_value;
            }
        };
      private:

        // Hash map to store indices of the elements
        HashMap m_hash_map;
        // Storage to store the actual data
        Vector<Entry> m_storage;

      public:
        // Constructors

        explicit Map() : m_hash_map(), m_storage(sizeof(Entry))
        {
        }

        Map(const Map &other) : m_hash_map(other.m_hash_map), m_storage(other.m_storage)
        {
        }

        Map(Map &&other) noexcept : m_hash_map(std::move(other.m_hash_map)), m_storage(std::move(other.m_storage))
        {
        }

        // All destruction is handled by hash_map and vector ! :D

        // Methods

        [[nodiscard]] inline size_t count() const
        {
            return m_hash_map.count();
        }

        [[nodiscard]] inline bool is_empty() const
        {
            return m_hash_map.is_empty();
        }

        [[nodiscard]] inline bool exists(const Key &key) const
        {
            return m_hash_map.exists(key);
        }

        OptionalPtr<const T> get(const Key &key) const
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                return OptionalPtr(m_storage[res.value()->as_size].value());
            }
            return OptionalPtr<const T>();
        }

        /** Get a mutable reference to a value by its key.
         * This can be useful if many modifications are needed to the value, to avoid having to look it up again.
         *
         * However, be cautious: the returned value is actually a smart pointer to the value. If the value is modified,
         * the pointer may not point to the right value anymore. Thus, only use the reference in the immediate scope and don't call
         * methods that can move values (e.g remove).
         */
        OptionalPtr<T> get(const Key &key)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                return OptionalPtr(m_storage[res.value()->as_size].value());
            }
            return OptionalPtr<T>();
        }

        void set(const Key &key, const T &value)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                m_storage[res.value()->as_size].m_data = value;
            }
            else
            {
                auto index = m_storage.size();
                m_storage.push_back(Entry {value, key});
                m_hash_map.set(key, HashMap::Value {index});
            }
        }

        void set(const Key &key, T &&value)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                m_storage[res.value()->as_size].value() = std::move(value);
            }
            else
            {
                auto index = m_storage.size();
                m_storage.push_back(Entry {key, std::move(value)});
                m_hash_map.set(key, HashMap::Value {index});
            }
        }

        void remove(const Key &key)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                auto index = res.value()->as_size;
                // Move the last element to the index of the removed element
                m_storage.remove_at(index);
                // Remove deleted key from hash map
                m_hash_map.remove(key);
                // Update the entry in the hash map for the moved element
                if (index < m_storage.size())
                {
                    m_hash_map.set(m_storage[index].key(), HashMap::Value {index});
                }
            }
        }

        void clear()
        {
            m_hash_map.clear();
            m_storage.clear();
        }

        // Operators

        Map &operator=(const Map &other)
        {
            m_hash_map = other.m_hash_map;
            m_storage  = other.m_storage;
            return *this;
        }

        Map &operator=(Map &&other) noexcept
        {
            m_hash_map = std::move(other.m_hash_map);
            m_storage  = std::move(other.m_storage);
            return *this;
        }

        /** Shorthand for accesses: gets the slot of the key.
         *
         * If the key exists, it returns the existing slot. If it does not exist, it creates a new slot and returns it
         * (requires a default constructor for T).
         */
        T &operator[](const Key &key)
        {
            auto res = m_hash_map.get(key);

            if (res.has_value())
            {
                return m_storage[res.take()->as_size].value();
            }
            else
            {
                auto index = m_storage.size();
                m_storage.push_back(Entry {key});
                m_hash_map.set(key, HashMap::Value {index});
                return m_storage[index].value();
            }
        }

        // Iterator: we just use the vector iterator, no need to touch the map

        class Iterator
        {
          public:
            // Traits
            using difference_type   = ptrdiff_t;
            using value_type        = Entry;
            using pointer           = value_type *;
            using reference         = value_type &;
            using iterator_category = std::forward_iterator_tag;

          private:
            typename Vector<value_type>::iterator m_it;

          public:
            // Constructors
            explicit Iterator(const typename Vector<value_type>::iterator &&it) : m_it(std::move(it))
            {
            }

            // Operators
            Iterator &operator++()
            {
                ++m_it;
                return *this;
            }

            bool operator!=(const Iterator &other) const
            {
                return m_it != other.m_it;
            }

            // Accessors
            reference operator*()
            {
                return *m_it;
            }

            pointer operator->() const
            {
                return &(operator*());
            }
        };

        Iterator begin() const
        {
            return Iterator(m_storage.begin());
        }

        Iterator end() const
        {
            return Iterator(m_storage.end());
        }
    };
} // namespace rg