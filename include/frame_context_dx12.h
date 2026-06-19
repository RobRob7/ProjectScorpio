#ifndef FRAME_CONTEXT_DX12_H
#define FRAME_CONTEXT_DX12_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>

#include <cstdint>

struct FrameContextDX12
{
    ID3D12GraphicsCommandList4* cmd{ nullptr };

    uint32_t width{ 0 };
    uint32_t height{ 0 };

    uint32_t frameIndex{ 0 };
    uint32_t imageIndex{ 0 };

    ID3D12Resource* colorImage{ nullptr };
    D3D12_CPU_DESCRIPTOR_HANDLE colorRTV{};
    D3D12_RESOURCE_STATES* colorState{ nullptr };

    ID3D12Resource* depthImage{ nullptr };
    D3D12_CPU_DESCRIPTOR_HANDLE depthDSV{};
    D3D12_RESOURCE_STATES* depthState{ nullptr };

    //void transitionColorImageToAttachment(vk::CommandBuffer cmd)
    //{
    //    VkUtils::TransitionImageLayout(
    //        cmd,
    //        colorImage,
    //        vk::ImageAspectFlagBits::eColor,
    //        *colorLayout,
    //        vk::ImageLayout::eColorAttachmentOptimal,
    //        1,
    //        1
    //    );
    //} // end of transitionColorImageToAttachment()

    //void transitionColorImageToPresent(vk::CommandBuffer cmd)
    //{
    //    VkUtils::TransitionImageLayout(
    //        cmd,
    //        colorImage,
    //        vk::ImageAspectFlagBits::eColor,
    //        *colorLayout,
    //        vk::ImageLayout::ePresentSrcKHR,
    //        1,
    //        1
    //    );
    //} // end of transitionColorImageToPresent()
};

#endif
