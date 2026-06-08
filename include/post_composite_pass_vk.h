#ifndef POST_COMPOSITE_PASS_VK_H
#define POST_COMPOSITE_PASS_VK_H

#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"
#include "image_vk.h"

#include <memory>
#include <utility>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct FrameContext;
struct RenderSettings;

class PostCompositePassVk
{
public:
	explicit PostCompositePassVk(VulkanMain& vk);
	~PostCompositePassVk();

	void init();
	void resize();

	void render(FrameContext& frame);

	void setInput(
		ImageVk& inputFogTex,
		ImageVk& inputGodRayTex,
		ImageVk& inputSceneColorTex
	)
	{
		fogColorImage_ = &inputFogTex;
		godRayColorImage_ = &inputGodRayTex;
		sceneColorImage_ = &inputSceneColorTex;
	} // end of setInput()

	const ImageVk& getOutColorImage() const { return postColorImage_; }
	ImageVk& getOutColorImage() { return postColorImage_; }

private:
	void updateDescriptorSet(uint32_t frameIndex);
	void createAttachment();
	void createDescriptorSet();
	void createPipeline();

private:
	VulkanMain& vk_;

	uint32_t width_{};
	uint32_t height_{};

	ImageVk* fogColorImage_{ nullptr };
	ImageVk* godRayColorImage_{ nullptr };
	ImageVk* sceneColorImage_{ nullptr };

	ImageVk postColorImage_;
	vk::Format postColorFormat_{ vk::Format::eR16G16B16A16Sfloat };

	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif
