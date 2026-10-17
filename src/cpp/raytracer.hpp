#pragma once

#include "vulkan.hpp"
#include "scene.hpp"
#include "buffer.hpp"


class RayTracer {
    bool headless;
    VulkanHelper &vulkanHelper;
    RaytracerConfig config;
 
    // Resources
    UniformBuffer configBuffer;
    InputBuffer objects;
    InputBuffer spheres;
    InputBuffer triangles;
    InputBuffer lights;
    InputBuffer photons;

    VkImage outputImage;
    VkDeviceMemory outputImageMemory;

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    // Pipeline stuff
    VkShaderModule shaderModule;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    public:
        RayTracer(Scene &scene, VulkanHelper &helper, StagingBuffer photons);
        void run();
        ~RayTracer();

    private:
        void createResources();
        void createPipeline();
        void drawFrame();
};
