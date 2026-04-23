#ifndef FOG_PASS_VK_H
#define FOG_PASS_VK_H

#include "constants.h"
#include "frame_context_vk.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct RenderSettings;

class FogPassVk
{
public:
	FogPassVk(VulkanMain& vk, RenderSettings& rs);
	~FogPassVk();

	void init();
	void resize();

	void render(
		FrameContext& frame,
		float nearPlane,
		float farPlane,
		float ambStr
	);

	void setInput(ImageVk& inputColor, ImageVk& inputDepth)
	{
		inputColorImage_ = &inputColor;
		inputDepthImage_ = &inputDepth;

		refreshInput();
	} // end of setInput()

	ImageVk& getOutputImage() { return outputImage_; }

private:
	void refreshInput();
	void createAttachment();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
private:
	VulkanMain& vk_;

	RenderSettings& rs_;

	ImageVk* inputColorImage_{ nullptr };
	ImageVk* inputDepthImage_{ nullptr };

	ImageVk outputImage_;
	vk::Format outputFormat_{ vk::Format::eR16G16B16A16Sfloat };
	vk::ImageLayout outputLayout_{ vk::ImageLayout::eUndefined };

	Fog_Constants::FogPassUBO ubo_;

	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif
