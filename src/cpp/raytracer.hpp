#pragma once

#include "vulkan.hpp"
#include "scene.hpp"

class RayTracer {
    bool headless;
    Scene scene;
    VulkanHelper vulkanHelper;
 
    // Resources
    VkBuffer uniformBuffer;
    void *uniformBufferMap;
    VkBuffer objectBuffer;
    VkBuffer lightBuffer;
    VkImage outputImage;
    VkImage textureImage;
    VkSampler textureSampler;
    std::vector<VkDeviceMemory> allocations;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    // Pipeline stuff
    VkShaderModule shaderModule;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

   public:
    RayTracer(Scene scene, bool headless);
    void run();
    ~RayTracer();

   private:
    void createResources();
    void createPipeline();
    void drawFrame();
};

std::vector<char> readFile(const std::string& filename);
std::tuple<std::vector<unsigned char>, VkExtent2D> readImage(const std::filesystem::path& filename);
