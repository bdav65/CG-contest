#include "photonmap.hpp"
#include "kdify.hpp"
#include "timer.hpp"

#define LIGHT_PHOTONS 100000
#define BOUNCES 4

#define DEBUG

#ifdef DEBUG
#include <iostream>


static std::ostream &operator<<(std::ostream &os, const glm::vec3 &v) {
    return os << '[' << v.x << ", " << v.y << ", " << v.z << ']';
}


static std::ostream &operator<<(std::ostream &os, const Photon &p) {
    return os << "Photon {" << p.color << ", " << p.incident << ", " << p.location << ", " << p.axis << '}';
}

#endif



struct PhotonConfig {
    uint photons_per_light;
    uint photons_total;
    int bounces;
};


size_t PhotonMap::photonMapCapacity() {
    // todo a better way to set this
    uint photons_per_light = LIGHT_PHOTONS;
    uint bounces = BOUNCES;

    return photons_per_light * scene.getLights().size() * (bounces + 1) * sizeof(Photon);
}


PhotonMap::PhotonMap(Scene &scene, VulkanHelper &helper):
    scene(scene),
    vulkanHelper(helper),
    config(vulkanHelper, sizeof(PhotonConfig)),
    objects(vulkanHelper, scene.getObjects()),
    spheres(vulkanHelper, scene.getSpheres()),
    triangles(vulkanHelper, scene.getTriangles()),
    lights(vulkanHelper, scene.getLights()),
    photons(vulkanHelper, 16 + photonMapCapacity())
{
    createResources();
    createPipeline();
}


void PhotonMap::createResources() {
    // already initialized in constructor
}


void PhotonMap::createPipeline() {
    // Create shader module
    std::vector<char> shaderCode = readFile("./build/shaders/photonmap.comp.spv");
	shaderModule = vulkanHelper.createShaderModule(shaderCode);

    // Create descriptor set
    std::vector<ResourceDescriptor> descriptors {6};

    config.setDescriptor(descriptors[0]);
    objects.setDescriptor(descriptors[1]);
    spheres.setDescriptor(descriptors[2]);
    triangles.setDescriptor(descriptors[3]);
    lights.setDescriptor(descriptors[4]);
    photons.setDescriptor(descriptors[5]);

    auto [descriptorSet, descriptorLayout] = vulkanHelper.createDescriptorSet(descriptors);
    this->descriptorSet = descriptorSet;
    this->descriptorSetLayout = descriptorLayout;

	// Create pipeline
	auto [pipeline, pipelineLayout] = vulkanHelper.createComputePipeline(shaderModule, {descriptorLayout});
    this->pipeline = pipeline;
    this->pipelineLayout = pipelineLayout;
}


MappedStorageBuffer PhotonMap::compute() {
    Timer steps;
    Timer total;

	// Update uniform buffer
    PhotonConfig photonConfig {
        LIGHT_PHOTONS,
        static_cast<uint>(LIGHT_PHOTONS * scene.getLights().size()),
        BOUNCES
    };

	memcpy(config.mapped, &photonConfig, sizeof(PhotonConfig));
    
    // Get the command buffer
    VkCommandBuffer commandBuffer = vulkanHelper.getCommandBuffer();

	// Write the commands to render the image
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(
	    commandBuffer,
	    VK_PIPELINE_BIND_POINT_COMPUTE,
	    pipelineLayout,
	    0,
	    1, &descriptorSet,
	    0, nullptr
	);

    uint groupSizeX = photonMapCapacity() / 64 + !!(photonMapCapacity() % 64);
	vkCmdDispatch(commandBuffer, groupSizeX, 1, 1);

    // Submit the commands, and present once they're done executing
    vulkanHelper.submitCommands(commandBuffer);
    vulkanHelper.waitForCommandCompletion();

    steps.log(std::format("Finished tracing {} photons", photonMapCapacity() / sizeof(Photon)));

    std::vector<Photon> hits;
    Photon *raw = static_cast<Photon *>(photons.mapped_data);

    for (size_t i = 0; i < photonMapCapacity() / sizeof(Photon); i++) {
        if (raw[i].axis >= 0) {
            hits.push_back(raw[i]);
        }
    }

    steps.log("Copied photon data to RAM");

    kdify(hits.data(), hits.size());

    steps.log("Constructed KD-tree");

    photons.setSize(hits.size());
    memcpy(photons.mapped_data, hits.data(), hits.size() * sizeof(Photon));
    
    steps.log("Sent back photon data to GPU");
    total.log("Finished preparing the photon map");

    return photons;
}


PhotonMap::~PhotonMap() {
    VkDevice device = vulkanHelper.getDevice();
    vkDeviceWaitIdle(device); // wait until all submitted stuff is done

    config.destroy();
    objects.destroy(); // these could be reused in the raytracer
    spheres.destroy();
    triangles.destroy();
    lights.destroy();

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr); 
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
}
