#ifndef CHUNK_PASS_VK_H
#define CHUNK_PASS_VK_H

#include "constants.h"
#include "render_target_vk.h"

#include "shader_vk.h"
#include "texture_2d_vk.h"
#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include <glm/glm.hpp>

#include <memory>
#include <cstdlib>

class VulkanMain;
class ImageVk;
struct RenderInputs;
struct RenderSettings;
struct DrawContext;
struct FrameContext;

class ChunkPassVk
{
public:
	explicit ChunkPassVk(
		VulkanMain& vk,
		RenderSettings& rs,
		const ImageVk& ssaoBlurImage, 
		const ImageVk& shadowMapImage
	);
	~ChunkPassVk();

	void init(
		RenderTargetFormatsVk defaultFormats,
		RenderTargetFormatsVk gbufferFormats,
		RenderTargetFormatsVk shadowFormats
	);
	void resize();

	void renderOpaque(
		RenderTargetVk renderTarget,
		const RenderInputs& in,
		const FrameContext& frame,
		const glm::mat4& view,
		const glm::mat4& proj,
		const glm::mat4& lightSpaceMatrix = {},
		const uint32_t waterPassWidth = {},
		const uint32_t waterPassHeight = {}
	);

private:
	void refreshTexBinding();
	void createResources();
	void createDescriptorSets();
	void createPipelines(
		RenderTargetFormatsVk defaultFormats,
		RenderTargetFormatsVk gbufferFormats,
		RenderTargetFormatsVk shadowFormats
	);
private:
	VulkanMain& vk_;

	RenderSettings& rs_;

	const ImageVk& ssaoBlurImage_;
	const ImageVk& shadowMapImage_;

	std::unique_ptr<ShaderModuleVk> opaqueShader_;
	std::unique_ptr<ShaderModuleVk> opaqueGBufferShader_;
	std::unique_ptr<ShaderModuleVk> opaqueShadowShader_;

	Texture2DVk atlas_;

	BufferVk opaqueUBOBuffer_;
	BufferVk reflUBOBuffer_;
	BufferVk refrUBOBuffer_;
	Chunk_Constants::ChunkOpaqueUBO chunkUBOData_{};
	BufferVk opaqueGBufferUBOBuffer_;
	Gbuffer_Constants::GbufferUBO gbufferUBOData_{};
	BufferVk opaqueShadowUBOBuffer_;
	Shadow_Map_Constants::ShadowMapPassUBO shadowUBOData_{};

	DescriptorSetVk opaqueDescriptorSet_;
	DescriptorSetVk reflectionDescriptorSet_;
	DescriptorSetVk refractionDescriptorSet_;
	DescriptorSetVk opaqueGBufferDescriptorSet_;
	DescriptorSetVk opaqueShadowDescriptorSet_;

	GraphicsPipelineVk opaquePipeline_;
	GraphicsPipelineVk opaqueGBufferPipeline_;
	GraphicsPipelineVk opaqueShadowPipeline_;
};

#endif
