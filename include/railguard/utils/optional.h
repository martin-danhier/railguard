#pragma once

#include <algorithm>
#include <stdexcept>

namespace rg
{
    template<typename T>
    class Optional
    {
      private:
        bool m_has_value;
        T    m_value;

      public:
        Optional() : m_has_value(false)
        {
        }
        explicit Optional(const T &value) : m_has_value(true), m_value(value)
        {
        }
        explicit Optional(T &&value) : m_has_value(true), m_value(std::move(value))
        {
        }

        [[nodiscard]] bool has_value() const
        {
            return m_has_value;
        }

        /**
         * @brief Returns the value of the optional. Throws an exception if the optional is empty.
         */
        const T &value() const
        {
            if (!m_has_value)
            {
                throw std::runtime_error("Optional has no value");
            }
            return m_value;
        }
        T &value()
        {
            if (!m_has_value)
            {
                throw std::runtime_error("Optional has no value");
            }
            return m_value;
        }

        /**
         * Takes the value of the optional and returns it.
         * The optional is then empty.
         * */
        T &&take()
        {
            if (!m_has_value)
            {
                throw std::runtime_error("Optional has no value");
            }
            m_has_value = false;
            return std::move(m_value);
        }

        T &expect(const std::string &msg)
        {
            if (!m_has_value)
            {
                throw std::runtime_error(msg);
            }
            return m_value;
        }

        T& operator*()
        {
            return value();
        }

        const T& operator*() const
        {
            return value();
        }

        T* operator->()
        {
            return &value();
        }

        const T* operator->() const
        {
            return &value();
        }
    };

    /** Optional variant that doesn't hold the data itself, but a pointer to it. */
    template<typename T> class OptionalPtr {
      private:
        Optional<T*> m_optional;
      public:
        OptionalPtr() = default;
        explicit OptionalPtr(T &value) : m_optional(&value) {}
        OptionalPtr(const OptionalPtr&) = default;
        OptionalPtr(OptionalPtr&&)  noexcept = default;
        OptionalPtr& operator=(const OptionalPtr&) = default;
        OptionalPtr& operator=(OptionalPtr&&)  noexcept = default;
        ~OptionalPtr() = default;
        [[nodiscard]] bool has_value() const { return m_optional.has_value(); }
        T &value() const { return *m_optional.value(); }
        T &expect(const std::string &msg) { return *m_optional.expect(msg); }
        T *operator->() { return m_optional.value(); }
        const T *operator->() const { return m_optional.value(); }
        T &operator*() { return *m_optional.value(); }
        const T &operator*() const { return *m_optional.value(); }
    };

    // Some functions to have an easier time creating optional values

    template<typename T>
    Optional<T> some(const T &value)
    {
        return Optional<T>(value);
    }

    template<typename T>
    Optional<T> some(T &&value)
    {
        return Optional<T>(std::forward<T>(value));
    }

    template<typename T>
    Optional<T> none()
    {
        return Optional<T>();
    }
} // namespace rg
