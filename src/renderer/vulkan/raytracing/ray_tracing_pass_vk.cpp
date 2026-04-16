#include "ray_tracing_pass_vk.h"

#include "render_inputs.h"

#include "camera.h"

#include "utils_vk.h"
#include "vulkan_main.h"
#include "frame_context_vk.h"

#include "ray_tracing_shader_vk.h"

#include <glm/glm.hpp>

using namespace RayTracing;

//--- PUBLIC ---//
RayTracingPassVk::RayTracingPassVk(VulkanMain& vk)
	: vk_(vk),
	outputImage_(vk),
	pipeline_(vk),
	sbt_(vk)
{
	descriptorSets_.reserve(vk_.getMaxFramesInFlight());
	cameraUBOs_.reserve(vk_.getMaxFramesInFlight());
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

	for (uint32_t i = 0; i < descriptorSets_.size(); ++i)
	{
		updateDescriptorSet(i);
	} // end for
} // end of resize()

void RayTracingPassVk::render(
	const RenderInputs& in,
	const FrameContext& frame,
	const glm::mat4& view,
	const glm::mat4& proj
)
{
	vk::CommandBuffer cmd = frame.cmd;

	vk::DescriptorSet set = descriptorSets_[frame.frameIndex].getSet();

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

	cameraData_.invView = glm::inverse(view);
	cameraData_.invProj = glm::inverse(proj);
	cameraData_.cameraPos = glm::vec4(in.camera->getCameraPosition(), 1.0f);

	cameraUBOs_[frame.frameIndex].upload(&cameraData_, sizeof(cameraData_));

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

void RayTracingPassVk::setTopLevelAS(
	uint32_t frameIndex,
	vk::AccelerationStructureKHR tlas
)
{
	topLevelAS_ = tlas;

	updateDescriptorSet(frameIndex);
} // end of setTopLevelAS()

void RayTracingPassVk::updateDescriptorSet(uint32_t frameIndex)
{
	if (frameIndex >= descriptorSets_.size())
	{
		return;
	}

	DescriptorSetVk& set = descriptorSets_[frameIndex];
	if (!set.valid())
	{
		return;
	}

	set.writeStorageImage(
		0,
		outputImage_.view(),
		vk::ImageLayout::eGeneral
	);

	set.writeUniformBuffer(
		2,
		cameraUBOs_[frameIndex].getBuffer(),
		sizeof(RTCameraUBO)
	);

	if (topLevelAS_)
	{
		set.writeAccelerationStructure(1, topLevelAS_);
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
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		BufferVk ubo(vk_);
		ubo.create(
			sizeof(RTCameraUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);
		cameraUBOs_.push_back(std::move(ubo));
	} // end for
} // end of createResources()

void RayTracingPassVk::createDescriptorSet()
{
	descriptorSets_.clear();

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		DescriptorSetVk set(vk_);

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

		vk::DescriptorSetLayoutBinding cameraUBOBinding{};
		cameraUBOBinding.binding = 2;
		cameraUBOBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		cameraUBOBinding.descriptorCount = 1;
		cameraUBOBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		set.createLayout({ outputBinding, tlasBinding, cameraUBOBinding });

		vk::DescriptorPoolSize outputPool{};
		outputPool.type = vk::DescriptorType::eStorageImage;
		outputPool.descriptorCount = 1;

		vk::DescriptorPoolSize tlasPool{};
		tlasPool.type = vk::DescriptorType::eAccelerationStructureKHR;
		tlasPool.descriptorCount = 1;

		vk::DescriptorPoolSize cameraUBOPool{};
		cameraUBOPool.type = vk::DescriptorType::eUniformBuffer;
		cameraUBOPool.descriptorCount = 1;

		set.createPool({ outputPool, tlasPool, cameraUBOPool }, 1);
		set.allocate();

		descriptorSets_.push_back(std::move(set));
	} // end for

	for (uint32_t i = 0; i < descriptorSets_.size(); ++i)
	{
		updateDescriptorSet(i);
	} // end for
} // end of createDescriptorSet()

void RayTracingPassVk::createPipeline()
{
	RayTracingPipelineDescVk desc{};
	desc.rayGenShader = shader_->rayGenShader();
	desc.missShader = shader_->missShader();
	desc.closestHitShader = shader_->closestHitShader();
	
	desc.setLayouts = { descriptorSets_[0].getLayout()};
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