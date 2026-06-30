#ifndef POST_COMPOSITE_PASS_DX12_H
#define POST_COMPOSITE_PASS_DX12_H

#include "constants.h"

#include "descriptor_set_dx12.h"
#include "compute_pipeline_dx12.h"
#include "image_dx12.h"

#include <memory>
#include <utility>
#include <vector>

class DX12Main;
class ComputeShaderDX12;
struct FrameContextDX12;
struct RenderSettings;

class PostCompositePassDX12
{
public:
	explicit PostCompositePassDX12(DX12Main& dx);
	~PostCompositePassDX12();

	void init();
	void resize();

	void render(FrameContextDX12& frame);

	void setInput(
		ImageDX12& inputFogTex,
		ImageDX12& inputGodRayTex,
		ImageDX12& inputSceneColorTex
	)
	{
		fogColorImage_ = &inputFogTex;
		godRayColorImage_ = &inputGodRayTex;
		sceneColorImage_ = &inputSceneColorTex;
	} // end of setInput()

	const ImageDX12& getOutColorImage() const { return postColorImage_; }
	ImageDX12& getOutColorImage() { return postColorImage_; }

private:
	void updateDescriptorSet(uint32_t frameIndex);
	void createAttachment();
	void createDescriptorSet();
	void createPipeline();

private:
	DX12Main* dx_{ nullptr };

	uint32_t numWorkGroups_{ Post_Composite_Constants::WORK_GROUPS };
	uint32_t workGroupX_{};
	uint32_t workGroupY_{};

	ImageDX12* fogColorImage_{ nullptr };
	ImageDX12* godRayColorImage_{ nullptr };
	ImageDX12* sceneColorImage_{ nullptr };

	ImageDX12 postColorImage_;
	DXGI_FORMAT postColorFormat_{ DXGI_FORMAT_R16G16B16A16_FLOAT };

	std::unique_ptr<ComputeShaderDX12> shader_;

	std::vector<DescriptorSetDX12> descriptorSets_;
	ComputePipelineDX12 pipeline_;
};

#endif
