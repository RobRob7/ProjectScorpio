#ifndef RAY_TRACING_PASS_VK_H
#define RAY_TRACING_PASS_VK_H

#include "constants.h"

#include <vulkan/vulkan.hpp>

#include "image_vk.h"
#include "shader_binding_table_vk.h"
#include "descriptor_set_vk.h"
#include "ray_tracing_pipeline_vk.h"
#include "acceleration_structure_vk.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>
#include <cstdint>

class VulkanMain;
class RayTracingShaderModuleVk;
struct FrameContext;
struct RenderInputs;

class RayTracingPassVk
{
public:
	explicit RayTracingPassVk(VulkanMain& vk);
	~RayTracingPassVk();

	void init();
	void resize();

	void render(
		const RenderInputs& in, 
		const FrameContext& frame,
		const glm::mat4& view,
		const glm::mat4& proj
	);

	void setTopLevelAS(
		uint32_t frameIndex,
		vk::AccelerationStructureKHR tlas
	);

	void updateDescriptorSet(uint32_t frameIndex);

	bool valid() const { return outputImage_.valid(); }

	ImageVk& getOutputImageVk() { return outputImage_; }
	const ImageVk& getOutputImageVk() const { return outputImage_; }

	vk::ImageLayout& getOutputLayout() { return outputLayout_; }
	const vk::ImageLayout& getOutputLayout() const { return outputLayout_; }

private:
	void createOutputImage();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
	void createSBT();
private:
	VulkanMain& vk_;

	ImageVk outputImage_;

	std::unique_ptr<RayTracingShaderModuleVk> shader_;

	std::vector<BufferVk> cameraUBOs_;
	RayTracing::RTCameraUBO cameraData_{};

	std::vector<DescriptorSetVk> descriptorSets_;
	RayTracingPipelineVk pipeline_;
	ShaderBindingTableVk sbt_;
	vk::AccelerationStructureKHR topLevelAS_{};

	vk::ImageLayout outputLayout_ = vk::ImageLayout::eGeneral;
};

#endif
