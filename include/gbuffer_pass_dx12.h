#ifndef GBUFFER_PASS_DX12_H
#define GBUFFER_PASS_DX12_H

#include "constants.h"

#include "buffer_dx12.h"
#include "descriptor_set_dx12.h"
#include "graphics_pipeline_dx12.h"
#include "image_dx12.h"

#include <glm/glm.hpp>

#include <vector>
#include <memory>

class DX12Main;
class ShaderDX12;
struct RenderInputs;
struct FrameContextDX12;
class ChunkPassDX12;

class GBufferPassDX12
{
public:
	explicit GBufferPassDX12(DX12Main& dx);
	~GBufferPassDX12();

	void init();
	void resize();

	void render(
		const Gbuffer_Constants::GbufferUBO uboData,
		const RenderInputs& in,
		const FrameContextDX12& frame
	);

	const ImageDX12& getNormalImage() const { return gNormalImage_; }
	ImageDX12& getNormalImage() { return gNormalImage_; }
	const ImageDX12& getDepthImage() const { return gDepthImage_; }
	ImageDX12& getDepthImage() { return gDepthImage_; }

private:
	void createAttachments();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
private:
	DX12Main* dx_{ nullptr };

	ImageDX12 gNormalImage_;
	DXGI_FORMAT normalFormat_{ DXGI_FORMAT_R16G16B16A16_FLOAT };

	ImageDX12 gDepthImage_;
	DXGI_FORMAT depthFormat_{ DXGI_FORMAT_D32_FLOAT};

	std::unique_ptr<ShaderDX12> shader_;

	std::vector<BufferDX12> uboBuffers_;
	std::vector<DescriptorSetDX12> descriptorSets_;
	GraphicsPipelineDX12 pipeline_;
};

#endif
