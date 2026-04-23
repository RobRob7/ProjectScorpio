#ifndef RENDER_TARGET_VK_H
#define RENDER_TARGET_VK_H

#include <vulkan/vulkan.hpp>

#include <vector>

enum class RenderTargetVk
{
	Default,
	GBuffer,
	Shadow,
	WaterReflection,
	WaterRefraction
};

struct RenderTargetFormatsVk
{
	vk::Format colorFormat{ vk::Format::eUndefined };
	vk::Format depthFormat{ vk::Format::eUndefined };
};

#endif
