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
        Vector<VkDescriptorPool> descriptor_pools {1};
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
        // We start with empty capacity
        m_data->remaining_capacity = {};
    }

    DynamicDescriptorPool::~DynamicDescriptorPool()
    {
        if (m_data != nullptr)
        {
            clear();

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
        if (single_pool_balance.storage_count != 0)
        {
            pool_sizes.push_back(VkDescriptorPoolSize {
                .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = single_pool_balance.storage_count,
            });
        }
        if (single_pool_balance.combined_image_sampler_count != 0)
        {
            pool_sizes.push_back(VkDescriptorPoolSize {
                .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = single_pool_balance.combined_image_sampler_count,
            });
        }

        VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            // Max case: one descriptor per set
            .flags         = 0,
            .maxSets       = single_pool_balance.total(),
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes    = pool_sizes.data(),
        };
        VkDescriptorPool pool = VK_NULL_HANDLE;
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
                if (m_data->remaining_capacity >= set_balances[i] + total_allocated_balance)
                {
                    sets_to_allocate++;
                    total_allocated_balance += set_balances[i];
                }
                // It doesn't fit in any pool
                else if (!(m_data->single_pool_balance >= set_balances[i]))
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
            allocate_info.pSetLayouts        = &layouts[index];

            // Allocate the descriptors

            // Create array to hold the allocated sets
            Array<VkDescriptorSet> allocated_sets(sets_to_allocate);
            handle(vkAllocateDescriptorSets(m_data->device, &allocate_info, allocated_sets.data()));

            // Copy the allocated sets to the target array
            for (size_t i = 0; i < sets_to_allocate; i++)
            {
                *target_sets[index + i] = allocated_sets[i];
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
            m_data       = other.m_data;
            other.m_data = nullptr;
        }
        return *this;
    }

    void DynamicDescriptorPool::clear() const
    {
        if (m_data != nullptr)
        {
            // Destroy all pools
            for (auto &pool : m_data->descriptor_pools)
            {
                vkDestroyDescriptorPool(m_data->device, pool, nullptr);
            }

            m_data->remaining_capacity  = {};
            m_data->single_pool_balance = {};

            m_data->descriptor_pools.clear();
        }
    }

    // endregion DynamicDescriptorPool

    // ---- Set builder

    // region DescriptorSetBuilder

    // --- Types ---

    enum class InfoType
    {
        BUFFER,
        IMAGE,
    };

    struct InfoPtr
    {
        size_t   index     = 0;
        InfoType info_type = InfoType::BUFFER;
    };

    struct DescriptorSetBuilder::Data
    {
        // Reference to the types we will need
        DynamicDescriptorPool *pool   = nullptr;
        VkDevice               device = VK_NULL_HANDLE;
        // Vectors to accumulate data for creation
        Vector<VkDescriptorSet *>      sets {2};
        Vector<VkDescriptorSetLayout>  layouts {2};
        Vector<size_t>                 binding_counts  = Vector<size_t>(2);
        uint32_t                       binding_index   = 0;
        DescriptorBalance              current_balance = {};
        Vector<VkDescriptorBufferInfo> buffer_infos {2};
        Vector<VkDescriptorImageInfo>  image_infos {2};
        // We cannot directly store the pointers to buffer and image infos in the writes until the build function,
        // because the vectors might grow. If this happened, the set pointers would continue to point to old data.
        // To solve this, we store the index instead and update the writes just before submitting them.
        Vector<InfoPtr>              info_ptrs {2};
        Vector<VkWriteDescriptorSet> write_descriptor_sets {2};
        Vector<DescriptorBalance>    balances {2};
    };

    // --- Methods ---

    DescriptorSetBuilder::DescriptorSetBuilder(VkDevice device, DynamicDescriptorPool &pool) : m_data(new Data)
    {
        m_data->pool   = &pool;
        m_data->device = device;
    }

    DescriptorSetBuilder::~DescriptorSetBuilder()
    {
        delete m_data;
        m_data = nullptr;
    }

    DescriptorSetBuilder &DescriptorSetBuilder::add_buffer(VkDescriptorType type, VkBuffer buffer, size_t range, size_t offset)
    {
        // Save index
        m_data->info_ptrs.push_back(InfoPtr {
            .index     = m_data->buffer_infos.size(),
            .info_type = InfoType::BUFFER,
        });

        // Set buffer info
        m_data->buffer_infos.push_back(VkDescriptorBufferInfo {
            .buffer = buffer,
            .offset = offset,
            .range  = range,
        });

        // Set write
        m_data->write_descriptor_sets.push_back(VkWriteDescriptorSet {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = VK_NULL_HANDLE,
            .dstBinding       = m_data->binding_index,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = type,
            .pImageInfo       = nullptr,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        });

        // Update balance
        switch (type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: m_data->current_balance.dynamic_uniform_count += 1; break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: m_data->current_balance.dynamic_storage_count += 1; break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: m_data->current_balance.storage_count += 1; break;
            default: throw std::runtime_error("DescriptorSetBuilder::add_buffer: unsupported descriptor type");
        }

        // Increase binding index
        m_data->binding_index++;

        return *this;
    }

    DescriptorSetBuilder &DescriptorSetBuilder::add_dynamic_uniform_buffer(VkBuffer buffer, size_t range, size_t offset)
    {
        return add_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, buffer, range, offset);
    }

    DescriptorSetBuilder &DescriptorSetBuilder::add_dynamic_storage_buffer(VkBuffer buffer, size_t range, size_t offset)
    {
        return add_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, buffer, range, offset);
    }

    DescriptorSetBuilder &DescriptorSetBuilder::add_storage_buffer(VkBuffer buffer, size_t range, size_t offset)
    {
        return add_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffer, range, offset);
    }

    DescriptorSetBuilder &DescriptorSetBuilder::add_combined_image_sampler(VkSampler sampler, VkImageView image_view)
    {
        // Save index
        m_data->info_ptrs.push_back(InfoPtr {
            .index     = m_data->image_infos.size(),
            .info_type = InfoType::IMAGE,
        });

        // Set buffer info
        m_data->image_infos.push_back(VkDescriptorImageInfo {
            .sampler     = sampler,
            .imageView   = image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

        // Set write
        m_data->write_descriptor_sets.push_back(VkWriteDescriptorSet {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = VK_NULL_HANDLE,
            .dstBinding       = m_data->binding_index,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = nullptr,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        });

        // Update balance
        m_data->current_balance.combined_image_sampler_count += 1;

        // Increase binding index
        m_data->binding_index++;

        return *this;
    }

    DescriptorSetBuilder &DescriptorSetBuilder::save_descriptor_set(VkDescriptorSetLayout layout, VkDescriptorSet *set)
    {
        m_data->layouts.push_back(layout);
        m_data->binding_counts.push_back(m_data->binding_index);

        // Reset binding index
        m_data->binding_index = 0;

        // Save and reset balance
        m_data->balances.push_back(m_data->current_balance);
        m_data->current_balance = {};

        // Save set pointer
        m_data->sets.push_back(set);

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

                // Set info pointers
                auto &info_ptr = m_data->info_ptrs[j];
                if (info_ptr.info_type == InfoType::BUFFER) {
                    m_data->write_descriptor_sets[j].pBufferInfo = &m_data->buffer_infos[info_ptr.index];
                }
                else {
                    m_data->write_descriptor_sets[j].pImageInfo = &m_data->image_infos[info_ptr.index];
                }
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

    // region DescriptorSetLayoutBuilder

    // --- Types ---

    struct DescriptorSetLayoutBuilder::Data
    {
        VkDevice                             device = VK_NULL_HANDLE;
        Vector<VkDescriptorSetLayoutBinding> current_bindings {3};
    };

    // --- Methods ---

    DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(VkDevice device) : m_data(new Data)
    {
        m_data->device = device;
    }

    DescriptorSetLayoutBuilder::~DescriptorSetLayoutBuilder()
    {
        if (m_data != nullptr)
        {
            delete m_data;
            m_data = nullptr;
        }
    }

    DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::add_buffer(VkShaderStageFlags stages, VkDescriptorType type)
    {
        // Set binding
        m_data->current_bindings.push_back(VkDescriptorSetLayoutBinding {
            .binding            = static_cast<uint32_t>(m_data->current_bindings.size()),
            .descriptorType     = type,
            .descriptorCount    = 1,
            .stageFlags         = stages,
            .pImmutableSamplers = nullptr,
        });
        return *this;
    }
    DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::add_dynamic_uniform_buffer(VkShaderStageFlags stages)
    {
        return add_buffer(stages, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    }
    DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::add_storage_buffer(VkShaderStageFlags stages)
    {
        return add_buffer(stages, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::add_combined_image_sampler(VkShaderStageFlags stages)
    {
        return add_buffer(stages, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
    DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::save_descriptor_set_layout(VkDescriptorSetLayout *layout)
    {
        if (*layout != nullptr)
        {
            throw std::runtime_error("DescriptorSetLayoutBuilder::save_descriptor_set_layout: layout must be null");
        }

        VkDescriptorSetLayoutCreateInfo layout_info {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = nullptr,
            .flags        = 0,
            .bindingCount = static_cast<uint32_t>(m_data->current_bindings.size()),
            .pBindings    = m_data->current_bindings.data(),
        };
        if (vkCreateDescriptorSetLayout(m_data->device, &layout_info, nullptr, layout) != VK_SUCCESS)
            throw std::runtime_error("DescriptorSetBuilder::save_descriptor_set: failed to create descriptor set layout");

        // Reset current_bindings
        m_data->current_bindings.clear();

        return *this;
    }

    // endregion
} // namespace rg

#endif // RENDERER_VULKAN