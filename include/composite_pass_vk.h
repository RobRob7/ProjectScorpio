#ifndef COMPOSITE_PASS_VK_H
#define COMPOSITE_PASS_VK_H

#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"
#include "image_vk.h"

#include <memory>
#include <utility>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct FrameContext;

class CompositePassVk
{
public:
	CompositePassVk(VulkanMain& vk);
	~CompositePassVk();

	void init();
	void resize();

	void render(
		FrameContext& frame,
		float nearPlane,
		float farPlane
	);

	void setInput(
		std::pair<ImageVk&, ImageVk&> rasterColorDepth,
		std::pair<ImageVk&, ImageVk&> rtColorDepth
	)
	{
		rasterColor_ = &rasterColorDepth.first;
		rasterDepth_ = &rasterColorDepth.second;
		rtColor_ = &rtColorDepth.first;
		rtDepth_ = &rtColorDepth.second;
		refreshInput();
	} // end of setInput()

	const ImageVk& getOutColorImage() const { return hybridColorImage_; }
	ImageVk& getOutColorImage() { return hybridColorImage_; }
	const ImageVk& getOutDepthImage() const { return hybridDepthImage_; }
	ImageVk& getOutDepthImage() { return hybridDepthImage_; }

private:
	void refreshInput();
	void createAttachment();
	void createDescriptorSet();
	void createPipeline();

private:
	VulkanMain& vk_;

	ImageVk* rasterColor_{ nullptr };
	ImageVk* rasterDepth_{ nullptr };
	ImageVk* rtColor_{ nullptr };
	ImageVk* rtDepth_{ nullptr };

	ImageVk hybridColorImage_;
	vk::Format hybridColorFormat_{ vk::Format::eR16G16B16A16Sfloat };

	ImageVk hybridDepthImage_;
	vk::Format hybridDepthFormat_{ vk::Format::eD32Sfloat };

	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif
