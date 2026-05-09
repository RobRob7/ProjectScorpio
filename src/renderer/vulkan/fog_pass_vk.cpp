#include "fog_pass_vk.h"

#include "constants.h"

#include "bindings.h"
#include "shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

using namespace Fog_Constants;

//--- PUBLIC ---//
FogPassVk::FogPassVk(VulkanMain& vk)
	: vk_(vk),
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
	Fog_Constants::FogPassUBO& fogUBO
)
{
	if (!inputShadowMapImage_ ||
		!inputColorImage_ || 
		!inputDepthImage_ ||
		!pipeline_.valid())
	{
		return;
	}

	DescriptorSetVk& desc = descriptorSets_[frame.frameIndex];
	if (!desc.valid()) return;

	desc.writeCombinedImageSampler(
		TO_API_FORM(FogPassBinding::ForwardColorTex),
		inputColorImage_->view(),
		inputColorImage_->sampler()
	);

	desc.writeCombinedImageSampler(
		TO_API_FORM(FogPassBinding::ForwardDepthTex),
		inputDepthImage_->view(),
		inputDepthImage_->sampler()
	);

	desc.writeCombinedImageSampler(
		TO_API_FORM(FogPassBinding::ShadowMapTex),
		inputShadowMapImage_->view(),
		inputShadowMapImage_->sampler()
	);

	vk::DescriptorSet set = desc.getSet();

	vk::CommandBuffer cmd = frame.cmd;
	vk::Extent2D extent = frame.extent;

	outputImage_.transitionToColorAttachment(cmd);

	uboBuffers_[frame.frameIndex].upload(&fogUBO, sizeof(fogUBO));

	vk::ClearValue clear{ {0.0f, 0.0f, 0.0f, 1.0f} };

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

	outputImage_.transitionToShaderRead(cmd);
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

		set.writeCombinedImageSampler(
			TO_API_FORM(FogPassBinding::ShadowMapTex),
			inputShadowMapImage_->view(),
			inputShadowMapImage_->sampler()
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

		vk::DescriptorSetLayoutBinding inputShadowBinding{};
		inputShadowBinding.binding = TO_API_FORM(FogPassBinding::ShadowMapTex);
		inputShadowBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputShadowBinding.descriptorCount = 1;
		inputShadowBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		descriptorSets_[i].createLayout({
			uboBinding, 
			inputColorBinding, 
			inputDepthBinding,
			inputShadowBinding
			});

		vk::DescriptorPoolSize uboPool;
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputColorPool;
		inputColorPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputColorPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputDepthPool;
		inputDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputDepthPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputShadowPool;
		inputShadowPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputShadowPool.descriptorCount = 1;

		descriptorSets_[i].createPool({
			uboPool, 
			inputColorPool, 
			inputDepthPool,
			inputShadowPool
			});
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			"FogPassVk::descriptorSets_ frame " + std::to_string(i)
		);

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
	desc.depthTestEnable = false;
	desc.depthWriteEnable = false;

	pipeline_.create(desc);
} // end of createPipeline()