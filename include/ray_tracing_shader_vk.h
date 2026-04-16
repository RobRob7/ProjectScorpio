#ifndef RAY_TRACING_SHADER_VK_H
#define RAY_TRACING_SHADER_VK_H

#include <vulkan/vulkan.hpp>

#include <string_view>
#include <vector>
#include <cstdint>

class RayTracingShaderModuleVk
{
public:
	RayTracingShaderModuleVk(
		vk::Device device, 
		std::string_view rayGenPathFile, 
		std::string_view missPathFile,
		std::string_view closestHitPathFile
	);
	~RayTracingShaderModuleVk() noexcept;

	RayTracingShaderModuleVk(const RayTracingShaderModuleVk&) = delete;
	RayTracingShaderModuleVk& operator=(const RayTracingShaderModuleVk&) = delete;

	RayTracingShaderModuleVk(RayTracingShaderModuleVk&& other) noexcept = default;
	RayTracingShaderModuleVk& operator=(RayTracingShaderModuleVk&& other) noexcept = default;

	vk::ShaderModule rayGenShader() const noexcept { return rayGenShaderModule_.get(); }
	vk::ShaderModule missShader() const noexcept { return missShaderModule_.get(); }
	vk::ShaderModule closestHitShader() const noexcept { return closestHitShaderModule_.get(); }

private:
	vk::UniqueShaderModule createShaderModule(const std::vector<uint32_t>& code);
private:
	vk::Device device_{};
	vk::UniqueShaderModule rayGenShaderModule_{};
	vk::UniqueShaderModule missShaderModule_{};
	vk::UniqueShaderModule closestHitShaderModule_{};
};

#endif
