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
	LightVk(
		VulkanMain& vk, 
		const glm::vec3& pos, 
		const glm::vec3& dir = glm::vec3(-0.6f, -1.0f, -0.35f),
		const glm::vec3& color = glm::vec3(1.0f)
	);
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
		const glm::vec3& position,
		uint32_t width,
		uint32_t height
	);

	virtual void updateLightDirection(float time) override;

	float& getSpeed() override { return speed_; }
	const float& getSpeed() const override { return speed_; }
	glm::vec3& getDirection() override { return direction_; }
	const glm::vec3& getDirection() const override { return direction_; };
	glm::vec3& getPosition() override { return position_; }
	const glm::vec3& getPosition() const override { return position_; }
	glm::vec3& getColor() override { return color_; }
	const glm::vec3& getColor() const override { return color_; }

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

	void setColor(const glm::vec3& color) override
	{
		color_ = {
		std::clamp(color.x, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR),
		std::clamp(color.y, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR),
		std::clamp(color.z, Light_Constants::MIN_COLOR, Light_Constants::MAX_COLOR)
		};
	} // end of setColor()

private:
	void createVertexBuffer();
	void createUBOs();
	void createDescriptorSets();
	void createPipeline();
private:
	VulkanMain& vk_;

	std::unique_ptr<ShaderModuleVk> shader_;

	BufferVk vertexBuffer_;

	std::vector<BufferVk> uboBuffers_;
	std::vector<BufferVk> uboBuffersOffscreen_;

	uint32_t vertexCount_{};

	std::vector<DescriptorSetVk> descriptorSets_;
	std::vector<DescriptorSetVk> descriptorSetsOffscreen_;

	GraphicsPipelineVk pipeline_;
	GraphicsPipelineVk pipelineOffscreen_;

	float speed_{ 0.1f };
	glm::vec3 direction_{};
	glm::vec3 position_{};
	glm::vec3 color_{};
};

#endif