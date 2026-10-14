#include "vulkan.hpp"

using namespace vkutil;

VulkanHelper::VulkanHelper(bool headless):
    headless(headless),
    descriptorPool(nullptr)
{
    initializeVulkan();
}

void VulkanHelper::initializeVulkan() {
    instance = createInstance(true, headless);

    auto [window, surface] = headless
        ? std::make_pair(nullptr, std::nullopt)
        : WindowRenderTarget::createWindow(instance, "Vulkan RayTracer", {512, 512});

    gpu = pickPhysicalDevice(instance, surface);
    uint32_t queueFamily = findQueueFamily(gpu, surface).value();
    device = createLogicalDevice(gpu, queueFamily, headless);
    vkGetDeviceQueue(device, queueFamily, 0, &computeQueue);
    commandPool = createCommandPool(device, queueFamily);
    commandBuffer = createCommandBuffer(device, commandPool);

    if (headless) {
        renderTarget = std::make_unique<FileRenderTarget>("output.png", gpu, device, commandPool, computeQueue);
    } else {
        renderTarget = std::make_unique<WindowRenderTarget>(window, instance, surface.value(), gpu, device, commandPool);
    }

    // Synchronization structure initialization
    VkFenceCreateInfo fenceInfo {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT // pre-signaled for first frame
    };
    vkCreateFence(device, &fenceInfo, nullptr, &computeInFlightFence);
}

void VulkanHelper::waitForCommandCompletion() {
    vkWaitForFences(device, 1, &computeInFlightFence, VK_TRUE, UINT64_MAX);
}

void VulkanHelper::submitCommands(VkCommandBuffer const &commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    vkResetFences(device, 1, &computeInFlightFence);

    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 0,
        .pWaitDstStageMask = &stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
    };

    vkQueueSubmit(computeQueue, 1, &submitInfo, computeInFlightFence);
}

void VulkanHelper::presentImage(VkImage const &outputImage, VkExtent2D imageExtent) {
    renderTarget->presentImage(outputImage, imageExtent);
}

void VulkanHelper::terminate() {
    renderTarget.reset();

    vkDeviceWaitIdle(device); // wait until all submitted stuff is done

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyFence(device, computeInFlightFence, nullptr);

    for (auto imageView : imageViews)
        vkDestroyImageView(device, imageView, nullptr);

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

VkShaderModule VulkanHelper::createShaderModule(std::vector<char> shaderSrc) {
    VkShaderModuleCreateInfo shaderInfo {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderSrc.size(),
        .pCode = reinterpret_cast<const uint32_t*>(shaderSrc.data()),
    };

    VkShaderModule shaderModule;
    vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule);

    return shaderModule;
}

std::tuple<VkPipeline, VkPipelineLayout> VulkanHelper::createComputePipeline(VkShaderModule shaderModule, std::vector<VkDescriptorSetLayout> descriptorLayouts) {
    // Step 1: Create a pipeline layout (pipelines can have multiple descriptor set layouts)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size()),
        .pSetLayouts = descriptorLayouts.data(),
    };
    
    VkPipelineLayout layout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);

    // Step 2: Create the pipeline
    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaderModule,
            .pName = "main",
        },
        .layout = layout,
    };

    VkPipeline computePipeline;
    vkCreateComputePipelines(device, 0, 1, &pipelineInfo, nullptr, &computePipeline);

    return std::make_tuple(computePipeline, layout);
}

VkBuffer VulkanHelper::createBuffer(VkBufferUsageFlags usage, size_t size) {
    return vkutil::createBuffer(device, usage, size);
}

VkImage VulkanHelper::createImage(VkImageUsageFlags usage, VkExtent2D size) {
    return vkutil::createImage(device, usage, size);
}

VkSampler VulkanHelper::createSampler(VkFilter filter) {
    return vkutil::createSampler(device, filter);
}

VkDeviceMemory VulkanHelper::allocateBufferMemory(VkBuffer const &buffer, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);

    VkDeviceMemory memory = allocateMemory(gpu, device, memReqs, properties);
    vkBindBufferMemory(device, buffer, memory, 0);

    return memory;
}

VkDeviceMemory VulkanHelper::allocateImageMemory(VkImage const &image) {
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkDeviceMemory memory = vkutil::allocateMemory(gpu, device, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkBindImageMemory(device, image, memory, 0);

    // Now that the image has memory, transition its layout to VK_IMAGE_LAYOUT_GENERAL
    runSingleTimeCommand(this->device, this->commandPool, this->computeQueue, [&image](auto cmd) {
        imageTransitionBarrier(
            cmd,
            image,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_ACCESS_NONE,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_NONE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL
        );
    });

    return memory;
}

VkCommandBuffer VulkanHelper::getCommandBuffer() {
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

VkExtent2D VulkanHelper::outputSize() {
    return renderTarget->targetExtent();
}

VkDevice VulkanHelper::getDevice() {
    return device;
}

std::tuple<VkDescriptorSet, VkDescriptorSetLayout> VulkanHelper::createDescriptorSet(std::vector<ResourceDescriptor> descriptors) {
    // Step 1: Create a descriptor set pool
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto &descriptor : descriptors) {
        poolSizes.push_back(VkDescriptorPoolSize {
            .type = descriptor.type,
            .descriptorCount = 16 // abundance,
        });
    }

    VkDescriptorPoolCreateInfo poolInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // to allow descriptor sets to destroy themselves
        .maxSets = static_cast<uint32_t>(1),
        .poolSizeCount = static_cast<uint32_t>(descriptors.size()),
        .pPoolSizes = poolSizes.data(),
    };

    if (descriptorPool != nullptr) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    // Step 2: Create a descriptor set layout (define the layout of the shader IO)
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (uint idx = 0; idx < descriptors.size(); ++idx) {
        bindings.push_back(VkDescriptorSetLayoutBinding {
            .binding = idx,
            .descriptorType = descriptors[idx].type,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        });
    }

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout descriptorLayout;
    vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorLayout);


    // Step 3: Allocate the descriptor set
    VkDescriptorSetAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorLayout,
    };

    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);


    // Step 4: Populate the descriptor set with the descriptors
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstSet = descriptorSet;

    for (size_t j = 0; j < descriptors.size(); j++) {
        ResourceDescriptor descriptor = descriptors[j];

        descriptorWrite.dstBinding = j;
        descriptorWrite.descriptorType = descriptor.type;

        bool isImage = descriptor.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || descriptor.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bool isBuffer = descriptor.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || descriptor.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bool isSampler = descriptor.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || descriptor.type == VK_DESCRIPTOR_TYPE_SAMPLER;

        if (isImage) {
            VkImageView imageView = createImageView(device, descriptor.image, IMAGE_FORMAT);
            imageViews.push_back(imageView);

            VkDescriptorImageInfo imageInfo;
            imageInfo.imageView = imageView;
            if (isSampler)
                imageInfo.sampler = descriptor.sampler;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            descriptorWrite.pImageInfo = &imageInfo;
            descriptorWrite.pBufferInfo = VK_NULL_HANDLE;
            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        } else if (isBuffer) {
            VkDescriptorBufferInfo bufferInfo;
            bufferInfo.buffer = descriptor.buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = VK_WHOLE_SIZE;

            descriptorWrite.pImageInfo = VK_NULL_HANDLE;
            descriptorWrite.pBufferInfo = &bufferInfo;
            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        } else throw std::runtime_error("Encountered unknown descriptor!");
    }

    return std::make_tuple(descriptorSet, descriptorLayout);
}

void VulkanHelper::uploadToImage(std::vector<unsigned char> data, VkImage image, VkExtent2D imageExtent) {
    // Step 1: Create a CPU-mapped buffer
    size_t size = 4 * imageExtent.width * imageExtent.height;

    VkBuffer buffer = createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size);
    VkDeviceMemory bufferMemory = allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void *bufferMemoryMap;
    vkMapMemory(device, bufferMemory, 0, size, 0, &bufferMemoryMap);

    // Step 2: Copy the texture data to the buffer
    memcpy(bufferMemoryMap, data.data(), size);

    // Step 3: Copy the data to the final image and free
    runSingleTimeCommand(device, commandPool, computeQueue, [image, imageExtent, buffer](auto commandBuffer){
        VkImageSubresourceLayers subresource {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        VkBufferImageCopy imageBufferCopy {
            .bufferOffset = 0,
            .imageSubresource = subresource,
            .imageOffset = {},
            .imageExtent = {imageExtent.width, imageExtent.height, 1},
        };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_GENERAL, 1, &imageBufferCopy);
    });

    vkFreeMemory(device, bufferMemory, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);
}

/**
 * Creates a debug messenger.
 *
 * Since this function is retrieved dynamically at runtime, it is not defined in vulkan.h. This means that
 * unless it is defined here, linking will fail.
 *
 * This implementation just ends up calling the retrieved pointer.
 */
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger
) {
    static PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerPointer =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
        );

    return createDebugUtilsMessengerPointer(instance, pCreateInfo, pAllocator, pMessenger);
}

/**
 * Destroyes a debug messenger, see above.
 */
VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    VkAllocationCallbacks const * pAllocator
) {
    static PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerPointer =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
        );

    return destroyDebugUtilsMessengerPointer(instance, messenger, pAllocator);
}

VkInstance vkutil::createInstance(bool validation, bool headless) {
    // Check that validation layers are supported
    if (!allValidationLayersSupported(VALIDATION_LAYERS))
        throw std::runtime_error("Some validation layers needed aren't supported");

    std::vector<const char *> extensions;
    if (!headless) {
        glfwInit();
        uint32_t count = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
        extensions = std::vector(glfwExtensions, glfwExtensions + count);
    }

    if (validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan RayTracer",
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_MAKE_VERSION(1, 3, 0),
    };

    // info about the instance
    VkInstanceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size()),
        .ppEnabledLayerNames = VALIDATION_LAYERS.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkInstance instance;
    vkCreateInstance(&createInfo, nullptr, &instance);
    printf("log: Made instance with %ld extensions!\n", extensions.size());

    return instance;
}

/*
 * Checks if all given validation layers are supported by the current instance
 */
bool vkutil::allValidationLayersSupported(std::vector<const char*> const &validationLayers) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers{layerCount};
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            printf("log: could not find layer %s\n", layerName);
            return false;
        }
    }

    return true;
}

VkPhysicalDevice vkutil::pickPhysicalDevice(VkInstance instance, std::optional<VkSurfaceKHR> surface) {
    // Get a list of all GPUs
    uint32_t gpuCount;
    vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> gpus {gpuCount};
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());

    // Find the best supported one
    int bestScore = -INT32_MAX;
    VkPhysicalDevice const *bestGpu = VK_NULL_HANDLE;
    for (const auto& gpu : gpus) {
        if (!isDeviceSuitable(gpu, surface)) continue;

        if (const int score = getDeviceScore(gpu); score > bestScore) {
            bestGpu = &gpu;
            bestScore = score;
        }
    }

    if (bestGpu == VK_NULL_HANDLE)
        throw std::runtime_error("Failed to find a suitable GPU!");

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(*bestGpu, &properties);
    printf("log: picked device %s\n", properties.deviceName);

    return *bestGpu;
}

bool vkutil::isDeviceSuitable(VkPhysicalDevice gpu, std::optional<VkSurfaceKHR> surface) {
    bool supportsSurface = true;

    // if we are rendering to a surface, check swapchain and surface support
    if (surface.has_value()) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, availableExtensions.data());

        for (auto ext : availableExtensions) {
            if (!strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                auto [caps, formats] = querySwapChainSupport(gpu, surface.value());
                VkSurfaceFormatKHR surfaceFormat = {IMAGE_FORMAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
                supportsSurface = std::find_if(formats.begin(), formats.end(), [surfaceFormat](auto lhs) {
                    return lhs.format == surfaceFormat.format && lhs.colorSpace == surfaceFormat.colorSpace;
                }) != formats.end();
                break;
            }

            supportsSurface = false;
        }
    }

    VkImageFormatProperties _;
    bool supportsFormat = vkGetPhysicalDeviceImageFormatProperties(
        gpu,
        IMAGE_FORMAT,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_LINEAR,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        0,
        &_
    ) == VK_SUCCESS;

    return supportsFormat && supportsSurface && findQueueFamily(gpu, surface).has_value();
}

std::tuple<VkSurfaceCapabilitiesKHR, std::vector<VkSurfaceFormatKHR>> vkutil::querySwapChainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats {formatCount};
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, formats.data());

    return std::make_tuple(capabilities, formats);
}

int vkutil::getDeviceScore(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

/// Returns a family index if a family was found with compute and presentation support, empty optional otherwise.
std::optional<uint32_t> vkutil::findQueueFamily(VkPhysicalDevice gpu, std::optional<VkSurfaceKHR> surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies{queueFamilyCount};
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());

    for (uint32_t familyIdx = 0; familyIdx < queueFamilies.size(); familyIdx++) {
        VkBool32 supportsPresenting = false;
        VkQueueFamilyProperties familyProperties = queueFamilies[familyIdx];

        if (surface.has_value())
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, familyIdx, surface.value(), &supportsPresenting);

        if (familyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT
            && (!surface.has_value() || supportsPresenting))
            return std::make_optional(familyIdx);
    }

    return std::nullopt;
}

VkDevice vkutil::createLogicalDevice(VkPhysicalDevice gpu, uint32_t queueFamily, bool headless) {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    const std::vector deviceExtensions = headless
        ? std::vector<const char *>{}
        : std::vector{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = {},
    };

    VkDevice device;
    vkCreateDevice(gpu, &createInfo, nullptr, &device);
    printf("log: Created logical device.\n");

    return device;
}

VkCommandPool vkutil::createCommandPool(VkDevice device, uint32_t queueFamilyIndex) {
    VkCommandPoolCreateInfo poolInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // command buffers can be rerecorded individually
        .queueFamilyIndex = queueFamilyIndex,
    };

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool!");

    printf("log: Created command pool\n");
    return commandPool;
}

VkCommandBuffer vkutil::createCommandBuffer(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = static_cast<uint32_t>(1)
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    printf("log: Created command buffer\n");

    return commandBuffer;
}

uint32_t vkutil::findMemoryType(VkPhysicalDevice gpu, uint32_t suitableTypes, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (suitableTypes & (1 << i) // it has to be a suitable memory type (memoryTypeBits indicate the supported indices of memory types of physical device)
            && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) { // it has to match all of the required properties
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

VkImageView vkutil::createImageView(VkDevice device, VkImage image, VkFormat format) {
    VkImageViewCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    VkImageView view;
    vkCreateImageView(device, &createInfo, nullptr, &view);

    return view;
}

VkBuffer vkutil::createBuffer(VkDevice device, VkBufferUsageFlags usage, size_t size) {
    VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer;
    vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

    return buffer;
}

VkSampler vkutil::createSampler(VkDevice device, VkFilter filter) {
    VkSamplerCreateInfo samplerInfo {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .compareEnable = VK_FALSE,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler sampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    return sampler;
}

VkImage vkutil::createImage(VkDevice device, VkImageUsageFlags usage, VkExtent2D size) {
    // Create a simple image
    VkImageCreateInfo imageInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = IMAGE_FORMAT,
        .extent = VkExtent3D(size.width, size.height, 1),
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR, // while being slower, this allows us to transfer from and to without annoying copies
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // this should never be presented
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage image;
    vkCreateImage(device, &imageInfo, nullptr, &image);

    return image;
}

VkDeviceMemory vkutil::allocateMemory(VkPhysicalDevice gpu, VkDevice device, VkMemoryRequirements requirements, VkMemoryPropertyFlags properties) {
    VkMemoryAllocateInfo memoryInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = findMemoryType(gpu, requirements.memoryTypeBits, properties),
    };

    VkDeviceMemory memory;
    vkAllocateMemory(device, &memoryInfo, nullptr, &memory);

    return memory;
}

void vkutil::imageTransitionBarrier(
    VkCommandBuffer const &commandBuffer,
    VkImage const &image,
    VkPipelineStageFlags srcStageMask,
    VkAccessFlags srcAccessMask,
    VkPipelineStageFlags dstStageMask,
    VkAccessFlags dstAccessMask,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
) {
    VkImageMemoryBarrier imageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,     // flush these caches,
        .dstAccessMask = dstAccessMask,    // invalidate these caches,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0, 0, nullptr, 0, nullptr,
        1, &imageBarrier
    );
}

void vkutil::runSingleTimeCommand(VkDevice const &device, VkCommandPool const &commandPool, VkQueue const &queue, std::function<void(VkCommandBuffer)> const &commands) {
    VkCommandBufferAllocateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &bufferInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    commands(commandBuffer);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
