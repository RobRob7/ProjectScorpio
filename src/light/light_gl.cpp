#include "light_gl.h"

#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>

#include <cstddef>
#include <memory>

using namespace Light_Constants;

//--- PUBLIC ---//
LightGL::LightGL() = default;

LightGL::~LightGL() = default;

void LightGL::init()
{
	shader_ = std::make_unique<Shader>(
		"light/light.vert", 
		"light/light.frag"
	);

	// UBO
	ubo_.init<sizeof(LightUBO)>();
} // end of init()

void LightGL::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& proj
)
{
	if (!shader_)
		return;

	// bind ubo
	ubo_.bind();

	shader_->use();

	LightUBO ubo{};
	ubo.u_invViewProj = glm::inverse(proj * view);
	ubo.u_viewProj = proj * view;
	ubo.u_camPos = camPos_;
	ubo.u_sunDistance = SUN_DISTANCE;
	ubo.u_lightPos = glm::vec4(position_, 1.0f);
	ubo.u_lightVisualColor = visualColor_;
	ubo.u_sunRadius = SUN_SCALE / 2.0f;
	ubo_.update(&ubo, sizeof(ubo));

	glDrawArrays(GL_TRIANGLES, 0, 3);
} // end of render

void LightGL::updateLight(
	float time, 
	const glm::vec3& camPos,
	bool paused
)
{
	camPos_ = camPos;

	if (firstUpdate_)
	{
		lastTime_ = time;
		firstUpdate_ = false;
	}

	float dt = time - lastTime_;
	lastTime_ = time;

	if (!paused)
	{
		sunTime_ += dt * speed_;
	}

	// update direction
	float cycle = sunTime_ * glm::two_pi<float>();

	float azimuth = cycle;
	float elevation = glm::sin(cycle) * glm::radians(75.0f);

	glm::vec3 sunDir = glm::normalize(glm::vec3(
		glm::cos(elevation) * glm::cos(azimuth),
		glm::sin(elevation),
		glm::cos(elevation) * glm::sin(azimuth)
	));
	setDirection(sunDir);

	// update position
	glm::vec3 sunVisualPos = camPos - direction_ * SUN_DISTANCE;
	setPosition(sunVisualPos);

	// update visual color
	float sunHeight = glm::max(-direction_.y, 0.0f);
	float tColor = glm::smoothstep(0.0f, 0.35f, sunHeight);

	glm::vec3 sunsetColor = glm::vec3(
		INIT_VISUAL_COLOR.r,
		INIT_VISUAL_COLOR.g / 2.0f,
		INIT_VISUAL_COLOR.b
	);

	glm::vec3 noonColor = INIT_VISUAL_COLOR;

	visualColor_ = glm::mix(
		sunsetColor,
		noonColor,
		tColor
	);
} // end of updateLight()
