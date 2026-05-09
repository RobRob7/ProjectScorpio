#ifndef WATER_PASS_VK_H
#define WATER_PASS_VK_H

#include "image_vk.h"
#include "texture_2d_vk.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct RenderInputs;
class ChunkPassVk;
struct FrameContext;
struct RenderSettings;

class WaterPassVk
{
public:
	WaterPassVk(
		VulkanMain& vk,
		const ImageVk& shadowMapTex
	);
	~WaterPassVk();

	void init();
	void resize();

	void renderOffscreen(
		const RenderSettings& rs,
		const FrameContext& frame,
		ChunkPassVk& chunk,
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	);

	void renderWater(
		const FrameContext& frame,
		const RenderSettings& rs,
		const RenderInputs& in,
		const glm::mat4& view,
		const glm::mat4& proj,
		const glm::mat4& lightSpaceMatrix,
		int width, int height
	);

	ImageVk& getReflColorImage() { return reflColorImage_; }
	ImageVk& getReflDepthImage() { return reflDepthImage_; }

	ImageVk& getRefrColorImage() { return refrColorImage_; }
	ImageVk& getRefrDepthImage() { return refrDepthImage_; }

private:
	void createAttachments();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
	void waterPass(
		const RenderSettings& rs,
		const FrameContext& frame,
		ChunkPassVk& chunk, 
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	);
	void waterReflectionPass(
		const RenderSettings& rs,
		const FrameContext& frame,
		ChunkPassVk& chunk, 
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	) const;
	void waterRefractionPass(
		const RenderSettings& rs,
		const FrameContext& frame,
		ChunkPassVk& chunk, 
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	) const;
private:
	VulkanMain& vk_;

	const ImageVk& shadowMapImage_;

	uint32_t factor_{};

	uint32_t width_{ 0 };
	uint32_t height_{ 0 };

	ImageVk reflColorImage_;
	ImageVk reflDepthImage_;

	ImageVk refrColorImage_;
	ImageVk refrDepthImage_;

	vk::Format colorFormat_ = vk::Format::eR16G16B16A16Sfloat;
	vk::Format depthFormat_ = vk::Format::eD32Sfloat;

	std::unique_ptr<ShaderModuleVk> shader_;

	Texture2DVk dudvTex_;
	Texture2DVk normalTex_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif
