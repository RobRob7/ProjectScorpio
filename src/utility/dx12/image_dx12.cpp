#include "image_dx12.h"

#include "dx12_main.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>

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
    dsvFormat_ = format;
    srvFormat_ = format;
    state_ = initState;
    sampleCount_ = sampleCount;

    mipLevels_ = 1;
    if (autoMipLevels)
    {
        mipLevels_ = std::floor(std::log2(std::max(width_, height_))) + 1;
    }

    // MSAA mip map not allowed
    if (sampleCount_ > 1)
    {
        mipLevels_ = 1;
    }

    subresourceStates_.assign(
        mipLevels_ * layers_,
        initState
    );

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
        dx_->getDevice(),
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

void ImageDX12::createSampledDepthImage(
    uint32_t width,
    uint32_t height
)
{
    D3D12_CLEAR_VALUE depthClear{};
    depthClear.Format = DXGI_FORMAT_D32_FLOAT;
    depthClear.DepthStencil.Depth = 1.0f;
    depthClear.DepthStencil.Stencil = 0;

    createImage(
        width,
        height,
        1,
        false,
        DXGI_FORMAT_R32_TYPELESS,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClear
    );

    dsvFormat_ = DXGI_FORMAT_D32_FLOAT;
    srvFormat_ = DXGI_FORMAT_R32_FLOAT;

    createDSV();
} // end of createSampledDepthImage()

void ImageDX12::createRTV(
    D3D12_RTV_DIMENSION dimension
)
{
    if (!image_)
    {
        throw std::runtime_error("ImageDX12::createRTV - invalid image");
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };

    DX12Utils::ThrowIfFailed(
        dx_->getDevice(),
        dx_->getDevice()->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(&rtvHeap_)
        ),
        "ImageDX12::createRTV - failed to create RTV heap"
    );

    createRTV(
        rtvHeap_->GetCPUDescriptorHandleForHeapStart(),
        dimension
    );
} // end of createRTV()

void ImageDX12::createDSV(
    D3D12_DSV_DIMENSION dimension
)
{
    if (!image_)
    {
        throw std::runtime_error("ImageDX12::createDSV - invalid image");
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };

    DX12Utils::ThrowIfFailed(
        dx_->getDevice(),
        dx_->getDevice()->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(&dsvHeap_)
        ),
        "ImageDX12::createDSV - failed to create DSV heap"
    );

    createDSV(
        dsvHeap_->GetCPUDescriptorHandleForHeapStart(),
        dimension
    );
} // end of createDSV()

void ImageDX12::destroy()
{
    image_.Reset();

    rtvHeap_.Reset();
    dsvHeap_.Reset();

    state_ = D3D12_RESOURCE_STATE_COMMON;
    subresourceStates_.clear();
    format_ = DXGI_FORMAT_UNKNOWN;
    dsvFormat_ = DXGI_FORMAT_UNKNOWN;
    srvFormat_ = DXGI_FORMAT_UNKNOWN;

    width_ = 0;
    height_ = 0;
    layers_ = 1;
    mipLevels_ = 1;
    sampleCount_ = 1;

    rtvCpu_ = {};
    dsvCpu_ = {};
} // end of destroy()


//--- PRIVATE ---//
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
    desc.Format = dsvFormat_;
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