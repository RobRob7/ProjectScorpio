#ifndef IMAGE_DX12_H
#define IMAGE_DX12_H

#include "utils_dx12.h"

#include <cstdint>
#include <string>
#include <array>

class DX12Main;

using Microsoft::WRL::ComPtr;

class ImageDX12
{
public:
    explicit ImageDX12(DX12Main& dx);
    ~ImageDX12();

    ImageDX12(const ImageDX12&) = delete;
    ImageDX12& operator=(const ImageDX12&) = delete;

    ImageDX12(ImageDX12&&) noexcept = default;
    ImageDX12& operator=(ImageDX12&&) noexcept = default;

    void setDebugName(const std::wstring& name);

    void clearColorThenShaderRead(
        ID3D12GraphicsCommandList* cmd,
        const std::array<float, 4>& color
    );

    void clearDepthThenShaderRead(
        ID3D12GraphicsCommandList* cmd,
        float depth = 1.0f,
        uint8_t stencil = 0
    );

    void createImage(
        uint32_t width,
        uint32_t height,
        uint32_t layers,
        bool autoMipLevels,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initState,
        const D3D12_CLEAR_VALUE* clearValue = nullptr,
        uint32_t sampleCount = 1
    );

    void createSRV(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {},
        D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2D
    );

    void createRTV(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D
    );

    void createDSV(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_DSV_DIMENSION dimension = D3D12_DSV_DIMENSION_TEXTURE2D
    );

    void createUAV(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {},
        D3D12_UAV_DIMENSION dimension = D3D12_UAV_DIMENSION_TEXTURE2D
    );

    void destroy();

    void transitionToShaderRead(
        ID3D12GraphicsCommandList* cmd,
        bool pixelShader = true
    )
    {
        if (!image_) return;

        const D3D12_RESOURCE_STATES newState =
            pixelShader
            ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        DX12Utils::TransitionResource(
            cmd,
            image_.Get(),
            state_,
            newState
        );
    } // end of transitionToShaderRead()

    void transitionToRenderTarget(ID3D12GraphicsCommandList* cmd)
    {
        if (!image_) return;

        DX12Utils::TransitionResource(
            cmd,
            image_.Get(),
            state_,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
    } // end of transitionToRenderTarget()

    void transitionToDepthWrite(ID3D12GraphicsCommandList* cmd)
    {
        if (!image_) return;

        DX12Utils::TransitionResource(
            cmd,
            image_.Get(),
            state_,
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        );
    } // end of transitionToDepthWrite()

    void transitionToUnorderedAccess(ID3D12GraphicsCommandList* cmd)
    {
        if (!image_) return;

        DX12Utils::TransitionResource(
            cmd,
            image_.Get(),
            state_,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        );
    } // end of transitionToUnorderedAccess()

    void transitionToCopyDst(ID3D12GraphicsCommandList* cmd)
    {
        if (!image_) return;

        DX12Utils::TransitionResource(
            cmd,
            image_.Get(),
            state_,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
    } // end of transitionToCopyDst()

    void transitionToCopySrc(ID3D12GraphicsCommandList* cmd)
    {
        if (!image_) return;

        DX12Utils::TransitionResource(
            cmd,
            image_.Get(),
            state_,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        );
    } // end of transitionToCopySrc()

    bool valid() const { return static_cast<bool>(image_); }

    ID3D12Resource* resource() const { return image_.Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE srvCPU() const { return srvCpu_; }
    D3D12_GPU_DESCRIPTOR_HANDLE srvGPU() const { return srvGpu_; }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvCPU() const { return rtvCpu_; }
    D3D12_CPU_DESCRIPTOR_HANDLE dsvCPU() const { return dsvCpu_; }

    D3D12_CPU_DESCRIPTOR_HANDLE uavCPU() const { return uavCpu_; }
    D3D12_GPU_DESCRIPTOR_HANDLE uavGPU() const { return uavGpu_; }

    D3D12_RESOURCE_STATES state() const { return state_; }
    D3D12_RESOURCE_STATES& stateRef() { return state_; }

    DXGI_FORMAT format() const { return format_; }

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t layers() const { return layers_; }
    uint32_t mipLevels() const { return mipLevels_; }

private:
    DX12Main* dx_{ nullptr };

    ComPtr<ID3D12Resource> image_;

    D3D12_RESOURCE_STATES state_{ D3D12_RESOURCE_STATE_COMMON };
    DXGI_FORMAT format_{ DXGI_FORMAT_UNKNOWN };

    uint32_t width_{ 0 };
    uint32_t height_{ 0 };
    uint32_t layers_{ 1 };
    uint32_t mipLevels_{ 1 };
    uint32_t sampleCount_{ 1 };

    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu_{};

    D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvCpu_{};

    D3D12_CPU_DESCRIPTOR_HANDLE uavCpu_{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpu_{};
};

#endif
