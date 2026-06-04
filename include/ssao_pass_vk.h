#ifndef SSAO_PASS_VK_H
#define SSAO_PASS_VK_H

#include "constants.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"
#include "image_vk.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>
#include <array>
#include <vector>

class VulkanMain;
class ShaderModuleVk;
struct FrameContext;
struct RenderSettings;

struct SSAOPassUBOs
{
	SSAO_Constants::SSAOBlurUBO blurData{};
	SSAO_Constants::SSAORawSamplesUBO rawSamplesData{};
	SSAO_Constants::SSAORawUBO rawData{};
};

class SSAOPassVk
{
public:
	explicit SSAOPassVk(VulkanMain& vk, const RenderSettings& rs);
	~SSAOPassVk();

	void init();
	void resize();

	void render(
		const SSAOPassUBOs& ubos,
		const FrameContext& frame
	);

	void setInput(
		ImageVk& gNormalTex,
		ImageVk& gDepthTex
	)
	{
		gNormalTex_ = &gNormalTex;
		gDepthTex_ = &gDepthTex;
	} // end of setInput()

	ImageVk& ssaoBlurImage() { return ssaoBlurImage_; }
	vk::Extent2D getExtent() const { return { width_, height_ }; }
	const std::array<glm::vec4, SSAO_Constants::MAX_KERNEL_SIZE>& getSamples() const { return samples_; }

private:
	void syncSettings();
	void updateDescriptorSet(uint32_t frameIndex);
	void createAttachments();
	void createResources();
	void createDescriptorSets();
	void createPipelines();

	void createNoiseTexture();
	void createKernel();
private:
	VulkanMain& vk_;
	const RenderSettings& rs_;

	ImageVk* gNormalTex_{ nullptr };
	ImageVk* gDepthTex_{ nullptr };

	uint32_t factor_{};
	uint32_t width_{};
	uint32_t height_{};

	std::unique_ptr<ShaderModuleVk> ssaoRawShader_;
	std::unique_ptr<ShaderModuleVk> ssaoBlurShader_;

	ImageVk ssaoNoiseImage_;
	vk::Format noiseFormat_{ vk::Format::eR16G16B16A16Sfloat };

	ImageVk ssaoRawImage_;
	ImageVk ssaoBlurImage_;
	vk::Format singleChannelFormat_{ vk::Format::eR8Unorm };

	std::vector<BufferVk> ssaoRawSamplesUBOBuffers_;
	std::vector<BufferVk> ssaoRawUBOBuffers_;
	std::vector<BufferVk> ssaoBlurUBOBuffers_;

	std::vector<DescriptorSetVk> ssaoRawDescriptorSets_;
	std::vector<DescriptorSetVk> ssaoBlurDescriptorSets_;

	GraphicsPipelineVk ssaoRawPipeline_;
	GraphicsPipelineVk ssaoBlurPipeline_;

	std::array<glm::vec4, SSAO_Constants::MAX_KERNEL_SIZE> samples_{};
};

#endif
