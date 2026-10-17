#include "photonmap.hpp"
#include "kdify.hpp"
#include "timer.hpp"
#include <iostream>


PhotonMap::PhotonMap(Scene &scene, VulkanHelper &helper):
    vulkanHelper(helper),
    config(scene.getCausticsConfig()),
    configBuffer(vulkanHelper, sizeof(CausticsConfig)),
    objects(vulkanHelper, scene.getObjects()),
    spheres(vulkanHelper, scene.getSpheres()),
    triangles(vulkanHelper, scene.getTriangles()),
    lights(vulkanHelper, scene.getLights()),
    photons(vulkanHelper, 16 + config.photonCount * sizeof(Photon))
{
    createPipeline();
}


void PhotonMap::createPipeline() {
    // Create shader module
    std::vector<char> shaderCode = readFile("./build/shaders/causticsmap.comp.spv");
	shaderModule = vulkanHelper.createShaderModule(shaderCode);

    // Create descriptor set
    std::vector<ResourceDescriptor> descriptors {6};

    configBuffer.setDescriptor(descriptors[0]);
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


StagingBuffer PhotonMap::compute() {
    Timer steps;
    Timer total;

	// Update uniform buffer
	memcpy(configBuffer.mapped, &config, sizeof(CausticsConfig));

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

    uint groupSizeX = config.photonCount / 64 + !!(config.photonCount % 64);
	vkCmdDispatch(commandBuffer, groupSizeX, 1, 1);

    // Submit the commands, and present once they're done executing
    vulkanHelper.submitCommands(commandBuffer);
    vulkanHelper.waitForCommandCompletion();

    steps.log(std::format("Finished tracing {} photons", config.photonCount));

    StagingBuffer photonData = photons.readBack();

    Photon *raw = static_cast<Photon *>(photonData.mapped_data);
    int hits = 0;

    for (size_t i = 0; i < config.photonCount; i++) {
        if (raw[i].axis >= 0) {
            raw[hits] = raw[i];
            hits++;
        }
    }

    memcpy(photonData.mapped_size, &hits, sizeof(int));

    if (hits) {
        steps.log(std::format("Copied photon data to readable memory ({} hits)", hits));
    
        kdify(raw, hits);

        steps.log("Constructed KD-tree");
        total.log("Finished preparing the photon map");
    } else {
        steps.log("No photon hits!");
    }

    return photonData;
}


PhotonMap::~PhotonMap() {
    VkDevice device = vulkanHelper.getDevice();
    vkDeviceWaitIdle(device); // wait until all submitted stuff is done

    configBuffer.destroy();
    objects.destroy(); // these could be reused in the raytracer
    spheres.destroy();
    triangles.destroy();
    lights.destroy();
    photons.destroy();

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr); 
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
}
