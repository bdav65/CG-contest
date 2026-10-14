#pragma once

#include "vulkan.hpp"


struct UniformBuffer {
    VulkanHelper &helper;
    VkBuffer buffer;
    void *mapped;
    VkDeviceMemory memory;

    void setDescriptor(ResourceDescriptor &descriptor);
    void destroy();

    UniformBuffer(VulkanHelper &helper, size_t capacity);
};


// Buffer that can be read by CPU
struct StagingBuffer {
    static const VkBufferUsageFlags DEFAULT_FLAGS = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VulkanHelper &helper;
    VkBuffer buffer;
    void *mapped;
    void *mapped_size;
    void *mapped_data;
    VkDeviceMemory memory;

    size_t getCapacity() const { return capacity; };
    void setDescriptor(ResourceDescriptor &descriptor);
    void destroy();

    StagingBuffer(VulkanHelper &helper, size_t capacity);
    
    template<typename T>
    StagingBuffer(VulkanHelper &helper, std::vector<T> const &data):
        helper(helper)
    {
        initialize(data.data(), data.size(), sizeof(T), DEFAULT_FLAGS);
    }

    StagingBuffer(VulkanHelper &helper, void const *data, int n, int size, VkBufferUsageFlags flags = DEFAULT_FLAGS):
        helper(helper)
    {
        initialize(data, n, size, flags);
    }
    
    private:
        size_t capacity;
        void initialize(void const *data, int n, int size, VkBufferUsageFlags flags);
};


// Buffer for compute shader input
// Only used on GPU after initialization
struct InputBuffer {
    VulkanHelper &helper;
    VkBuffer buffer;
    VkDeviceMemory memory;

    void setDescriptor(ResourceDescriptor &descriptor);
    void destroy();

    template<typename T>
    InputBuffer(VulkanHelper &helper, std::vector<T> const &data):
        helper(helper)
    {
        initialize(data.data(), data.size(), sizeof(T));
    }

    InputBuffer(VulkanHelper &helper, StagingBuffer const &data):
        helper(helper)
    {
        copyData(data);
    }

    private:
        void initialize(void const *data, int n, int size);
        void copyData(StagingBuffer const &data);
};


// Buffer for compute shader output
// Cannot be initialized, but can be copied back to a staging buffer
struct OutputBuffer {
    size_t getCapacity() const { return capacity; }
    void setDescriptor(ResourceDescriptor &descriptor);
    void destroy();

    StagingBuffer readBack();

    // Allocate output buffer of a certain size
    OutputBuffer(VulkanHelper &helper, size_t capacity);

    private:
        VulkanHelper &helper;
        size_t capacity;
        VkBuffer buffer;
        VkDeviceMemory memory;
};


std::vector<char> readFile(const std::string& filename);
std::tuple<std::vector<unsigned char>, VkExtent2D> readImage(const std::filesystem::path& filename);
