#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace rg
{
    template<typename T>
    class Optional;

    /**
     * A hash map stores a set of key-value pairs. Unlike most languages, both the key and the value are 64-bit integers.
     *
     * In particular, the value is as long as a size_t, meaning it can store an index or a pointer. This is useful in combination with
     * a vector to store actual m_data. See the StructMap for an implementation of this.
     */
    class HashMap
    {
      private:
        struct Data;

        // Store opaque fields for implementation
        Data *m_data;

        // Private method
        void expand();

      public:
        typedef uint64_t     Key;
        constexpr static Key NULL_KEY = 0;
        typedef union
        {
            size_t as_size;
            void  *as_ptr;
        } Value;
        struct Entry
        {
            HashMap::Key   key;
            HashMap::Value value;
        };

        // Constructors & destructors

        HashMap();
        HashMap(const HashMap &other);
        HashMap(HashMap &&other) noexcept;
        ~HashMap();

        // Operators

        HashMap &operator=(const HashMap &other);
        HashMap &operator=(HashMap &&other) noexcept;

        // Methods

        [[nodiscard]] Optional<Value *> get(const Key &key) const;
        void                            set(const Key &key, const Value &value);
        void                            remove(const Key &key);
        [[nodiscard]] bool              exists(const Key &key) const;
        void                            clear();
        [[nodiscard]] size_t            count() const;
        [[nodiscard]] bool              is_empty() const;

        // Iterator
        class Iterator
        {
          private:
            const HashMap *m_map;
            size_t         m_index;
            Entry         *m_entry;

          public:
            // Iterator traits
            using difference_type   = std::ptrdiff_t;
            using value_type        = HashMap::Entry;
            using pointer           = value_type *;
            using reference         = value_type &;
            using iterator_category = std::forward_iterator_tag;

            // Constructors & destructors
            Iterator(const HashMap *map, size_t index);

            // Operators
            Entry     operator*();
            Iterator &operator++();
            bool      operator!=(const Iterator &other) const;
        };

        [[nodiscard]] Iterator begin() const;
        [[nodiscard]] Iterator end() const;
    };
} // namespace rg