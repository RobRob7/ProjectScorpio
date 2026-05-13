#ifndef VULKAN_MAIN_H
#define VULKAN_MAIN_H

#include "image_vk.h"
#include "buffer_vk.h"
#include "acceleration_structure_vk.h"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <cstdint>
#include <optional>
#include <array>

struct GLFWwindow;
struct FrameContext;

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const 
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    } // end of isComplete()
};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities{};
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

struct PendingUpload
{
    vk::CommandBuffer cmd{};
    vk::Fence fence{};
    std::vector<BufferVk> stagingBuffers;
};

struct RetiredFrameResources
{
    std::vector<BufferVk> buffers;
    std::vector<ImageVk> images;
    std::vector<AccelerationStructureVk> accelStructures;
};

class VulkanMain
{
public:
    explicit VulkanMain(GLFWwindow* window);
    ~VulkanMain();

    void setDebugName(
        const vk::ObjectType type,
        const uint64_t handle,
        const std::string_view& name
    ) const;

    void init();
    void waitIdle() const;

    bool beginFrame(FrameContext& out);
    bool endFrame(const FrameContext& frame);

    void notifyFramebufferResized() { framebufferResized_ = true; }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

    void discardSingleTimeCommands(vk::CommandBuffer cmd) const;

    vk::CommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(vk::CommandBuffer commandBuffer) const;

    void submitUpload(
        vk::CommandBuffer cmd,
        std::vector<BufferVk>&& stagingBuffers
    );

    void processPendingUploads();

    void recordCopyBuffer(
        vk::CommandBuffer,
        vk::Buffer srcBuffer,
        vk::Buffer dstBuffer,
        vk::DeviceSize size
    ) const;

    vk::Format findDepthFormat() const;
    vk::Format getDepthFormat() const { return depthFormat_; }

    vk::DescriptorPool getImGuiDescriptorPool() const { return imguiDescriptorPool_.get(); }

    uint32_t getGraphicsQueueFamilyIndex()
    {
        return findQueueFamilies(physicalDevice_).graphicsFamily.value();
    } // end of getGraphicsQueueFamilyIndex()

    uint32_t getMinImageCount()
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice_);
        return swapChainSupport.capabilities.minImageCount;
    } // end of getMinImageCount()

    uint32_t getSwapchainImageCount() const { return static_cast<uint32_t>(swapChainImages_.size()); }


    vk::Device getDevice() const { return device_.get(); }
    vk::PhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    const vk::PhysicalDeviceProperties& getPhysicalDeviceProperties() const { return physicalDeviceProperties_; }
    vk::Queue getGraphicsQueue() const { return graphicsQueue_; }
    vk::Queue getPresentQueue() const { return presentQueue_; }
    vk::CommandPool getCommandPool() const { return commandPool_.get(); }

    vk::Format getSwapChainImageFormat() const { return swapChainImageFormat_; }
    vk::Extent2D getSwapChainExtent() const { return swapChainExtent_; }
    vk::ImageView getSwapChainImageView(size_t i) const { return swapChainImageViews_[i].get(); }
    size_t getSwapChainImageViewCount() const { return swapChainImageViews_.size(); }
    const std::vector<vk::Image>& getSwapChainImages() const { return swapChainImages_; }

    vk::SwapchainKHR getSwapChain() const { return swapChain_.get(); }
    vk::SurfaceKHR getSurface() const { return surface_.get(); }
    vk::Instance getInstance() const { return instance_.get(); }

    vk::ImageLayout getSwapChainLayout(uint32_t imageIndex) const { return swapChainLayouts_[imageIndex]; }
    void setSwapChainLayout(uint32_t imageIndex, vk::ImageLayout layout) { swapChainLayouts_[imageIndex] = layout; }

    uint32_t getMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }

    uint32_t currentFrameIndex() const { return currentFrame_; }

    void retireBuffer(uint32_t frameIndex, BufferVk&& buffer)
    {
        if (buffer.valid())
        {
            retired_[frameIndex].buffers.push_back(std::move(buffer));
        }
    } // end of retireBuffer()
    void retireImage(uint32_t frameIndex, ImageVk&& image)
    {
        if (image.valid())
        {
            retired_[frameIndex].images.push_back(std::move(image));
        }
    } // end of retireImage()
    void retireAccelerationStructure(uint32_t frameIndex, AccelerationStructureVk&& as)
    {
        if (as.valid())
        {
            retired_[frameIndex].accelStructures.push_back(std::move(as));
        }
    } // end of retireAccelerationStructure()

    vk::PresentModeKHR getVsyncMode() const{ return vsyncMode_; }
    bool getVSync() const { return vsyncEnabled_; }
    void setVSync(bool enabled)
    {
        if (vsyncEnabled_ == enabled)
            return;

        vsyncEnabled_ = enabled;
        framebufferResized_ = true;
    } // end of setVSync

    bool supportsRayTracing() const { return supportsRayTracing_; }

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createImGuiDescriptorPool();
    void createLogicalDevice();
    void createSwapChain(vk::SwapchainKHR oldSwapchain);
    void createImageViews();
    void createDepthResources();
    void createCommandPool();

    void createCommandBuffers();
    void createSyncObjects();

    void cleanupSwapChain();
    void recreateSwapChain();

    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo);

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(vk::PhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
    vk::SampleCountFlagBits getMaxUsableSampleCount();
    bool checkDeviceExtensionSupport(vk::PhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);

    vk::UniqueImageView createImageView(vk::Image image, vk::Format format,
        vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilites);

    void createPerImageSync();

    void flushRetiredResources(uint32_t frameIndex);
    void flushAllRetiredResources();

private:
    const std::vector<const char*> validationLayers_ = { "VK_LAYER_KHRONOS_validation" };
    const std::vector<const char*> requiredDeviceExtensions_ = 
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };
    const std::vector<const char*> rayTracingDeviceExtensions_ = 
    {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    bool enableValidationLayers_{ false };
    bool initialized_{ false };

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    std::vector<PendingUpload> pendingUploads_;

    std::array<RetiredFrameResources, MAX_FRAMES_IN_FLIGHT> retired_;

    bool framebufferResized_{ false };
    uint32_t currentFrame_ = 0;

    bool vsyncEnabled_ = true;
    vk::PresentModeKHR vsyncMode_;

    vk::SampleCountFlagBits msaaSamples_{ vk::SampleCountFlagBits::e1 };

    GLFWwindow* window_{};

    bool supportsRayTracing_{ false };

    vk::UniqueInstance instance_{};
    vk::UniqueDebugUtilsMessengerEXT debugMessenger_{};
    vk::UniqueSurfaceKHR surface_{};

    vk::PhysicalDevice physicalDevice_{};
    vk::PhysicalDeviceProperties physicalDeviceProperties_;
    vk::UniqueDevice device_{};

    vk::Queue graphicsQueue_{};
    vk::Queue presentQueue_{};

    vk::UniqueImage depthImage_{};
    vk::UniqueDeviceMemory depthImageMemory_{};
    vk::UniqueImageView depthImageView_{};
    vk::Format depthFormat_{ vk::Format::eUndefined };
    vk::ImageLayout depthImageLayout_{ vk::ImageLayout::eUndefined };

    vk::UniqueSwapchainKHR swapChain_{};
    std::vector<vk::Image> swapChainImages_;
    vk::Format swapChainImageFormat_{ vk::Format::eUndefined };
    vk::Extent2D swapChainExtent_{};
    std::vector<vk::UniqueImageView> swapChainImageViews_;
    std::vector<vk::ImageLayout> swapChainLayouts_;

    vk::UniqueCommandPool commandPool_{};

    std::vector<vk::CommandBuffer> commandBuffers_;
    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores_;
    std::vector<vk::UniqueFence> inFlightFences_;

    std::vector<vk::UniqueSemaphore> renderFinishedPerImage_;
    std::vector<vk::Fence> imagesInFlight_;

    vk::UniqueDescriptorPool imguiDescriptorPool_;
};

#endif