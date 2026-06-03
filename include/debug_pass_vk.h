#ifndef DEBUG_PASS_VK_H
#define DEBUG_PASS_VK_H

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include <memory>
#include <vector>

class VulkanMain;
class GBufferPassVk;
class ShaderModuleVk;
struct FrameContext;
class ImageVk;

class DebugPassVk
{
public:
	DebugPassVk(VulkanMain& vk);
	~DebugPassVk();

	void init();
	void resize();

	void render(
		FrameContext& frame,
		float nearPlane,
		float farPlane,
		int mode
	);

	void updateDescriptorSet(uint32_t frameIndex);

	void setInput(
		ImageVk& normalTex,
		ImageVk& depthTex,
		ImageVk& shadowMapTex,
		ImageVk& rtDepthTex
	)
	{
		normalImage_ = &normalTex;
		depthImage_ = &depthTex;
		shadowMapImage_ = &shadowMapTex;
		rtDepthImage_ = &rtDepthTex;
	} // end of setInput()

private:
	void refreshInputs();
	void createResources();
	void createDescriptorSets();
	void createPipeline();
private:
	VulkanMain& vk_;

	ImageVk* normalImage_{ nullptr };
	ImageVk* depthImage_{ nullptr };
	ImageVk* shadowMapImage_{ nullptr };
	ImageVk* rtDepthImage_{ nullptr };
	
	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif