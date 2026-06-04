#ifndef CUBEMAP_VK_H
#define CUBEMAP_VK_H

#include "constants.h"

#include "i_cubemap.h"

#include "shader_vk.h"
#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"
#include "texture_cubemap_vk.h"

#include <glm/glm.hpp>

#include <memory>
#include <string_view>
#include <array>
#include <cstdint>
#include <vector>

class VulkanMain;

class CubemapVk final : public ICubemap
{
public:
    CubemapVk(
		VulkanMain& vk, 
		const std::array<std::string_view, 6>& textures = Cubemap_Constants::DEFAULT_FACES
	);
    ~CubemapVk() override;

    void init() override;

    void render(
		const FrameContext* frame,
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& sunDir,
		const float time
	) override;
	void renderOffscreen(
		const FrameContext* frame,
		const glm::mat4& view,
		const glm::mat4& projection,
		uint32_t width,
		uint32_t height,
		const glm::vec3& sunDir,
		const float time
	);

	TextureCubemapVk& getNightTexture() { return cubemapTextureNight_; }
	TextureCubemapVk& getDayTexture() { return cubemapTextureDay_; }

private:
	void createVertexBuffer();
	void createUBOs();
	void createDescriptorSets();
	void createPipeline();
private:
	VulkanMain& vk_;

	std::array<std::string_view, 6> faces_;

	std::unique_ptr<ShaderModuleVk> shader_;

	TextureCubemapVk cubemapTextureNight_;
	TextureCubemapVk cubemapTextureDay_;

	BufferVk vertexBuffer_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<BufferVk> uboBuffersOffscreen_;

	uint32_t vertexCount_{};

	std::vector<DescriptorSetVk> descriptorSets_;
	std::vector<DescriptorSetVk> descriptorSetsOffscreen_;

	GraphicsPipelineVk pipeline_;
	GraphicsPipelineVk pipelineOffscreen_;
};

#endif
