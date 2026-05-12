#ifndef COMPUTE_SHADER_VK_H
#define COMPUTE_SHADER_VK_H

#include <vulkan/vulkan.hpp>

#include <string_view>

class ComputeShaderModuleVk
{
public:
	ComputeShaderModuleVk(
		vk::Device device,
		std::string_view compShaderPath
	);
	~ComputeShaderModuleVk() noexcept;

	ComputeShaderModuleVk(const ComputeShaderModuleVk&) = delete;
	ComputeShaderModuleVk& operator=(const ComputeShaderModuleVk&) = delete;

	ComputeShaderModuleVk(ComputeShaderModuleVk&& other) noexcept = default;
	ComputeShaderModuleVk& operator=(ComputeShaderModuleVk&& other) noexcept = default;

	vk::ShaderModule shader() const noexcept { return compShaderModule_.get(); }

private:
	vk::UniqueShaderModule createShaderModule(const std::vector<uint32_t>& code);
private:
	vk::Device device_{};
	vk::UniqueShaderModule compShaderModule_{};
};

#endif
