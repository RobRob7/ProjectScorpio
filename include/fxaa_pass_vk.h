#ifndef FXAA_PASS_VK_H
#define FXAA_PASS_VK_H

#include "constants.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct FrameContext;

class FXAAPassVk
{
public:
	explicit FXAAPassVk(VulkanMain& vk);
	~FXAAPassVk();

	void init();
	void resize();

	void render(FrameContext& frame);

	void setInput(ImageVk& input) { inputImage_ = &input; }

	ImageVk& getOutputImage() { return outputImage_; }

private:
	void refreshInput();
	void createAttachment();
	void createResources();
	void createDescriptorSets();
	void createPipeline();
private:
	VulkanMain& vk_;

	ImageVk* inputImage_{ nullptr };

	ImageVk outputImage_;
	vk::Format outputFormat_{ vk::Format::eR16G16B16A16Sfloat };

	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<BufferVk> uboBuffers_;
	FXAA_Constants::FXAAPassUBO uboData_{};

	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif
