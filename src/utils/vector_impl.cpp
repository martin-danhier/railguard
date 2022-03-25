#include "railguard/utils/impl/vector_impl.h"

#include <cstring>
#include <stdexcept>

using namespace rg::_impl;

// Constructor

VectorImpl::VectorImpl(size_t initial_capacity, size_t element_size)
    : m_capacity(initial_capacity),
      m_element_size(element_size),
      m_count(0),
      m_data(new char[m_capacity * m_element_size]),
      m_growth_amount(1)
{
}

VectorImpl::VectorImpl(const VectorImpl &other)
    : m_capacity(other.m_capacity),
      m_element_size(other.m_element_size),
      m_count(other.m_count),
      m_data(new char[m_capacity * m_element_size]),
      m_growth_amount(other.m_growth_amount)
{
    // Copy contents
    memcpy(m_data, other.m_data, m_count * m_element_size);
}

VectorImpl::VectorImpl(VectorImpl &&other) noexcept
    : m_capacity(other.m_capacity),
      m_element_size(other.m_element_size),
      m_count(other.m_count),
      m_data(other.m_data),
      m_growth_amount(other.m_growth_amount)
{
    // Take the other's data without copying
    // To prevent the other from freeing it, we set it to nullptr
    other.m_data = nullptr;
}

VectorImpl &VectorImpl::operator=(const VectorImpl &other)
{
    // Check for self-assignment
    if (this == &other)
        return *this;

    // Free old data
    delete[] m_data;

    // Copy new data
    m_capacity = other.m_capacity;
    m_element_size = other.m_element_size;
    m_count = other.m_count;
    m_data = new char[m_capacity * m_element_size];
    memcpy(m_data, other.m_data, m_count * m_element_size);
    m_growth_amount = other.m_growth_amount;

    return *this;
}

VectorImpl &VectorImpl::operator=(VectorImpl &&other) noexcept
{
    // Check for self-assignment
    if (this == &other)
        return *this;

    // Free old data
    delete[] m_data;

    // Take the other's data without copying
    // To prevent the other from freeing it, we set it to nullptr
    m_capacity = other.m_capacity;
    m_element_size = other.m_element_size;
    m_count = other.m_count;
    m_data = other.m_data;
    m_growth_amount = other.m_growth_amount;
    other.m_data = nullptr;

    return *this;
}


VectorImpl::~VectorImpl()
{
    delete[] m_data;
    m_data = nullptr;
}

// Methods

void VectorImpl::ensure_capacity(size_t required_minimum_capacity)
{
    // Resize if smaller than required capacity
    if (m_capacity < required_minimum_capacity)
    {
        // Grow with the growth amount, except if it does not reach the required capacity
        size_t new_capacity = m_count + m_growth_amount;
        if (required_minimum_capacity > new_capacity)
        {
            new_capacity = required_minimum_capacity;
        }
        else
        {
            // Multiply the growth amount, that way the more we push in a vector, the more it will try to anticipate
            // Same behavior as cpp vector
            m_growth_amount *= 2;
        }

        // Create new buffer
        char *new_data = new char[new_capacity * m_element_size];
        // Copy old data to new buffer
        memcpy(new_data, m_data, m_count * m_element_size);
        // Delete old buffer
        delete[] m_data;

        // Save
        m_data     = new_data;
        m_capacity = new_capacity;
    }
}


void *VectorImpl::push_slot()
{
    // Make sure that there is enough room in the allocation for this new p_data
    ensure_capacity(m_count + 1);

    // Get new slot
    char *element = m_data + (m_count * m_element_size);
    m_count++;

    // Zero the new slot
    memset(element, 0, m_element_size);

    // Return it for manual initialization
    return element;
}

void *VectorImpl::last_element() const
{
    // Return the last element
    return m_data + ((m_count - 1) * m_element_size);
}

void *VectorImpl::get_element(size_t pos) const {
    if (pos < m_count)
    {
        return ((char *) m_data) + (pos * m_element_size);
    }
    else
    {
        return nullptr;
    }
}
