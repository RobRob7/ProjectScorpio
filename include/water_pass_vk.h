#ifndef WATER_PASS_VK_H
#define WATER_PASS_VK_H

#include "constants.h"

#include "image_vk.h"
#include "texture_2d_vk.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <vector>
#include <cstdint>

class VulkanMain;
class ShaderModuleVk;
struct RenderInputs;
class ChunkPassVk;
struct FrameContext;
struct RenderSettings;
struct ChunkDrawList;

struct WaterPassUBOs
{
	Chunk_Constants::ChunkWaterUBO waterData{};
};

class WaterPassVk
{
public:
	explicit WaterPassVk(VulkanMain& vk, const RenderSettings& rs);
	~WaterPassVk();

	void init();
	void resize();

	void renderOffscreen(
		const RenderSettings& rs,
		const FrameContext& frame,
		const glm::mat4& proj,
		ChunkPassVk& chunk,
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	);

	void render(
		const WaterPassUBOs& ubos,
		const ChunkDrawList& drawList,
		const FrameContext& frame
	);

	void setInput(ImageVk& shadowMapTex)
	{
		shadowMapTex_ = &shadowMapTex;
	} // end of setInput()

	ImageVk& getReflColorImage() { return reflColorImage_; }
	ImageVk& getReflDepthImage() { return reflDepthImage_; }

	ImageVk& getRefrColorImage() { return refrColorImage_; }
	ImageVk& getRefrDepthImage() { return refrDepthImage_; }

	vk::Extent2D getExtent() const { return { width_, height_ }; }

private:
	void syncSettings();
	void updateDescriptorSet(uint32_t frameIndex);
	void createAttachments();
	void createResources();
	void createDescriptorSet();
	void createPipeline();

	void waterPass(
		const RenderSettings& rs,
		const FrameContext& frame,
		const glm::mat4& proj,
		ChunkPassVk& chunk, 
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	);
	void waterReflectionPass(
		const RenderSettings& rs,
		const FrameContext& frame,
		const glm::mat4& proj,
		ChunkPassVk& chunk, 
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	) const;
	void waterRefractionPass(
		const RenderSettings& rs,
		const FrameContext& frame,
		const glm::mat4& proj,
		ChunkPassVk& chunk, 
		const RenderInputs& in,
		const glm::mat4& lightSpaceMatrix
	) const;
private:
	VulkanMain& vk_;
	const RenderSettings& rs_;

	ImageVk* shadowMapTex_{ nullptr };

	uint32_t factor_{};
	uint32_t width_{};
	uint32_t height_{};

	ImageVk reflColorImage_;
	ImageVk reflDepthImage_;

	ImageVk refrColorImage_;
	ImageVk refrDepthImage_;

	vk::Format colorFormat_{ vk::Format::eR16G16B16A16Sfloat };
	vk::Format depthFormat_{ vk::Format::eD32Sfloat };

	std::unique_ptr<ShaderModuleVk> shader_;

	Texture2DVk dudvTex_;
	Texture2DVk normalTex_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif
