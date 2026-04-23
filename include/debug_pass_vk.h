#ifndef DEBUG_PASS_VK_H
#define DEBUG_PASS_VK_H

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include <memory>

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
		const ImageVk& shadowMapImage
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
	void createDescriptorSet();
	void createPipeline();
private:
	VulkanMain& vk_;

	const ImageVk& normalImage_;
	const ImageVk& depthImage_;
	const ImageVk& shadowMapImage_;
	
	std::unique_ptr<ShaderModuleVk> shader_;

	BufferVk uboBuffer_;
	DescriptorSetVk descriptorSet_;
	GraphicsPipelineVk pipeline_;
};

#endif