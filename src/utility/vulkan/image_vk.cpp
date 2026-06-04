#include "image_vk.h"

#include "vulkan_main.h"

#include <vulkan/vulkan.hpp>

#include <stdexcept>
#include <algorithm>
#include <cmath>

//--- PUBLIC ---//
ImageVk::ImageVk(VulkanMain& vk)
    : vk_(&vk)
{
} // end of constructor

ImageVk::~ImageVk() = default;

void ImageVk::setDebugName(const std::string& name)
{
    if (!image_) return;

    vk_->setDebugName(
        vk::ObjectType::eImage,
        reinterpret_cast<uint64_t>(static_cast<VkImage>(image_.get())),
        name + "::Image"
    );
    vk_->setDebugName(
        vk::ObjectType::eImageView,
        reinterpret_cast<uint64_t>(static_cast<VkImageView>(view_.get())),
        name + "::ImageView"
    );
} // end of setDebugName()

void ImageVk::createImage(
    uint32_t width,
    uint32_t height,
    uint32_t layers,
    bool autoMipLevels,
    vk::SampleCountFlagBits samples,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    vk::ImageCreateFlags flags
)
{
	destroy();

	if (width == 0 || height == 0 || layers == 0)
	{
		throw std::runtime_error("ImageVk::createImage - invalid dimensions/layers");
	}

	width_ = width;
	height_ = height;
	layers_ = layers;
	format_ = format;

	vk::Device device = vk_->getDevice();

    if (autoMipLevels)
    {
        mipLevels_ = std::floor(std::log2(std::max(width_, height_))) + 1;
    }

	vk::ImageCreateInfo ici{};
	ici.flags = flags;
	ici.imageType = vk::ImageType::e2D;
	ici.extent.width = width;
	ici.extent.height = height;
	ici.extent.depth = 1;
	ici.mipLevels = mipLevels_;
	ici.arrayLayers = layers;
	ici.format = format;
	ici.tiling = tiling;
	ici.initialLayout = vk::ImageLayout::eUndefined;
	ici.usage = usage;
	ici.samples = samples;
	ici.sharingMode = vk::SharingMode::eExclusive;

	vk::ResultValue rvImage = device.createImageUnique(ici);
	if (rvImage.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("ImageVk::createImage - createImageUnique failed: " + vk::to_string(rvImage.result));
	}
	image_ = std::move(rvImage.value);

	vk::MemoryRequirements memReq = device.getImageMemoryRequirements(image_.get());

	vk::MemoryAllocateInfo mai{};
	mai.allocationSize = memReq.size;
	mai.memoryTypeIndex = vk_->findMemoryType(memReq.memoryTypeBits, properties);

	vk::ResultValue rvMem = device.allocateMemoryUnique(mai);
	if (rvMem.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("ImageVk::createImage - allocateMemoryUnique failed: " + vk::to_string(rvMem.result));
	}
	memory_ = std::move(rvMem.value);

	vk::Result bindRes = device.bindImageMemory(image_.get(), memory_.get(), 0);
	if (bindRes != vk::Result::eSuccess)
	{
		throw std::runtime_error("ImageVk::createImage - bindImageMemory failed: " + vk::to_string(bindRes));
	}

    layout_ = vk::ImageLayout::eUndefined;
} // end of createImage()

void ImageVk::generateMipmaps(
	vk::Image image,
	vk::Format imageFormat,
	int32_t texWidth,
	int32_t texHeight,
	uint32_t mipLevels,
	uint32_t layers
)
{
    // check format support for linear blitting
    vk::FormatProperties formatProps =
        vk_->getPhysicalDevice().getFormatProperties(imageFormat);

    if (!(formatProps.optimalTilingFeatures &
        vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
    {
        throw std::runtime_error(
            "generateMipmaps - texture format does not support linear blitting"
        );
    }

    vk::CommandBuffer cmd = vk_->beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.image = image;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; ++i)
    {
        // transition mip i-1: TRANSFER_DST -> TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            nullptr,
            nullptr,
            barrier
        );

        // blit mip i-1 -> mip i
        vk::ImageBlit blit{};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = layers;
        blit.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
        blit.srcOffsets[1] = vk::Offset3D{ mipWidth, mipHeight, 1 };

        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = layers;
        blit.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
        blit.dstOffsets[1] = vk::Offset3D{
            mipWidth > 1 ? mipWidth / 2 : 1,
            mipHeight > 1 ? mipHeight / 2 : 1,
            1
        };

        cmd.blitImage(
            image, vk::ImageLayout::eTransferSrcOptimal,
            image, vk::ImageLayout::eTransferDstOptimal,
            1, &blit,
            vk::Filter::eLinear
        );

        // transition mip i-1: TRANSFER_SRC -> SHADER_READ_ONLY
        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            nullptr,
            nullptr,
            barrier
        );

        if (mipWidth > 1)  mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // transition last mip level: TRANSFER_DST -> SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        nullptr,
        nullptr,
        barrier
    );

    vk_->endSingleTimeCommands(cmd);

    layout_ = vk::ImageLayout::eShaderReadOnlyOptimal;
} // end of generateMipmaps()

void ImageVk::createImageView(
    vk::Format format,
    vk::ImageAspectFlags aspectFlags,
    vk::ImageViewType viewType,
    uint32_t layers
)
{
	if (!image_)
	{
		throw std::runtime_error("ImageVk::createImageView - image not created");
	}

	vk::ImageViewCreateInfo ivci{};
	ivci.image = image_.get();
	ivci.viewType = viewType;
	ivci.format = format;
	ivci.subresourceRange.aspectMask = aspectFlags;
	ivci.subresourceRange.baseMipLevel = 0;
	ivci.subresourceRange.levelCount = mipLevels_;
	ivci.subresourceRange.baseArrayLayer = 0;
	ivci.subresourceRange.layerCount = layers;

	vk::ResultValue rv = vk_->getDevice().createImageViewUnique(ivci);
	if (rv.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("ImageVk::createImageView - createImageViewUnique failed: " + vk::to_string(rv.result));
	}
	view_ = std::move(rv.value);
} // end of createImageView()

void ImageVk::createSampler(
    vk::Filter magFilter,
    vk::Filter minFilter,
    vk::SamplerMipmapMode mipmapMode,
    vk::SamplerAddressMode addressMode,
    bool enableAnisotropy
)
{
	vk::SamplerCreateInfo sci{};
	sci.magFilter = magFilter;
	sci.minFilter = minFilter;
	sci.addressModeU = addressMode;
	sci.addressModeV = addressMode;
	sci.addressModeW = addressMode;

	sci.anisotropyEnable = enableAnisotropy;
	sci.maxAnisotropy = enableAnisotropy
        ? vk_->getPhysicalDeviceProperties().limits.maxSamplerAnisotropy
        : 1.0f;

	sci.borderColor = vk::BorderColor::eFloatOpaqueWhite;
	sci.unnormalizedCoordinates = vk::False;
	sci.compareEnable = vk::False;
	sci.compareOp = vk::CompareOp::eAlways;

	sci.mipmapMode = mipmapMode;
	sci.minLod = 0.0f;
	sci.maxLod = std::min(5.0f, static_cast<float>(mipLevels_ - 1));
	sci.mipLodBias = 0.0f; 

	vk::ResultValue rv = vk_->getDevice().createSamplerUnique(sci);
	if (rv.result != vk::Result::eSuccess)
	{
		throw std::runtime_error("ImageVk::createSampler - createSamplerUnique failed: " + vk::to_string(rv.result));
	}
	sampler_ = std::move(rv.value);
} // end of createSampler()

void ImageVk::destroy()
{
	sampler_.reset();
	view_.reset();
	image_.reset();
	memory_.reset();

    layout_ = vk::ImageLayout::eUndefined;
	format_ = vk::Format::eUndefined;
	width_ = 0;
	height_ = 0;
	layers_ = 0;
	mipLevels_ = 1;
} // end of destroy()