#include "fog_pass_vk.h"

#include "constants.h"

#include "bindings.h"
#include "compute_shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

//--- PUBLIC ---//
FogPassVk::FogPassVk(VulkanMain& vk)
	: vk_(vk),
	outputImage_(vk),
	computePipeline_(vk)
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
	compShader_ = std::make_unique<ComputeShaderModuleVk>(
		vk_.getDevice(),
		"fogpass/fog.comp.spv"
	);

	createResources();
	createDescriptorSet();
	createPipeline();
} // end of init()

void FogPassVk::resize()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	if (extent.width <= 0 || extent.height <= 0) return;

	uint32_t newWidth = (extent.width + resFactor_ - 1) / resFactor_;
	uint32_t newHeight = (extent.height + resFactor_ - 1) / resFactor_;

	if (newWidth == width_ && newHeight == height_) return;

	width_ = newWidth;
	height_ = newHeight;

	workGroupX_ = (width_ + (numWorkGroups_ - 1)) / numWorkGroups_;
	workGroupY_ = (height_ + (numWorkGroups_ - 1)) / numWorkGroups_;

	createAttachment();
	refreshInput();
} // end of resize()

void FogPassVk::render(
	FrameContext& frame,
	Fog_Constants::FogPassUBO& fogUBO
)
{
	if (!inputShadowMapImage_ ||
		!inputDepthImage_ ||
		!outputImage_.valid() ||
		!computePipeline_.valid())
	{
		return;
	}

	vk::CommandBuffer cmd = frame.cmd;

	cmd.beginDebugUtilsLabelEXT({ "FogPassVk::cmd" });

	DescriptorSetVk& desc = descriptorSets_[frame.frameIndex];
	if (!desc.valid()) return;

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

	desc.writeStorageImage(
		TO_API_FORM(FogPassBinding::OutColorTex),
		outputImage_.view(),
		vk::ImageLayout::eGeneral
	);

	outputImage_.transitionToGeneral(cmd);

	vk::DescriptorSet set = desc.getSet();

	uboBuffers_[frame.frameIndex].upload(&fogUBO, sizeof(fogUBO));

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline_.getPipeline());
	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		computePipeline_.getLayout(),
		0,
		1, &set,
		0, nullptr
	);

	cmd.dispatch(
		workGroupX_, 
		workGroupY_, 
		1
	);

	outputImage_.transitionToShaderRead(cmd);

	cmd.endDebugUtilsLabelEXT();
} // end of render()


//--- PRIVATE ---//
void FogPassVk::refreshInput()
{
	if (!inputDepthImage_ || 
		!inputShadowMapImage_)
		return;

	for (auto& set : descriptorSets_)
	{
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

		set.writeStorageImage(
			TO_API_FORM(FogPassBinding::OutColorTex),
			outputImage_.view(),
			vk::ImageLayout::eGeneral
		);
	} // end for
} // end of refreshInput()

void FogPassVk::createAttachment()
{
	outputImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		outputFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eSampled |
		vk::ImageUsageFlagBits::eStorage,
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
			sizeof(Fog_Constants::FogPassUBO),
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
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | 
			vk::ShaderStageFlagBits::eFragment |
			vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding inputDepthBinding{};
		inputDepthBinding.binding = TO_API_FORM(FogPassBinding::ForwardDepthTex);
		inputDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputDepthBinding.descriptorCount = 1;
		inputDepthBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding inputShadowBinding{};
		inputShadowBinding.binding = TO_API_FORM(FogPassBinding::ShadowMapTex);
		inputShadowBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputShadowBinding.descriptorCount = 1;
		inputShadowBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding outputColorBinding;
		outputColorBinding.binding = TO_API_FORM(FogPassBinding::OutColorTex);
		outputColorBinding.descriptorType = vk::DescriptorType::eStorageImage;
		outputColorBinding.descriptorCount = 1;
		outputColorBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		descriptorSets_[i].createLayout({
			uboBinding, 
			inputDepthBinding,
			inputShadowBinding,
			outputColorBinding
			});

		vk::DescriptorPoolSize uboPool;
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputDepthPool;
		inputDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputDepthPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputShadowPool;
		inputShadowPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputShadowPool.descriptorCount = 1;
		
		vk::DescriptorPoolSize outputColorPool;
		outputColorPool.type = vk::DescriptorType::eStorageImage;
		outputColorPool.descriptorCount = 1;

		descriptorSets_[i].createPool({
			uboPool, 
			inputDepthPool,
			inputShadowPool,
			outputColorPool
			});
		descriptorSets_[i].allocate();

		descriptorSets_[i].writeUniformBuffer(
			TO_API_FORM(FogPassBinding::UBO),
			uboBuffers_[i].getBuffer(),
			sizeof(Fog_Constants::FogPassUBO)
		);

		descriptorSets_[i].setDebugName(
			"FogPassVk::DescriptorSet frame " + std::to_string(i)
		);
	} // end for
} // end of createDescriptorSet()

void FogPassVk::createPipeline()
{
	ComputePipelineDescVk compDesc{};
	compDesc.computeShader = compShader_->shader();
	compDesc.setLayouts = { descriptorSets_[0].getLayout() };

	computePipeline_.create(compDesc);

	computePipeline_.setDebugName("FogPassVk::Pipeline");
} // end of createPipeline()