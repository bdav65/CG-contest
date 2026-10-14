#pragma once

#include <vulkan/vulkan.h>

#include "render-target.hpp"
#include "scene.hpp"
#include "vulkan.hpp"

const std::vector VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

/*
 * Since the requirements are quite complex (dump to disk, copy to framebuffer, image storage, ...)
 * these formats were manually chosen based on the information on https://vulkan.gpuinfo.org/.
 *
 * They are size-compatible with each-other, and support the required operations (TRANSFER_SRC/DST,
 * STORAGE_IMAGE, SAMPLED_IMAGE, surface) on over 90% of all devices out there.
 */
const VkFormat IMAGE_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

struct ResourceDescriptor {
    VkDescriptorType type;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};


class VulkanHelper {
    public:
    bool headless;
    std::unique_ptr<RenderTarget> renderTarget;

    // Vulkan environment
    VkInstance instance;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue computeQueue;
    VkDescriptorPool descriptorPool;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    // Synchronization
    VkFence computeInFlightFence;

    // Resources
    std::vector<VkImageView> imageViews{};

    void initializeVulkan();

    VulkanHelper(bool headless);

    // Actions
    void uploadToImage(std::vector<unsigned char> data, VkImage image, VkExtent2D imageExtent);
    void waitForCommandCompletion();
    void submitCommands(VkCommandBuffer const &commandBuffer);
    void presentImage(VkImage const &outputImage, VkExtent2D imageExtent);
    void terminate();

    // Constructors
    VkShaderModule createShaderModule(std::vector<char> shaderSrc);
    std::tuple<VkDescriptorSet, VkDescriptorSetLayout> createDescriptorSet(std::vector<ResourceDescriptor> descriptors);

    void dumpImageToDisk(VkImage image, const std::string &destination, VkExtent2D imageExtent);

    std::tuple<VkPipeline, VkPipelineLayout> createComputePipeline(VkShaderModule shaderModule, std::vector<VkDescriptorSetLayout> descriptorLayouts);
    VkBuffer createBuffer(VkBufferUsageFlags usage, size_t size);
    VkImage createImage(VkImageUsageFlags usage, VkExtent2D size);
    VkSampler createSampler(VkFilter filter);

    VkDeviceMemory allocateBufferMemory(const VkBuffer &buffer, VkMemoryPropertyFlags properties);
    VkDeviceMemory allocateImageMemory(const VkImage &image);

    // Getters
    VkCommandBuffer getCommandBuffer();
    VkExtent2D outputSize();
    VkDevice getDevice();
    std::unique_ptr<RenderTarget> &getRenderTarget() {return renderTarget;}
};

namespace vkutil {
    // Physical Device Selection
    VkPhysicalDevice pickPhysicalDevice(VkInstance instance, std::optional<VkSurfaceKHR> surface);
    bool isDeviceSuitable(VkPhysicalDevice gpu, std::optional<VkSurfaceKHR> surface);
    std::tuple<VkSurfaceCapabilitiesKHR, std::vector<VkSurfaceFormatKHR>> querySwapChainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface);
    std::optional<uint32_t> findQueueFamily(VkPhysicalDevice gpu, std::optional<VkSurfaceKHR> surface);
    int getDeviceScore(VkPhysicalDevice device);

    // Creation
    VkInstance createInstance(bool validation, bool headless);
    bool allValidationLayersSupported(const std::vector<const char*>& validationLayers);
    VkDevice createLogicalDevice(VkPhysicalDevice gpu, uint32_t queueFamily, bool headless);
    VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex);
    VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool);
    VkBuffer createBuffer(VkDevice device, VkBufferUsageFlags usage, size_t size);
    VkImage createImage(VkDevice device, VkImageUsageFlags usage, VkExtent2D size);
    VkImageView createImageView(VkDevice device, VkImage image, VkFormat format);
    VkSampler createSampler(VkDevice device, VkFilter filter);
    VkDeviceMemory allocateMemory(VkPhysicalDevice gpu, VkDevice device, VkMemoryRequirements requirements, VkMemoryPropertyFlags properties);

    // Runtime tools
    uint32_t findMemoryType(VkPhysicalDevice gpu, uint32_t suitableTypes, VkMemoryPropertyFlags properties);
    void runSingleTimeCommand(VkDevice const &device, VkCommandPool const &commandPool, VkQueue const &queue, std::function<void(VkCommandBuffer)> const &commands);
    void imageTransitionBarrier(
        VkCommandBuffer const &commandBuffer,
        VkImage const &image,
        VkPipelineStageFlags srcStageMask,
        VkAccessFlags srcAccessMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );
} // namespace vkutil