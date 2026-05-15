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

	if (!desc.rayGenShader || 
		!desc.missShader || 
		desc.hitGroups.empty())
	{
		throw std::runtime_error("RayTracingPipelineVk::create - need at least (raygen + miss + one hit group) shaders!");
	}


	for (HitGroupDescVk hitGroup : desc.hitGroups)
	{
		if (!hitGroup.closestHitShader &&
			!hitGroup.anyHitShader)
		{
			throw std::runtime_error(
				"RayTracingPipelineVk::create - hit group has no shaders!"
			);
		}
	} // end for

	if (desc.maxRecursionDepth < 1)
	{
		throw std::runtime_error("RayTracingPipelineVk::create - maxRecursionDepth must be at least 1!");
	}

	// get total number of shaders
	size_t totalStages = 2;

	for (const HitGroupDescVk& hg : desc.hitGroups)
	{
		if (hg.closestHitShader) totalStages++;
		if (hg.anyHitShader) totalStages++;
	} // end for

	// shader stages
	std::vector<vk::PipelineShaderStageCreateInfo> stages;
	stages.reserve(totalStages);

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

	// shader groups
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups;
	groups.reserve(totalStages);

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

	for (const HitGroupDescVk& hitGroup : desc.hitGroups)
	{
		uint32_t closestIndex = vk::ShaderUnusedKHR;
		uint32_t anyHitIndex = vk::ShaderUnusedKHR;

		if (hitGroup.closestHitShader)
		{
			closestIndex = static_cast<uint32_t>(stages.size());

			vk::PipelineShaderStageCreateInfo closestHitStage{};
			closestHitStage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
			closestHitStage.module = hitGroup.closestHitShader;
			closestHitStage.pName = "main";
			stages.push_back(closestHitStage);
		}

		if (hitGroup.anyHitShader)
		{
			anyHitIndex = static_cast<uint32_t>(stages.size());

			vk::PipelineShaderStageCreateInfo anyHitStage{};
			anyHitStage.stage = vk::ShaderStageFlagBits::eAnyHitKHR;
			anyHitStage.module = hitGroup.anyHitShader;
			anyHitStage.pName = "main";
			stages.push_back(anyHitStage);
		}

		vk::RayTracingShaderGroupCreateInfoKHR group{};
		group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
		group.generalShader = vk::ShaderUnusedKHR;
		group.closestHitShader = closestIndex;
		group.anyHitShader = anyHitIndex;
		group.intersectionShader = vk::ShaderUnusedKHR;

		groups.push_back(group);
	} // end for

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