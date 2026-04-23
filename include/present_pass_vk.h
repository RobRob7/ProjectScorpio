#ifndef PRESENT_PASS_VK_H
#define PRESENT_PASS_VK_H

#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include <memory>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct FrameContext;
class ImageVk;

class PresentPassVk
{
public:
	PresentPassVk(VulkanMain& vk);
	~PresentPassVk();

	void init();
	void resize();

	void render(
		FrameContext& frame
	);

	void setInput(ImageVk& input)
	{
		inputImage_ = &input;
		refreshInput();
	} // end of setInput()

private:
	void refreshInput();
	void createDescriptorSets();
	void createPipeline();
private:
	VulkanMain& vk_;
	ImageVk* inputImage_{ nullptr };

	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif
