#include "texture_cubemap_dx12.h"

#include "utils_dx12.h"
#include "buffer_dx12.h"
#include "dx12_main.h"

#include <stb/stb_image.h>

#include <vector>
#include <string_view>
#include <array>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <cstring>

//--- PUBLIC ---//
TextureCubemapDX12::TextureCubemapDX12(DX12Main& dx)
	: dx_(&dx), 
	image_(dx)
{
} // end of constructor

TextureCubemapDX12::~TextureCubemapDX12() = default;

void TextureCubemapDX12::setDebugName(const std::wstring& name)
{
	image_.setDebugName(name);
} // end of setDebugName()

void TextureCubemapDX12::loadFromFiles(
	const std::array<std::string_view, 6>& faces, 
	const bool needToFlip
)
{
	destroy();

	stbi_set_flip_vertically_on_load(needToFlip);

	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;

	std::vector<stbi_uc*> loadedFaces;
	loadedFaces.reserve(6);

	for (size_t i = 0; i < 6; ++i)
	{
		int w = 0;
		int h = 0;
		int c = 0;

		std::filesystem::path pathToTexture =
			std::filesystem::path(RESOURCES_PATH) / faces[i];

		const std::string texPath = pathToTexture.string();

		stbi_uc* pixels = stbi_load(
			texPath.c_str(),
			&w,
			&h,
			&c,
			STBI_rgb_alpha
		);

		if (!pixels)
		{
			for (stbi_uc* p : loadedFaces)
			{
				stbi_image_free(p);
			}

			throw std::runtime_error(
				"TextureCubemapDX12::loadFromFiles - failed to load face: " +
				pathToTexture.string()
			);
		}

		if (i == 0)
		{
			texWidth = w;
			texHeight = h;
			texChannels = c;
		}
		else if (w != texWidth || h != texHeight)
		{
			for (stbi_uc* p : loadedFaces)
			{
				stbi_image_free(p);
			}

			stbi_image_free(pixels);

			throw std::runtime_error(
				"TextureCubemapDX12::loadFromFiles - cubemap faces must match dimensions"
			);
		}

		loadedFaces.push_back(pixels);
	} // end for 

	const uint64_t layerSize =
		static_cast<uint64_t>(texWidth) *
		static_cast<uint64_t>(texHeight) *
		4ull;

	const uint64_t totalSize = layerSize * 6ull;

	std::vector<unsigned char> packed(static_cast<size_t>(totalSize));

	for (size_t i = 0; i < 6; ++i)
	{
		std::memcpy(
			packed.data() + i * static_cast<size_t>(layerSize),
			loadedFaces[i],
			static_cast<size_t>(layerSize)
		);

		stbi_image_free(loadedFaces[i]);
	} // end for

	image_.createImage(
		static_cast<uint32_t>(texWidth),
		static_cast<uint32_t>(texHeight),
		6,
		false,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		1
	);

	DX12Utils::UploadTexture2DArrayImmediate(
		*dx_,
		image_,
		packed.data(),
		static_cast<uint32_t>(texWidth),
		static_cast<uint32_t>(texHeight),
		6,
		4,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
} // end of loadFromFiles()


//--- PRIVATE ---//
void TextureCubemapDX12::destroy()
{
	image_.destroy();
} // end of destroy()