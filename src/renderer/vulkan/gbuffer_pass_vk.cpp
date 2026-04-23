#include "gbuffer_pass_vk.h"

#include "frame_context_vk.h"

#include "chunk_pass_vk.h"

#include "utils_vk.h"
#include "vulkan_main.h"

#include "vulkan/vulkan.hpp"

using namespace Gbuffer_Constants;
using namespace World;

//--- PUBLIC ---//
GBufferPassVk::GBufferPassVk(VulkanMain& vk)
	: vk_(vk),
	gNormalImage_(vk),
	gDepthImage_(vk)
{
} // end of constructor

GBufferPassVk::~GBufferPassVk() = default;

void GBufferPassVk::init()
{
	createAttachments();
} // end of init()

void GBufferPassVk::resize()
{
	createAttachments();
} // end of resize()

void GBufferPassVk::render(
	ChunkPassVk& chunk,
	const RenderInputs& in,
	const FrameContext& frame,
	const glm::mat4& view,
	const glm::mat4& proj
)
{
	vk::CommandBuffer cmd = frame.cmd;

	vk::Extent2D extent = vk_.getSwapChainExtent();

	VkUtils::TransitionImageLayout(
		cmd,
		gNormalImage_.image(),
		vk::ImageAspectFlagBits::eColor,
		normalLayout_,
		vk::ImageLayout::eColorAttachmentOptimal,
		1,
		1
	);

	VkUtils::TransitionImageLayout(
		cmd,
		gDepthImage_.image(),
		vk::ImageAspectFlagBits::eDepth,
		depthLayout_,
		vk::ImageLayout::eDepthAttachmentOptimal,
		1,
		1
	);

	vk::ClearValue normalClear{};
	normalClear.color.float32[0] = 0.0f;
	normalClear.color.float32[1] = 0.0f;
	normalClear.color.float32[2] = 0.0f;
	normalClear.color.float32[3] = 1.0f;

	vk::ClearValue depthClear{};
	depthClear.depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

	vk::RenderingAttachmentInfo colorAttachment{};
	colorAttachment.imageView = gNormalImage_.view();
	colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.clearValue = normalClear;

	vk::RenderingAttachmentInfo depthAttachment{};
	depthAttachment.imageView = gDepthImage_.view();
	depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	depthAttachment.clearValue = depthClear;

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = extent;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = extent;
		cmd.setScissor(0, 1, &scissor);

		// render world
		chunk.renderOpaque(
			RenderTargetVk::GBuffer,
			in,
			frame,
			view,
			proj
		);
	}
	cmd.endRendering();
	
	VkUtils::TransitionImageLayout(
		cmd,
		gNormalImage_.image(),
		vk::ImageAspectFlagBits::eColor,
		normalLayout_,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		1,
		1
	);

	VkUtils::TransitionImageLayout(
		cmd,
		gDepthImage_.image(),
		vk::ImageAspectFlagBits::eDepth,
		depthLayout_,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		1,
		1
	);
} // end of render()


//--- PRIVATE ---//
void GBufferPassVk::createAttachments()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();

	// NORMAL
	gNormalImage_.createImage(
		extent.width,
		extent.height,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		normalFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	gNormalImage_.createImageView(
		normalFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);
	gNormalImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);


	// DEPTH
	gDepthImage_.createImage(
		extent.width,
		extent.height,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		depthFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	gDepthImage_.createImageView(
		depthFormat_,
		vk::ImageAspectFlagBits::eDepth,
		vk::ImageViewType::e2D,
		1
	);
	gDepthImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);

	// RESET
	normalLayout_ = vk::ImageLayout::eUndefined;
	depthLayout_ = vk::ImageLayout::eUndefined;
} // end of createAttachments()
