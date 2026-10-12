#pragma once

#include "vulkan.hpp"


struct StorageBuffer {
    VulkanHelper &helper;
    VkBuffer buffer;
    VkDeviceMemory memory;

    void setDescriptor(ResourceDescriptor &descriptor) {
        descriptor.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptor.buffer = buffer;
    }

    void destroy() {
        vkDestroyBuffer(helper.getDevice(), buffer, nullptr);
        vkFreeMemory(helper.getDevice(), memory, nullptr);
    }

    template<typename T>
    StorageBuffer(VulkanHelper &helper, std::vector<T> const &data):
        helper(helper)
    {
        int n = data.size();
        int size = 16 + n * sizeof(T);

        buffer = helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, size);
        memory = helper.allocateBufferMemory(buffer);

        void *bufferMap;

        vkMapMemory(helper.getDevice(), memory, 0, size, 0, &bufferMap);

        memcpy(bufferMap, &n, sizeof(int));
        memcpy(((char *)bufferMap + 16), data.data(), n * sizeof(T));

        vkUnmapMemory(helper.getDevice(), memory);
    }
};
