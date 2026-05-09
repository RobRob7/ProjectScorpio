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
	DebugPassVk(
		VulkanMain& vk,
		const ImageVk& normalImage,
		const ImageVk& depthImage,
		const ImageVk& shadowMapImage,
		const ImageVk& rtDepthImage
	);
	~DebugPassVk();

	void init();
	void resize();

	void render(
		FrameContext& frame,
		float nearPlane,
		float farPlane,
		int mode
	);

private:
	void refreshInputs();
	void createResources();
	void createDescriptorSets();
	void createPipeline();
private:
	VulkanMain& vk_;

	const ImageVk& normalImage_;
	const ImageVk& depthImage_;
	const ImageVk& shadowMapImage_;
	const ImageVk& rtDepthImage_;
	
	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<DescriptorSetVk> descriptorSets_;
	GraphicsPipelineVk pipeline_;
};

#endif