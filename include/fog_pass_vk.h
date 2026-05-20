#ifndef FOG_PASS_VK_H
#define FOG_PASS_VK_H

#include "constants.h"
#include "frame_context_vk.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "compute_pipeline_vk.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>
#include <cstdint>

class VulkanMain;
class ComputeShaderModuleVk;
struct RenderSettings;

class FogPassVk
{
public:
	explicit FogPassVk(VulkanMain& vk);
	~FogPassVk();

	void init();
	void resize();

	void render(
		FrameContext& frame,
		Fog_Constants::FogPassUBO& fogUBO
	);

	void setInput(ImageVk& inputDepth, ImageVk& inputShadowMap)
	{
		inputDepthImage_ = &inputDepth;
		inputShadowMapImage_ = &inputShadowMap;
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

	uint32_t width_{};
	uint32_t height_{};

	uint32_t resFactor_{ Fog_Constants::RES_FACTOR };

	uint32_t numWorkGroups_{Fog_Constants::WORK_GROUPS};
	uint32_t workGroupX_{};
	uint32_t workGroupY_{};

	ImageVk* inputDepthImage_{ nullptr };
	ImageVk* inputShadowMapImage_{ nullptr };

	ImageVk outputImage_;
	vk::Format outputFormat_{ vk::Format::eR16G16B16A16Sfloat };

	std::unique_ptr<ComputeShaderModuleVk> compShader_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<DescriptorSetVk> descriptorSets_;
	ComputePipelineVk computePipeline_;
};

#endif
