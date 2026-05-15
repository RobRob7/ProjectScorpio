#ifndef RAY_TRACING_SHADER_VK_H
#define RAY_TRACING_SHADER_VK_H

#include <vulkan/vulkan.hpp>

#include <string_view>
#include <vector>
#include <cstdint>

struct HitGroupFilePath
{
	std::string_view closestHitPath;
	std::string_view anyHitPath;
};

struct HitGroupShaderModules
{
	vk::UniqueShaderModule closestHitShaderModule;
	vk::UniqueShaderModule anyHitShaderModule;
};

class RayTracingShaderModuleVk
{
public:
	RayTracingShaderModuleVk(
		vk::Device device, 
		std::string_view rayGenPathFile, 
		std::string_view missPathFile,
		std::vector<HitGroupFilePath> hitGroupPathFiles
	);
	~RayTracingShaderModuleVk() noexcept;

	RayTracingShaderModuleVk(const RayTracingShaderModuleVk&) = delete;
	RayTracingShaderModuleVk& operator=(const RayTracingShaderModuleVk&) = delete;

	RayTracingShaderModuleVk(RayTracingShaderModuleVk&& other) noexcept = default;
	RayTracingShaderModuleVk& operator=(RayTracingShaderModuleVk&& other) noexcept = default;

	vk::ShaderModule rayGenShader() const noexcept { return rayGenShaderModule_.get(); }
	vk::ShaderModule missShader() const noexcept { return missShaderModule_.get(); }
	const std::vector<HitGroupShaderModules>& hitGroupShaders() const noexcept { return hitGroupShaderModules_; }

private:
	vk::UniqueShaderModule createShaderModule(const std::vector<uint32_t>& code);
private:
	vk::Device device_{};
	vk::UniqueShaderModule rayGenShaderModule_{};
	vk::UniqueShaderModule missShaderModule_{};
	std::vector<HitGroupShaderModules> hitGroupShaderModules_{};
};

#endif
