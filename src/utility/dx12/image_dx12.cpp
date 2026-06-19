#include "image_dx12.h"

#include "dx12_main.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>

//--- HELPER ---//
static uint32_t CalculateMipLevels(uint32_t width, uint32_t height)
{
    const uint32_t largest = std::max(width, height);
    return static_cast<uint32_t>(std::floor(std::log2(largest))) + 1;
} // end of CalculateMipLevels()


//--- PUBLIC ---//
ImageDX12::ImageDX12(DX12Main& dx)
    : dx_(&dx)
{
} // end of constructor

ImageDX12::~ImageDX12() = default;

void ImageDX12::setDebugName(const std::wstring& name)
{
    if (!image_) return;

    image_->SetName(name.c_str());
} // end of setDebugName()

void ImageDX12::clearColorThenShaderRead(
    ID3D12GraphicsCommandList* cmd,
    const std::array<float, 4>& color
)
{
    if (!image_ || rtvCpu_.ptr == 0)
    {
        return;
    }

    transitionToRenderTarget(cmd);

    cmd->ClearRenderTargetView(
        rtvCpu_,
        color.data(),
        0,
        nullptr
    );

    transitionToShaderRead(cmd);
} // end of clearColorThenShaderRead()

void ImageDX12::clearDepthThenShaderRead(
    ID3D12GraphicsCommandList* cmd,
    float depth,
    uint8_t stencil
)
{
    if (!image_ || dsvCpu_.ptr == 0)
    {
        return;
    }

    transitionToDepthWrite(cmd);

    cmd->ClearDepthStencilView(
        dsvCpu_,
        D3D12_CLEAR_FLAG_DEPTH,
        depth,
        stencil,
        0,
        nullptr
    );

    transitionToShaderRead(cmd);
} // end of clearDepthThenShaderRead()

void ImageDX12::createImage(
    uint32_t width,
    uint32_t height,
    uint32_t layers,
    bool autoMipLevels,
    DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initState,
    const D3D12_CLEAR_VALUE* clearValue,
    uint32_t sampleCount
)
{
	destroy();

	if (width == 0 || height == 0 || layers == 0)
	{
		throw std::runtime_error("ImageDX12::createImage - invalid dimensions/layers");
	}

	width_ = width;
	height_ = height;
	layers_ = layers;
	format_ = format;
    state_ = initState;
    sampleCount_ = sampleCount;

    mipLevels_ = autoMipLevels ? CalculateMipLevels(width, height) : 1;

    if (sampleCount_ > 1)
    {
        mipLevels_ = 1;
    }

    D3D12_HEAP_PROPERTIES heapProps{
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1
    };

    D3D12_RESOURCE_DESC desc{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = width_,
        .Height = height_,
        .DepthOrArraySize = static_cast<UINT16>(layers_),
        .MipLevels = static_cast<UINT16>(mipLevels_),
        .Format = format_,
        .SampleDesc = {
            .Count = sampleCount_,
            .Quality = 0
        },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags
    };

    DX12Utils::ThrowIfFailed(
        dx_->getDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            initState,
            clearValue,
            IID_PPV_ARGS(&image_)
        ),
        "ImageDX12::createImage - failed to create texture resource"
    );
} // end of createImage()

void ImageDX12::createSRV(
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
    D3D12_SRV_DIMENSION dimension
)
{
    if (!image_)
    {
        throw std::runtime_error("ImageDX12::createSRV - invalid image");
    }

    srvCpu_ = cpuHandle;
    srvGpu_ = gpuHandle;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{
        .Format = format_,
        .ViewDimension = dimension,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
    };

    if (dimension == D3D12_SRV_DIMENSION_TEXTURE2D)
    {
        desc.Texture2D =
        {
            .MostDetailedMip = 0,
            .MipLevels = mipLevels_,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f
        };
    }
    else if (dimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
    {
        desc.Texture2DArray =
        {
            .MostDetailedMip = 0,
            .MipLevels = mipLevels_,
            .FirstArraySlice = 0,
            .ArraySize = layers_,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f
        };
    }
    else if (dimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
    {
        desc.TextureCube =
        {
            .MostDetailedMip = 0,
            .MipLevels = mipLevels_,
            .ResourceMinLODClamp = 0.0f
        };
    }
    else
    {
        throw std::runtime_error("ImageDX12::createSRV - unsupported SRV dimension");
    }

    dx_->getDevice()->CreateShaderResourceView(
        image_.Get(),
        &desc,
        srvCpu_
    );
} // end of createSRV()

void ImageDX12::createRTV(
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_RTV_DIMENSION dimension
)
{
    if (!image_)
    {
        throw std::runtime_error("ImageDX12::createRTV - invalid image");
    }

    rtvCpu_ = cpuHandle;

    D3D12_RENDER_TARGET_VIEW_DESC desc{};
    desc.Format = format_;
    desc.ViewDimension = dimension;

    if (dimension == D3D12_RTV_DIMENSION_TEXTURE2D)
    {
        desc.Texture2D.MipSlice = 0;
        desc.Texture2D.PlaneSlice = 0;
    }
    else if (dimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
    {
        desc.Texture2DArray.MipSlice = 0;
        desc.Texture2DArray.FirstArraySlice = 0;
        desc.Texture2DArray.ArraySize = layers_;
        desc.Texture2DArray.PlaneSlice = 0;
    }
    else
    {
        throw std::runtime_error("ImageDX12::createRTV - unsupported RTV dimension");
    }

    dx_->getDevice()->CreateRenderTargetView(
        image_.Get(),
        &desc,
        rtvCpu_
    );
} // end of createRTV()

void ImageDX12::createDSV(
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_DSV_DIMENSION dimension
)
{
    if (!image_)
    {
        throw std::runtime_error("ImageDX12::createDSV - invalid image");
    }

    dsvCpu_ = cpuHandle;

    D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
    desc.Format = format_;
    desc.ViewDimension = dimension;
    desc.Flags = D3D12_DSV_FLAG_NONE;

    if (dimension == D3D12_DSV_DIMENSION_TEXTURE2D)
    {
        desc.Texture2D.MipSlice = 0;
    }
    else if (dimension == D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
    {
        desc.Texture2DArray.MipSlice = 0;
        desc.Texture2DArray.FirstArraySlice = 0;
        desc.Texture2DArray.ArraySize = layers_;
    }
    else
    {
        throw std::runtime_error("ImageDX12::createDSV - unsupported DSV dimension");
    }

    dx_->getDevice()->CreateDepthStencilView(
        image_.Get(),
        &desc,
        dsvCpu_
    );
} // end of createDSV()

void ImageDX12::createUAV(
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
    D3D12_UAV_DIMENSION dimension
)
{
    if (!image_)
    {
        throw std::runtime_error("ImageDX12::createUAV - invalid image");
    }

    uavCpu_ = cpuHandle;
    uavGpu_ = gpuHandle;

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
    desc.Format = format_;
    desc.ViewDimension = dimension;

    if (dimension == D3D12_UAV_DIMENSION_TEXTURE2D)
    {
        desc.Texture2D.MipSlice = 0;
        desc.Texture2D.PlaneSlice = 0;
    }
    else if (dimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
    {
        desc.Texture2DArray.MipSlice = 0;
        desc.Texture2DArray.FirstArraySlice = 0;
        desc.Texture2DArray.ArraySize = layers_;
        desc.Texture2DArray.PlaneSlice = 0;
    }
    else
    {
        throw std::runtime_error("ImageDX12::createUAV - unsupported UAV dimension");
    }

    dx_->getDevice()->CreateUnorderedAccessView(
        image_.Get(),
        nullptr,
        &desc,
        uavCpu_
    );
} // end of createUAV()

void ImageDX12::destroy()
{
    image_.Reset();

    state_ = D3D12_RESOURCE_STATE_COMMON;
    format_ = DXGI_FORMAT_UNKNOWN;

    width_ = 0;
    height_ = 0;
    layers_ = 1;
    mipLevels_ = 1;
    sampleCount_ = 1;

    srvCpu_ = {};
    srvGpu_ = {};
    rtvCpu_ = {};
    dsvCpu_ = {};
    uavCpu_ = {};
    uavGpu_ = {};
} // end of destroy()