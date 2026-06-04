#include "rtao_pass_vk.h"

#include "bindings.h"

#include "render_settings.h"

#include "vulkan_main.h"
#include "frame_context_vk.h"

#include "ray_tracing_shader_vk.h"

#include <vector>
#include <algorithm>

//--- PUBLIC ---//
RTAOPassVk::RTAOPassVk(
	VulkanMain& vk,
	const RenderSettings& rs,
	const std::vector<AccelerationStructureVk>& tlas
)
	: vk_(vk),
	rs_(rs),
	tlas_(tlas),
	outColorImage_(vk),
	pipeline_(vk),
	sbt_(vk)
{
	factor_ = std::max(1u, rs_.resScale.RTAO);

	rayGenUBOs_.reserve(vk_.getMaxFramesInFlight());
	rayGenDescriptorSets_.reserve(vk_.getMaxFramesInFlight());

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		rayGenUBOs_.emplace_back(vk_);
		rayGenDescriptorSets_.emplace_back(vk_);
	} // end for
} // end of constructor

RTAOPassVk::~RTAOPassVk() = default;

void RTAOPassVk::init()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	width_ = std::max(1u, (extent.width + factor_ - 1) / factor_);
	height_ = std::max(1u, (extent.height + factor_ - 1) / factor_);

	shader_ = std::make_unique<RayTracingShaderModuleVk>(
		vk_.getDevice(),
		"raytracing/rtao/rtao_raygen.rgen.spv",
		"raytracing/rtao/rtao_miss.rmiss.spv",
		std::vector<HitGroupFilePath>{
			{
				"",
				"raytracing/rtao/rtao_anyhit.rahit.spv"
			}
		}
	);

	createOutputImage();
	createResources();
	createDescriptorSet();
	createPipeline();
	createSBT();
} // end of init()

void RTAOPassVk::resize()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	if (extent.width <= 0 || extent.height <= 0) return;

	uint32_t newWidth = std::max(1u, (extent.width + factor_ - 1) / factor_);
	uint32_t newHeight = std::max(1u, (extent.height + factor_ - 1) / factor_);

	if (newWidth == width_ && newHeight == height_)
		return;

	width_ = newWidth;
	height_ = newHeight;

	const uint32_t retireFrame = vk_.getPrevFrameIndex();

	vk_.retireImage(retireFrame, std::move(outColorImage_));

	outColorImage_ = ImageVk(vk_);

	createOutputImage();
	updateDescriptorSet(vk_.currentFrameIndex());
} // end of resize()

void RTAOPassVk::render(
	const RTAOPassUBOs& ubos,
	const FrameContext& frame
)
{
	syncSettings();

	if (!outColorImage_.valid()||
		!tlas_[frame.frameIndex].valid() ||
		!normalTex_ || !normalTex_->valid() ||
		!depthTex_ || !depthTex_->valid())
	{
		return;
	}

	updateDescriptorSet(frame.frameIndex);

	vk::CommandBuffer cmd = frame.cmd;

	cmd.beginDebugUtilsLabelEXT({ "RTAOPassVk::cmd" });

	outColorImage_.transitionToGeneral(cmd);

	std::vector<vk::DescriptorSet> sets = {
		rayGenDescriptorSets_[frame.frameIndex].getSet()
	};

	cmd.bindPipeline(
		vk::PipelineBindPoint::eRayTracingKHR,
		pipeline_.getPipeline()
	);

	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eRayTracingKHR,
		pipeline_.getLayout(),
		0,
		sets.size(),
		sets.data(),
		0,
		nullptr
	);

	rayGenUBOs_[frame.frameIndex].upload(&ubos.rayGenData, sizeof(ubos.rayGenData));

	cmd.traceRaysKHR(
		&sbt_.rayGenRegion(),
		&sbt_.missRegion(),
		&sbt_.hitRegion(),
		&sbt_.callableRegion(),
		width_,
		height_,
		1
	);

	outColorImage_.transitionToShaderRead(cmd);

	cmd.endDebugUtilsLabelEXT();
} // end of render()


//--- PRIVATE ---//
void RTAOPassVk::syncSettings()
{
	uint32_t newFactor = std::max(1u, rs_.resScale.RTAO);

	if (newFactor == factor_)
		return;

	factor_ = newFactor;
	resize();
} // end of syncSettings()

void RTAOPassVk::updateDescriptorSet(uint32_t frameIndex)
{
	// RAYGEN SET
	{
		DescriptorSetVk& set = rayGenDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		set.writeStorageImage(
			TO_API_FORM(RTAORayGenBinding::OutColorImage),
			outColorImage_.view(),
			vk::ImageLayout::eGeneral
		);

		if (tlas_[frameIndex].valid())
		{
			set.writeAccelerationStructure(
				TO_API_FORM(RTAORayGenBinding::TLAS),
				tlas_[frameIndex].handle()
			);
		}

		if (rayGenUBOs_[frameIndex].getBuffer())
		{
			set.writeUniformBuffer(
				TO_API_FORM(RTAORayGenBinding::UBO),
				rayGenUBOs_[frameIndex].getBuffer(),
				sizeof(RTAO_Constants::RayGenUBO)
			);
		}

		if (normalTex_ && normalTex_->valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(RTAORayGenBinding::NormalTex),
				normalTex_->view(),
				normalTex_->sampler()
			);
		}

		if (depthTex_ && depthTex_->valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(RTAORayGenBinding::DepthTex),
				depthTex_->view(),
				depthTex_->sampler()
			);
		}
	}
} // end of updateDescriptorSet()

void RTAOPassVk::createOutputImage()
{
	// output color image
	outColorImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		outImageFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eStorage |
		vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	outColorImage_.createImageView(
		outColorImage_.format(),
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	outColorImage_.createSampler(
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		false
	);

	outColorImage_.setDebugName("RTAOPassVk-ColorImage");
} // end of createOutputImage()

void RTAOPassVk::createResources()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		rayGenUBOs_[i].create(
			sizeof(RTAO_Constants::RayGenUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void RTAOPassVk::createDescriptorSet()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		// RAYGEN DS + POOL
		{
			vk::DescriptorSetLayoutBinding outColorBinding{};
			outColorBinding.binding = TO_API_FORM(RTAORayGenBinding::OutColorImage);
			outColorBinding.descriptorType = vk::DescriptorType::eStorageImage;
			outColorBinding.descriptorCount = 1;
			outColorBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			vk::DescriptorSetLayoutBinding tlasBinding{};
			tlasBinding.binding = TO_API_FORM(RTAORayGenBinding::TLAS);
			tlasBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
			tlasBinding.descriptorCount = 1;
			tlasBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(RTAORayGenBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;
			
			vk::DescriptorSetLayoutBinding normalTexBinding{};
			normalTexBinding.binding = TO_API_FORM(RTAORayGenBinding::NormalTex);
			normalTexBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			normalTexBinding.descriptorCount = 1;
			normalTexBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			vk::DescriptorSetLayoutBinding depthTexBinding{};
			depthTexBinding.binding = TO_API_FORM(RTAORayGenBinding::DepthTex);
			depthTexBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			depthTexBinding.descriptorCount = 1;
			depthTexBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			rayGenDescriptorSets_[i].createLayout({
				outColorBinding,
				tlasBinding,
				uboBinding,
				normalTexBinding,
				depthTexBinding
				});

			vk::DescriptorPoolSize outColorPool{};
			outColorPool.type = vk::DescriptorType::eStorageImage;
			outColorPool.descriptorCount = 1;

			vk::DescriptorPoolSize tlasPool{};
			tlasPool.type = vk::DescriptorType::eAccelerationStructureKHR;
			tlasPool.descriptorCount = 1;

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize normalTexPool{};
			normalTexPool.type = vk::DescriptorType::eCombinedImageSampler;
			normalTexPool.descriptorCount = 1;

			vk::DescriptorPoolSize depthTexPool{};
			depthTexPool.type = vk::DescriptorType::eCombinedImageSampler;
			depthTexPool.descriptorCount = 1;

			rayGenDescriptorSets_[i].createPool({
				outColorPool,
				tlasPool,
				uboPool,
				normalTexPool,
				depthTexPool
				});
			rayGenDescriptorSets_[i].allocate();

			rayGenDescriptorSets_[i].setDebugName(
				"RTAOPassVk-RayGen::DescriptorSet frame " + std::to_string(i)
			);
		}
	} // end for
} // end of createDescriptorSet()

void RTAOPassVk::createPipeline()
{
	RayTracingPipelineDescVk desc{};
	desc.rayGenShader = shader_->rayGenShader();
	desc.missShader = shader_->missShader();

	const std::vector<HitGroupShaderModules>& hitGroupShaderModules = shader_->hitGroupShaders();
	desc.hitGroups.reserve(hitGroupShaderModules.size());
	for (const HitGroupShaderModules& hg : hitGroupShaderModules)
	{
		desc.hitGroups.push_back({
			hg.closestHitShaderModule.get(),
			hg.anyHitShaderModule.get()
			});
	} // end for

	desc.setLayouts =
	{
		{0, rayGenDescriptorSets_[0].getLayout()}
	};

	desc.maxRecursionDepth = 1;

	pipeline_.create(desc);

	pipeline_.setDebugName("RTAOPassVk::Pipeline");
} // end of createPipeline()

void RTAOPassVk::createSBT()
{
	sbt_.create(
		pipeline_.getPipeline(),
		3,
		0,
		1,
		{ 2 }
	);
} // end of createSBT()
