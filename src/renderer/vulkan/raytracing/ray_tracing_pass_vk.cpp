#include "ray_tracing_pass_vk.h"

#include "utils_vk.h"
#include "vulkan_main.h"
#include "frame_context_vk.h"

#include "ray_tracing_shader_vk.h"

//--- PUBLIC ---//
RayTracingPassVk::RayTracingPassVk(VulkanMain& vk)
	: vk_(vk),
	outputImage_(vk),
	descriptorSet_(vk),
	pipeline_(vk),
	sbt_(vk)
{
} // end of constructor

RayTracingPassVk::~RayTracingPassVk() = default;

void RayTracingPassVk::init()
{
	shader_ = std::make_unique<RayTracingShaderModuleVk>(
		vk_.getDevice(),
		"raytracing/raygen.rgen.spv",
		"raytracing/miss.rmiss.spv",
		"raytracing/closesthit.rchit.spv"
	);

	createOutputImage();
	createResources();
	createDescriptorSet();
	createPipeline();
	createSBT();
} // end of init()

void RayTracingPassVk::resize()
{
	outputImage_.destroy();
	createOutputImage();
	updateDescriptorSet();
} // end of resize()

void RayTracingPassVk::render(const FrameContext& frame)
{
	vk::CommandBuffer cmd = frame.cmd;

	vk::DescriptorSet set = descriptorSet_.getSet();

	cmd.bindPipeline(
		vk::PipelineBindPoint::eRayTracingKHR,
		pipeline_.getPipeline()
	);

	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eRayTracingKHR,
		pipeline_.getLayout(),
		0,
		1,
		&set,
		0,
		nullptr
	);

	vk::Extent2D extent = vk_.getSwapChainExtent();

	cmd.traceRaysKHR(
		&sbt_.rayGenRegion(),
		&sbt_.missRegion(),
		&sbt_.hitRegion(),
		&sbt_.callableRegion(),
		extent.width,
		extent.height,
		1
	);
} // end of render()

void RayTracingPassVk::setTopLevelAS(vk::AccelerationStructureKHR tlas)
{
	topLevelAS_ = tlas;

	updateDescriptorSet();
} // end of setTopLevelAS()

void RayTracingPassVk::updateDescriptorSet()
{
	if (!descriptorSet_.valid())
	{
		return;
	}

	descriptorSet_.writeStorageImage(
		0,
		outputImage_.view(),
		vk::ImageLayout::eGeneral
	);

	if (topLevelAS_)
	{
		descriptorSet_.writeAccelerationStructure(1, topLevelAS_);
	}
} // end of updateDescriptorSet()


//--- PRIVATE ---//
void RayTracingPassVk::createOutputImage()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();

	outputImage_.createImage(
		extent.width,
		extent.height,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eStorage |
		vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	outputImage_.createImageView(
		outputImage_.format(),
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	outputImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		false
	);

	VkUtils::TransitionImageLayoutImmediate(
		vk_,
		outputImage_.image(),
		vk::ImageAspectFlagBits::eColor,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eGeneral,
		1,
		1
	);
	outputLayout_ = vk::ImageLayout::eGeneral;
} // end of createOutputImage

void RayTracingPassVk::createResources()
{

} // end of createResources()

void RayTracingPassVk::createDescriptorSet()
{
	descriptorSet_.destroy();

	vk::DescriptorSetLayoutBinding outputBinding{};
	outputBinding.binding = 0;
	outputBinding.descriptorType = vk::DescriptorType::eStorageImage;
	outputBinding.descriptorCount = 1;
	outputBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

	vk::DescriptorSetLayoutBinding tlasBinding{};
	tlasBinding.binding = 1;
	tlasBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
	tlasBinding.descriptorCount = 1;
	tlasBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

	descriptorSet_.createLayout({ outputBinding, tlasBinding });

	vk::DescriptorPoolSize outputPool{};
	outputPool.type = vk::DescriptorType::eStorageImage;
	outputPool.descriptorCount = 1;

	vk::DescriptorPoolSize tlasPool{};
	tlasPool.type = vk::DescriptorType::eAccelerationStructureKHR;
	tlasPool.descriptorCount = 1;

	descriptorSet_.createPool({ outputPool, tlasPool }, 1);
	descriptorSet_.allocate();

	updateDescriptorSet();
} // end of createDescriptorSet()

void RayTracingPassVk::createPipeline()
{
	RayTracingPipelineDescVk desc{};
	desc.rayGenShader = shader_->rayGenShader();
	desc.missShader = shader_->missShader();
	desc.closestHitShader = shader_->closestHitShader();
	
	desc.setLayouts = { descriptorSet_.getLayout() };
	desc.maxRecursionDepth = 1;

	pipeline_.create(desc);
} // end of createPipeline()

void RayTracingPassVk::createSBT()
{
	sbt_.create(
		pipeline_.getPipeline(),
		3,
		0,
		1,
		2
	);
} // end of createSBT()