#ifndef SHADOW_MAP_PASS_VK_H
#define SHADOW_MAP_PASS_VK_H

#include "constants.h"
#include "render_target_vk.h"

#include "image_vk.h"

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

class VulkanMain;
class ShaderModuleVk;
class ChunkPassVk;
struct RenderInputs;
struct FrameContext;

class ShadowMapPassVk
{
public:
	explicit ShadowMapPassVk(VulkanMain& vk);
	~ShadowMapPassVk();

	void init();

	void renderBegin(
		const RenderInputs& in,
		const FrameContext& frame
	);

	void renderEnd(const FrameContext& frame);

	void render(
		ChunkPassVk& chunk,
		const RenderInputs& in,
		const FrameContext& frame
	);

	ImageVk& getDepthImage() { return depthImage_; }
	const ImageVk& getDepthImage() const { return depthImage_; }

	const glm::mat4& getLightSpaceMatrix() const { return lightSpaceMatrix_; }

private:
	void buildLightSpaceBounds(
		const RenderInputs& in,
		const glm::vec3& minWS,
		const glm::vec3& maxWS
	);
	void createAttachments();
private:
	VulkanMain& vk_;

	uint32_t width_{ Shadow_Map_Constants::SHADOW_RESOLUTION };
	uint32_t height_{ Shadow_Map_Constants::SHADOW_RESOLUTION };

	glm::mat4 lightSpaceMatrix_{};
	glm::mat4 lightView_{};
	glm::mat4 lightProj_{};

	ImageVk depthImage_;
	vk::Format depthFormat_ = vk::Format::eD32Sfloat;
};

#endif
