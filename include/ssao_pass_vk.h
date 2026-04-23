#ifndef SSAO_PASS_VK_H
#define SSAO_PASS_VK_H

#include "constants.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <memory>
#include <array>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct FrameContext;

class SSAOPassVk
{
public:
	SSAOPassVk(
		VulkanMain& vk,
		const ImageVk& gNormalImage,
		const ImageVk& gDepthImage
	);
	~SSAOPassVk();

	void init();
	void resize();

	void renderOffscreen(
		const FrameContext& frame,
		const glm::mat4& proj
	);

	const ImageVk& ssaoBlurImage() const { return ssaoBlurImage_; }

private:
	void createAttachments();
	void createResources();
	void createDescriptorSets();
	void createPipelines();

	void createNoiseTexture();
	void createKernel();
private:
	VulkanMain& vk_;

	const ImageVk& gNormalImage_;
	const ImageVk& gDepthImage_;

	std::unique_ptr<ShaderModuleVk> ssaoRawShader_;
	std::unique_ptr<ShaderModuleVk> ssaoBlurShader_;

	ImageVk ssaoNoiseImage_;
	vk::Format noiseFormat_ = vk::Format::eR16G16B16A16Sfloat;

	ImageVk ssaoRawImage_;
	vk::ImageLayout singleChannelRawLayout_ = vk::ImageLayout::eUndefined;

	ImageVk ssaoBlurImage_;
	vk::Format singleChannelFormat_ = vk::Format::eR8Unorm;
	vk::ImageLayout singleChannelBlurLayout_ = vk::ImageLayout::eUndefined;

	std::vector<BufferVk> ssaoRawUBOBuffers_;
	SSAO_Constants::SSAORawUBO rawUBO_{};
	std::vector<BufferVk> ssaoBlurUBOBuffers_;
	SSAO_Constants::SSAOBlurUBO blurUBO_{};

	std::vector<DescriptorSetVk> ssaoRawDescriptorSets_;
	std::vector<DescriptorSetVk> ssaoBlurDescriptorSets_;

	GraphicsPipelineVk ssaoRawPipeline_;
	GraphicsPipelineVk ssaoBlurPipeline_;

	std::array<glm::vec4, SSAO_Constants::MAX_KERNEL_SIZE> samples_{};
};

#endif
