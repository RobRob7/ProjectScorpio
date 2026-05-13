#include "ray_tracing_pipeline_vk.h"

#include "vulkan_main.h"

#include <stdexcept>

//--- PUBLIC ---//
RayTracingPipelineVk::RayTracingPipelineVk(VulkanMain& vk)
	: vk_(vk)
{
} // end of constructor

RayTracingPipelineVk::~RayTracingPipelineVk() = default;

void RayTracingPipelineVk::setDebugName(const std::string& name)
{
	if (!pipeline_) return;

	vk_.setDebugName(
		vk::ObjectType::ePipeline,
		reinterpret_cast<uint64_t>(static_cast<VkPipeline>(pipeline_.get())),
		name
	);
} // end of setDebugName()

void RayTracingPipelineVk::create(const RayTracingPipelineDescVk& desc)
{
	destroy();

	vk::Device device = vk_.getDevice();

	if (!desc.rayGenShader || !desc.missShader || desc.closestHitShaders.empty())
	{
		throw std::runtime_error("RayTracingPipelineVk::create - need at least (raygen, miss, hit) shaders!");
	}


	for (vk::ShaderModule hitShader : desc.closestHitShaders)
	{
		if (!hitShader)
		{
			throw std::runtime_error("RayTracingPipelineVk::create - closest hit shader is null!");
		}
	} // end for

	if (desc.maxRecursionDepth < 1)
	{
		throw std::runtime_error("RayTracingPipelineVk::create - maxRecursionDepth must be at least 1!");
	}

	// shader stages
	std::vector<vk::PipelineShaderStageCreateInfo> stages;
	stages.reserve(2 + desc.closestHitShaders.size());

	// 0 - raygen
	vk::PipelineShaderStageCreateInfo rayGenStage{};
	rayGenStage.stage = vk::ShaderStageFlagBits::eRaygenKHR;
	rayGenStage.module = desc.rayGenShader;
	rayGenStage.pName = "main";
	stages.push_back(rayGenStage);

	// 1 - miss
	vk::PipelineShaderStageCreateInfo missStage{};
	missStage.stage = vk::ShaderStageFlagBits::eMissKHR;
	missStage.module = desc.missShader;
	missStage.pName = "main";
	stages.push_back(missStage);

	// 2+ - closest-hit shaders
	for (vk::ShaderModule hitShader : desc.closestHitShaders)
	{
		vk::PipelineShaderStageCreateInfo hitStage{};
		hitStage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
		hitStage.module = hitShader;
		hitStage.pName = "main";
		stages.push_back(hitStage);
	} // end for

	// shader groups
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups;
	groups.reserve(2 + desc.closestHitShaders.size());

	// 0 - raygen
	vk::RayTracingShaderGroupCreateInfoKHR rayGenGroup{};
	rayGenGroup.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	rayGenGroup.generalShader = 0;
	rayGenGroup.closestHitShader = vk::ShaderUnusedKHR;
	rayGenGroup.anyHitShader = vk::ShaderUnusedKHR;
	rayGenGroup.intersectionShader = vk::ShaderUnusedKHR;
	groups.push_back(rayGenGroup);

	// 1 - miss
	vk::RayTracingShaderGroupCreateInfoKHR missGroup{};
	missGroup.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	missGroup.generalShader = 1;
	missGroup.closestHitShader = vk::ShaderUnusedKHR;
	missGroup.anyHitShader = vk::ShaderUnusedKHR;
	missGroup.intersectionShader = vk::ShaderUnusedKHR;
	groups.push_back(missGroup);

	// 2+ - closest-hit
	for (uint32_t i = 0; i < static_cast<uint32_t>(desc.closestHitShaders.size()); ++i)
	{
		vk::RayTracingShaderGroupCreateInfoKHR hitGroup{};
		hitGroup.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
		hitGroup.generalShader = vk::ShaderUnusedKHR;
		hitGroup.closestHitShader = 2 + i;
		hitGroup.anyHitShader = vk::ShaderUnusedKHR;
		hitGroup.intersectionShader = vk::ShaderUnusedKHR;
		groups.push_back(hitGroup);
	}

	// pipeline layout
	std::vector<vk::DescriptorSetLayout> ordered;

	// find max set index
	uint32_t maxSet = 0;
	for (const auto& s : desc.setLayouts)
	{
		maxSet = std::max(maxSet, s.setNumber);
	} // end for

	ordered.resize(maxSet + 1);

	for (const auto& s : desc.setLayouts)
	{
		ordered[s.setNumber] = s.layout;
	} // end for

	vk::PipelineLayoutCreateInfo pli{};
	pli.setLayoutCount = static_cast<uint32_t>(ordered.size());
	pli.pSetLayouts = ordered.data();
	pli.pushConstantRangeCount = static_cast<uint32_t>(desc.pushConstantRanges.size());
	pli.pPushConstantRanges = desc.pushConstantRanges.empty() ? nullptr : desc.pushConstantRanges.data();
	{
		vk::ResultValue rv = device.createPipelineLayoutUnique(pli);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"RayTracingPipelineVk::create - createPipelineLayoutUnique failed: " +
				vk::to_string(rv.result)
			);
		}
		layout_ = std::move(rv.value);
	}

	// RT pipeline creation
	vk::RayTracingPipelineCreateInfoKHR rtp{};
	rtp.stageCount = static_cast<uint32_t>(stages.size());
	rtp.pStages = stages.data();
	rtp.groupCount = static_cast<uint32_t>(groups.size());
	rtp.pGroups = groups.data();
	rtp.maxPipelineRayRecursionDepth = desc.maxRecursionDepth;
	rtp.layout = layout_.get();

	{
		vk::ResultValue rv = device.createRayTracingPipelineKHRUnique(nullptr, nullptr, rtp);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"RayTracingPipelineVk::create - createRayTracingPipelineKHRUnique failed: " +
				vk::to_string(rv.result)
			);
		}

		pipeline_ = std::move(rv.value);
	}
} // end of create()

//--- PRIVATE ---//
void RayTracingPipelineVk::destroy()
{
	pipeline_.reset();
	layout_.reset();
} // end of destroy()