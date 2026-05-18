#ifndef RAY_TRACING_WORLD_PASS_VK_H
#define RAY_TRACING_WORLD_PASS_VK_H

#include "constants.h"

#include <vulkan/vulkan.hpp>

#include "buffer_vk.h"
#include "image_vk.h"
#include "texture_2d_vk.h"
#include "texture_cubemap_vk.h"
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
struct ChunkDrawList;

struct RTPackedSceneCPU
{
	std::vector<World::RTChunkInfo> opaqueChunkInfos;
	std::vector<World::RTChunkInfo> waterChunkInfos;
};

class RayTracingWorldPassVk
{
public:
	explicit RayTracingWorldPassVk(VulkanMain& vk);
	~RayTracingWorldPassVk();

	void init();
	void resize();

	void upload(
		vk::CommandBuffer cmd,
		const ChunkDrawList& drawList,
		const glm::mat4& view,
		const glm::mat4& proj,
		uint32_t frameIndex
	);

	void render(
		const RenderInputs& in, 
		const FrameContext& frame,
		const glm::mat4& view,
		const glm::mat4& proj,
		const glm::vec3& sunDir
	);

	void setSkybox(
		uint32_t frameIndex,
		const TextureCubemapVk& nightTex,
		const TextureCubemapVk& dayTex
	);

	void updateDescriptorSet(uint32_t frameIndex);

	const ImageVk& getOutColorImage() const { return outColorImage_; }
	ImageVk& getOutColorImage() { return outColorImage_; }
	const ImageVk& getOutDepthImage() const { return outDepthImage_; }
	ImageVk& getOutDepthImage() { return outDepthImage_; }

private:
	void createOutputImages();
	void createResources();
	void createDescriptorSet();
	void createPipeline();
	void createSBT();

	void buildOpaqueRTInstancesFromDrawList(
		const ChunkDrawList& opaqueDrawList,
		std::vector<vk::AccelerationStructureInstanceKHR>& out
	);
	void buildWaterRTInstancesFromDrawList(
		const ChunkDrawList& waterDrawList,
		std::vector<vk::AccelerationStructureInstanceKHR>& out
	);

	void buildPackedOpaqueRTSceneFromDrawList(
		const ChunkDrawList& opaqueDrawList,
		RTPackedSceneCPU& out
	);
	void buildPackedWaterRTSceneFromDrawList(
		const ChunkDrawList& waterDrawList,
		RTPackedSceneCPU& out
	);


	void uploadPackedRTScene(
		vk::CommandBuffer cmd,
		uint32_t frameIndex,
		const RTPackedSceneCPU& cpuScene
	);

	void buildRTSceneKeys(
		const ChunkDrawList& rtDrawList,
		std::vector<uint64_t>& out
	);
private:
	VulkanMain& vk_;

	int factor_{ 1 };
	uint32_t width_{};
	uint32_t height_{};

	bool rtSceneReady_{ false };

	std::vector<uint64_t> lastSceneKeys_;
	bool rtSceneDirty_{ true };

	ImageVk outColorImage_;
	vk::Format outImageFormat_{ vk::Format::eR16G16B16A16Sfloat };
	ImageVk outDepthImage_;
	vk::Format outDepthFormat_{ vk::Format::eR32Sfloat };

	Texture2DVk atlas_;
	Texture2DVk dudvTex_;
	Texture2DVk normalTex_;

	std::unique_ptr<RayTracingShaderModuleVk> shader_;

	// UBOs
	std::vector<BufferVk> rayGenUBOs_;
	RT_Chunk_Constants::RayGenUBO rayGenData_{};
	std::vector<BufferVk> missUBOs_;
	RT_Chunk_Constants::MissUBO missData_{};
	std::vector<BufferVk> closestHitUBOs_;
	RT_Chunk_Constants::ClosestHitUBO closestHitData_{};

	std::vector<BufferVk> packedRTOpaqueInfoBuffer_;
	std::vector<vk::DeviceSize> packedRTOpaqueInfoBufferSize_;
	std::vector<vk::DeviceSize> packedRTOpaqueInfoBufferCapacity_;

	std::vector<BufferVk> packedRTWaterInfoBuffer_;
	std::vector<vk::DeviceSize> packedRTWaterInfoBufferSize_;
	std::vector<vk::DeviceSize> packedRTWaterInfoBufferCapacity_;

	std::vector<DescriptorSetVk> rayGenDescriptorSets_;
	std::vector<DescriptorSetVk> missDescriptorSets_;
	std::vector<DescriptorSetVk> closestHitOpaqueDescriptorSets_;
	std::vector<DescriptorSetVk> closestHitWaterDescriptorSets_;

	RayTracingPipelineVk pipeline_;
	ShaderBindingTableVk sbt_;
	std::vector<AccelerationStructureVk> tlas_;
	std::vector<bool> rtDescriptorsValid_;
};

#endif
