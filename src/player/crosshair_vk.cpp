#include "crosshair_vk.h"

#include "constants.h"
#include "vulkan_main.h"
#include "frame_context_vk.h"

#include "image_vk.h"
#include "buffer_vk.h"
#include "graphics_pipeline_vk.h"
#include "shader_vk.h"

#include <vulkan/vulkan.hpp>

#include <memory>

using namespace Crosshair_Constants;

//--- PUBLIC ---//
CrosshairVk::CrosshairVk(VulkanMain& vk)
	: vk_(vk),
	vBuffer_(vk),
	pipeline_(vk)
{
} // end of constructor

CrosshairVk::~CrosshairVk() = default;

void CrosshairVk::init()
{

	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"crosshair/crosshair.vert.spv", 
		"crosshair/crosshair.frag.spv"
	);

	createResources();
	createPipeline();
} // end of init()

void CrosshairVk::render(const FrameContext* frame)
{
	if (!frame->cmd || !vBuffer_.valid())
		return;

	vk::CommandBuffer cmd = frame->cmd;

	vk::RenderingAttachmentInfo colorAttach{};
	colorAttach.imageView = frame->colorImageView;
	colorAttach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttach.loadOp = vk::AttachmentLoadOp::eLoad;
	colorAttach.storeOp = vk::AttachmentStoreOp::eStore;

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = frame->extent;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttach;
	renderingInfo.pDepthAttachment = nullptr;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(frame->extent.width);
		viewport.height = static_cast<float>(frame->extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = frame->extent;
		cmd.setScissor(0, 1, &scissor);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());

		vk::Buffer vb = vBuffer_.getBuffer();
		vk::DeviceSize offset = 0;

		cmd.bindVertexBuffers(0, 1, &vb, &offset);
		cmd.draw(4, 1, 0, 0);
	}
	cmd.endRendering();
} // end of render()

//--- PRIVATE ---//
void CrosshairVk::createResources()
{
	vk::DeviceSize vbSize = sizeof(VERTICES);

	BufferVk newVB(vk_);

	vk::CommandBuffer cmd = vk_.beginSingleTimeCommands();

	// VB staging
	BufferVk stagingVB(vk_);
	stagingVB.create(
		vbSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	stagingVB.upload(VERTICES, vbSize);

	// VB device local
	newVB.create(
		vbSize,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	vk_.recordCopyBuffer(
		cmd,
		stagingVB.getBuffer(),
		newVB.getBuffer(),
		vbSize
	);

	vk_.endSingleTimeCommands(cmd);

	vBuffer_ = std::move(newVB);
} // end of createResources()

void CrosshairVk::createPipeline()
{
	GraphicsPipelineDescVk desc{};
	desc.vertShader = shader_->vertShader();
	desc.fragShader = shader_->fragShader();

	vk::VertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(float) * 2;
	binding.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription attr{};
	attr.location = 0;
	attr.binding = 0;
	attr.format = vk::Format::eR32G32Sfloat;
	attr.offset = 0;

	desc.vertexBinding = binding;
	desc.vertexAttributes = { attr };

	desc.topology = vk::PrimitiveTopology::eLineList;

	desc.colorFormat = vk_.getSwapChainImageFormat();

	desc.cullMode = vk::CullModeFlagBits::eNone;
	desc.depthTestEnable = false;
	desc.depthWriteEnable = false;

	pipeline_.create(desc);
} // end of createPipeline()