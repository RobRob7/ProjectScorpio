#ifndef RAY_TRACING_PIPELINE_VK_H
#define RAY_TRACING_PIPELINE_VK_H

#include <vulkan/vulkan.hpp>

#include <vector>
#include <cstdint>

class VulkanMain;

struct NamedSetLayoutVk
{
	uint32_t setNumber{};
	vk::DescriptorSetLayout layout{};
};


struct RayTracingPipelineDescVk
{
	// shaders
	vk::ShaderModule rayGenShader{};
	vk::ShaderModule missShader{};
	std::vector<vk::ShaderModule> closestHitShaders{};

	// push contant
	std::vector<vk::PushConstantRange> pushConstantRanges;

	// descriptor layouts
	std::vector<NamedSetLayoutVk> setLayouts;

	// max bounces
	uint32_t maxRecursionDepth{ 0 };
};
 
class RayTracingPipelineVk
{
public:
	explicit RayTracingPipelineVk(VulkanMain& vk);
	~RayTracingPipelineVk();

	RayTracingPipelineVk(const RayTracingPipelineVk&) = delete;
	RayTracingPipelineVk& operator=(const RayTracingPipelineVk&) = delete;

	RayTracingPipelineVk(RayTracingPipelineVk&&) noexcept = delete;
	RayTracingPipelineVk& operator=(RayTracingPipelineVk&&) noexcept = delete;

	void setDebugName(const std::string& name);

	void create(const RayTracingPipelineDescVk& desc);

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
