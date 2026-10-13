#pragma once

#include "vulkan.hpp"
#include "scene.hpp"
#include "buffer.hpp"


class RayTracer {
    bool headless;
    Scene &scene;
    VulkanHelper &vulkanHelper;
 
    // Resources
    UniformBuffer camera;
    StorageBuffer objects;
    StorageBuffer spheres;
    StorageBuffer triangles;
    StorageBuffer lights;
    MappedStorageBuffer photons;

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
        RayTracer(Scene &scene, VulkanHelper &helper, MappedStorageBuffer photons);
        void run();
        ~RayTracer();

    private:
        void createResources();
        void createPipeline();
        void drawFrame();
};
