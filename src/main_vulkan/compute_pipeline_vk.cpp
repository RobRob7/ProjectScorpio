#include "compute_pipeline_vk.h"

#include "vulkan_main.h"

#include <stdexcept>
#include <utility>

//--- PUBLIC ---//
ComputePipelineVk::ComputePipelineVk(VulkanMain& vk)
	: vk_(vk)
{
} // end of constructor

ComputePipelineVk::~ComputePipelineVk() = default;

void ComputePipelineVk::setDebugName(const std::string& name)
{
	if (!pipeline_) return;

	vk_.setDebugName(
		vk::ObjectType::ePipeline,
		reinterpret_cast<uint64_t>(static_cast<VkPipeline>(pipeline_.get())),
		name
	);
} // end of setDebugName()

void ComputePipelineVk::create(const ComputePipelineDescVk& desc)
{
	destroy();

	vk::Device device = vk_.getDevice();

	if (!desc.computeShader)
	{
		throw std::runtime_error("ComputePipelineVk::create - need compute shader!");
	}

	// shader stage
	vk::PipelineShaderStageCreateInfo sci{};
	sci.stage = vk::ShaderStageFlagBits::eCompute;
	sci.module = desc.computeShader;
	sci.pName = "main";

	// pipeline layout
	vk::PipelineLayoutCreateInfo plci{};
	plci.setLayoutCount = static_cast<uint32_t>(desc.setLayouts.size());
	plci.pSetLayouts = desc.setLayouts.data();
	plci.pushConstantRangeCount = static_cast<uint32_t>(desc.pushConstantRanges.size());
	plci.pPushConstantRanges = desc.pushConstantRanges.empty() ? 0 : desc.pushConstantRanges.data();

	{
		vk::ResultValue rv = device.createPipelineLayoutUnique(plci);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"ComputePipelineVk::create - createPipelineLayoutUnique failed: " +
				vk::to_string(rv.result)
			);
		}
		layout_ = std::move(rv.value);
	}

	// pipeline creation
	vk::ComputePipelineCreateInfo cpi{};
	cpi.stage = sci;
	cpi.layout = *layout_;
	
	{
		vk::ResultValue rv = device.createComputePipelineUnique(nullptr, cpi);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"ComputePipelineVk::create - createComputePipelineUnique failed: " +
				vk::to_string(rv.result)
			);
		}
		pipeline_ = std::move(rv.value);
	}
} // end of create()


//--- PRIVATE ---//
void ComputePipelineVk::destroy()
{
	pipeline_.reset();
	layout_.reset();
} // end of destroy()
