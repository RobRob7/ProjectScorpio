#ifndef GOD_RAY_PASS_VK_H
#define GOD_RAY_PASS_VK_H

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

struct GodRayUBOs
{
	God_Ray_Constants::GodRayPassUBO ubo{};
};

class GodRayPassVk
{
public:
	explicit GodRayPassVk(VulkanMain& vk, const RenderSettings& rs);
	~GodRayPassVk();

	void init();
	void resize();

	void render(const GodRayUBOs& ubos, const FrameContext& frame);

	void setInput(
		ImageVk& inputDepth, 
		ImageVk& inputShadowMap
	)
	{
		inputDepthImage_ = &inputDepth;
		inputShadowMapImage_ = &inputShadowMap;
	} // end of setInput()

	ImageVk& getOutputImage() { return outputImage_; }

private:
	void syncSettings();
	void updateDescriptorSet(uint32_t frameIndex);
	void createAttachment();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
private:
	VulkanMain& vk_;
	const RenderSettings& rs_;

	uint32_t factor_{};
	uint32_t width_{};
	uint32_t height_{};

	uint32_t numWorkGroups_{ God_Ray_Constants::WORK_GROUPS };
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
