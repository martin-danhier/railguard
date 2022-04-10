#pragma once

#include <cstdint>
#ifndef RENDERER_VULKAN
#error "This header is Vulkan-specific and should only be included if RENDERER_VULKAN is defined"
#endif

#ifndef VULKAN_H_
#error "Include this file after Vulkan headers"
#endif

namespace rg
{
    template<typename T>
    class Array;

    // Structs

    /** Condensed version of a VkDescriptorPoolSize. Describes a number of various types of descriptors. */
    struct DescriptorBalance
    {
        uint32_t dynamic_uniform_count;
        uint32_t dynamic_storage_count;

        [[nodiscard]] inline uint32_t total() const
        {
            return dynamic_uniform_count + dynamic_storage_count;
        }

        inline DescriptorBalance operator*(uint32_t v) const
        {
            return {dynamic_uniform_count * v, dynamic_storage_count * v};
        }

        inline DescriptorBalance &operator+=(const DescriptorBalance &other)
        {
            dynamic_uniform_count += other.dynamic_uniform_count;
            dynamic_storage_count += other.dynamic_storage_count;
            return *this;
        }

        inline DescriptorBalance operator+(const DescriptorBalance &other) const
        {
            return {dynamic_uniform_count + other.dynamic_uniform_count, dynamic_storage_count + other.dynamic_storage_count};
        }

        inline DescriptorBalance &operator-=(const DescriptorBalance &other)
        {
            dynamic_uniform_count -= other.dynamic_uniform_count;
            dynamic_storage_count -= other.dynamic_storage_count;
            return *this;
        }

        inline bool operator>=(const DescriptorBalance &other) const
        {
            return dynamic_uniform_count >= other.dynamic_uniform_count && dynamic_storage_count >= other.dynamic_storage_count;
        }
    };

    /**
     * Manages descriptor pools to handle dynamic creation of sets. Multiple fixed size pools are created, and new ones are created
     * when the fixed size pools are exhausted.
     * */
    class DynamicDescriptorPool
    {
      private:
        struct Data;
        Data *m_data = nullptr;

      public:
        DynamicDescriptorPool() = default;
        DynamicDescriptorPool(VkDevice device, DescriptorBalance balance);
        DynamicDescriptorPool(DynamicDescriptorPool &&other) noexcept;
        DynamicDescriptorPool &operator=(DynamicDescriptorPool &&other) noexcept;
        ~DynamicDescriptorPool();

        // Methods

        void clear() const;

        /**
         * Allocates the given descriptor sets using the given layouts.
         * */
        [[nodiscard]] VkResult allocate_descriptor_sets(ArrayLike<VkDescriptorSet *>           &target_sets,
                                                        const ArrayLike<VkDescriptorSetLayout> &layouts,
                                                        const ArrayLike<DescriptorBalance>     &set_balances) const;

        /**
         * Reset all descriptor pools, which frees all descriptors and descriptor sets.
         *
         * If one of the calls returns something else than VK_SUCCESS, stop looping and return the error.
         * In this case, some of the descriptor pools might not be reset. You should thus wrap a call to this function
         * in a vk_check call, to exit if the value is not VK_SUCCESS. This function returns VK_SUCCESS if everything worked.
         */
        [[nodiscard]] VkResult reset() const;
    };

    /** Allows to easily create descriptor sets. */
    class DescriptorSetBuilder
    {
      private:
        struct Data;
        Data *m_data;

      public:
        explicit DescriptorSetBuilder(VkDevice device, DynamicDescriptorPool &pool);
        ~DescriptorSetBuilder();

        DescriptorSetBuilder &
            add_buffer(VkShaderStageFlags stages, VkDescriptorType type, VkBuffer buffer, size_t range, size_t offset=0);
        DescriptorSetBuilder &add_dynamic_uniform_buffer(VkShaderStageFlags stages, VkBuffer buffer, size_t range, size_t offset=0);
        DescriptorSetBuilder &add_dynamic_storage_buffer(VkShaderStageFlags stages, VkBuffer buffer, size_t range, size_t offset=0);

        DescriptorSetBuilder &save_descriptor_set(VkDescriptorSetLayout *layout, VkDescriptorSet *set);

        [[nodiscard]] VkResult build() const;
    };
} // namespace rg