#pragma once

#include <cstddef>

namespace rg::_impl
{
    /**
     * Implementation doesn't care about the type of the value, reduces size of produced code
     **/
    class VectorImpl
    {
      private:
        /** @brief Number of elements in the vector.
         *  @invariant Smaller or equal than capacity.
         */
        size_t m_count;
        /** @brief Maximum number of elements that the allocation can fit without resizing.
         *  A value of 0 indicates that the vector is not allocated (data is NULL).
         */
        size_t m_capacity;
        /** @brief Size of a single element. */
        size_t m_element_size;
        /** @brief Pointer to the first element of the vector
         *  @invariant Is NULL if capacity is 0, otherwise there is enough memory allocated after the pointed location to contain
         *  capacity * element_size bytes.
         */
        char *m_data = nullptr;
        /**
         * Value defining by how much the vector should grow if a push doesn't have enough space.
         */
        size_t m_growth_amount;

      public:
        VectorImpl(size_t initial_capacity, size_t element_size);
        VectorImpl(const VectorImpl &other);
        VectorImpl(VectorImpl &&other) noexcept;
        ~VectorImpl();

        // Operators
        VectorImpl &operator=(const VectorImpl &other);
        VectorImpl &operator=(VectorImpl &&other) noexcept;

        void  ensure_capacity(size_t required_minimum_capacity);
        void *push_slot();
        /**
         * Pops the last slot from the vector. The data is not freed, and it is assumed that there is at least one element in the
         * vector.
         */
        inline void pop_back()
        {
            m_count--;
        }
        [[nodiscard]] inline size_t last_index() const
        {
            return m_count - 1;
        }

        [[nodiscard]] void *last_element() const;

        [[nodiscard]] inline bool is_empty() const
        {
            return m_count == 0;
        }

        [[nodiscard]] inline size_t size() const
        {
            return m_count;
        }

        [[nodiscard]] inline size_t capacity() const
        {
            return m_capacity;
        }

        [[nodiscard]] void *get_element(size_t index) const;

        bool  copy(size_t src_pos, size_t dst_pos);

        inline void  clear() {
            m_count = 0;
        }
    };

} // namespace rg::_impl