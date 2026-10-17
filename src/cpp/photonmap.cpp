#include "photonmap.hpp"
#include "kdify.hpp"
#include "timer.hpp"

#define PHOTONS 25000
#define BOUNCES 32


struct PhotonConfig {
    uint photons_per_light;
    uint photons_total;
    int bounces;
};


size_t PhotonMap::photonMapCapacity() {
    // todo a better way to set this
    uint photons_per_light = PHOTONS / scene.getLights().size();
    uint bounces = BOUNCES;

    if (caustics) {
        return photons_per_light * scene.getLights().size() * sizeof(Photon);
    } else {
        return photons_per_light * scene.getLights().size() * (bounces + 1) * sizeof(Photon);
    }
}


PhotonMap::PhotonMap(Scene &scene, VulkanHelper &helper, bool caustics):
    scene(scene),
    vulkanHelper(helper),
    caustics(caustics),
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
    std::vector<char> shaderCode = readFile(caustics ? "./build/shaders/causticsmap.comp.spv" : "./build/shaders/photonmap.comp.spv");
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


StagingBuffer PhotonMap::compute() {
    Timer steps;
    Timer total;

	// Update uniform buffer
    PhotonConfig photonConfig {
        static_cast<uint>(PHOTONS / scene.getLights().size()),
        static_cast<uint>(PHOTONS / scene.getLights().size() * scene.getLights().size()),
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

    StagingBuffer photonData = photons.readBack();

    Photon *raw = static_cast<Photon *>(photonData.mapped_data);
    int hits = 0;

    // std::vector<Photon> hits;

    for (size_t i = 0; i < photonMapCapacity() / sizeof(Photon); i++) {
        if (raw[i].axis >= 0) {
            raw[hits] = raw[i];
            hits++;
        }
    }

    memcpy(photonData.mapped_size, &hits, sizeof(int));

    // std::cout << "log: Cleared miss photons, " << hits.size() << " photons remaining\n";
    if (hits) {
        steps.log(std::format("Copied photon data to readable memory ({} hits)", hits));
    
        kdify(raw, hits);
    
        steps.log("Constructed KD-tree");
    
        // photons.setSize(hits.size());
        // memcpy(photons.mapped_data, hits.data(), hits.size() * sizeof(Photon));
        
        // steps.log("Sent back photon data to GPU");
        total.log("Finished preparing the photon map");
    } else {
        steps.log("No photon hits!");
    }

    return photonData;
}


PhotonMap::~PhotonMap() {
    VkDevice device = vulkanHelper.getDevice();
    vkDeviceWaitIdle(device); // wait until all submitted stuff is done

    config.destroy();
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
