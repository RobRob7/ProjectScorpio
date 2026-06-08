#ifndef IMAGE_VK_H
#define IMAGE_VK_H

#include "utils_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string>
#include <array>

class VulkanMain;

class ImageVk
{
public:
    explicit ImageVk(VulkanMain& vk);
    ~ImageVk();

    ImageVk(const ImageVk&) = delete;
    ImageVk& operator=(const ImageVk&) = delete;

    ImageVk(ImageVk&&) noexcept = default;
    ImageVk& operator=(ImageVk&&) noexcept = default;

    void setDebugName(const std::string& name);

    void clearColorThenShaderRead(vk::CommandBuffer cmd, const std::array<float, 4>& color);
    void clearDepthThenShaderRead(
        vk::CommandBuffer cmd,
        float depth = 1.0f,
        uint32_t stencil = 0
    );

    void createImage(
        uint32_t width,
        uint32_t height,
        uint32_t layers,
        bool autoMipLevels,
        vk::SampleCountFlagBits samples,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::ImageCreateFlags flags = {}
    );

    void generateMipmaps(
        vk::Image image,
        vk::Format imageFormat,
        int32_t texWidth,
        int32_t texHeight,
        uint32_t mipLevels,
        uint32_t layers = 1
    );

    void createImageView(
        vk::Format format,
        vk::ImageAspectFlags aspectFlags,
        vk::ImageViewType viewType,
        uint32_t layers
    );

    void createSampler(
        vk::Filter magFilter = vk::Filter::eLinear,
        vk::Filter minFilter = vk::Filter::eLinear,
        vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat,
        bool enableAnisotropy = false
    );

	void destroy();

    bool valid() const { return static_cast<bool>(image_); }

    vk::Image image() const { return image_.get(); }
    vk::DeviceMemory memory() const { return memory_.get(); }
    vk::ImageView view() const { return view_.get(); }
    vk::Sampler sampler() const { return sampler_.get(); }

    void transitionToShaderRead(
        vk::CommandBuffer cmd,
        vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor
    )
    {
        if (!image_) return;

        VkUtils::TransitionImageLayout(
            cmd,
            image_.get(),
            aspect,
            layout_,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            layers_,
            mipLevels_
        );
    } // end of transitionToShaderRead()

    void transitionToColorAttachment(vk::CommandBuffer cmd)
    {
        if (!image_) return;

        VkUtils::TransitionImageLayout(
            cmd,
            image_.get(),
            vk::ImageAspectFlagBits::eColor,
            layout_,
            vk::ImageLayout::eColorAttachmentOptimal,
            layers_,
            mipLevels_
        );
    } // end of transitionToColorAttachment()

    void transitionToDepthAttachment(vk::CommandBuffer cmd)
    {
        if (!image_) return;

        VkUtils::TransitionImageLayout(
            cmd,
            image_.get(),
            vk::ImageAspectFlagBits::eDepth,
            layout_,
            vk::ImageLayout::eDepthAttachmentOptimal,
            layers_,
            mipLevels_
        );
    } // end of transitionToDepthAttachment()

    void transitionToGeneral(
        vk::CommandBuffer cmd,
        vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor
    )
    {
        if (!image_) return;

        VkUtils::TransitionImageLayout(
            cmd,
            image_.get(),
            aspect,
            layout_,
            vk::ImageLayout::eGeneral,
            layers_,
            mipLevels_
        );
    } // end of transitionToGeneral()

    void transitionToTransferDst(
        vk::CommandBuffer cmd,
        vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor
    )
    {
        if (!image_) return;

        VkUtils::TransitionImageLayout(
            cmd,
            image_.get(),
            aspect,
            layout_,
            vk::ImageLayout::eTransferDstOptimal,
            layers_,
            mipLevels_
        );
    } // end of transitionToTransferDst()

    vk::ImageLayout layout() const { return layout_; }

    vk::Format format() const { return format_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t layers() const { return layers_; }
    uint32_t mipLevels() const { return mipLevels_; }

private:
    VulkanMain* vk_{ nullptr };

    vk::UniqueImage image_{};
    vk::UniqueDeviceMemory memory_{};
    vk::UniqueImageView view_{};
    vk::UniqueSampler sampler_{};

    vk::ImageLayout layout_{ vk::ImageLayout::eUndefined };
    vk::Format format_{ vk::Format::eUndefined };

    uint32_t width_{ 0 };
    uint32_t height_{ 0 };
    uint32_t layers_{ 0 };

    uint32_t mipLevels_{ 1 };
};

#endif
