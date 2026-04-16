#include "utils_vk.h"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <stdexcept>

namespace VkUtils
{
	void TransitionImageLayoutImmediate(
		VulkanMain& vk,
		vk::Image image,
		vk::ImageAspectFlags aspectMask,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		uint32_t layers,
		uint32_t mipLevels
	)
	{
		vk::CommandBuffer cmd = vk.beginSingleTimeCommands();

		vk::ImageMemoryBarrier barrier{};
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
		barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = aspectMask;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = layers;

		vk::PipelineStageFlags srcStage;
		vk::PipelineStageFlags dstStage;

		if (oldLayout == vk::ImageLayout::eUndefined &&
			newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
			newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			srcStage = vk::PipelineStageFlagBits::eTransfer;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else if (oldLayout == vk::ImageLayout::eUndefined &&
			newLayout == vk::ImageLayout::eGeneral)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask =
				vk::AccessFlagBits::eShaderRead |
				vk::AccessFlagBits::eShaderWrite;

			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
		}
		else
		{
			throw std::runtime_error("Unsupported immediate image layout transition");
		}

		cmd.pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);
		vk.endSingleTimeCommands(cmd);
	} // end of TransitionImageLayoutImmediate()

	void CopyBufferToImageImmediate(
		VulkanMain& vk,
		vk::Buffer buffer,
		vk::Image image,
		uint32_t width,
		uint32_t height,
		uint32_t layers
	)
	{
		vk::CommandBuffer cmd = vk.beginSingleTimeCommands();

		std::vector<vk::BufferImageCopy> regions;
		regions.reserve(layers);

		const vk::DeviceSize layerSize = static_cast<vk::DeviceSize>(width) * height * 4;

		for (uint32_t layer = 0; layer < layers; ++layer)
		{
			vk::BufferImageCopy region{};
			region.bufferOffset = layer * layerSize;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = layer;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = vk::Offset3D{ 0, 0, 0 };
			region.imageExtent = vk::Extent3D{ width, height, 1 };
			regions.push_back(region);
		}

		cmd.copyBufferToImage(
			buffer,
			image,
			vk::ImageLayout::eTransferDstOptimal,
			static_cast<uint32_t>(regions.size()),
			regions.data()
		);

		vk.endSingleTimeCommands(cmd);
	} // end of CopyBufferToImageImmediate()

	void TransitionImageLayout(
		vk::CommandBuffer cmd,
		vk::Image image,
		vk::ImageAspectFlags aspectMask,
		vk::ImageLayout& oldLayout,
		vk::ImageLayout newLayout,
		uint32_t layers,
		uint32_t mipLevels
	)
	{
		// early return if old == new
		if (oldLayout == newLayout)
		{
			return;
		}

		vk::ImageMemoryBarrier barrier{};
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
		barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = aspectMask;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = layers;

		vk::PipelineStageFlags srcStage;
		vk::PipelineStageFlags dstStage;

		if ((oldLayout == vk::ImageLayout::eUndefined ||
			oldLayout == vk::ImageLayout::ePresentSrcKHR) &&
			newLayout == vk::ImageLayout::eColorAttachmentOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		}
		else if (oldLayout == vk::ImageLayout::eUndefined &&
			newLayout == vk::ImageLayout::eDepthAttachmentOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

			srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
			dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		}
		else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
			newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else if (oldLayout == vk::ImageLayout::eDepthAttachmentOptimal &&
			newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			srcStage = vk::PipelineStageFlagBits::eLateFragmentTests;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal &&
			newLayout == vk::ImageLayout::eColorAttachmentOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

			srcStage = vk::PipelineStageFlagBits::eFragmentShader;
			dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		}
		else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal &&
			newLayout == vk::ImageLayout::eDepthAttachmentOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

			srcStage = vk::PipelineStageFlagBits::eFragmentShader;
			dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		}
		else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
			newLayout == vk::ImageLayout::ePresentSrcKHR)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			barrier.dstAccessMask = {};

			srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dstStage = vk::PipelineStageFlagBits::eBottomOfPipe;
		}
		else if (oldLayout == vk::ImageLayout::eGeneral &&
			newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			srcStage = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal &&
			newLayout == vk::ImageLayout::eGeneral)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			barrier.dstAccessMask =
				vk::AccessFlagBits::eShaderRead |
				vk::AccessFlagBits::eShaderWrite;

			srcStage = vk::PipelineStageFlagBits::eFragmentShader;
			dstStage = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
		}
		else
		{
			throw std::runtime_error(
				"Unsupported frame image layout transition: " + vk::to_string(oldLayout) + " -> " + vk::to_string(newLayout)
			);
		}

		cmd.pipelineBarrier(
			srcStage,
			dstStage,
			{},
			{},
			{},
			barrier
		);

		oldLayout = newLayout;
	} // end of TransitionImageLayout()
};