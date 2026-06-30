#include "texture_2d_dx12.h"

#include "utils_dx12.h"
#include "buffer_dx12.h"
#include "dx12_main.h"

#include <stb/stb_image.h>

#include <string_view>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <vector>
#include <utility>

//--- PUBLIC ---//
Texture2DDX12::Texture2DDX12(DX12Main& dx)
	: dx_(&dx), 
	image_(dx),
    mipGen_(dx)
{
} // end of constructor

Texture2DDX12::~Texture2DDX12() = default;

void Texture2DDX12::setDebugName(const std::wstring& name)
{
	image_.setDebugName(name);
} // end of setDebugName()

void Texture2DDX12::loadFromFile(
	std::string_view path, 
	const bool needToFlip,
    const bool generateMips
)
{
	destroy();

	stbi_set_flip_vertically_on_load(needToFlip);

	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;

	std::filesystem::path pathToTexture = 
		std::filesystem::path(RESOURCES_PATH) / "texture" / path;

	const std::string texPath = pathToTexture.string();
	stbi_uc* pixels = stbi_load(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels)
	{
		throw std::runtime_error("Texture2DDX12::loadFromFile - failed to load texture: " + pathToTexture.string());
	}

    const uint32_t width = static_cast<uint32_t>(texWidth);
    const uint32_t height = static_cast<uint32_t>(texHeight);

    const DXGI_FORMAT textureFormat =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    const D3D12_RESOURCE_FLAGS textureFlags =
        generateMips
        ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        : D3D12_RESOURCE_FLAG_NONE;

    image_.createImage(
        width,
        height,
        1,
        generateMips,
        textureFormat,
        textureFlags,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        1
    );

    D3D12_RESOURCE_DESC textureDesc =
        image_.resource()->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;

    dx_->getDevice()->GetCopyableFootprints(
        &textureDesc,
        0,
        1,
        0,
        &layout,
        &numRows,
        &rowSizeInBytes,
        &totalBytes
    );

    std::vector<std::uint8_t> uploadData(
        static_cast<size_t>(totalBytes)
    );

    const std::uint8_t* srcPixels =
        reinterpret_cast<const std::uint8_t*>(pixels);

    std::uint8_t* dstPixels =
        uploadData.data() + layout.Offset;

    const size_t srcRowPitch =
        static_cast<size_t>(width) * 4;

    const size_t dstRowPitch =
        static_cast<size_t>(layout.Footprint.RowPitch);

    for (uint32_t y = 0; y < height; ++y)
    {
        std::memcpy(
            dstPixels + y * dstRowPitch,
            srcPixels + y * srcRowPitch,
            srcRowPitch
        );
    } // end for

    stbi_image_free(pixels);

    BufferDX12 staging(*dx_);

    staging.create(
        totalBytes,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_FLAG_NONE,
        false
    );

    staging.upload(
        uploadData.data(),
        totalBytes
    );

    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList4> cmd;

    DX12Utils::ThrowIfFailed(
        dx_->getDevice(),
        dx_->getDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&allocator)
        ),
        "Texture2DDX12::loadFromFile - failed to create command allocator"
    );

    DX12Utils::ThrowIfFailed(
        dx_->getDevice(),
        dx_->getDevice()->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(),
            nullptr,
            IID_PPV_ARGS(&cmd)
        ),
        "Texture2DDX12::loadFromFile - failed to create command list"
    );

    D3D12_TEXTURE_COPY_LOCATION srcLocation{};
    srcLocation.pResource = staging.getResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = layout;

    D3D12_TEXTURE_COPY_LOCATION dstLocation{};
    dstLocation.pResource = image_.resource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    image_.transitionSubresource(
        cmd.Get(),
        0,
        D3D12_RESOURCE_STATE_COPY_DEST
    );

    cmd->CopyTextureRegion(
        &dstLocation,
        0,
        0,
        0,
        &srcLocation,
        nullptr
    );

    // check for mips gen
    if (generateMips && image_.mipLevels() > 1)
    {
        mipGen_.generate(image_, cmd.Get());
    }
    else
    {
        image_.transitionSubresource(
            cmd.Get(),
            0,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );

        image_.setAllSubresourceStates(
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );
    }

    std::vector<BufferDX12> uploadBuffers;
    uploadBuffers.push_back(std::move(staging));

    dx_->submitUpload(
        std::move(allocator),
        std::move(cmd),
        std::move(uploadBuffers)
    );
} // end of loadFromFile()


//--- PRIVATE ---//
void Texture2DDX12::destroy()
{
	image_.destroy();
} // end of destroy()