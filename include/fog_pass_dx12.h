#ifndef FOG_PASS_DX12_H
#define FOG_PASS_DX12_H

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

struct FogPassUBOs
{
	Fog_Constants::FogPassUBO ubo{};
};

class FogPassDX12
{
public:
	explicit FogPassDX12(
		DX12Main& dx, 
		const RenderSettings& rs
	);
	~FogPassDX12();

	void init();
	void resize();

	void render(
		const FogPassUBOs& ubos, 
		const FrameContextDX12& frame
	);

	void setInput(ImageDX12& inputDepth)
	{
		inputDepthImage_ = &inputDepth;
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

	uint32_t numWorkGroups_{Fog_Constants::WORK_GROUPS};
	uint32_t workGroupX_{};
	uint32_t workGroupY_{};

	ImageDX12* inputDepthImage_{ nullptr };

	ImageDX12 outputImage_;
	DXGI_FORMAT outputFormat_{ DXGI_FORMAT_R16G16B16A16_FLOAT };

	std::unique_ptr<ComputeShaderDX12> shader_;

	std::vector<BufferDX12> uboBuffers_;
	std::vector<DescriptorSetDX12> descriptorSets_;
	ComputePipelineDX12 pipeline_;
};

#endif
