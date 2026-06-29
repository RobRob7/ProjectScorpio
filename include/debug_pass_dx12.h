#ifndef DEBUG_PASS_DX12_H
#define DEBUG_PASS_DX12_H

#include "constants.h"

#include "buffer_dx12.h"
#include "descriptor_set_dx12.h"
#include "graphics_pipeline_dx12.h"

#include <memory>
#include <vector>

class DX12Main;
class ShaderDX12;
struct FrameContextDX12;
class ImageDX12;

class DebugPassDX12
{
public:
	DebugPassDX12(DX12Main& dx);
	~DebugPassDX12();

	void init();
	void resize();

	void render(
		const Debug_Constants::DebugPassUBO& uboData,
		FrameContextDX12& frame
	);

	void setInput(
		ImageDX12& normalTex,
		ImageDX12& depthTex,
		ImageDX12& shadowMapTex,
		ImageDX12& rtDepthTex
	)
	{
		normalImage_ = &normalTex;
		depthImage_ = &depthTex;
		shadowMapImage_ = &shadowMapTex;
		rtDepthImage_ = &rtDepthTex;
	} // end of setInput()

private:
	void updateDescriptorSet(uint32_t frameIndex);
	void createResources();
	void createDescriptorSets();
	void createPipeline();
private:
	DX12Main* dx_{ nullptr };

	ImageDX12* normalImage_{ nullptr };
	ImageDX12* depthImage_{ nullptr };
	ImageDX12* shadowMapImage_{ nullptr };
	ImageDX12* rtDepthImage_{ nullptr };
	
	std::unique_ptr<ShaderDX12> shader_;

	std::vector<BufferDX12> uboBuffers_;
	std::vector<DescriptorSetDX12> descriptorSets_;
	GraphicsPipelineDX12 pipeline_;
};

#endif