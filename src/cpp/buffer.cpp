#include "buffer.hpp"

#include <fstream>
#include <iostream>

#include "lode/lodepng.hpp"


void UniformBuffer::setDescriptor(ResourceDescriptor &descriptor) {
    descriptor.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor.buffer = buffer;
}


void UniformBuffer::destroy() {
    vkUnmapMemory(helper.getDevice(), memory);
    vkDestroyBuffer(helper.getDevice(), buffer, nullptr);
    vkFreeMemory(helper.getDevice(), memory, nullptr);
}


UniformBuffer::UniformBuffer(VulkanHelper &helper, size_t capacity):
    helper(helper)
{
    buffer = helper.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, capacity);
    memory = helper.allocateBufferMemory(buffer);

    vkMapMemory(helper.getDevice(), memory, 0, capacity, 0, &mapped);
}


void MappedStorageBuffer::setDescriptor(ResourceDescriptor &descriptor) {
    descriptor.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor.buffer = buffer;
}


void MappedStorageBuffer::destroy() {
    vkUnmapMemory(helper.getDevice(), memory);
    vkDestroyBuffer(helper.getDevice(), buffer, nullptr);
    vkFreeMemory(helper.getDevice(), memory, nullptr);
}


MappedStorageBuffer::MappedStorageBuffer(VulkanHelper &helper, size_t capacity):
    helper(helper)
{
    buffer = helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, capacity);
    memory = helper.allocateBufferMemory(buffer);

    vkMapMemory(helper.getDevice(), memory, 0, capacity, 0, &mapped);
}


void StorageBuffer::setDescriptor(ResourceDescriptor &descriptor) {
    descriptor.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor.buffer = buffer;
}


void StorageBuffer::destroy() {
    vkDestroyBuffer(helper.getDevice(), buffer, nullptr);
    vkFreeMemory(helper.getDevice(), memory, nullptr);
}


void StorageBuffer::initialize(void const *data, int n, int size) {
    int capacity = 16 + n * size;

    buffer = helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, capacity);
    memory = helper.allocateBufferMemory(buffer);

    void *bufferMap;

    vkMapMemory(helper.getDevice(), memory, 0, capacity, 0, &bufferMap);

    memcpy(bufferMap, &n, sizeof(int));
    memcpy(((char *)bufferMap + 16), data, n * size);

    vkUnmapMemory(helper.getDevice(), memory);

}


/** Utility function to read a file's contents into a vector of characters */
std::vector<char> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}


/** Utility function to read and decode a PNG into a vector of bytes (RGBA) */
std::tuple<std::vector<unsigned char>, VkExtent2D> readImage(const std::filesystem::path& filename) {
    std::vector<unsigned char> image; //the raw pixels
    uint width, height;

    if (uint error = lodepng::decode(image, width, height, filename)) {
        std::cout << "Failed to read image! Error " << error << ": " << lodepng_error_text(error) << std::endl;
        throw std::runtime_error("Failed to load image.");
    }

    return std::make_pair(image, VkExtent2D {width, height});
}
