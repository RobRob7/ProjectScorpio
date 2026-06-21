#ifndef CUBEMAP_DX12_H
#define CUBEMAP_DX12_H

#include "constants.h"

#include "i_cubemap.h"

#include "buffer_dx12.h"
#include "descriptor_set_dx12.h"
#include "graphics_pipeline_dx12.h"
#include "texture_cubemap_dx12.h"

#include <glm/glm.hpp>

#include <memory>
#include <string_view>
#include <array>
#include <cstdint>
#include <vector>

class DX12Main;
class ShaderDX12;

class CubemapDX12 final : public ICubemap
{
public:
    CubemapDX12(
		DX12Main& dx, 
		const std::array<std::string_view, 6>& textures = Cubemap_Constants::DEFAULT_FACES
	);
    ~CubemapDX12() override;

    void init() override;

    void render(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& sunDir,
		const float time
	) override;
	void renderOffscreen(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		const glm::mat4& view,
		const glm::mat4& projection,
		uint32_t width,
		uint32_t height,
		const glm::vec3& sunDir,
		const float time
	);

	TextureCubemapDX12& getNightTexture() { return cubemapTextureNight_; }
	TextureCubemapDX12& getDayTexture() { return cubemapTextureDay_; }

private:
	void createVertexBuffer();
	void createUBOs();
	void createDescriptorSets();
	void createPipeline();
private:
	DX12Main* dx_{ nullptr };

	std::array<std::string_view, 6> faces_;

	std::unique_ptr<ShaderDX12> shader_;

	TextureCubemapDX12 cubemapTextureNight_;
	TextureCubemapDX12 cubemapTextureDay_;

	BufferDX12 vertexBuffer_;

	std::vector<BufferDX12> uboBuffers_;
	std::vector<BufferDX12> uboBuffersOffscreen_;

	uint32_t vertexCount_{};

	std::vector<DescriptorSetDX12> descriptorSets_;
	std::vector<DescriptorSetDX12> descriptorSetsOffscreen_;

	GraphicsPipelineDX12 pipeline_;
	GraphicsPipelineDX12 pipelineOffscreen_;
};

#endif
