#include "fog_pass_vk.h"

#include "constants.h"

#include "bindings.h"
#include "compute_shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>

using namespace Fog_Constants;

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
		//!inputColorImage_ || 
		!inputDepthImage_ ||
		!outputImage_.valid() ||
		!computePipeline_.valid())
	{
		return;
	}

	vk::CommandBuffer cmd = frame.cmd;
	vk::Extent2D extent = frame.extent;

	DescriptorSetVk& desc = descriptorSets_[frame.frameIndex];
	if (!desc.valid()) return;

	//desc.writeCombinedImageSampler(
	//	TO_API_FORM(FogPassBinding::ForwardColorTex),
	//	inputColorImage_->view(),
	//	inputColorImage_->sampler()
	//);

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

	uint32_t fogWidth = (extent.width + 1) / resFactor_;
	uint32_t fogHeight = (extent.height + 1) / resFactor_;

	uint32_t groupX = (fogWidth + 7) / 8;
	uint32_t groupY = (fogHeight + 7) / 8;

	cmd.dispatch(groupX, groupY, 1);

	outputImage_.transitionToShaderRead(cmd);
} // end of render()


//--- PRIVATE ---//
void FogPassVk::refreshInput()
{
	if (!inputDepthImage_ || !inputShadowMapImage_)
		return;

	for (auto& set : descriptorSets_)
	{
		//set.writeCombinedImageSampler(
		//	TO_API_FORM(FogPassBinding::ForwardColorTex),
		//	inputColorImage_->view(),
		//	inputColorImage_->sampler()
		//);

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
	vk::Extent2D extent = vk_.getSwapChainExtent();

	uint32_t fogWidth = (extent.width + 1) / resFactor_;
	uint32_t fogHeight = (extent.height + 1) / resFactor_;

	outputImage_.createImage(
		fogWidth,
		fogHeight,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		outputFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | 
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
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | 
			vk::ShaderStageFlagBits::eFragment |
			vk::ShaderStageFlagBits::eCompute;

		//vk::DescriptorSetLayoutBinding inputColorBinding{};
		//inputColorBinding.binding = TO_API_FORM(FogPassBinding::ForwardColorTex);
		//inputColorBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		//inputColorBinding.descriptorCount = 1;
		//inputColorBinding.stageFlags = vk::ShaderStageFlagBits::eFragment |
		//	vk::ShaderStageFlagBits::eCompute;

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
			//inputColorBinding, 
			inputDepthBinding,
			inputShadowBinding,
			outputColorBinding
			});

		vk::DescriptorPoolSize uboPool;
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		//vk::DescriptorPoolSize inputColorPool;
		//inputColorPool.type = vk::DescriptorType::eCombinedImageSampler;
		//inputColorPool.descriptorCount = 1;

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
			//inputColorPool, 
			inputDepthPool,
			inputShadowPool,
			outputColorPool
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
	ComputePipelineDescVk compDesc{};
	compDesc.computeShader = compShader_->shader();
	compDesc.setLayouts = { descriptorSets_[0].getLayout() };

	computePipeline_.create(compDesc);
} // end of createPipeline()