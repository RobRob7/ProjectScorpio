#ifndef LIGHT_GL_H
#define LIGHT_GL_H

#include "constants.h"
#include "bindings.h"

#include "i_light.h"

#include "ubo_gl.h"

#include <cstdint>
#include <memory>
#include <algorithm>

class Shader;

class LightGL final : public ILight
{
public:
	LightGL();
	~LightGL() override;

	void init() override;

	void render(
		const FrameContext* frame,
		const glm::mat4& view,
		const glm::mat4& proj
	) override;

	void updateLight(
		float time,
		glm::vec3& camPos,
		bool paused
	) override;

	const float& getSpeed() const override { return speed_; }
	const glm::vec3& getDirection() const override { return direction_; }
	const glm::vec3& getPosition() const override { return position_; }
	const glm::vec3& getLightColor() const override { return lightColor_; }

	void setSpeed(const float speed) override
	{
		speed_ = std::clamp(speed, MIN_SPEED, MAX_SPEED);
	} // end of setSpeed()

	void setDirection(const glm::vec3& dir) override
	{
		if (glm::length(dir) > 0.0001f)
			direction_ = glm::normalize(dir);
	} // end of setDirection()

	void setPosition(const glm::vec3& pos) override { position_ = pos; }

	void setLightColor(const glm::vec3& color) override
	{
		lightColor_ = {
			std::clamp(color.x, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR),
			std::clamp(color.y, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR),
			std::clamp(color.z, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR)
		};
	} // end of setColor()

private:
	std::unique_ptr<Shader> shader_;

	UBOGL ubo_{ TO_API_FORM(LightBinding::UBO) };

	float sunTime_{ 0.0f };
	float lastTime_{ 0.0f };
	bool firstUpdate_{ true };

	float speed_{ 0.1f };
	glm::vec3 visualColor_{ INIT_VISUAL_COLOR };
	glm::vec3 direction_{};
	glm::vec3 position_{};
	glm::vec3 lightColor_{ INIT_LIGHT_COLOR };
	glm::vec3 camPos_{};
};

#endif
