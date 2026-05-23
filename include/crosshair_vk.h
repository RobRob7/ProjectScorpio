#ifndef CROSSHAIR_VK_H
#define CROSSHAIR_VK_H

#include "i_crosshair.h"

#include "buffer_vk.h"
#include "graphics_pipeline_vk.h"

#include <memory>

class VulkanMain;
class ShaderModuleVk;
struct FrameContext;

class CrosshairVk : public ICrosshair
{
public:
	CrosshairVk(VulkanMain& vk);
	~CrosshairVk();

	void init() override;
	void render(const FrameContext* frame) override;

private:
	void createResources();
	void createPipeline();
private:
	VulkanMain& vk_;

	std::unique_ptr<ShaderModuleVk> shader_;

	BufferVk vBuffer_;
	GraphicsPipelineVk pipeline_;
};

#endif
