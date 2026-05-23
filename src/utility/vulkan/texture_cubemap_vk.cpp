#include "texture_cubemap_vk.h"

#include "utils_vk.h"
#include "buffer_vk.h"
#include "vulkan_main.h"

#include <stb/stb_image.h>

#include <vector>
#include <string_view>
#include <array>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <cstring>

//--- PUBLIC ---//
TextureCubemapVk::TextureCubemapVk(VulkanMain& vk)
	: vk_(vk), image_(vk)
{
} // end of constructor

TextureCubemapVk::~TextureCubemapVk() = default;

void TextureCubemapVk::setDebugName(const std::string& name)
{
	image_.setDebugName(name);
} // end of setDebugName()

void TextureCubemapVk::loadFromFiles(const std::array<std::string_view, 6>& faces, const bool needToFlip)
{
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

		std::filesystem::path pathToTexture = std::filesystem::path(RESOURCES_PATH) / faces[i];
		const std::string texPath = pathToTexture.string();
		stbi_uc* pixels = stbi_load(texPath.c_str(), &w, &h, &c, STBI_rgb_alpha);
		if (!pixels)
		{
			for (stbi_uc* p : loadedFaces) stbi_image_free(p);
			throw std::runtime_error("TextureCubemapVk::loadFromFiles - failed to load face: " + pathToTexture.string());
		}

		if (i == 0)
		{
			texWidth = w;
			texHeight = h;
			texChannels = c;
		}
		else if (w != texWidth || h != texHeight)
		{
			for (stbi_uc* p : loadedFaces) stbi_image_free(p);
			stbi_image_free(pixels);
			throw std::runtime_error("TextureCubemapVk::loadFromFiles - cubemap faces must match dimensions");
		}

		loadedFaces.push_back(pixels);
	}

	const vk::DeviceSize layerSize = static_cast<vk::DeviceSize>(texWidth) * texHeight * 4;
	const vk::DeviceSize totalSize = layerSize * 6;

	std::vector<unsigned char> packed(static_cast<size_t>(totalSize));
	for (size_t i = 0; i < 6; ++i)
	{
		std::memcpy(packed.data() + i * static_cast<size_t>(layerSize), loadedFaces[i], static_cast<size_t>(layerSize));
		stbi_image_free(loadedFaces[i]);
	}

	BufferVk staging(vk_);
	staging.create(
		totalSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	staging.upload(packed.data(), totalSize);

	image_.createImage(
		static_cast<uint32_t>(texWidth),
		static_cast<uint32_t>(texHeight),
		6,
		true,
		vk::SampleCountFlagBits::e1,
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		vk::ImageCreateFlagBits::eCubeCompatible
	);

	VkUtils::TransitionImageLayoutImmediate(
		vk_, 
		image_.image(), 
		vk::ImageAspectFlagBits::eColor, 
		vk::ImageLayout::eUndefined, 
		vk::ImageLayout::eTransferDstOptimal, 
		6, 
		image_.mipLevels()
	);
	VkUtils::CopyBufferToImageImmediate(
		vk_, 
		staging.getBuffer(), 
		image_.image(), 
		texWidth, 
		texHeight, 
		6
	);

	image_.generateMipmaps(
		image_.image(),
		vk::Format::eR8G8B8A8Unorm,
		texWidth,
		texHeight,
		image_.mipLevels(),
		6
	);

	image_.createImageView(
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::eCube,
		6
	);

	image_.createSampler(
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eLinear,
		vk::SamplerAddressMode::eClampToEdge,
		false
	);
} // end of loadFromFiles()


//--- PRIVATE ---//
void TextureCubemapVk::destroy()
{
	image_.destroy();
} // end of destroy()