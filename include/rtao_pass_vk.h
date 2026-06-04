#ifndef RTAO_PASS_VK_H
#define RTAO_PASS_VK_H

#include "constants.h"

#include <vulkan/vulkan.hpp>

#include "image_vk.h"
#include "texture_2d_vk.h"
#include "shader_binding_table_vk.h"
#include "descriptor_set_vk.h"
#include "ray_tracing_pipeline_vk.h"
#include "acceleration_structure_vk.h"

#include <memory>
#include <vector>
#include <cstdint>

class VulkanMain;
class RayTracingShaderModuleVk;
struct FrameContext;
struct RenderInputs;
struct ChunkDrawList;
struct RenderSettings;

struct RTAOPassUBOs
{
	RTAO_Constants::RayGenUBO rayGenData{};
};

class RTAOPassVk
{
public:
	explicit RTAOPassVk(
		VulkanMain& vk,
		const RenderSettings& rs,
		const std::vector<AccelerationStructureVk>& tlas
	);
	~RTAOPassVk();

	void init();
	void resize();

	void render(
		const RTAOPassUBOs& ubos,
		const FrameContext& frame
	);

	void setInput(
		ImageVk& normalTex,
		ImageVk& depthTex
	)
	{
		normalTex_ = &normalTex;
		depthTex_ = &depthTex;
	} // end of setInput()

	const ImageVk& getOutColorImage() const { return outColorImage_; }
	ImageVk& getOutColorImage() { return outColorImage_; }

private:
	void syncSettings();
	void updateDescriptorSet(uint32_t frameIndex);
	void createOutputImage();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
	void createSBT();
private:
	VulkanMain& vk_;
	const RenderSettings& rs_;
	const std::vector<AccelerationStructureVk>& tlas_;

	ImageVk* normalTex_{ nullptr };
	ImageVk* depthTex_{ nullptr };

	uint32_t factor_{};
	uint32_t width_{};
	uint32_t height_{};

	ImageVk outColorImage_;
	vk::Format outImageFormat_{ vk::Format::eR16G16B16A16Sfloat };

	std::unique_ptr<RayTracingShaderModuleVk> shader_;

	std::vector<BufferVk> rayGenUBOs_;

	std::vector<DescriptorSetVk> rayGenDescriptorSets_;

	RayTracingPipelineVk pipeline_;
	ShaderBindingTableVk sbt_;
};

#endif
