#ifndef GOD_RAY_PASS_DX12_H
#define GOD_RAY_PASS_DX12_H

#include "constants.h"
#include "frame_context_dx12.h"

#include "buffer_dx12.h"
#include "descriptor_set_dx12.h"
#include "compute_pipeline_dx12.h"
#include "image_dx12.h"

#include <memory>
#include <vector>
#include <cstdint>

class DX12Main;
class ComputeShaderDX12;
struct RenderSettings;

struct GodRayUBOs
{
	God_Ray_Constants::GodRayPassUBO ubo{};
};

class GodRayPassDX12
{
public:
	explicit GodRayPassDX12(
		DX12Main& dx,
		const RenderSettings& rs
	);
	~GodRayPassDX12();

	void init();
	void resize();

	void render(
		const GodRayUBOs& ubos, 
		const FrameContextDX12& frame
	);

	void setInput(
		ImageDX12& inputDepth, 
		ImageDX12& inputShadowMap
	)
	{
		inputDepthImage_ = &inputDepth;
		inputShadowMapImage_ = &inputShadowMap;
	} // end of setInput()

	ImageDX12& getOutputImage() { return outputImage_; }

private:
	void syncSettings();
	void updateDescriptorSet(uint32_t frameIndex);
	void createAttachment();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
private:
	DX12Main* dx_{ nullptr };
	const RenderSettings& rs_;

	uint32_t factor_{};
	uint32_t width_{};
	uint32_t height_{};

	uint32_t numWorkGroups_{ God_Ray_Constants::WORK_GROUPS };
	uint32_t workGroupX_{};
	uint32_t workGroupY_{};

	ImageDX12* inputDepthImage_{ nullptr };
	ImageDX12* inputShadowMapImage_{ nullptr };

	ImageDX12 outputImage_;
	DXGI_FORMAT outputFormat_{ DXGI_FORMAT_R16G16B16A16_FLOAT };

	std::unique_ptr<ComputeShaderDX12> compShader_;

	std::vector<BufferDX12> uboBuffers_;
	std::vector<DescriptorSetDX12> descriptorSets_;
	ComputePipelineDX12 computePipeline_;
};

#endif
