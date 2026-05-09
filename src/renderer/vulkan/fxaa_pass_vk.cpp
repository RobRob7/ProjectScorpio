#include "fxaa_pass_vk.h"

#include "constants.h"
#include "frame_context_vk.h"

#include "bindings.h"
#include "shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

using namespace FXAA_Constants;

//--- PUBLIC ---//
FXAAPassVk::FXAAPassVk(VulkanMain& vk)
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

FXAAPassVk::~FXAAPassVk() = default;

void FXAAPassVk::init()
{
	shader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"fxaapass/fxaa.vert.spv",
		"fxaapass/fxaa.frag.spv"
	);

	createAttachment();
	createResources();
	createDescriptorSets();
	createPipeline();
} // end of init()

void FXAAPassVk::resize()
{
	createAttachment();
} // end of resize()

void FXAAPassVk::render(FrameContext& frame)
{
	if (!inputImage_ || 
		!pipeline_.valid())
	{
		return;
	}

	DescriptorSetVk& desc = descriptorSets_[frame.frameIndex];
	if (!desc.valid()) return;

	desc.writeCombinedImageSampler(
		TO_API_FORM(FXAAPassBinding::ForwardColorTex),
		inputImage_->view(),
		inputImage_->sampler()
	);

	vk::DescriptorSet set = desc.getSet();

	vk::CommandBuffer cmd = frame.cmd;
	vk::Extent2D extent = frame.extent;
	outputImage_.transitionToColorAttachment(cmd);

	// update UBO
	uboData_ = {};
	uboData_.u_inverseScreenSize = 
		glm::vec2(1.0f / static_cast<float>(extent.width), 
			1.0f / static_cast<float>(extent.height));
	uboData_.u_edgeSharpnessQuality = EDGE_SHARP_QUALITY;
	uboData_.u_edgeThresholdMax = EDGE_THRESH_MAX;
	uboData_.u_edgeThresholdMin = EDGE_THRESH_MIN;

	uboBuffers_[frame.frameIndex].upload(&uboData_, sizeof(uboData_));

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
		viewport.width = static_cast<float>(frame.extent.width);
		viewport.height = static_cast<float>(frame.extent.height);
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
void FXAAPassVk::refreshInput()
{
	if (!inputImage_)
		return;

	for (auto& set : descriptorSets_)
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(FXAAPassBinding::ForwardColorTex),
			inputImage_->view(),
			inputImage_->sampler()
		);
	} // end for
} // end of refreshInput()

void FXAAPassVk::createAttachment()
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

void FXAAPassVk::createResources()
{
	for (auto& buffer : uboBuffers_)
	{
		buffer.create(
			sizeof(FXAAPassUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void FXAAPassVk::createDescriptorSets()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		vk::DescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = TO_API_FORM(FXAAPassBinding::UBO);
		uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding inputImgBinding{};
		inputImgBinding.binding = TO_API_FORM(FXAAPassBinding::ForwardColorTex);
		inputImgBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputImgBinding.descriptorCount = 1;
		inputImgBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		descriptorSets_[i].createLayout({
			uboBinding, 
			inputImgBinding
			});

		vk::DescriptorPoolSize uboPool;
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputImgPool;
		inputImgPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputImgPool.descriptorCount = 1;

		descriptorSets_[i].createPool({ 
			uboPool, 
			inputImgPool 
			});
		descriptorSets_[i].allocate();

		descriptorSets_[i].setDebugName(
			"FXAAPass::descriptorSets_ frame " + std::to_string(i)
		);

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(FXAAPassBinding::UBO),
			uboBuffers_[i].getBuffer(),
			sizeof(FXAAPassUBO)
		);
	} // end for
} // end of createDescriptorSet()

void FXAAPassVk::createPipeline()
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