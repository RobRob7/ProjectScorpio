#ifndef I_LIGHT_H
#define I_LIGHT_H

#include "constants.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

struct FrameContext;

using namespace Light_Constants;

class ILight
{
public:
	virtual ~ILight() = default;

	virtual void init() = 0;

	virtual void render(
		const FrameContext* frame,
		const glm::mat4& view, 
		const glm::mat4& proj
	) = 0;
	virtual void renderOffscreen(
		const FrameContext* frame,
		const glm::mat4& view,
		const glm::mat4& proj,
		uint32_t width,
		uint32_t height
	)
	{
	}

	virtual void updateLight(
		float time, 
		const glm::vec3& camPos, 
		bool paused) = 0;

	virtual const float& getSpeed() const = 0;
	virtual const glm::vec3& getDirection() const = 0;
	virtual const glm::vec3& getPosition() const = 0;
	virtual const glm::vec3& getLightColor() const = 0;

	virtual void setSpeed(const float speed) = 0;
	virtual void setDirection(const glm::vec3& dir) = 0;
	virtual void setPosition(const glm::vec3& pos) = 0;
	virtual void setLightColor(const glm::vec3& color) = 0;
};

#endif
