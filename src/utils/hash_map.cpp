#include "railguard/utils/hash_map.h"

#include "railguard/utils/optional.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>

// --=== Constants ===--

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

namespace rg
{

    // --=== Types ===--

    struct HashMap::Data
    {
        Entry *entries;
        size_t capacity;
        size_t count;
    };

    // --=== Utils ===---

    // FNV hash of key
    // Related wiki page: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    uint64_t hash(uint64_t key)
    {
        uint64_t hash = FNV_OFFSET;
        for (size_t i = 0; i < sizeof(key); i++)
        {
            hash ^= ((uint8_t *) &key)[i];
            hash *= FNV_PRIME;
        }
        return hash;
    }

    void set_entry(HashMap::Entry       *entries,
                   const size_t         &capacity,
                   const HashMap::Key   &key,
                   const HashMap::Value &value,
                   size_t               *count = nullptr)
    {
        // Prevent the use of the zero key, which is reserved for the empty entry
        if (key == HashMap::NULL_KEY)
        {
            throw std::invalid_argument("NULL_KEY is reserved for the empty entry");
        }

        // Compute the index of the key in the array
        size_t index = hash(key) & static_cast<uint64_t>(capacity - 1);

        while (entries[index].key != HashMap::NULL_KEY)
        {
            // If the slot is not empty, but the key is the same
            // We want to edit that slot
            if (entries[index].key == key)
            {
                // Edit value
                entries[index].value = value;
                return;
            }

            // Else, increment to find the next suitable slot
            index++;
            // Wrap around to stay inside the array
            if (index >= capacity)
            {
                index = 0;
            }
        }

        // The key didn't already exist
        // We need to add it to an empty slot
        // Index is guarantied to point to an empty slot now, given the loop above.
        // We will use that one.
        if (count != nullptr)
        {
            // Increment count, if provided
            (*count)++;
        }
        entries[index].key   = key;
        entries[index].value = value;
    }

    // --=== Constructors ===--

    HashMap::HashMap() : m_data(new Data)
    {
        m_data->capacity = 2;
        m_data->count    = 0;
        m_data->entries  = new Entry[m_data->capacity];
        // Zero the entries
        std::memset(m_data->entries, 0, sizeof(Entry) * m_data->capacity);
    }

    HashMap::HashMap(const HashMap &other) : m_data(new Data)
    {
        m_data->capacity = other.m_data->capacity;
        m_data->count    = other.m_data->count;
        m_data->entries  = new Entry[m_data->capacity];

        // Copy entries
        memcpy(m_data->entries, other.m_data->entries, sizeof(Entry) * m_data->capacity);
    }

    HashMap::HashMap(HashMap &&other) noexcept : m_data(other.m_data)
    {
        // Just take the other's data, and set it to null so that it can't delete it
        other.m_data = nullptr;
    }

    HashMap::~HashMap()
    {
        if (m_data)
        {
            delete[] m_data->entries;
            m_data->entries = nullptr;
            delete m_data;
            m_data = nullptr;
        }
    }

    // --=== Operators ===--

    HashMap &HashMap::operator=(const HashMap &other)
    {
        if (this != &other)
        {
            // Delete old data
            this->~HashMap();

            // Copy new data
            m_data           = new Data;
            m_data->capacity = other.m_data->capacity;
            m_data->count    = other.m_data->count;
            m_data->entries  = new Entry[m_data->capacity];

            // Copy entries
            memcpy(m_data->entries, other.m_data->entries, sizeof(Entry) * m_data->capacity);
        }

        return *this;
    }

    HashMap &HashMap::operator=(HashMap &&other) noexcept
    {
        if (this != &other)
        {
            // Delete old data
            this->~HashMap();

            // Take other's data
            m_data = other.m_data;

            // Set other's data to null so that it can't delete it
            other.m_data = nullptr;
        }

        return *this;
    }

    // --=== Private methods ===--

    void HashMap::expand()
    {
        // Always use powers of 2, so we can replace modulo with and operation
        size_t new_capacity = m_data->capacity * 2;
        if (new_capacity < m_data->capacity)
        {
            // In case of overflow, do not allow
            // In practice, this will probably not happen
            throw std::overflow_error("Cannot expand hashmap: capacity overflow");
        }

        auto new_entries = new Entry[new_capacity];
        // Zero the entries
        std::memset(new_entries, 0, sizeof(Entry) * new_capacity);

        // Move the entries to the new array
        // We cannot just copy the memory because the indexes will be different
        // So we take each entry and insert it in the new array
        for (size_t i = 0; i < m_data->capacity; i++)
        {
            auto entry = m_data->entries[i];
            if (entry.key != NULL_KEY)
            {
                set_entry(new_entries, new_capacity, entry.key, entry.value, nullptr);
            }
        }

        // Delete old entries
        delete[] m_data->entries;

        // Update data
        m_data->capacity = new_capacity;
        m_data->entries  = new_entries;
    }

    // --=== Public methods ===--

    Optional<HashMap::Value *> HashMap::get(const HashMap::Key &key) const
    {
        // Directly filter invalid keys
        if (key == NULL_KEY)
        {
            return none<Value *>();
        }

        // Compute the index of the key in the array
        size_t index = hash(key) & static_cast<uint64_t>(m_data->capacity - 1);

        // Search for the value in the location, until we find an empty slot
        while (m_data->entries[index].key != NULL_KEY)
        {
            // If the key is the same, we found the good slot !
            if (key == m_data->entries[index].key)
            {
                return some(&m_data->entries[index].value);
            }

            index++;
            // Wrap around to stay inside the array
            if (index >= m_data->capacity)
            {
                index = 0;
            }
        }
        return none<Value *>();
    }

    void HashMap::set(const HashMap::Key &key, const HashMap::Value &value)
    {
        // Expand the capacity of the array if it is more than half full
        if (m_data->count >= m_data->capacity / 2)
        {
            expand();
        }

        return set_entry(m_data->entries, m_data->capacity, key, value, &m_data->count);
    }

    void HashMap::remove(const HashMap::Key &key)
    {
        // Compute the index of the key in the array
        size_t index = hash(key) & static_cast<uint64_t>(m_data->capacity - 1);

        Entry *deleted_slot           = nullptr;
        size_t deleted_index          = 0;
        size_t invalidated_block_size = 0;

        while (m_data->entries[index].key != NULL_KEY)
        {
            // We found the slot to remove if the key is the same
            if (key == m_data->entries[index].key)
            {
                deleted_slot  = &m_data->entries[index];
                deleted_index = index;
            }

            // We keep iterating because now, the slots are disconnected from the base index
            // If the deleted slot is not NULL, then we are between it and the NULL key
            // The current slot may then become inaccessible. We need to invalidate it.
            // For that, we need to count the number of slots between the deleted slot and the NULL key
            if (deleted_slot != nullptr)
            {
                invalidated_block_size++;
            }

            // Else, increment to find the next empty slot
            index++;
            // Wrap around to stay inside the array
            if (index >= m_data->capacity)
            {
                index = 0;
            }
        }

        // If we found a slot to remove, remove it and add the next ones again
        if (deleted_slot != nullptr)
        {
            // If we have other slots to invalidate, store them in another array first and set them to NULL
            Entry *invalidated_slots = nullptr;
            if (invalidated_block_size > 1)
            {
                invalidated_slots = new Entry[invalidated_block_size - 1];

                // Copy the invalidated slots. Wrap around if needed
                size_t j = (deleted_index + 1) % m_data->capacity;
                for (size_t i = 0; i < invalidated_block_size - 1; i++)
                {
                    // Copy the slot
                    invalidated_slots[i] = m_data->entries[j];
                    // Set it to NULL
                    m_data->entries[j] = Entry {
                        .key   = NULL_KEY,
                        .value = {.as_ptr = nullptr},
                    };

                    // Increment the index
                    j++;
                    // Wrap around to stay inside the array
                    if (j >= m_data->capacity)
                    {
                        j = 0;
                    }
                }
            }

            // Set the deleted slot to NULL
            *deleted_slot = Entry {
                .key   = NULL_KEY,
                .value = {.as_ptr = nullptr},
            };

            // Decrement the count.
            m_data->count--;

            // If we have slots to invalidate, add them back
            if (invalidated_slots != nullptr)
            {
                for (size_t i = 0; i < invalidated_block_size - 1; i++)
                {
                    set_entry(m_data->entries, m_data->capacity, invalidated_slots[i].key, invalidated_slots[i].value);
                }

                delete[] invalidated_slots;
            }
        }
    }

    void HashMap::clear()
    {
        if (m_data->count > 0)
        {
            for (size_t i = 0; i < m_data->capacity; i++)
            {
                m_data->entries[i] = Entry {
                    .key   = NULL_KEY,
                    .value = {.as_ptr = nullptr},
                };
            }
            m_data->count = 0;
        }
    }

    HashMap::Iterator HashMap::begin() const
    {
        return {this, 0};
    }

    HashMap::Iterator HashMap::end() const
    {
        return {this, m_data->capacity};
    }

    size_t HashMap::count() const
    {
        return m_data->count;
    }

    bool HashMap::is_empty() const
    {
        return m_data->count == 0;
    }

    bool HashMap::exists(const Key &key) const
    {
        // Get the key
        auto res = get(key);
        return res.has_value();
    }

    // Iterator

    HashMap::Iterator::Iterator(const HashMap *map, size_t index)
        : m_map(map), m_index(index), m_entry(nullptr)
    {
        if (index < m_map->m_data->capacity)
        {
            // Start at this index
            m_entry = &m_map->m_data->entries[index];

            // Then find the first non-NULL key
            while (m_entry->key == NULL_KEY)
            {
                m_index++;
                // Stop if we reached the end
                if (m_index >= m_map->m_data->capacity)
                {
                    m_index = m_map->m_data->capacity;
                    m_entry = nullptr;
                    break;
                }

                m_entry++;
            }
        }
    }

    HashMap::Entry HashMap::Iterator::operator*()
    {
        return *m_entry;
    }

    HashMap::Iterator &HashMap::Iterator::operator++()
    {
        if (m_entry != nullptr)
        {
            // Find the next non-NULL key
            do
            {
                m_index++;
                // Stop if we reached the end
                if (m_index >= m_map->m_data->capacity)
                {
                    m_index = m_map->m_data->capacity;
                    m_entry = nullptr;
                    break;
                }

                m_entry++;
            } while (m_entry->key == NULL_KEY);
        }

        return *this;
    }

    bool HashMap::Iterator::operator!=(const HashMap::Iterator &other) const
    {
        return m_index != other.m_index || m_map != other.m_map;
    }

} // namespace rg