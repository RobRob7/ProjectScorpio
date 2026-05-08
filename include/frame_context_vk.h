#ifndef FRAME_CONTEXT_H
#define FRAME_CONTEXT_H

#include "utils_vk.h"
#include "timestamp_gpu_vk.h"

#include <vulkan/vulkan.hpp>

struct FrameContext
{
    vk::CommandBuffer cmd{};

    TimestampGPUVk gpuTimestamps;

    vk::Extent2D extent{};

    uint32_t frameIndex = 0;
    uint32_t imageIndex = 0;

    vk::Image colorImage{};
    vk::ImageView colorImageView{};
    vk::ImageLayout* colorLayout{ nullptr };

    vk::Image depthImage{};
    vk::ImageView depthImageView{};
    vk::ImageLayout* depthLayout{ nullptr };

    vk::Format* colorFormat{ nullptr };
    vk::Format* depthFormat{ nullptr };

    void transitionColorImageToAttachment(vk::CommandBuffer cmd)
    {
        VkUtils::TransitionImageLayout(
            cmd,
            colorImage,
            vk::ImageAspectFlagBits::eColor,
            *colorLayout,
            vk::ImageLayout::eColorAttachmentOptimal,
            1,
            1
        );
    } // end of transitionColorImageToAttachment()

    void transitionColorImageToPresent(vk::CommandBuffer cmd)
    {
        VkUtils::TransitionImageLayout(
            cmd,
            colorImage,
            vk::ImageAspectFlagBits::eColor,
            *colorLayout,
            vk::ImageLayout::ePresentSrcKHR,
            1,
            1
        );
    } // end of transitionColorImageToPresent()
};


#endif
