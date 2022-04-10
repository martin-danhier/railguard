#ifdef RENDERER_VULKAN

#include <railguard/utils/array.h>
#include <railguard/utils/vector.h>

#include <volk.h>
// Must be included after Volk
#include <railguard/utils/vulkan/descriptor_set_helpers.h>

// Helper macro
#define handle(x)              \
    do                         \
    {                          \
        VkResult res = (x);    \
        if (res != VK_SUCCESS) \
            return res;        \
    } while (0)

namespace rg
{

    // region DynamicDescriptorPool

    // ---- Types ----

    struct DynamicDescriptorPool::Data
    {
        // Store pools in a vector, add them when more sets are allocated than the capacity of the pool
        Vector<VkDescriptorPool> descriptor_pools;
        // Device to be able to call functions
        VkDevice          device              = VK_NULL_HANDLE;
        DescriptorBalance remaining_capacity  = {};
        DescriptorBalance single_pool_balance = {};

        // Internal method
        [[nodiscard]] VkResult push_new_pool();
    };

    // ---- Methods ----

    DynamicDescriptorPool::DynamicDescriptorPool(VkDevice device, DescriptorBalance balance) : m_data(new Data)
    {
        m_data->device              = device;
        m_data->single_pool_balance = balance;
        m_data->remaining_capacity  = balance;
    }

    DynamicDescriptorPool::~DynamicDescriptorPool()
    {
        if (m_data != nullptr)
        {
            // Destroy all pools
            for (auto &pool : m_data->descriptor_pools)
            {
                vkDestroyDescriptorPool(m_data->device, pool, nullptr);
            }

            delete m_data;
            m_data = nullptr;
        }
    }

    VkResult DynamicDescriptorPool::reset() const
    {
        // Reset all pools
        for (auto &pool : m_data->descriptor_pools)
        {
            handle(vkResetDescriptorPool(m_data->device, pool, 0));
        }
        m_data->remaining_capacity = m_data->single_pool_balance * m_data->descriptor_pools.size();

        return VK_SUCCESS;
    }

    VkResult DynamicDescriptorPool::Data::push_new_pool()
    {
        // Create a pool that only store one type of descriptors

        // Create descriptor sizes
        Vector<VkDescriptorPoolSize> pool_sizes(2);
        if (single_pool_balance.dynamic_uniform_count != 0)
        {
            pool_sizes.push_back(VkDescriptorPoolSize {
                .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = single_pool_balance.dynamic_uniform_count,
            });
        }
        if (single_pool_balance.dynamic_storage_count != 0)
        {
            pool_sizes.push_back(VkDescriptorPoolSize {
                .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                .descriptorCount = single_pool_balance.dynamic_storage_count,
            });
        }

        VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            // Max case: one descriptor per set
            .maxSets       = single_pool_balance.total(),
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes    = pool_sizes.data(),
        };
        VkDescriptorPool pool;
        handle(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));

        // Add the pool to the vector
        descriptor_pools.push_back(pool);

        // When the new pool is added, it is likely that the previous one is not completely full
        // Though, it would be complicated to regroup sets to allocate correctly and look in previous pools for space
        // We prefer to simply forget the previous ones and focus on the new one
        // The previous space is not forgotten forever, since the pool is reset each frame anyway
        // Maybe later, an optimization can be added to this (if someone wants to contribute, go ahead)
        remaining_capacity = single_pool_balance;

        return VK_SUCCESS;
    }

    VkResult DynamicDescriptorPool::allocate_descriptor_sets(ArrayLike<VkDescriptorSet *>           &target_sets,
                                                             const ArrayLike<VkDescriptorSetLayout> &layouts,
                                                             const ArrayLike<DescriptorBalance>     &set_balances) const
    {
        auto sets_left_to_allocate = layouts.size();

        VkDescriptorSetAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
        };

        size_t index = 0;

        while (sets_left_to_allocate > 0)
        {
            // Determine how many sets we can allocate in this pool
            uint32_t          sets_to_allocate        = 0;
            DescriptorBalance total_allocated_balance = {};

            for (auto i = index; i < set_balances.size(); i++)
            {
                // There is enough room
                if (set_balances[i] + total_allocated_balance >= m_data->remaining_capacity)
                {
                    sets_to_allocate++;
                    total_allocated_balance += set_balances[i];
                }
                // It doesn't fit in any pool
                else if (m_data->single_pool_balance >= set_balances[i])
                {
                    return VK_ERROR_OUT_OF_POOL_MEMORY;
                }
                else
                {
                    // Stop here
                    break;
                }
            }

            // There is not even room for one: create a new one
            if (sets_to_allocate == 0)
            {
                handle(m_data->push_new_pool());
                continue;
            }

            // Update allocation info
            allocate_info.descriptorPool     = m_data->descriptor_pools.last();
            allocate_info.descriptorSetCount = sets_to_allocate;
            allocate_info.pSetLayouts        = layouts.data() + index;

            // Allocate the descriptors

            // Create array to hold the allocated sets
            Array<VkDescriptorSet> allocated_sets(sets_to_allocate);
            handle(vkAllocateDescriptorSets(m_data->device, &allocate_info, allocated_sets.data()));

            // Copy the allocated sets to the target array
            for (size_t i = 0; i < sets_to_allocate; i++)
            {
                target_sets[index + i] = &allocated_sets[i];
            }

            // Update counters
            index += sets_to_allocate;
            sets_left_to_allocate -= sets_to_allocate;
            m_data->remaining_capacity -= total_allocated_balance;
        }

        return VK_SUCCESS;
    }

    DynamicDescriptorPool::DynamicDescriptorPool(DynamicDescriptorPool &&other) noexcept : m_data(other.m_data)
    {
        other.m_data = nullptr;
    }
    DynamicDescriptorPool &DynamicDescriptorPool::operator=(DynamicDescriptorPool &&other) noexcept
    {
        if (this != &other)
        {
            m_data = other.m_data;
            other.m_data = nullptr;
        }
        return *this;
    }

    // endregion DynamicDescriptorPool

    // ---- Set builder

    // region DescriptorSetBuilder

    // --- Types ---

    struct DescriptorSetBuilder::Data
    {
        // Reference to the types we will need
        DynamicDescriptorPool *pool   = nullptr;
        VkDevice               device = VK_NULL_HANDLE;
        // Vectors to accumulate data for creation
        Vector<VkDescriptorSet *>            sets {2};
        Vector<VkDescriptorSetLayout>        layouts {2};
        Vector<size_t>                       binding_counts {2};
        Vector<VkDescriptorSetLayoutBinding> current_bindings {3};
        DescriptorBalance                    current_balance = {};
        Vector<VkWriteDescriptorSet>         write_descriptor_sets {2};
        Vector<DescriptorBalance>            balances {2};
    };

    // --- Methods ---

    DescriptorSetBuilder::DescriptorSetBuilder(VkDevice device, DynamicDescriptorPool &pool) : m_data(new Data)
    {
        m_data->pool = &pool;
    }

    DescriptorSetBuilder::~DescriptorSetBuilder()
    {
        delete m_data;
        m_data = nullptr;
    }

    DescriptorSetBuilder &DescriptorSetBuilder::add_buffer(VkShaderStageFlags stages,
                                                           VkDescriptorType   type,
                                                           VkBuffer           buffer,
                                                           size_t             offset,
                                                           size_t             range)
    {
        uint32_t binding_index = m_data->current_bindings.size();

        // Set binding
        m_data->current_bindings.push_back({
            .binding            = binding_index,
            .descriptorType     = type,
            .descriptorCount    = 1,
            .stageFlags         = stages,
            .pImmutableSamplers = nullptr,
        });

        // Set buffer info
        VkDescriptorBufferInfo buffer_info {
            .buffer = buffer,
            .offset = offset,
            .range  = range,
        };

        // Set write
        m_data->write_descriptor_sets.push_back(VkWriteDescriptorSet {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = VK_NULL_HANDLE,
            .dstBinding       = binding_index,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = type,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &buffer_info,
            .pTexelBufferView = nullptr,
        });

        // Update balance
        switch (type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: m_data->current_balance.dynamic_uniform_count += 1; break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: m_data->current_balance.dynamic_storage_count += 1; break;
            default: throw std::runtime_error("DescriptorSetBuilder::add_buffer: unsupported descriptor type");
        }

        return *this;
    }

    DescriptorSetBuilder &
        DescriptorSetBuilder::add_dynamic_uniform_buffer(VkShaderStageFlags stages, VkBuffer buffer, size_t offset, size_t range)
    {
        return add_buffer(stages, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, buffer, offset, range);
    }

    DescriptorSetBuilder &
        DescriptorSetBuilder::add_dynamic_storage_buffer(VkShaderStageFlags stages, VkBuffer buffer, size_t offset, size_t range)
    {
        return add_buffer(stages, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, buffer, offset, range);
    }

    DescriptorSetBuilder &DescriptorSetBuilder::save_descriptor_set(VkDescriptorSetLayout *layout, VkDescriptorSet *set)
    {
        // Create layout
        VkDescriptorSetLayoutCreateInfo layout_info {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = nullptr,
            .flags        = 0,
            .bindingCount = static_cast<uint32_t>(m_data->current_bindings.size()),
            .pBindings    = m_data->current_bindings.data(),
        };
        if (vkCreateDescriptorSetLayout(m_data->device, &layout_info, nullptr, layout) != VK_SUCCESS)
            throw std::runtime_error("DescriptorSetBuilder::save_descriptor_set: failed to create descriptor set layout");

        m_data->layouts.push_back(*layout);
        m_data->binding_counts.push_back(m_data->current_bindings.size());

        // Save and reset balance
        m_data->balances.push_back(m_data->current_balance);
        m_data->current_balance = {};

        // Save set pointer
        m_data->sets.push_back(set);

        // Reset current_bindings
        m_data->current_bindings.clear();

        return *this;
    }

    VkResult DescriptorSetBuilder::build() const
    {
        // Allocate sets
        handle(m_data->pool->allocate_descriptor_sets(m_data->sets, m_data->layouts, m_data->balances));

        // Update writes
        size_t w = 0;
        for (size_t i = 0; i < m_data->sets.size(); ++i)
        {
            for (size_t j = w; j < w + m_data->binding_counts[i]; ++j)
            {
                m_data->write_descriptor_sets[j].dstSet = *m_data->sets[i];
            }
            w += m_data->binding_counts[i];
        }

        // Update descriptor sets
        vkUpdateDescriptorSets(m_data->device,
                               static_cast<uint32_t>(m_data->write_descriptor_sets.size()),
                               m_data->write_descriptor_sets.data(),
                               0,
                               nullptr);

        return VK_SUCCESS;
    }
    // endregion DescriptorSetBuilder
} // namespace rg

#endif // RENDERER_VULKAN