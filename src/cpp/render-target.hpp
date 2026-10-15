#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>


const VkExtent2D RESOLUTION = {1024, 1024};


class RenderTarget {
public:
    virtual ~RenderTarget() = default;
    virtual VkExtent2D targetExtent() = 0;
    virtual bool renderNextFrame() = 0;
    virtual void presentImage(VkImage image, VkExtent2D imageSize) = 0;
};


class WindowRenderTarget : public RenderTarget {
    GLFWwindow* glfwWindow;

    VkInstance instance;
    VkPhysicalDevice gpu;
    VkSurfaceKHR surface;
    VkDevice device;
    VkCommandBuffer commandBuffer;
    VkQueue queue;
    VkSwapchainKHR swapchain;

    std::vector<VkImage> framebuffers;
    std::vector<VkSemaphore> computeDoneSemaphores;
    VkSemaphore imageAvailableSemaphore;

public:
    WindowRenderTarget(
        GLFWwindow *window,
        VkInstance instance,
        VkSurfaceKHR surface,
        VkPhysicalDevice gpu,
        VkDevice device,
        VkCommandPool commandPool
    );
    WindowRenderTarget(const WindowRenderTarget &obj) = delete; // do not allow copies of this class

    static std::pair<GLFWwindow *, std::optional<VkSurfaceKHR>> createWindow(
        VkInstance instance, const char *title, VkExtent2D extent);

    ~WindowRenderTarget() override;
    VkExtent2D targetExtent() override;
    bool renderNextFrame() override;
    void presentImage(VkImage image, VkExtent2D imageSize) override;
};


class FileRenderTarget : public RenderTarget {
    std::string destination;
    bool finished = false;

    VkPhysicalDevice gpu;
    VkDevice device;
    VkCommandPool commandPool;
    VkQueue queue;

public:
    FileRenderTarget(
        const std::string& destination,
        VkPhysicalDevice gpu,
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue
    );

    ~FileRenderTarget() override;
    VkExtent2D targetExtent() override;
    bool renderNextFrame() override;
    void presentImage(VkImage image, VkExtent2D imageExtent) override;
};
