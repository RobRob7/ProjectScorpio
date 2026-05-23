#include "texture_2d_vk.h"

#include "utils_vk.h"
#include "buffer_vk.h"
#include "vulkan_main.h"

#include <stb/stb_image.h>

#include <string_view>
#include <stdexcept>
#include <string>
#include <filesystem>

//--- PUBLIC ---//
Texture2DVk::Texture2DVk(VulkanMain& vk)
	: vk_(vk), image_(vk)
{
} // end of constructor

Texture2DVk::~Texture2DVk() = default;

void Texture2DVk::setDebugName(const std::string& name)
{
	image_.setDebugName(name);
} // end of setDebugName()

void Texture2DVk::loadFromFile(std::string_view path, const bool needToFlip)
{
	stbi_set_flip_vertically_on_load(needToFlip);

	int texWidth = 0;
	int texHeight = 0;
	int texChannels = 0;

	std::filesystem::path pathToTexture = std::filesystem::path(RESOURCES_PATH) / "texture" / path;

	const std::string texPath = pathToTexture.string();
	stbi_uc* pixels = stbi_load(texPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels)
	{
		throw std::runtime_error("Texture2DVk::loadFromFile - failed to load texture: " + pathToTexture.string());
	}

	const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(texWidth) * texHeight * 4;

	BufferVk staging(vk_);
	staging.create(
		imageSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	staging.upload(pixels, imageSize);

	stbi_image_free(pixels);

	image_.createImage(
		static_cast<uint32_t>(texWidth),
		static_cast<uint32_t>(texHeight),
		1,
		true,
		vk::SampleCountFlagBits::e1,
		vk::Format::eR8G8B8A8Srgb,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	VkUtils::TransitionImageLayoutImmediate(
		vk_, 
		image_.image(), 
		vk::ImageAspectFlagBits::eColor, 
		vk::ImageLayout::eUndefined, 
		vk::ImageLayout::eTransferDstOptimal, 
		1, 
		image_.mipLevels()
	);
	VkUtils::CopyBufferToImageImmediate(
		vk_, 
		staging.getBuffer(), 
		image_.image(), 
		texWidth, 
		texHeight, 
		1
	);
	
	image_.generateMipmaps(
		image_.image(),
		vk::Format::eR8G8B8A8Srgb,
		texWidth,
		texHeight,
		image_.mipLevels(),
		1
	);

	image_.createImageView(
		vk::Format::eR8G8B8A8Srgb,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	image_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);
} // end of loadFromFile()


//--- PRIVATE ---//
void Texture2DVk::destroy()
{
	image_.destroy();
} // end of destroy()