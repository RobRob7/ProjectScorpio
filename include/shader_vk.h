#ifndef SHADER_MODULE_VK_H
#define SHADER_MODULE_VK_H

#include <vulkan/vulkan.hpp>

#include <string_view>
#include <vector>
#include <cstdint>

class ShaderModuleVk
{
public:
	ShaderModuleVk(
		vk::Device device, 
		std::string_view vertexPathFile, 
		std::string_view fragPathFile
	);
	~ShaderModuleVk();

	ShaderModuleVk(const ShaderModuleVk&) = delete;
	ShaderModuleVk& operator=(const ShaderModuleVk&) = delete;

	ShaderModuleVk(ShaderModuleVk&& other) noexcept = default;
	ShaderModuleVk& operator=(ShaderModuleVk&& other) noexcept = default;

	vk::ShaderModule vertShader() const noexcept { return vertShaderModule_.get(); }
	vk::ShaderModule fragShader() const noexcept { return fragShaderModule_.get(); }

private:
	vk::UniqueShaderModule createShaderModule(const std::vector<uint32_t>& code);
private:
	vk::Device device_{};
	vk::UniqueShaderModule vertShaderModule_{};
	vk::UniqueShaderModule fragShaderModule_{};
};

#endif
