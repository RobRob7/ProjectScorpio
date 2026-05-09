#ifndef COMPUTE_PIPELINE_VK_H
#define COMPUTE_PIPELINE_VK_H

#include <vulkan/vulkan.hpp>

#include <vector>

class VulkanMain;

struct ComputePipelineDescVk
{
	// shader
	vk::ShaderModule computeShader{};

	// push constant
	std::vector<vk::PushConstantRange> pushConstantRanges;

	// descriptor layouts
	std::vector<vk::DescriptorSetLayout> setLayouts;
};

class ComputePipelineVk
{
public:
	explicit ComputePipelineVk(VulkanMain& vk);
	~ComputePipelineVk();

	ComputePipelineVk(const ComputePipelineVk&) = delete;
	ComputePipelineVk& operator=(const ComputePipelineVk&) = delete;

	ComputePipelineVk(ComputePipelineVk&&) noexcept = delete;
	ComputePipelineVk& operator=(ComputePipelineVk&&) noexcept = delete;

	void create(const ComputePipelineDescVk& desc);

	bool valid() const { return static_cast<bool>(pipeline_); }

	vk::Pipeline getPipeline() const { return *pipeline_; }
	vk::PipelineLayout getLayout() const { return *layout_; }

private:
	void destroy();
private:
	VulkanMain& vk_;

	vk::UniquePipeline pipeline_{};
	vk::UniquePipelineLayout layout_{};
};

#endif
