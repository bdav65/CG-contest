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


struct MappedStorageBuffer {
    VulkanHelper &helper;
    VkBuffer buffer;
    void *mapped;
    VkDeviceMemory memory;

    void setDescriptor(ResourceDescriptor &descriptor);
    void destroy();

    MappedStorageBuffer(VulkanHelper &helper, size_t capacity);
};


struct StorageBuffer {
    VulkanHelper &helper;
    VkBuffer buffer;
    VkDeviceMemory memory;

    void setDescriptor(ResourceDescriptor &descriptor);
    void destroy();

    template<typename T>
    StorageBuffer(VulkanHelper &helper, std::vector<T> const &data):
        helper(helper)
    {
        initialize(data.data(), data.size(), sizeof(T));
    }
    
    private:
        void initialize(void const *data, int n, int size);
};


std::vector<char> readFile(const std::string& filename);
std::tuple<std::vector<unsigned char>, VkExtent2D> readImage(const std::filesystem::path& filename);
