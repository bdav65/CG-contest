#include "raytracer.hpp"
#include "lode/lodepng.hpp"

#include <iostream>
#include <fstream>
#include <utility>

#ifdef FRAMETIME
#include <chrono>
#endif


RayTracer::RayTracer(Scene scene, bool headless)
    : headless(headless), scene(std::move(scene)), vulkanHelper(VulkanHelper(headless))
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
    size_t uniformBufferSize = sizeof(Camera);
    VkExtent2D outputSize = vulkanHelper.outputSize();
    auto [textureData, textureSize] = readImage(scene.texturePath());

    // First, we create the buffers
    uniformBuffer = vulkanHelper.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBufferSize);

    storageBuffers.push_back(StorageBuffer(vulkanHelper, scene.getObjects()));
    storageBuffers.push_back(StorageBuffer(vulkanHelper, scene.getSpheres()));
    storageBuffers.push_back(StorageBuffer(vulkanHelper, scene.getTriangles()));
    storageBuffers.push_back(StorageBuffer(vulkanHelper, scene.getLights()));

    outputImage = vulkanHelper.createImage(VK_IMAGE_USAGE_STORAGE_BIT, outputSize);
    textureImage = vulkanHelper.createImage(VK_IMAGE_USAGE_SAMPLED_BIT, textureSize);
    textureSampler = vulkanHelper.createSampler(VK_FILTER_LINEAR);

    // Then, now that we have the buffers, we need to allocate GPU memory for them
    VkDeviceMemory uniformBufferMemory = vulkanHelper.allocateBufferMemory(uniformBuffer);
    VkDeviceMemory outputImageMemory = vulkanHelper.allocateImageMemory(outputImage);
    VkDeviceMemory textureImageMemory = vulkanHelper.allocateImageMemory(textureImage);
    this->allocations = {uniformBufferMemory, outputImageMemory, textureImageMemory};

    // Finally, we map the GPU memory to some CPU-visible memory so we can write to them
    vkMapMemory(vulkanHelper.getDevice(), uniformBufferMemory, 0, uniformBufferSize, 0, &uniformBufferMap);

    // Write texture
    vulkanHelper.uploadToImage(textureData, textureImage, textureSize);
}

void RayTracer::createPipeline() {
    // Create shader module
    std::vector<char> shaderCode = readFile("./build/shaders/raytracer.comp.spv");
	shaderModule = vulkanHelper.createShaderModule(shaderCode);

    // Create descriptor set
    std::vector<ResourceDescriptor> descriptors {7};

    descriptors[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptors[0].buffer = uniformBuffer;

    for (int i = 0; i < storageBuffers.size(); i++) {
        storageBuffers[i].setDescriptor(descriptors[i + 1]);
    }

    descriptors[5].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptors[5].image = outputImage;

    descriptors[6].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptors[6].image = textureImage;
    descriptors[6].sampler = textureSampler;

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
    Camera camera {
        scene.cameraOrigin(),
        4.0,
        glm::ivec2(outputSize.width, outputSize.height),
        4,
        scene.renderShadows(),
        scene.maxBounces(),
    };
	memcpy(uniformBufferMap, &camera, sizeof(Camera));
    
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

    vkDestroyBuffer(device, this->uniformBuffer, nullptr);

    for (auto storage : storageBuffers) {
        storage.destroy();
    }

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



/** Utility function to read a file's contents into a vector of characters */
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

/** Utility function to read and decode a PNG into a vector of bytes (RGBA) */
std::tuple<std::vector<unsigned char>, VkExtent2D> readImage(const std::filesystem::path& filename) {
    std::vector<unsigned char> image; //the raw pixels
    uint width, height;

    if (uint error = lodepng::decode(image, width, height, filename)) {
        std::cout << "Failed to read image! Error " << error << ": " << lodepng_error_text(error) << std::endl;
        throw std::runtime_error("Failed to load image.");
    }

    return std::make_pair(image, VkExtent2D {width, height});
}
