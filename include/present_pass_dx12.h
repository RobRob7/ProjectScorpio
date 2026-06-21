#ifndef PRESENT_PASS_DX12_H
#define PRESENT_PASS_DX12_H

#include "descriptor_set_dx12.h"
#include "graphics_pipeline_dx12.h"

#include <memory>
#include <vector>

class DX12Main;
class ShaderDX12;
struct FrameContextDX12;
class ImageDX12;

class PresentPassDX12
{
public:
	explicit PresentPassDX12(DX12Main& dx);
	~PresentPassDX12();

	void init();
	void resize();

	void render(FrameContextDX12& frame);

	void setInput(ImageDX12& input)
	{
		inputImage_ = &input;
		refreshInput();
	} // end of setInput()

private:
	void refreshInput();
	void createDescriptorSets();
	void createPipeline();
private:
	DX12Main* dx_{ nullptr };
	ImageDX12* inputImage_{ nullptr };

	std::unique_ptr<ShaderDX12> shader_;

	std::vector<DescriptorSetDX12> descriptorSets_;
	GraphicsPipelineDX12 pipeline_;
};

#endif
