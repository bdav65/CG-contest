#include "raytracer.hpp"

#ifdef FRAMETIME
#include <chrono>
#endif


RayTracer::RayTracer(Scene &scene, bool headless, std::vector<Photon> const &photons):
    scene(scene),
    vulkanHelper(headless),
    camera(vulkanHelper, sizeof(Camera)),
    objects(vulkanHelper, scene.getObjects()),
    spheres(vulkanHelper, scene.getSpheres()),
    triangles(vulkanHelper, scene.getTriangles()),
    lights(vulkanHelper, scene.getLights()),
    photons(vulkanHelper, photons)
{
   createResources();
   createPipeline();
}


void RayTracer::run() {
    while (vulkanHelper.getRenderTarget()->renderNextFrame()) {
        #ifdef FRAMETIME
        auto start = std::chrono::high_resolution_clock::now();
        #endif

        drawFrame();

        #ifdef FRAMETIME
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = duration_cast<std::chrono::microseconds>(stop - start);
        std::cout << duration.count() << " microseconds" << std::endl;
        #endif
    }
}


void RayTracer::createResources() {
    VkExtent2D outputSize = vulkanHelper.outputSize();
    auto [textureData, textureSize] = readImage(scene.texturePath());

    // Create images
    outputImage = vulkanHelper.createImage(VK_IMAGE_USAGE_STORAGE_BIT, outputSize);
    textureImage = vulkanHelper.createImage(VK_IMAGE_USAGE_SAMPLED_BIT, textureSize);
    textureSampler = vulkanHelper.createSampler(VK_FILTER_LINEAR);

    // Memory
    VkDeviceMemory outputImageMemory = vulkanHelper.allocateImageMemory(outputImage);
    VkDeviceMemory textureImageMemory = vulkanHelper.allocateImageMemory(textureImage);
    this->allocations = {outputImageMemory, textureImageMemory};

    // Write texture
    vulkanHelper.uploadToImage(textureData, textureImage, textureSize);
}


void RayTracer::createPipeline() {
    // Create shader module
    std::vector<char> shaderCode = readFile("./build/shaders/viewphotons.comp.spv");
	shaderModule = vulkanHelper.createShaderModule(shaderCode);

    // Create descriptor set
    std::vector<ResourceDescriptor> descriptors {7};

    camera.setDescriptor(descriptors[0]);
    objects.setDescriptor(descriptors[1]);
    spheres.setDescriptor(descriptors[2]);
    triangles.setDescriptor(descriptors[3]);
    lights.setDescriptor(descriptors[4]);
    photons.setDescriptor(descriptors[5]);

    descriptors[6].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptors[6].image = outputImage;

    auto [descriptorSet, descriptorLayout] = vulkanHelper.createDescriptorSet(descriptors);
    this->descriptorSet = descriptorSet;
    this->descriptorSetLayout = descriptorLayout;

	// Create pipeline
	auto [pipeline, pipelineLayout] = vulkanHelper.createComputePipeline(shaderModule, {descriptorLayout});
    this->pipeline = pipeline;
    this->pipelineLayout = pipelineLayout;
}


void RayTracer::drawFrame() {
	// Update uniform buffer
    VkExtent2D outputSize = vulkanHelper.outputSize();

    Camera currentCamera {
        scene.cameraOrigin(),
        4.0,
        glm::ivec2(outputSize.width, outputSize.height),
        4,
        scene.renderShadows(),
        scene.maxBounces(),
    };

	memcpy(camera.mapped, &currentCamera, sizeof(Camera));
    
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

    uint groupSizeX = outputSize.width / 8u, groupSizeY = outputSize.height / 8u;
	vkCmdDispatch(commandBuffer, groupSizeX, groupSizeY, 1);

    // Submit the commands, and present once they're done executing
    vulkanHelper.submitCommands(commandBuffer);
    vulkanHelper.waitForCommandCompletion();
	vulkanHelper.presentImage(outputImage, outputSize);
}


RayTracer::~RayTracer() {
    VkDevice device = vulkanHelper.getDevice();
    vkDeviceWaitIdle(device); // wait until all submitted stuff is done

    camera.destroy();
    objects.destroy();
    spheres.destroy();
    triangles.destroy();
    lights.destroy();
    photons.destroy();

    vkDestroyImage(device, this->outputImage, nullptr);
    vkDestroyImage(device, this->textureImage, nullptr);
    vkDestroySampler(device, this->textureSampler, nullptr);
    for (auto alloc : this->allocations)
        vkFreeMemory(device, alloc, nullptr); 
    vkDestroyDescriptorSetLayout(device, this->descriptorSetLayout, nullptr); 
    vkDestroyPipelineLayout(device, this->pipelineLayout, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr); 
    vkDestroyPipeline(device, this->pipeline, nullptr);
    vulkanHelper.terminate();
}
