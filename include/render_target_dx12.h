#ifndef RENDER_TARGET_VK_H
#define RENDER_TARGET_VK_H

#include <dxgiformat.h>

enum class RenderTargetDX12
{
	Default,
	GBuffer,
	Shadow,
	WaterReflection,
	WaterRefraction
};

struct RenderTargetFormatsDX12
{
	DXGI_FORMAT colorFormat{ DXGI_FORMAT_UNKNOWN };
	DXGI_FORMAT depthFormat{ DXGI_FORMAT_UNKNOWN };
};

#endif
