#ifndef LIGHT_VK_H
#define LIGHT_VK_H

#include "constants.h"

#include "i_light.h"
#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include <memory>
#include <algorithm>
#include <cstdint>
#include <vector>

class VulkanMain;
class ShaderModuleVk;

class LightVk final : public ILight
{
public:
	LightVk(VulkanMain& vk);
	~LightVk() override;
	
	void init() override;

	void render(
		const FrameContext* frame,
		const glm::mat4& view,
		const glm::mat4& proj
	) override;
	void renderOffscreen(
		const FrameContext* frame,
		const glm::mat4& view,
		const glm::mat4& proj,
		uint32_t width,
		uint32_t height
	);

	void updateLight(
		float time, 
		const glm::vec3& camPos, 
		bool paused
	) override;

	const float& getSpeed() const override { return speed_; }
	const glm::vec3& getDirection() const override { return direction_; };
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
		lightColor_ =
		{
			std::clamp(color.x, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR),
			std::clamp(color.y, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR),
			std::clamp(color.z, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR)
		};
	}

private:
	void createUBOs();
	void createDescriptorSets();
	void createPipeline();
private:
	VulkanMain& vk_;

	std::unique_ptr<ShaderModuleVk> shader_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<BufferVk> uboBuffersOffscreen_;

	std::vector<DescriptorSetVk> descriptorSets_;
	std::vector<DescriptorSetVk> descriptorSetsOffscreen_;

	GraphicsPipelineVk pipeline_;
	GraphicsPipelineVk pipelineOffscreen_;

	float sunTime_{ 0.0f };
	float lastTime_{ 0.0f };
	bool firstUpdate_{ true };

	float speed_{ INIT_LIGHT_SPEED };
	glm::vec3 visualColor_{};
	glm::vec3 direction_{};
	glm::vec3 position_{};
	glm::vec3 lightColor_{ INIT_LIGHT_COLOR };
	glm::vec3 camPos_{};
};

#endif