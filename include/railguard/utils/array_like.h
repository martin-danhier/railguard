#pragma once

#include <cstddef>
#include <iterator>

namespace rg
{
    /**
     * Regroups common methods between Arrays and Vectors, so that they can be used interchangeably in functions.
     *
     * Also, this class is cheaper to include.
     * */
    template<typename T>
    class ArrayLike
    {
      public:
        virtual ~ArrayLike()                                        = default;
        virtual T                   *data()                         = 0;
        virtual const T             *data() const                   = 0;
        [[nodiscard]] virtual size_t size() const                   = 0;
        virtual T                   &operator[](size_t index)       = 0;
        virtual const T             &operator[](size_t index) const = 0;

        // Those methods can be used to create an iterator

        class Iterator
        {
          public:
            // Traits
            using difference_type [[maybe_unused]]   = ptrdiff_t;
            using value_type                         = T;
            using pointer                            = value_type *;
            using reference                          = value_type &;
            using iterator_category [[maybe_unused]] = std::bidirectional_iterator_tag;

          private:
            T     *ptr;
            size_t index;
            size_t size;

          public:
            inline Iterator(T *ptr, size_t index, size_t size) : ptr(ptr), index(index), size(size)
            {
            }

            inline Iterator &operator++()
            {
                // It will end after the last element, so the != check returns false
                if (index < size)
                {
                    ++index;
                }
                return *this;
            }

            inline Iterator operator--()
            {
                if (index > 0)
                {
                    --index;
                }
                return *this;
            }

            inline bool operator==(const Iterator &other) const
            {
                return ptr == other.ptr && index == other.index;
            }

            inline bool operator!=(const Iterator &other) const
            {
                return ptr != other.ptr || index != other.index;
            }

            inline reference operator*()
            {
                return ptr[index];
            }

            inline pointer operator->()
            {
                return &ptr[index];
            }
        };

        [[nodiscard]] inline Iterator begin()
        {
            return Iterator(data(), 0, size());
        }

        [[nodiscard]] inline Iterator end()
        {
            return Iterator(data(), size(), size());
        }

        class ConstIterator
        {
          public:
            // Traits
            using difference_type [[maybe_unused]]   = ptrdiff_t;
            using value_type                         = T;
            using pointer                            = const value_type *;
            using reference                          = const value_type &;
            using iterator_category [[maybe_unused]] = std::bidirectional_iterator_tag;

          private:
            const T *ptr;
            size_t   index;
            size_t   size;

          public:
            inline ConstIterator(const T *ptr, size_t index, size_t size) : ptr(ptr), index(index), size(size)
            {
            }

            inline ConstIterator &operator++()
            {
                // It will end after the last element, so the != check returns false
                if (index < size)
                {
                    ++index;
                }
                return *this;
            }

            inline ConstIterator operator--()
            {
                if (index > 0)
                {
                    --index;
                }
                return *this;
            }

            inline bool operator==(const ConstIterator &other) const
            {
                return ptr == other.ptr && index == other.index;
            }

            inline bool operator!=(const ConstIterator &other) const
            {
                return ptr != other.ptr || index != other.index;
            }

            inline reference operator*()
            {
                return ptr[index];
            }

            inline pointer operator->()
            {
                return &ptr[index];
            }
        };

        [[nodiscard]] inline ConstIterator begin() const
        {
            return ConstIterator(data(), 0, size());
        }

        [[nodiscard]] inline ConstIterator end() const
        {
            return ConstIterator(data(), size(), size());
        }
    };
} // namespace rg