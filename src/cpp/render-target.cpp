#include "render-target.hpp"

#include "raytracer.hpp"

#include "lode/lodepng.hpp"

VkSwapchainKHR createSwapchain(
    VkPhysicalDevice gpu,
    VkDevice device,
    VkSurfaceKHR surface,
    uint32_t queueFamily,
    VkExtent2D resolution,
    std::vector<VkImage>& framebuffers
) {
    VkSurfaceFormatKHR surfaceFormat = {IMAGE_FORMAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    auto [capabilities, formats] = vkutil::querySwapChainSupport(gpu, surface);

    bool formatExists = std::any_of(formats.begin(), formats.end(), [surfaceFormat](auto f){return f.format == surfaceFormat.format && f.colorSpace == surfaceFormat.colorSpace;});
    bool swapchainSupportsTransfer = capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (!(formatExists && swapchainSupportsTransfer)) {
        throw std::runtime_error("Unfortunately the current GPU's swapchain images do not support the required features!");
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = resolution,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueFamily,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR, // most supported, over 97% of devices
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VkSwapchainKHR swapchain;
    vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    framebuffers = std::vector<VkImage>{imageCount};
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, framebuffers.data());
    printf("log: Created swapchain.\n");

    return swapchain;
}


std::pair<GLFWwindow *, std::optional<VkSurfaceKHR>> WindowRenderTarget::createWindow(VkInstance instance, const char *title, VkExtent2D extent) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    auto glfwWindow = glfwCreateWindow(extent.width, extent.height, title, nullptr, nullptr);

    VkSurfaceKHR surface;
    if (auto e = glfwCreateWindowSurface(instance, glfwWindow, nullptr, &surface); e != VK_SUCCESS) {
        printf("%d\n", e);
        throw std::runtime_error("Failed to create window surface!");
    }

    printf("log: initialized glfw\n");

    return std::make_pair(glfwWindow, std::optional(surface));
}

WindowRenderTarget::WindowRenderTarget(
    GLFWwindow *window,
    VkInstance instance,
    VkSurfaceKHR surface,
    VkPhysicalDevice gpu,
    VkDevice device,
    VkCommandPool commandPool
) : glfwWindow(window), instance(instance), gpu(gpu), surface(surface), device(device) {
    glfwSetWindowUserPointer(glfwWindow, this);

    // Create a command buffer, used for copying image to swapchain
    commandBuffer = vkutil::createCommandBuffer(device, commandPool);

    // Create a swapchain
    uint32_t queueFamily = vkutil::findQueueFamily(gpu, surface).value();
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
    VkExtent2D framebufferSize = targetExtent();
    swapchain = createSwapchain(gpu, device, surface, queueFamily, framebufferSize, framebuffers);

    // Create synchronization primitives
    VkSemaphoreCreateInfo semaphoreInfo {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    for (int idx = 0; idx < framebuffers.size(); idx++) {
        VkSemaphore semaphore;
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
        computeDoneSemaphores.push_back(semaphore);
    }
}

WindowRenderTarget::~WindowRenderTarget() {
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

    for (int idx = 0; idx < framebuffers.size(); idx++)
        vkDestroySemaphore(device, computeDoneSemaphores[idx], nullptr);

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);

    glfwDestroyWindow(this->glfwWindow);
    this->glfwWindow = nullptr;
    glfwTerminate();
}

VkExtent2D WindowRenderTarget::targetExtent() {
    int width, height;
    glfwGetFramebufferSize(glfwWindow, &width, &height);

    return VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

bool WindowRenderTarget::renderNextFrame() {
    glfwPollEvents();
    return !glfwWindowShouldClose(glfwWindow);
}

void WindowRenderTarget::presentImage(VkImage image, VkExtent2D imageSize) {
    // Step 1: Wait for the next framebuffer to be free
    uint32_t framebufferIdx;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, nullptr, &framebufferIdx);

    VkCommandBufferBeginInfo beginInfo {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Step 2: Record command buffer
    vkutil::imageTransitionBarrier(
        commandBuffer,
        framebuffers[framebufferIdx],
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    VkImageSubresourceLayers subresource {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkImageCopy imageCopy = {
        .srcSubresource = subresource,
        .srcOffset = {0, 0, 0},
        .dstSubresource = subresource,
        .dstOffset = {0, 0, 0},
        .extent = {imageSize.width, imageSize.height, 1},
    };

    vkCmdCopyImage(
        commandBuffer,
        image,
        VK_IMAGE_LAYOUT_GENERAL,
        framebuffers[framebufferIdx],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopy
    );

    vkutil::imageTransitionBarrier(
        commandBuffer,
        framebuffers[framebufferIdx],
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_NONE,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    vkEndCommandBuffer(commandBuffer);

    // Step 3: Submit compute job
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphore,
        .pWaitDstStageMask = &stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &computeDoneSemaphores[framebufferIdx]
    };
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

    // Step 4: Submit presentation / dump to disk
    VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &computeDoneSemaphores[framebufferIdx],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &framebufferIdx
    };

    if (vkQueuePresentKHR(queue, &presentInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to present rendered image to screen!");
    }
}


FileRenderTarget::FileRenderTarget(
    const std::string& destination,
    VkPhysicalDevice gpu,
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue
) : destination(destination), gpu(gpu), device(device), commandPool(commandPool), queue(queue) {}

FileRenderTarget::~FileRenderTarget() = default;

VkExtent2D FileRenderTarget::targetExtent() {
    return RESOLUTION;
}

bool FileRenderTarget::renderNextFrame() {
    return !finished;
}

void FileRenderTarget::presentImage(VkImage image, VkExtent2D imageExtent) {
    // Step 1: Create a CPU-mapped buffer
    size_t size = 4 * imageExtent.width * imageExtent.height;

    VkBuffer buffer = vkutil::createBuffer(device, VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkDeviceMemory bufferMemory = vkutil::allocateMemory(gpu, device, memReqs, properties);

    void *bufferMemoryMap;
    vkMapMemory(device, bufferMemory, 0, size, 0, &bufferMemoryMap);
    vkBindBufferMemory(device, buffer, bufferMemory, 0);

    // Step 2: Copy the framebuffer to the cpu buffer
    vkutil::runSingleTimeCommand(device, commandPool, queue, [image, imageExtent, buffer](auto commandBuffer){
        VkImageSubresourceLayers subresource {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        VkBufferImageCopy imageBufferCopy {
            .bufferOffset = 0,
            .imageSubresource = subresource,
            .imageOffset = {0, 0, 0},
            .imageExtent = {imageExtent.width, imageExtent.height, 1},
        };
        vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, buffer, 1, &imageBufferCopy);
    });

    // Step 3: dump to file and free
    const auto *data = static_cast<const unsigned char *>(bufferMemoryMap);
    if (unsigned error = lodepng::encode(destination, data, imageExtent.width, imageExtent.height)) {
        std::cout << "Failed to encode image. Error " << error << ": "<< lodepng_error_text(error) << std::endl;
    }

    vkFreeMemory(device, bufferMemory, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);

    finished = true;
}
