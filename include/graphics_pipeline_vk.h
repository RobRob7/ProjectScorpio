#ifndef GRAPHICS_PIPELINE_VK_H
#define GRAPHICS_PIPELINE_VK_H

#include <vulkan/vulkan.hpp>

#include <vector>
#include <string>

class VulkanMain;

struct GraphicsPipelineDescVk
{
	// shaders
	vk::ShaderModule vertShader{};
	vk::ShaderModule fragShader{};

	// push contant
	std::vector<vk::PushConstantRange> pushConstantRanges;

	// descriptor layouts
	std::vector<vk::DescriptorSetLayout> setLayouts;

	// vertex input
	vk::VertexInputBindingDescription vertexBinding{};
	std::vector<vk::VertexInputAttributeDescription> vertexAttributes;

	// primitive assembly
	vk::PrimitiveTopology topology{ vk::PrimitiveTopology::eTriangleList };

	// rasterizer
	vk::PolygonMode polygonMode{ vk::PolygonMode::eFill };
	vk::CullModeFlags cullMode{ vk::CullModeFlagBits::eBack };
	vk::FrontFace frontFace{ vk::FrontFace::eCounterClockwise };
	float lineWidth{ 1.0f };

	// multisampling
	vk::SampleCountFlagBits rasterSamples{ vk::SampleCountFlagBits::e1 };

	// blending
	bool blendEnable{ false };
	vk::ColorComponentFlags colorWriteMask =
		vk::ColorComponentFlagBits::eR |
		vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB |
		vk::ColorComponentFlagBits::eA;

	// depth
	bool depthTestEnable{ true };
	bool depthWriteEnable{ true };
	vk::CompareOp depthCompareOp{ vk::CompareOp::eLessOrEqual };

	// render target + depth formats
	vk::Format colorFormat{ vk::Format::eUndefined };
	vk::Format depthFormat{ vk::Format::eUndefined };

	// dynamic state
	std::vector<vk::DynamicState> dynamicStates =
	{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
};

class GraphicsPipelineVk
{
public:
	explicit GraphicsPipelineVk(VulkanMain& vk);
	~GraphicsPipelineVk();

	GraphicsPipelineVk(const GraphicsPipelineVk&) = delete;
	GraphicsPipelineVk& operator=(const GraphicsPipelineVk&) = delete;

	GraphicsPipelineVk(GraphicsPipelineVk&&) noexcept = default;
	GraphicsPipelineVk& operator=(GraphicsPipelineVk&&) noexcept = default;

	void setDebugName(const std::string& name);

	void create(const GraphicsPipelineDescVk& desc);

	bool valid() const { return static_cast<bool>(pipeline_); }

	vk::Pipeline getPipeline() const { return pipeline_.get(); }
	vk::PipelineLayout getLayout() const { return layout_.get(); }

private:
	void destroy();
private:
	VulkanMain& vk_;

	vk::UniquePipeline pipeline_{};
	vk::UniquePipelineLayout layout_{};
};

#endif