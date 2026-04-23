#include "fog_pass_vk.h"

#include "constants.h"
#include "render_settings.h"

#include "bindings.h"
#include "shader_vk.h"
#include "utils_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

using namespace Fog_Constants;

//--- PUBLIC ---//
FogPassVk::FogPassVk(VulkanMain& vk, RenderSettings& rs)
	: vk_(vk),
	rs_(rs),
	outputImage_(vk),
	pipeline_(vk)
{
	uboBuffers_.reserve(vk.getMaxFramesInFlight());
	descriptorSets_.reserve(vk.getMaxFramesInFlight());
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_.emplace_back(vk_);
		descriptorSets_.emplace_back(vk_);
	} // end for
} // end of constuctor

FogPassVk::~FogPassVk() = default;

void FogPassVk::init()
{
	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"fogpass/fog.vert.spv",
		"fogpass/fog.frag.spv"
	);

	rs_.fogSettings.color = FOG_COLOR;
	rs_.fogSettings.start = FOG_START;
	rs_.fogSettings.end = FOG_END;

	createAttachment();
	createResources();
	createDescriptorSet();
	createPipeline();
} // end of init()

void FogPassVk::resize()
{
	createAttachment();
} // end of resize()

void FogPassVk::render(
	FrameContext& frame,
	float nearPlane,
	float farPlane,
	float ambStr
)
{
	if (!inputColorImage_ || !inputDepthImage_ ||
		!uboBuffers_[frame.frameIndex].valid() || 
		!descriptorSets_[frame.frameIndex].valid() 
		|| !pipeline_.valid())
	{
		return;
	}

	vk::CommandBuffer cmd = frame.cmd;
	vk::Extent2D extent = frame.extent;

	// update UBO
	ubo_.u_near = nearPlane;
	ubo_.u_far = farPlane;
	ubo_.u_ambStr = ambStr;
	ubo_.u_fogColor = rs_.fogSettings.color;
	ubo_.u_fogStart = rs_.fogSettings.start;
	ubo_.u_fogEnd = rs_.fogSettings.end;

	uboBuffers_[frame.frameIndex].upload(&ubo_, sizeof(ubo_));

	VkUtils::TransitionImageLayout(
		cmd,
		outputImage_.image(),
		vk::ImageAspectFlagBits::eColor,
		outputLayout_,
		vk::ImageLayout::eColorAttachmentOptimal,
		1,
		1
	);

	vk::ClearValue clear{};
	clear.color.float32[0] = 0.0f;
	clear.color.float32[1] = 0.0f;
	clear.color.float32[2] = 0.0f;
	clear.color.float32[3] = 1.0f;

	vk::RenderingAttachmentInfo colorAttach{};
	colorAttach.imageView = outputImage_.view();
	colorAttach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttach.loadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttach.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttach.clearValue = clear;

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = extent;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttach;
	renderingInfo.pDepthAttachment = nullptr;

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

		vk::DescriptorSet set = descriptorSets_[frame.frameIndex].getSet();

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.getPipeline());
		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		cmd.draw(3, 1, 0, 0);
	}
	cmd.endRendering();

	VkUtils::TransitionImageLayout(
		cmd,
		outputImage_.image(),
		vk::ImageAspectFlagBits::eColor,
		outputLayout_,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		1,
		1
	);
} // end of render()


//--- PRIVATE ---//
void FogPassVk::refreshInput()
{
	if (!inputColorImage_ || !inputDepthImage_)
		return;

	for (auto& set : descriptorSets_)
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(FogPassBinding::ForwardColorTex),
			inputColorImage_->view(),
			inputColorImage_->sampler()
		);

		set.writeCombinedImageSampler(
			TO_API_FORM(FogPassBinding::ForwardDepthTex),
			inputDepthImage_->view(),
			inputDepthImage_->sampler()
		);
	} // end for
} // end of refreshInput()

void FogPassVk::createAttachment()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();

	outputImage_.createImage(
		extent.width,
		extent.height,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		outputFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	outputImage_.createImageView(
		outputFormat_,
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	outputImage_.createSampler(
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		vk::False
	);

	// RESET
	outputLayout_ = vk::ImageLayout::eUndefined;
} // end of createAttachment()

void FogPassVk::createResources()
{
	for (auto& buffer : uboBuffers_)
	{
		buffer.create(
			sizeof(FogPassUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void FogPassVk::createDescriptorSet()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		vk::DescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = TO_API_FORM(FogPassBinding::UBO);
		uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding inputColorBinding{};
		inputColorBinding.binding = TO_API_FORM(FogPassBinding::ForwardColorTex);
		inputColorBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputColorBinding.descriptorCount = 1;
		inputColorBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding inputDepthBinding{};
		inputDepthBinding.binding = TO_API_FORM(FogPassBinding::ForwardDepthTex);
		inputDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputDepthBinding.descriptorCount = 1;
		inputDepthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		descriptorSets_[i].createLayout({uboBinding, inputColorBinding, inputDepthBinding});

		vk::DescriptorPoolSize uboPool;
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputColorPool;
		inputColorPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputColorPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputDepthPool;
		inputDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputDepthPool.descriptorCount = 1;

		descriptorSets_[i].createPool({uboPool, inputColorPool, inputDepthPool});
		descriptorSets_[i].allocate();

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(FogPassBinding::UBO),
			uboBuffers_[i].getBuffer(),
			sizeof(FogPassUBO)
		);
	} // end for
} // end of createDescriptorSet()

void FogPassVk::createPipeline()
{
	GraphicsPipelineDescVk desc{};
	desc.vertShader = shader_->vertShader();
	desc.fragShader = shader_->fragShader();

	desc.setLayouts = { descriptorSets_[0].getLayout()};

	desc.colorFormat = outputFormat_;

	desc.cullMode = vk::CullModeFlagBits::eNone;
	desc.frontFace = vk::FrontFace::eClockwise;
	desc.depthTestEnable = vk::False;
	desc.depthWriteEnable = vk::False;

	pipeline_.create(desc);
} // end of createPipeline()