#ifndef IMAGE_DX12_H
#define IMAGE_DX12_H

#include "utils_dx12.h"

#include <cstdint>
#include <string>
#include <array>
#include <vector>

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

    void createSampledDepthImage(
        uint32_t width,
        uint32_t height
    );

    void createRTV(
        D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D
    );

    void createDSV(
        D3D12_DSV_DIMENSION dimension = D3D12_DSV_DIMENSION_TEXTURE2D
    );

    void destroy();

    void transitionSubresource(
        ID3D12GraphicsCommandList* cmd,
        uint32_t mip,
        D3D12_RESOURCE_STATES newState
    )
    {
        if (!image_) return;

        if (mip >= subresourceStates_.size()) return;

        DX12Utils::TransitionSubresource(
            cmd,
            image_.Get(),
            mip,
            subresourceStates_[mip],
            newState
        );
    } // end of transitionSubresource()

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
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );
    } // end of transitionToShaderRead()

    void setAllSubresourceStates(D3D12_RESOURCE_STATES state)
    {
        state_ = state;

        for (auto& subState : subresourceStates_)
        {
            subState = state;
        }
    }

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

    D3D12_RESOURCE_STATES state() const { return state_; }
    D3D12_RESOURCE_STATES& state() { return state_; }

    DXGI_FORMAT format() const { return format_; }
    DXGI_FORMAT srvFormat() const { return srvFormat_; }
    DXGI_FORMAT dsvFormat() const { return dsvFormat_; }

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    uint32_t layers() const { return layers_; }

    uint32_t mipLevels() const { return mipLevels_; }
    uint32_t getMipWidth(uint32_t mip) const { return std::max(1u, width_ >> mip); }
    uint32_t getMipHeight(uint32_t mip) const { return std::max(1u, height_ >> mip); }

    uint32_t sampleCount() const { return sampleCount_; }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvCPU() const { return rtvCpu_; }
    D3D12_CPU_DESCRIPTOR_HANDLE dsvCPU() const { return dsvCpu_; }

private:
    void createRTV(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D
    );

    void createDSV(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_DSV_DIMENSION dimension = D3D12_DSV_DIMENSION_TEXTURE2D
    );
private:
    DX12Main* dx_{ nullptr };

    ComPtr<ID3D12Resource> image_;

    D3D12_RESOURCE_STATES state_{ D3D12_RESOURCE_STATE_COMMON };
    std::vector<D3D12_RESOURCE_STATES> subresourceStates_;
    DXGI_FORMAT format_{ DXGI_FORMAT_UNKNOWN };
    DXGI_FORMAT dsvFormat_{ DXGI_FORMAT_UNKNOWN };
    DXGI_FORMAT srvFormat_{ DXGI_FORMAT_UNKNOWN };

    uint32_t width_{ 0 };
    uint32_t height_{ 0 };
    uint32_t layers_{ 1 };
    uint32_t mipLevels_{ 1 };
    uint32_t sampleCount_{ 1 };

    D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvCpu_{};

    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;
};

#endif
