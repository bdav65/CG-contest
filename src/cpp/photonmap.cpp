#include "photonmap.hpp"

#define LIGHT_PHOTONS 10000
#define BOUNCES 4

#define DEBUG

#ifdef DEBUG
#include <iostream>
#include <chrono>


std::ostream &operator<<(std::ostream &os, const glm::vec3 &v) {
    return os << '[' << v.x << ", " << v.y << ", " << v.z << ']';
}


std::ostream &operator<<(std::ostream &os, const Photon &p) {
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

    return photons_per_light * scene.getLights().size() * bounces * sizeof(Photon);
}


PhotonMap::PhotonMap(Scene &scene):
    scene(scene),
    vulkanHelper(true),
    config(vulkanHelper, sizeof(PhotonConfig)),
    objects(vulkanHelper, scene.getObjects()),
    spheres(vulkanHelper, scene.getSpheres()),
    triangles(vulkanHelper, scene.getTriangles()),
    lights(vulkanHelper, scene.getLights()),
    photons(vulkanHelper, photonMapCapacity())
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

    #ifdef DEBUG
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << "Generated photon map in " << duration.count() << " microseconds" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    #endif

    std::vector<Photon> kd;

    for (int i = 0; i < photonMapCapacity() / sizeof(Photon); i++) {
        Photon p = ((Photon *)photons.mapped)[i];
        
        if (p.color != glm::vec3{0, 0, 0}) {
            kd.push_back(p);
        }
    }

    kdBalance(kd);

    #ifdef DEBUG
    stop = std::chrono::high_resolution_clock::now();
    duration = duration_cast<std::chrono::microseconds>(stop - start);
    std::cout << "Constructed kd-tree (" << kd.size() << " photons) in " << duration.count() << " microseconds" << std::endl;
    #endif

    return kd;
}


static void kdBalanceRecursive(std::vector<Photon> &ps, int l, int r) {
    if (r - l <= 1) {
        return;
    }

    // std::cout << "Call(" << l << ", " << r << ")\n";

    glm::vec3 minimum {INFINITY, INFINITY, INFINITY};
    glm::vec3 maximum {-INFINITY, -INFINITY, -INFINITY};

    for (int i = l; i < r; i++) {
        minimum = glm::min(minimum, ps[i].location);
        maximum = glm::max(maximum, ps[i].location);
    }

    // std::cout << "Minimum " << minimum << " and maximum " << maximum << '\n';

    glm::vec3 delta = maximum - minimum;

    int m = l + (r - l) / 2;

    for (int i = 0; i < 3; i++) {
        if (glm::max(delta.x, glm::max(delta.y, delta.z)) != delta[i]) {
            continue;
        }

        // std::cout << "Splitting on axis " << i << '\n';

        std::sort(ps.begin() + l, ps.begin() + r, [i](Photon const &p1, Photon const &p2){
            return p1.location[i] < p2.location[i];
        });

        ps[m].axis = i;
        break;
    }

    kdBalanceRecursive(ps, l, m - 1);
    kdBalanceRecursive(ps, m + 1, r);
}


void PhotonMap::kdBalance(std::vector<Photon> &ps) {
    kdBalanceRecursive(ps, 0, ps.size());
}


PhotonMap::~PhotonMap() {
    std::cout << "Destroying photon map Vulkan context\n";

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
