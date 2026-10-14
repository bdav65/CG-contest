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
    memory = helper.allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkMapMemory(helper.getDevice(), memory, 0, capacity, 0, &mapped);
}


void StagingBuffer::setDescriptor(ResourceDescriptor &descriptor) {
    descriptor.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor.buffer = buffer;
}


void StagingBuffer::destroy() {
    vkUnmapMemory(helper.getDevice(), memory);
    vkDestroyBuffer(helper.getDevice(), buffer, nullptr);
    vkFreeMemory(helper.getDevice(), memory, nullptr);
}


StagingBuffer::StagingBuffer(VulkanHelper &helper, size_t capacity):
    helper(helper),
    capacity(capacity)
{
    buffer = helper.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, capacity);
    memory = helper.allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkMapMemory(helper.getDevice(), memory, 0, capacity, 0, &mapped);

    mapped_size = mapped;
    mapped_data = static_cast<char *>(mapped) + 16;
}


void StagingBuffer::initialize(void const *data, int n, int size, VkBufferUsageFlags flags) {
    capacity = 16 + n * size;

    buffer = helper.createBuffer(flags, capacity);
    memory = helper.allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkMapMemory(helper.getDevice(), memory, 0, capacity, 0, &mapped);

    memcpy(mapped, &n, sizeof(int));
    memcpy(((char *)mapped + 16), data, n * size);

    mapped_size = mapped;
    mapped_data = static_cast<char *>(mapped) + 16;
}


void InputBuffer::setDescriptor(ResourceDescriptor &descriptor) {
    descriptor.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor.buffer = buffer;
}


void InputBuffer::destroy() {
    vkDestroyBuffer(helper.getDevice(), buffer, nullptr);
    vkFreeMemory(helper.getDevice(), memory, nullptr);
};


void InputBuffer::initialize(void const *data, int n, int size) {
    std::cout << "Initializing InputBuffer" << std::endl;

    StagingBuffer staging {helper, data, n, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT};

    copyData(staging);
    staging.destroy();
}


void InputBuffer::copyData(StagingBuffer const &data) {
    std::cout << "Copying staging buffer data into InputBuffer" << std::endl;

    buffer = helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, data.getCapacity());
    memory = helper.allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkutil::runSingleTimeCommand(helper.device, helper.commandPool, helper.computeQueue, [&](auto commandBuffer){
        VkBufferCopy region {0, 0, static_cast<VkDeviceSize>(data.getCapacity())};
        vkCmdCopyBuffer(commandBuffer, data.buffer, buffer, 1, &region);
    });
}


void OutputBuffer::setDescriptor(ResourceDescriptor &descriptor) {
    descriptor.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor.buffer = buffer;
}


void OutputBuffer::destroy() {
    vkDestroyBuffer(helper.getDevice(), buffer, nullptr);
    vkFreeMemory(helper.getDevice(), memory, nullptr);
};


StagingBuffer OutputBuffer::readBack() {
    StagingBuffer staging {helper, capacity};

    vkutil::runSingleTimeCommand(helper.device, helper.commandPool, helper.computeQueue, [&](auto commandBuffer){
        VkBufferCopy region {0, 0, capacity};
        vkCmdCopyBuffer(commandBuffer, buffer, staging.buffer, 1, &region);
    });

    return staging;
}


OutputBuffer::OutputBuffer(VulkanHelper &helper, size_t capacity):
    helper(helper),
    capacity(capacity)
{
    buffer = helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, capacity);
    memory = helper.allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
