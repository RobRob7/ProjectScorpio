#ifndef CHUNK_PASS_DX12_H
#define CHUNK_PASS_DX12_H

#include "constants.h"
#include "render_target_dx12.h"

#include "shader_dx12.h"
#include "frame_context_dx12.h"
#include "texture_2d_dx12.h"
#include "buffer_dx12.h"
#include "descriptor_set_dx12.h"
#include "graphics_pipeline_dx12.h"

#include <glm/glm.hpp>

#include <memory>
#include <cstdlib>
#include <vector>
#include <cstdint>

class DX12Main;
class ImageDX12;
struct RenderInputs;
struct RenderSettings;
//struct FrameContextDX12;

struct ChunkPassUBOs
{
	Chunk_Constants::ChunkOpaqueUBO chunkUBOData{};
	Gbuffer_Constants::GbufferUBO gbufferUBOData{};
	Shadow_Map_Constants::ShadowMapPassUBO shadowUBOData{};
};

class ChunkPassDX12
{
public:
	explicit ChunkPassDX12(
		DX12Main& dx, 
		RenderSettings& rs
	);
	~ChunkPassDX12();

	void init(
		RenderTargetFormatsDX12 defaultFormats,
		RenderTargetFormatsDX12 gbufferFormats,
		RenderTargetFormatsDX12 shadowFormats
	);

	void resize();

	void renderOpaque(
		RenderTargetDX12 renderTarget,
		const RenderInputs& in,
		const FrameContextDX12& frame,
		const glm::mat4& view,
		const glm::mat4& proj,
		const glm::mat4& lightSpaceMatrix = {},
		const uint32_t waterPassWidth = {},
		const uint32_t waterPassHeight = {}
	);

	void setInput(
		ImageDX12& ssaoBlurTex,
		ImageDX12& shadowMapTex
	)
	{
		ssaoBlurTex_ = &ssaoBlurTex;
		shadowMapTex_ = &shadowMapTex;
	} // end of setInput()

private:
	void updateDescriptorSet(uint32_t frameIndex);
	void updateSingleDescriptorSet(uint32_t frameIndex, RenderTargetDX12 renderTarget);
	void createResources();
	void createDescriptorSets();
	void createPipelines(
		RenderTargetFormatsDX12 defaultFormats,
		RenderTargetFormatsDX12 gbufferFormats,
		RenderTargetFormatsDX12 shadowFormats
	);
private:
	DX12Main* dx_{ nullptr };
	RenderSettings* rs_{ nullptr };

	ImageDX12* ssaoBlurTex_{ nullptr };
	ImageDX12* shadowMapTex_{ nullptr };

	std::unique_ptr<ShaderDX12> opaqueShader_;
	std::unique_ptr<ShaderDX12> opaqueGBufferShader_;
	std::unique_ptr<ShaderDX12> opaqueShadowShader_;

	Texture2DDX12 atlas_;

	std::vector<BufferDX12> opaqueUBOBuffers_;
	std::vector<BufferDX12> reflUBOBuffers_;
	std::vector<BufferDX12> refrUBOBuffers_;
	Chunk_Constants::ChunkOpaqueUBO chunkUBOData_{};
	std::vector<BufferDX12> opaqueGBufferUBOBuffers_;
	Gbuffer_Constants::GbufferUBO gbufferUBOData_{};
	std::vector<BufferDX12> opaqueShadowUBOBuffers_;
	Shadow_Map_Constants::ShadowMapPassUBO shadowUBOData_{};

	std::vector<DescriptorSetDX12> opaqueDescriptorSets_;
	std::vector<DescriptorSetDX12> reflectionDescriptorSets_;
	std::vector<DescriptorSetDX12> refractionDescriptorSets_;
	std::vector<DescriptorSetDX12> opaqueGBufferDescriptorSets_;
	std::vector<DescriptorSetDX12> opaqueShadowDescriptorSets_;

	GraphicsPipelineDX12 opaquePipeline_;
	GraphicsPipelineDX12 opaqueGBufferPipeline_;
	GraphicsPipelineDX12 opaqueShadowPipeline_;
};

#endif
