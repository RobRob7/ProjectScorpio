#ifndef I_CUBEMAP_H
#define I_CUBEMAP_H

#include "constants.h"

#include <glm/glm.hpp>

#include <cstdint>

struct FrameContext;
struct FrameContextDX12;

class ICubemap
{
public:
	ICubemap() = default;
	virtual ~ICubemap() = default;

	ICubemap(const ICubemap&) = delete;
	ICubemap& operator=(const ICubemap&) = delete;

	ICubemap(ICubemap&&) = default;
	ICubemap& operator=(ICubemap&&) = default;

	virtual void init() = 0;
	virtual void render(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		const glm::mat4& view, 
		const glm::mat4& projection, 
		const glm::vec3& sunDir = glm::vec3(0.0f, -1.0f, 0.0f),
		const float time = -1.0
	) = 0;
	virtual void renderOffscreen(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		const glm::mat4& view,
		const glm::mat4& projection,
		uint32_t width,
		uint32_t height,
		const glm::vec3& sunDir = glm::vec3(0.0f, -1.0f, 0.0f),
		const float time = -1.0
	) 
	{
	};
};

#endif
