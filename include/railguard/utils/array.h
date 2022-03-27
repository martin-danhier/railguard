#pragma once

namespace rg
{
    /** Simple array of dynamic size. */
    template<typename T>
    class Array
    {
      private:
        size_t m_count {};
        T     *m_data;

      public:
        // Constructor
        explicit Array(size_t count) : m_count(count), m_data(new T[count])
        {
        }

        ~Array()
        {
            delete[] this->data;
        }

        // Operators
        T &operator[](size_t index)
        {
            return this->data[index];
        }

        const T &operator[](size_t index) const
        {
            return this->data[index];
        }

        // Getters
        [[nodiscard]] size_t count() const
        {
            return this->m_count;
        }

        [[nodiscard]] T *data() const
        {
            return this->m_data;
        }
    };

} // namespace rg