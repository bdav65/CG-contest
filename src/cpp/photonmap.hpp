#pragma once

#include "vulkan.hpp"
#include "scene.hpp"
#include "buffer.hpp"


struct Photon {
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 incident;
    alignas(16) glm::vec3 location;
    int axis;
};


class PhotonMap {
    Scene &scene;
    VulkanHelper &vulkanHelper;
    bool caustics;
 
    UniformBuffer config;
    InputBuffer objects;
    InputBuffer spheres;
    InputBuffer triangles;
    InputBuffer lights;
    OutputBuffer photons;

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    VkShaderModule shaderModule;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    public:
        PhotonMap(Scene &scene, VulkanHelper &helper, bool caustics);
        [[nodiscard]] StagingBuffer compute();
        ~PhotonMap();

    private:
        size_t photonMapCapacity();
        void createResources();
        void createPipeline();
};
