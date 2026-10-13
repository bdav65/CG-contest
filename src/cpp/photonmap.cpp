#include "photonmap.hpp"

#define LIGHT_PHOTONS 10000

#define DEBUG

#ifdef DEBUG
#include <iostream>
#include <chrono>


std::ostream &operator<<(std::ostream &os, const glm::vec3 &v) {
    return os << '[' << v.x << ", " << v.y << ", " << v.z << ']';
}


std::ostream &operator<<(std::ostream &os, const Photon &p) {
    return os << "Photon {" << p.color << ", " << p.incident << ", " << p.location << '}';
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
    uint bounces = 8;

    return photons_per_light * scene.getLights().size() * bounces * sizeof(Photon);
}


PhotonMap::PhotonMap(Scene &scene, VulkanHelper &helper):
    scene(scene),
    vulkanHelper(helper),
    config(helper, sizeof(PhotonConfig)),
    objects(helper, scene.getObjects()),
    spheres(helper, scene.getSpheres()),
    triangles(helper, scene.getTriangles()),
    lights(helper, scene.getLights()),
    photons(helper, photonMapCapacity())
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

std::vector<Photon> PhotonMap::compute() {
    #ifdef DEBUG
    auto start = std::chrono::high_resolution_clock::now();
    #endif

	// Update uniform buffer
    PhotonConfig photonConfig {
        LIGHT_PHOTONS,
        static_cast<uint>(LIGHT_PHOTONS * scene.getLights().size()),
        8
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

    #ifdef DEBUG
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << "Generated photon map in " << duration.count() << " microseconds" << std::endl;

    int pc = 0;

    for (int i = 0; i < photonMapCapacity() / sizeof(Photon); i++) {
        Photon p = ((Photon *)photons.mapped)[i];
        
        if (p.color != glm::vec3{0, 0, 0}) {
            pc++;
        }
    }

    std::cout << "Resulted in " << pc << " photon collisions" << std::endl;
    #endif

    return {};
}


static void kdBalanceRecursive(std::vector<Photon> &ps, int l, int r) {
    glm::vec3 minimum {INFINITY, INFINITY, INFINITY};
    glm::vec3 maximum {-INFINITY, -INFINITY, -INFINITY};

    for (int i = l; i < r; i++) {
        minimum = glm::min(minimum, ps[i].location);
        maximum = glm::max(maximum, ps[i].location);
    }


}


void PhotonMap::kdBalance(std::vector<Photon> &ps) {
    kdBalanceRecursive(ps, 0, ps.size());
}


PhotonMap::~PhotonMap() {
    VkDevice device = vulkanHelper.getDevice();
    vkDeviceWaitIdle(device); // wait until all submitted stuff is done

    config.destroy();
    objects.destroy(); // these could be reused in the raytracer
    spheres.destroy();
    triangles.destroy();
    photons.destroy();

    vkDestroyDescriptorSetLayout(device, this->descriptorSetLayout, nullptr); 
    vkDestroyPipelineLayout(device, this->pipelineLayout, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr); 
    vkDestroyPipeline(device, this->pipeline, nullptr);
}
