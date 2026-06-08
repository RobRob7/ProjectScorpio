#include "god_ray_pass_vk.h"

#include "render_settings.h"

#include "constants.h"

#include "bindings.h"
#include "compute_shader_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <algorithm>

//--- PUBLIC ---//
GodRayPassVk::GodRayPassVk(VulkanMain& vk, const RenderSettings& rs)
	: vk_(vk),
	rs_(rs),
	outputImage_(vk),
	computePipeline_(vk)
{
	factor_ = std::max(1u, rs_.resScale.GOD_RAYS);

	uboBuffers_.reserve(vk.getMaxFramesInFlight());
	descriptorSets_.reserve(vk.getMaxFramesInFlight());
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		uboBuffers_.emplace_back(vk_);
		descriptorSets_.emplace_back(vk_);
	} // end for
} // end of constuctor

GodRayPassVk::~GodRayPassVk() = default;

void GodRayPassVk::init()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	width_ = std::max(1u, (extent.width + factor_ - 1) / factor_);
	height_ = std::max(1u, (extent.height + factor_ - 1) / factor_);

	workGroupX_ = (width_ + numWorkGroups_ - 1) / numWorkGroups_;
	workGroupY_ = (height_ + numWorkGroups_ - 1) / numWorkGroups_;

	compShader_ = std::make_unique<ComputeShaderModuleVk>(
		vk_.getDevice(),
		"godraypass/godray.comp.spv"
	);

	createAttachment();
	createResources();
	createDescriptorSet();
	createPipeline();
} // end of init()

void GodRayPassVk::resize()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	if (extent.width <= 0 || extent.height <= 0) return;

	uint32_t newWidth = (extent.width + factor_ - 1) / factor_;
	uint32_t newHeight = (extent.height + factor_ - 1) / factor_;

	if (newWidth == width_ && newHeight == height_) return;

	width_ = newWidth;
	height_ = newHeight;

	workGroupX_ = (width_ + (numWorkGroups_ - 1)) / numWorkGroups_;
	workGroupY_ = (height_ + (numWorkGroups_ - 1)) / numWorkGroups_;

	const uint32_t retireFrame = vk_.getPrevFrameIndex();

	vk_.retireImage(retireFrame, std::move(outputImage_));
	outputImage_ = ImageVk(vk_);

	createAttachment();
	updateDescriptorSet(vk_.currentFrameIndex());
} // end of resize()

void GodRayPassVk::render(const GodRayUBOs& ubos, const FrameContext& frame)
{
	syncSettings();

	if (!inputShadowMapImage_ ||
		!inputDepthImage_ ||
		!outputImage_.valid() ||
		!computePipeline_.valid())
	{
		return;
	}

	updateDescriptorSet(frame.frameIndex);
	
	vk::CommandBuffer cmd = frame.cmd;

	cmd.beginDebugUtilsLabelEXT({ "GodRayPassVk::cmd" });

	outputImage_.transitionToGeneral(cmd);

	vk::DescriptorSet set = descriptorSets_[frame.frameIndex].getSet();

	uboBuffers_[frame.frameIndex].upload(&ubos.ubo, sizeof(ubos.ubo));

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
void GodRayPassVk::syncSettings()
{
	uint32_t newFactor = std::max(1u, rs_.resScale.GOD_RAYS);

	if (newFactor == factor_)
		return;

	factor_ = newFactor;
	resize();
} // end of syncSettings()

void GodRayPassVk::updateDescriptorSet(uint32_t frameIndex)
{
	DescriptorSetVk& set = descriptorSets_[frameIndex];
	if (!set.valid())
	{
		return;
	}

	if (outputImage_.valid())
	{
		set.writeStorageImage(
			TO_API_FORM(GodRayPassBinding::OutColorTex),
			outputImage_.view(),
			vk::ImageLayout::eGeneral
		);
	}

	if (inputDepthImage_ && inputDepthImage_->valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(GodRayPassBinding::ForwardDepthTex),
			inputDepthImage_->view(),
			inputDepthImage_->sampler()
		);
	}

	if (inputShadowMapImage_ && inputShadowMapImage_->valid())
	{
		set.writeCombinedImageSampler(
			TO_API_FORM(GodRayPassBinding::ShadowMapTex),
			inputShadowMapImage_->view(),
			inputShadowMapImage_->sampler()
		);
	}
} // end of updateDescriptorSet()

void GodRayPassVk::createAttachment()
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
		vk::ImageUsageFlagBits::eStorage |
		vk::ImageUsageFlagBits::eTransferDst,
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

	outputImage_.setDebugName("GodRayPassVk-OutputImage");
} // end of createAttachment()

void GodRayPassVk::createResources()
{
	for (auto& buffer : uboBuffers_)
	{
		buffer.create(
			sizeof(God_Ray_Constants::GodRayPassUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void GodRayPassVk::createDescriptorSet()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		vk::DescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = TO_API_FORM(GodRayPassBinding::UBO);
		uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding inputDepthBinding{};
		inputDepthBinding.binding = TO_API_FORM(GodRayPassBinding::ForwardDepthTex);
		inputDepthBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputDepthBinding.descriptorCount = 1;
		inputDepthBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding inputShadowBinding{};
		inputShadowBinding.binding = TO_API_FORM(GodRayPassBinding::ShadowMapTex);
		inputShadowBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		inputShadowBinding.descriptorCount = 1;
		inputShadowBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding outputColorBinding{};
		outputColorBinding.binding = TO_API_FORM(GodRayPassBinding::OutColorTex);
		outputColorBinding.descriptorType = vk::DescriptorType::eStorageImage;
		outputColorBinding.descriptorCount = 1;
		outputColorBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		descriptorSets_[i].createLayout({
			uboBinding, 
			inputDepthBinding,
			inputShadowBinding,
			outputColorBinding
			});

		vk::DescriptorPoolSize uboPool{};
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputDepthPool{};
		inputDepthPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputDepthPool.descriptorCount = 1;

		vk::DescriptorPoolSize inputShadowPool{};
		inputShadowPool.type = vk::DescriptorType::eCombinedImageSampler;
		inputShadowPool.descriptorCount = 1;
		
		vk::DescriptorPoolSize outputColorPool{};
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
			TO_API_FORM(GodRayPassBinding::UBO),
			uboBuffers_[i].getBuffer(),
			sizeof(God_Ray_Constants::GodRayPassUBO)
		);

		descriptorSets_[i].setDebugName(
			"GodRayPassVk::DescriptorSet frame " + std::to_string(i)
		);
	} // end for
} // end of createDescriptorSet()

void GodRayPassVk::createPipeline()
{
	ComputePipelineDescVk compDesc{};
	compDesc.computeShader = compShader_->shader();
	compDesc.setLayouts = { descriptorSets_[0].getLayout() };

	computePipeline_.create(compDesc);

	computePipeline_.setDebugName("GodRayPassVk::Pipeline");
} // end of createPipeline()