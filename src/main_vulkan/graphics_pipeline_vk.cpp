#include "graphics_pipeline_vk.h"

#include "vulkan_main.h"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <stdexcept>

//--- PUBLIC ---//
GraphicsPipelineVk::GraphicsPipelineVk(VulkanMain& vk)
	: vk_(vk)
{
} // end of constructor

GraphicsPipelineVk::~GraphicsPipelineVk() = default;

void GraphicsPipelineVk::setDebugName(const std::string& name)
{
	if (!pipeline_) return;

	vk_.setDebugName(
		vk::ObjectType::ePipeline,
		reinterpret_cast<uint64_t>(static_cast<VkPipeline>(pipeline_.get())),
		name
	);
} // end of setDebugName()

void GraphicsPipelineVk::create(const GraphicsPipelineDescVk& desc)
{
	destroy();

	vk::Device device = vk_.getDevice();

	if (!desc.vertShader || !desc.fragShader)
	{
		throw std::runtime_error("GraphicsPipelineVk::create - need vertex AND fragment shaders!");
	}

	// shader stages
	vk::PipelineShaderStageCreateInfo stages[2]{};
	stages[0].stage = vk::ShaderStageFlagBits::eVertex;
	stages[0].module = desc.vertShader;
	stages[0].pName = "main";

	stages[1].stage = vk::ShaderStageFlagBits::eFragment;
	stages[1].module = desc.fragShader;
	stages[1].pName = "main";

	// vertex input
	vk::PipelineVertexInputStateCreateInfo vi{};
	vi.vertexBindingDescriptionCount = desc.vertexAttributes.empty() ? 0 : 1;
	vi.pVertexBindingDescriptions = desc.vertexAttributes.empty() ? nullptr : &desc.vertexBinding;
	vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.vertexAttributes.size());
	vi.pVertexAttributeDescriptions = desc.vertexAttributes.empty() ? nullptr : desc.vertexAttributes.data();

	// input assembly
	vk::PipelineInputAssemblyStateCreateInfo ia{};
	ia.topology = desc.topology;

	// viewport / scissor
	vk::PipelineViewportStateCreateInfo vp{};
	vp.viewportCount = 1;
	vp.scissorCount = 1;

	// rasterizer
	vk::PipelineRasterizationStateCreateInfo rs{};
	rs.polygonMode = desc.polygonMode;
	rs.cullMode = desc.cullMode;
	rs.frontFace = desc.frontFace;
	rs.lineWidth = desc.lineWidth;

	// multisample
	vk::PipelineMultisampleStateCreateInfo ms{};
	ms.rasterizationSamples = desc.rasterSamples;

	// color blend
	vk::PipelineColorBlendAttachmentState cba{};
	cba.blendEnable = desc.blendEnable;
	cba.colorWriteMask = desc.colorWriteMask;

	vk::PipelineColorBlendStateCreateInfo cb{};
	const bool hasColorAttachment = (desc.colorFormat != vk::Format::eUndefined);
	cb.attachmentCount = hasColorAttachment ? 1u : 0u;
	cb.pAttachments = hasColorAttachment ? &cba : nullptr;

	// depth
	vk::PipelineDepthStencilStateCreateInfo ds{};
	ds.depthTestEnable = desc.depthTestEnable;
	ds.depthWriteEnable = desc.depthWriteEnable;
	ds.depthCompareOp = desc.depthCompareOp;

	// dynamic state
	vk::PipelineDynamicStateCreateInfo dyn{};
	dyn.dynamicStateCount = static_cast<uint32_t>(desc.dynamicStates.size());
	dyn.pDynamicStates = desc.dynamicStates.data();

	// pipeline layout
	vk::PipelineLayoutCreateInfo pli{};
	pli.setLayoutCount = static_cast<uint32_t>(desc.setLayouts.size());
	pli.pSetLayouts = desc.setLayouts.data();
	pli.pushConstantRangeCount = static_cast<uint32_t>(desc.pushConstantRanges.size());
	pli.pPushConstantRanges = desc.pushConstantRanges.empty() ? nullptr : desc.pushConstantRanges.data();

	{
		vk::ResultValue rv = device.createPipelineLayoutUnique(pli);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"GraphicsPipelineVk::create - createPipelineLayoutUnique failed: " +
				vk::to_string(rv.result)
			);
		}
		layout_ = std::move(rv.value);
	}

	// dynamic rendering info
	vk::PipelineRenderingCreateInfo rendering{};
	rendering.colorAttachmentCount = hasColorAttachment ? 1u : 0u;
	rendering.pColorAttachmentFormats = hasColorAttachment ? &desc.colorFormat : nullptr;
	rendering.depthAttachmentFormat = desc.depthFormat;

	// pipeline creation
	vk::GraphicsPipelineCreateInfo gp{};
	gp.pNext = &rendering;
	gp.stageCount = 2;
	gp.pStages = stages;

	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vp;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pColorBlendState = &cb;
	gp.pDepthStencilState = &ds;
	gp.pDynamicState = &dyn;

	gp.layout = layout_.get();
	gp.renderPass = nullptr;
	gp.subpass = 0;

	{
		vk::ResultValue rv = device.createGraphicsPipelineUnique(nullptr, gp);
		if (rv.result != vk::Result::eSuccess)
		{
			throw std::runtime_error(
				"GraphicsPipelineVk::create - createGraphicsPipelineUnique failed: " +
				vk::to_string(rv.result)
			);
		}

		pipeline_ = std::move(rv.value);
	}
} // end of create()


//--- PRIVATE ---//
void GraphicsPipelineVk::destroy()
{
	pipeline_.reset();
	layout_.reset();
} // end of destroy()