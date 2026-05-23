#include "shadow_map_pass_vk.h"

#include "constants.h"
#include "render_target_vk.h"
#include "render_inputs.h"
#include "chunk_manager.h"

#include "frame_context_vk.h"
#include "vulkan_main.h"

#include "light_vk.h"
#include "chunk_pass_vk.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

//--- PUBLIC ---//
ShadowMapPassVk::ShadowMapPassVk(VulkanMain& vk)
	: vk_(vk),
	depthImage_(vk)
{
} // end of constructor

ShadowMapPassVk::~ShadowMapPassVk() = default;

void ShadowMapPassVk::init()
{
	createAttachments();
} // end of init()

void ShadowMapPassVk::render(
	ChunkPassVk& chunk,
	const RenderInputs& in,
	const FrameContext& frame
)
{
	vk::CommandBuffer cmd = frame.cmd;
	vk::Extent2D extent = vk::Extent2D{ width_, height_ };

	cmd.beginDebugUtilsLabelEXT({ "ShadowMapPassVk::cmd" });

	depthImage_.transitionToDepthAttachment(cmd);

	vk::RenderingAttachmentInfo depthAttachment{};
	depthAttachment.imageView = depthImage_.view();
	depthAttachment.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	depthAttachment.clearValue.depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

	vk::RenderingInfo renderingInfo{};
	renderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderingInfo.renderArea.extent = extent;
	renderingInfo.layerCount = 1;
	renderingInfo.pDepthAttachment = &depthAttachment;

	cmd.beginRendering(renderingInfo);
	{
		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(width_);
		viewport.height = static_cast<float>(height_);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = extent;
		cmd.setScissor(0, 1, &scissor);

		// configure light space transform
		glm::vec3 minWS, maxWS;
		if (!in.world->buildVisibleChunkBounds(minWS, maxWS))
		{
			cmd.endRendering();

			depthImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);

			return;
		}
		buildLightSpaceBounds(in, minWS, maxWS);

		chunk.renderOpaque(
			RenderTargetVk::Shadow,
			in,
			frame,
			lightView_,
			lightProj_,
			lightSpaceMatrix_
		);
	}
	cmd.endRendering();

	depthImage_.transitionToShaderRead(cmd, vk::ImageAspectFlagBits::eDepth);

	cmd.endDebugUtilsLabelEXT();
} // end of render()


//--- PRIVATE ---//
void ShadowMapPassVk::buildLightSpaceBounds(
	const RenderInputs& in,
	const glm::vec3& minWS,
	const glm::vec3& maxWS
)
{
	glm::vec3 centerWS = 0.5f * (minWS + maxWS);

	glm::vec3 lightDir = in.light->getDirection();

	float lightDistance = 200.0f;
	glm::vec3 lightPos = centerWS - lightDir * lightDistance;

	// build light view
	lightView_ = glm::lookAt(
		lightPos,
		centerWS,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	// build the 8 corners of the visible world-space bounds
	glm::vec3 corners[8] =
	{
		{minWS.x, minWS.y, minWS.z},
		{maxWS.x, minWS.y, minWS.z},
		{minWS.x, maxWS.y, minWS.z},
		{maxWS.x, maxWS.y, minWS.z},
		{minWS.x, minWS.y, maxWS.z},
		{maxWS.x, minWS.y, maxWS.z},
		{minWS.x, maxWS.y, maxWS.z},
		{maxWS.x, maxWS.y, maxWS.z}
	};

	// transform bounds into light space and fit min/max
	glm::vec3 minLS(FLT_MAX);
	glm::vec3 maxLS(-FLT_MAX);

	for (const glm::vec3& c : corners)
	{
		glm::vec4 ls = lightView_ * glm::vec4(c, 1.0f);
		glm::vec3 p(ls);

		minLS = glm::min(minLS, p);
		maxLS = glm::max(maxLS, p);
	} // end for

	// stable shadow mapping
	// padding
	const float xyPad = 8.0f;
	const float zPad = 16.0f;

	float widthLS = maxLS.x - minLS.x;
	float heightLS = maxLS.y - minLS.y;

	float extent = std::max(widthLS, heightLS);
	extent += xyPad * 2.0f;
	extent = std::ceil(extent / CHUNK_SIZE) * CHUNK_SIZE;

	glm::vec3 centerLS = 0.5f * (minLS + maxLS);

	float texelSize = extent / 
		static_cast<float>(std::max(1, static_cast<int>(width_)));

	centerLS.x = std::round(centerLS.x / texelSize) * texelSize;
	centerLS.y = std::round(centerLS.y / texelSize) * texelSize;

	// rebuild snapped X/Y bounds
	minLS.x = centerLS.x - extent * 0.5f;
	maxLS.x = centerLS.x + extent * 0.5f;
	minLS.y = centerLS.y - extent * 0.5f;
	maxLS.y = centerLS.y + extent * 0.5f;

	float nearPlane = -maxLS.z - zPad;
	float farPlane = -minLS.z + zPad;

	// near/far plane clamp
	nearPlane = std::max(0.1f, nearPlane);
	farPlane = std::max(nearPlane + 1.0f, farPlane);

	// build fitted ortho projection
	lightProj_ = glm::orthoRH_ZO(
		minLS.x, maxLS.x,
		minLS.y, maxLS.y,
		nearPlane, farPlane
	);
	lightProj_[1][1] *= -1.0f;

	lightSpaceMatrix_ = lightProj_ * lightView_;
} // end of buildLightSpaceBounds()

void ShadowMapPassVk::createAttachments()
{
	depthImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		depthFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	depthImage_.createImageView(
		depthFormat_,
		vk::ImageAspectFlagBits::eDepth,
		vk::ImageViewType::e2D,
		1
	);
	depthImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToBorder,
		false
	);

	depthImage_.setDebugName("ShadowMapPassVk-DepthImage");
} // end of createAttachments()
