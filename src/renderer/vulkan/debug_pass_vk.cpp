#include "debug_pass_vk.h"

#include "frame_context_vk.h"
#include "utils_vk.h"

#include "constants.h"
#include "bindings.h"

#include "image_vk.h"
#include "shader_vk.h"
#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include "vulkan_main.h"

#include <vulkan/vulkan.hpp>

#include <memory>

using namespace Debug_Constants;

//--- PUBLIC ---//
DebugPassVk::DebugPassVk(
	VulkanMain& vk, 
	const ImageVk& normalImage, 
	const ImageVk& depthImage,
	const ImageVk& shadowMapImage,
	const ImageVk& rtDepthImage
)
	: vk_(vk),
	normalImage_(normalImage),
	depthImage_(depthImage),
	shadowMapImage_(shadowMapImage),
	rtDepthImage_(rtDepthImage),
	pipeline_(vk)
{
	uboBuffers_.reserve(vk_.getMaxFramesInFlight());
	descriptorSets_.reserve(vk_.getMaxFramesInFlight());

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_.emplace_back(vk_);
		descriptorSets_.emplace_back(vk_);
	} // end for
} // end of constructor

DebugPassVk::~DebugPassVk() = default;

void DebugPassVk::init()
{
	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"debugpass/debugpass.vert.spv",
		"debugpass/debugpass.frag.spv"
	);

	createResources();
	createDescriptorSets();
	createPipeline();
	refreshInputs();
} // end of init()

void DebugPassVk::resize()
{
	refreshInputs();
} // end of resize()

void DebugPassVk::render(
	FrameContext& frame,
	float nearPlane,
	float farPlane,
	int mode
)
{
	vk::CommandBuffer cmd = frame.cmd;

	DebugPassUBO ubo{};
	ubo.u_near = nearPlane;
	ubo.u_far = farPlane;
	ubo.u_mode = mode;

	uboBuffers_[frame.frameIndex].upload(&ubo, sizeof(DebugPassUBO));

	frame.transitionColorImageToAttachment(cmd);

	vk::ClearValue clear{};
	clear.color.float32[0] = 0.0f;
	clear.color.float32[1] = 0.0f;
	clear.color.float32[2] = 0.0f;
	clear.color.float32[3] = 1.0f;

	vk::RenderingAttachmentInfo colorAttachment{};
	colorAttachment.imageView = frame.colorImageView;
	colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.clearValue = clear;

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = frame.extent;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(frame.extent.width);
		viewport.height = static_cast<float>(frame.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = frame.extent;
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
	
	frame.transitionColorImageToPresent(cmd);
} // end of render()


//--- PRIVATE ---//
void DebugPassVk::refreshInputs()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::GNormalTex),
			normalImage_.view(),
			normalImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::GDepthTex),
			depthImage_.view(),
			depthImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::ShadowMapTex),
			shadowMapImage_.view(),
			shadowMapImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::RTDepthTex),
			rtDepthImage_.view(),
			rtDepthImage_.sampler()
		);
	} // end for
} // end of refreshInputs()

void DebugPassVk::createResources()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_[i].create(
			sizeof(DebugPassUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void DebugPassVk::createDescriptorSets()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		vk::DescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = TO_API_FORM(DebugBinding::UBO);
		uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding normalBinding{};
		normalBinding.binding = TO_API_FORM(DebugBinding::GNormalTex);
		normalBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		normalBinding.descriptorCount = 1;
		normalBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding depthBinding{};
		depthBinding.binding = TO_API_FORM(DebugBinding::GDepthTex);
		depthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		depthBinding.descriptorCount = 1;
		depthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding shadowMapBinding{};
		shadowMapBinding.binding = TO_API_FORM(DebugBinding::ShadowMapTex);
		shadowMapBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		shadowMapBinding.descriptorCount = 1;
		shadowMapBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding rtDepthBinding{};
		rtDepthBinding.binding = TO_API_FORM(DebugBinding::RTDepthTex);
		rtDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		rtDepthBinding.descriptorCount = 1;
		rtDepthBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		descriptorSets_[i].createLayout({
			uboBinding,
			normalBinding,
			depthBinding,
			shadowMapBinding,
			rtDepthBinding
			});

		vk::DescriptorPoolSize uboPool{};
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize normalPool{};
		normalPool.type = vk::DescriptorType::eCombinedImageSampler;
		normalPool.descriptorCount = 1;

		vk::DescriptorPoolSize depthPool{};
		depthPool.type = vk::DescriptorType::eCombinedImageSampler;
		depthPool.descriptorCount = 1;

		vk::DescriptorPoolSize shadowMapPool{};
		shadowMapPool.type = vk::DescriptorType::eCombinedImageSampler;
		shadowMapPool.descriptorCount = 1;

		vk::DescriptorPoolSize rtDepthPool{};
		rtDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
		rtDepthPool.descriptorCount = 1;

		descriptorSets_[i].createPool({
			uboPool,
			normalPool,
			depthPool,
			shadowMapPool,
			rtDepthPool
			});
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			"DebugPassVk::descriptorSets_ frame " + std::to_string(i)
		);

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(DebugBinding::UBO),
			uboBuffers_[i].getBuffer(),
			sizeof(DebugPassUBO)
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::GNormalTex),
			normalImage_.view(),
			normalImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::GDepthTex),
			depthImage_.view(),
			depthImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::ShadowMapTex),
			shadowMapImage_.view(),
			shadowMapImage_.sampler()
		);

		descriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(DebugBinding::RTDepthTex),
			rtDepthImage_.view(),
			rtDepthImage_.sampler()
		);
	} 
} // end of createDescriptorSets()

void DebugPassVk::createPipeline()
{
	GraphicsPipelineDescVk desc{};
	desc.vertShader = shader_->vertShader();
	desc.fragShader = shader_->fragShader();

	desc.setLayouts = { descriptorSets_[0].getLayout()};

	desc.colorFormat = vk_.getSwapChainImageFormat();

	desc.cullMode = vk::CullModeFlagBits::eNone;
	desc.frontFace = vk::FrontFace::eClockwise;
	desc.depthTestEnable = vk::False;
	desc.depthWriteEnable = vk::False;

	pipeline_.create(desc);
} // end of createPipeline()