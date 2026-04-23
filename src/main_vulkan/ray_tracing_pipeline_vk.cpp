#include "ray_tracing_pipeline_vk.h"

#include "vulkan_main.h"

#include <stdexcept>
#include <array>

//--- PUBLIC ---//
RayTracingPipelineVk::RayTracingPipelineVk(VulkanMain& vk)
	: vk_(vk)
{
} // end of constructor

RayTracingPipelineVk::~RayTracingPipelineVk() = default;

void RayTracingPipelineVk::create(const RayTracingPipelineDescVk& desc)
{
	destroy();

	vk::Device device = vk_.getDevice();

	if (!desc.rayGenShader || !desc.missShader || !desc.closestHitShader)
	{
		throw std::runtime_error("RayTracingPipelineVk::create - need at least (raygen, miss, hit) shaders!");
	}

	if (desc.maxRecursionDepth < 1)
	{
		throw std::runtime_error("RayTracingPipelineVk::create - maxRecursionDepth must be at least 1!");
	}

	// shader stages
	std::array<vk::PipelineShaderStageCreateInfo, 3> stages{};

	stages[0].stage = vk::ShaderStageFlagBits::eRaygenKHR;
	stages[0].module = desc.rayGenShader;
	stages[0].pName = "main";

	stages[1].stage = vk::ShaderStageFlagBits::eMissKHR;
	stages[1].module = desc.missShader;
	stages[1].pName = "main";

	stages[2].stage = vk::ShaderStageFlagBits::eClosestHitKHR;
	stages[2].module = desc.closestHitShader;
	stages[2].pName = "main";

	// shader groups
	std::array<vk::RayTracingShaderGroupCreateInfoKHR, 3> groups{};

	// raygen group
	groups[0].type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	groups[0].generalShader = 0;
	groups[0].closestHitShader = vk::ShaderUnusedKHR;
	groups[0].anyHitShader = vk::ShaderUnusedKHR;
	groups[0].intersectionShader = vk::ShaderUnusedKHR;

	// miss group
	groups[1].type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	groups[1].generalShader = 1;
	groups[1].closestHitShader = vk::ShaderUnusedKHR;
	groups[1].anyHitShader = vk::ShaderUnusedKHR;
	groups[1].intersectionShader = vk::ShaderUnusedKHR;

	// triangles hit group
	groups[2].type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
	groups[2].generalShader = vk::ShaderUnusedKHR;
	groups[2].closestHitShader = 2;
	groups[2].anyHitShader = vk::ShaderUnusedKHR;
	groups[2].intersectionShader = vk::ShaderUnusedKHR;

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