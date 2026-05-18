#include "ray_tracing_world_pass_vk.h"

#include "bindings.h"

#include "render_inputs.h"
#include "chunk_draw_list.h"
#include "chunk_mesh_gpu_vk.h"

#include "camera.h"
#include "light_vk.h"

#include "vulkan_main.h"
#include "frame_context_vk.h"

#include "ray_tracing_shader_vk.h"

#include <glm/glm.hpp>

#include <vector>
#include <algorithm>

using namespace RT_Chunk_Constants;

//--- PUBLIC ---//
RayTracingWorldPassVk::RayTracingWorldPassVk(VulkanMain& vk)
	: vk_(vk),
	atlas_(vk),
	dudvTex_(vk),
	normalTex_(vk),
	outColorImage_(vk),
	outDepthImage_(vk),
	pipeline_(vk),
	sbt_(vk)
{
	packedRTOpaqueInfoBuffer_.reserve(vk_.getMaxFramesInFlight());
	packedRTOpaqueInfoBufferSize_.resize(vk_.getMaxFramesInFlight());
	packedRTOpaqueInfoBufferCapacity_.resize(vk_.getMaxFramesInFlight());

	packedRTWaterInfoBuffer_.reserve(vk_.getMaxFramesInFlight());
	packedRTWaterInfoBufferSize_.resize(vk_.getMaxFramesInFlight());
	packedRTWaterInfoBufferCapacity_.resize(vk_.getMaxFramesInFlight());

	rayGenUBOs_.reserve(vk_.getMaxFramesInFlight());
	missUBOs_.reserve(vk_.getMaxFramesInFlight());
	closestHitUBOs_.reserve(vk_.getMaxFramesInFlight());

	rayGenDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	missDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	closestHitOpaqueDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	closestHitWaterDescriptorSets_.reserve(vk_.getMaxFramesInFlight());

	tlas_.reserve(vk_.getMaxFramesInFlight());
	rtDescriptorsValid_.resize(vk_.getMaxFramesInFlight());

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		packedRTOpaqueInfoBuffer_.emplace_back(vk_);
		packedRTWaterInfoBuffer_.emplace_back(vk_);

		rayGenUBOs_.emplace_back(vk_);
		missUBOs_.emplace_back(vk_);
		closestHitUBOs_.emplace_back(vk_);

		rayGenDescriptorSets_.emplace_back(vk_);
		missDescriptorSets_.emplace_back(vk_);
		closestHitOpaqueDescriptorSets_.emplace_back(vk_);
		closestHitWaterDescriptorSets_.emplace_back(vk_);

		tlas_.emplace_back(vk_);
	} // end for
} // end of constructor

RayTracingWorldPassVk::~RayTracingWorldPassVk() = default;

void RayTracingWorldPassVk::init()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	width_ = extent.width / factor_;
	height_ = extent.height / factor_;

	shader_ = std::make_unique<RayTracingShaderModuleVk>(
		vk_.getDevice(),
		"raytracing/raygen.rgen.spv",
		"raytracing/miss.rmiss.spv",
		std::vector<HitGroupFilePath>{
			{
				"raytracing/closesthit_opaque.rchit.spv",
				"raytracing/anyhit_alpha_mask.rahit.spv"
			},
			{
				"raytracing/closesthit_water.rchit.spv",
				""
			}
		}
	);

	atlas_.loadFromFile("blocks_padded.png", true);
	dudvTex_.loadFromFile("dudv.png", true);
	normalTex_.loadFromFile("waternormal.png", true);

	createOutputImages();
	createResources();
	createDescriptorSet();
	createPipeline();
	createSBT();
} // end of init()

void RayTracingWorldPassVk::resize()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();
	width_ = extent.width / factor_;
	height_ = extent.height / factor_;

	outColorImage_.destroy();
	outDepthImage_.destroy();
	createOutputImages();

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		updateDescriptorSet(i);
	} // end for
} // end of resize()

void RayTracingWorldPassVk::upload(
	vk::CommandBuffer cmd,
	const ChunkDrawList& drawList,
	const glm::mat4& view,
	const glm::mat4& proj,
	uint32_t frameIndex
)
{
	cmd.beginDebugUtilsLabelEXT({ "RayTracingWorldPassVk-Upload::cmd" });

	rtSceneReady_ = false;

	std::vector<uint64_t> currentKeys;
	buildRTSceneKeys(drawList, currentKeys);

	if (currentKeys != lastSceneKeys_)
	{
		lastSceneKeys_ = currentKeys;
		rtSceneDirty_ = true;

		std::fill(
			rtDescriptorsValid_.begin(),
			rtDescriptorsValid_.end(),
			false
		);
	}

	AccelerationStructureVk& frameTLAS = tlas_[frameIndex];

	if (rtSceneDirty_ || !frameTLAS.valid())
	{
		RTPackedSceneCPU chunkCPUScene;
		buildPackedOpaqueRTSceneFromDrawList(drawList, chunkCPUScene);
		buildPackedWaterRTSceneFromDrawList(drawList, chunkCPUScene);

		if (chunkCPUScene.opaqueChunkInfos.empty())
		{
			packedRTOpaqueInfoBufferSize_[frameIndex] = 0;
		}

		if (chunkCPUScene.waterChunkInfos.empty())
		{
			packedRTWaterInfoBufferSize_[frameIndex] = 0;
		}

		if (chunkCPUScene.opaqueChunkInfos.empty() &&
			chunkCPUScene.waterChunkInfos.empty())
		{
			rtSceneReady_ = false;
			return;
		}

		uploadPackedRTScene(cmd, frameIndex, chunkCPUScene);

		std::vector<vk::AccelerationStructureInstanceKHR> instances;
		std::vector<vk::AccelerationStructureInstanceKHR> waterInstances;

		buildOpaqueRTInstancesFromDrawList(drawList, instances);
		buildWaterRTInstancesFromDrawList(drawList, waterInstances);

		instances.insert(
			instances.end(),
			waterInstances.begin(),
			waterInstances.end()
		);

		if (instances.empty())
		{
			packedRTOpaqueInfoBufferSize_[frameIndex] = 0;
			packedRTWaterInfoBufferSize_[frameIndex] = 0;
			rtSceneReady_ = false;
			return;
		}

		if (frameTLAS.valid())
		{
			vk_.retireAccelerationStructure(
				frameIndex,
				std::move(frameTLAS)
			);
			frameTLAS = AccelerationStructureVk(vk_);
		}

		frameTLAS.buildTLASOnCmd(cmd, instances);

		vk::MemoryBarrier asBarrier{};
		asBarrier.srcAccessMask =
			vk::AccessFlagBits::eAccelerationStructureWriteKHR;
		asBarrier.dstAccessMask =
			vk::AccessFlagBits::eAccelerationStructureReadKHR |
			vk::AccessFlagBits::eShaderRead;

		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
			vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			{},
			asBarrier,
			nullptr,
			nullptr
		);

		updateDescriptorSet(frameIndex);
		rtDescriptorsValid_[frameIndex] = true;
	}
	else if (!rtDescriptorsValid_[frameIndex])
	{
		updateDescriptorSet(frameIndex);
		rtDescriptorsValid_[frameIndex] = true;
	}

	rtSceneReady_ =
		frameTLAS.valid() &&
		(packedRTOpaqueInfoBufferSize_[frameIndex] > 0 ||
		packedRTWaterInfoBufferSize_[frameIndex] > 0);

	cmd.endDebugUtilsLabelEXT();
} // end of upload()

void RayTracingWorldPassVk::render(
	const RenderInputs& in,
	const FrameContext& frame,
	const glm::mat4& view,
	const glm::mat4& proj,
	const glm::vec3& sunDir
)
{
	if (!outColorImage_.valid() ||
		!outDepthImage_.valid() ||
		!rtSceneReady_ ||
		!rtDescriptorsValid_[frame.frameIndex])
	{
		return;
	}

	vk::CommandBuffer cmd = frame.cmd;

	cmd.beginDebugUtilsLabelEXT({ "RayTracingWorldPassVk::cmd" });

	outColorImage_.transitionToGeneral(cmd);
	outDepthImage_.transitionToGeneral(cmd);

	std::vector<vk::DescriptorSet> sets = {
		rayGenDescriptorSets_[frame.frameIndex].getSet(),
		missDescriptorSets_[frame.frameIndex].getSet(),
		closestHitOpaqueDescriptorSets_[frame.frameIndex].getSet(),
		closestHitWaterDescriptorSets_[frame.frameIndex].getSet(),
	};

	cmd.bindPipeline(
		vk::PipelineBindPoint::eRayTracingKHR,
		pipeline_.getPipeline()
	);

	cmd.bindDescriptorSets(
		vk::PipelineBindPoint::eRayTracingKHR,
		pipeline_.getLayout(),
		0,
		sets.size(),
		sets.data(),
		0,
		nullptr
	);

	vk::Extent2D extent = vk_.getSwapChainExtent();

	rayGenData_.u_invView = glm::inverse(view);
	rayGenData_.u_invProj = glm::inverse(proj);
	rayGenData_.u_view = view;
	rayGenData_.u_proj = proj;
	rayGenData_.u_cameraPos = glm::vec4(in.camera->getCameraPosition(), 1.0f);
	rayGenUBOs_[frame.frameIndex].upload(&rayGenData_, sizeof(rayGenData_));

	missData_.u_mix = glm::vec4(
			glm::clamp((sunDir.y + 0.15f) / 0.30f, 0.0f, 1.0f),
			0.0f,
			0.0f,
			0.0f
		);
	missUBOs_[frame.frameIndex].upload(&missData_, sizeof(missData_));

	closestHitData_.u_lightDir = glm::vec4(in.light->getDirection(), 0.0f);
	closestHitData_.u_lightColor = glm::vec4(in.light->getLightColor(), 0.0f);
	closestHitData_.u_time = in.time;
	closestHitUBOs_[frame.frameIndex].upload(&closestHitData_, sizeof(closestHitData_));

	cmd.traceRaysKHR(
		&sbt_.rayGenRegion(),
		&sbt_.missRegion(),
		&sbt_.hitRegion(),
		&sbt_.callableRegion(),
		width_,
		height_,
		1
	);

	outColorImage_.transitionToShaderRead(cmd);
	outDepthImage_.transitionToShaderRead(cmd);

	cmd.endDebugUtilsLabelEXT();
} // end of render()

void RayTracingWorldPassVk::setSkybox(
	uint32_t frameIndex,
	const TextureCubemapVk& nightTex,
	const TextureCubemapVk& dayTex
)
{
	missDescriptorSets_[frameIndex].writeCombinedImageSampler(
		TO_API_FORM(RTChunkMissBinding::NightSkyboxTex),
		nightTex.view(),
		nightTex.sampler()
	);

	missDescriptorSets_[frameIndex].writeCombinedImageSampler(
		TO_API_FORM(RTChunkMissBinding::DaySkyboxTex),
		dayTex.view(),
		dayTex.sampler()
	);
} // end of setSkybox()

void RayTracingWorldPassVk::updateDescriptorSet(uint32_t frameIndex)
{
	// RAYGEN SET
	{
		DescriptorSetVk& set = rayGenDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		set.writeStorageImage(
			TO_API_FORM(RTChunkRayGenBinding::OutColorImage),
			outColorImage_.view(),
			vk::ImageLayout::eGeneral
		);

		set.writeStorageImage(
			TO_API_FORM(RTChunkRayGenBinding::OutDepthImage),
			outDepthImage_.view(),
			vk::ImageLayout::eGeneral
		);

		if (tlas_[frameIndex].valid())
		{
			set.writeAccelerationStructure(
				TO_API_FORM(RTChunkRayGenBinding::TLAS),
				tlas_[frameIndex].handle()
			);
		}

		if (rayGenUBOs_[frameIndex].getBuffer())
		{
			set.writeUniformBuffer(
				TO_API_FORM(RTChunkRayGenBinding::UBO),
				rayGenUBOs_[frameIndex].getBuffer(),
				sizeof(RayGenUBO)
			);
		}
	}

	// MISS SET
	{
		DescriptorSetVk& set = missDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (missUBOs_[frameIndex].getBuffer())
		{
			set.writeUniformBuffer(
				TO_API_FORM(RTChunkMissBinding::UBO),
				missUBOs_[frameIndex].getBuffer(),
				sizeof(MissUBO)
			);
		}
	}

	// CLOSEST HIT SET (OPAQUE)
	{
		DescriptorSetVk& set = closestHitOpaqueDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (tlas_[frameIndex].valid())
		{
			set.writeAccelerationStructure(
				TO_API_FORM(RTOpaqueClosestHitBinding::TLAS),
				tlas_[frameIndex].handle()
			);
		}

		if (packedRTOpaqueInfoBufferSize_[frameIndex] > 0 &&
			packedRTOpaqueInfoBuffer_[frameIndex].getBuffer())
		{
			set.writeStorageBuffer(
				TO_API_FORM(RTOpaqueClosestHitBinding::ChunkInfo),
				packedRTOpaqueInfoBuffer_[frameIndex].getBuffer(),
				packedRTOpaqueInfoBufferSize_[frameIndex]
			);
		}

		if (closestHitUBOs_[frameIndex].getBuffer())
		{
			set.writeUniformBuffer(
				TO_API_FORM(RTOpaqueClosestHitBinding::UBO),
				closestHitUBOs_[frameIndex].getBuffer(),
				sizeof(ClosestHitUBO)
			);
		}

		set.writeCombinedImageSampler(
			TO_API_FORM(RTOpaqueClosestHitBinding::AtlasTex),
			atlas_.view(),
			atlas_.sampler()
		);
	}

	// CLOSEST HIT SET (WATER)
	{
		DescriptorSetVk& set = closestHitWaterDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (tlas_[frameIndex].valid())
		{
			set.writeAccelerationStructure(
				TO_API_FORM(RTWaterClosestHitBinding::TLAS),
				tlas_[frameIndex].handle()
			);
		}

		if (packedRTWaterInfoBufferSize_[frameIndex] > 0 &&
			packedRTWaterInfoBuffer_[frameIndex].getBuffer())
		{
			set.writeStorageBuffer(
				TO_API_FORM(RTWaterClosestHitBinding::WaterInfo),
				packedRTWaterInfoBuffer_[frameIndex].getBuffer(),
				packedRTWaterInfoBufferSize_[frameIndex]
			);
		}

		if (closestHitUBOs_[frameIndex].getBuffer())
		{
			set.writeUniformBuffer(
				TO_API_FORM(RTWaterClosestHitBinding::UBO),
				closestHitUBOs_[frameIndex].getBuffer(),
				sizeof(ClosestHitUBO)
			);
		}

		set.writeCombinedImageSampler(
			TO_API_FORM(RTWaterClosestHitBinding::DudvTex),
			dudvTex_.view(),
			dudvTex_.sampler()
		);

		set.writeCombinedImageSampler(
			TO_API_FORM(RTWaterClosestHitBinding::NormalTex),
			normalTex_.view(),
			normalTex_.sampler()
		);
	}
} // end of updateDescriptorSet()


//--- PRIVATE ---//
void RayTracingWorldPassVk::createOutputImages()
{
	vk::Extent2D extent = vk_.getSwapChainExtent();

	// output color image
	outColorImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		outImageFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eStorage |
		vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	outColorImage_.createImageView(
		outColorImage_.format(),
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	outColorImage_.createSampler(
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		false
	);

	// output depth image
	outDepthImage_.createImage(
		width_,
		height_,
		1,
		false,
		vk::SampleCountFlagBits::e1,
		outDepthFormat_,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eStorage |
		vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst |
		vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	outDepthImage_.createImageView(
		outDepthImage_.format(),
		vk::ImageAspectFlagBits::eColor,
		vk::ImageViewType::e2D,
		1
	);

	outDepthImage_.createSampler(
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eClampToEdge,
		false
	);
} // end of createOutputImages()

void RayTracingWorldPassVk::createResources()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		rayGenUBOs_[i].create(
			sizeof(RayGenUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);

		missUBOs_[i].create(
			sizeof(MissUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);
	
		closestHitUBOs_[i].create(
			sizeof(ClosestHitUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void RayTracingWorldPassVk::createDescriptorSet()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		// RAYGEN DS + POOL
		{
			vk::DescriptorSetLayoutBinding outColorBinding{};
			outColorBinding.binding = TO_API_FORM(RTChunkRayGenBinding::OutColorImage);
			outColorBinding.descriptorType = vk::DescriptorType::eStorageImage;
			outColorBinding.descriptorCount = 1;
			outColorBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			vk::DescriptorSetLayoutBinding outDepthBinding{};
			outDepthBinding.binding = TO_API_FORM(RTChunkRayGenBinding::OutDepthImage);
			outDepthBinding.descriptorType = vk::DescriptorType::eStorageImage;
			outDepthBinding.descriptorCount = 1;
			outDepthBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			vk::DescriptorSetLayoutBinding tlasBinding{};
			tlasBinding.binding = TO_API_FORM(RTChunkRayGenBinding::TLAS);
			tlasBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
			tlasBinding.descriptorCount = 1;
			tlasBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(RTChunkRayGenBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

			rayGenDescriptorSets_[i].createLayout({
				outColorBinding,
				outDepthBinding,
				tlasBinding,
				uboBinding
			});

			vk::DescriptorPoolSize outColorPool{};
			outColorPool.type = vk::DescriptorType::eStorageImage;
			outColorPool.descriptorCount = 1;

			vk::DescriptorPoolSize outDepthPool{};
			outDepthPool.type = vk::DescriptorType::eStorageImage;
			outDepthPool.descriptorCount = 1;

			vk::DescriptorPoolSize tlasPool{};
			tlasPool.type = vk::DescriptorType::eAccelerationStructureKHR;
			tlasPool.descriptorCount = 1;

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			rayGenDescriptorSets_[i].createPool({
				outColorPool,
				outDepthPool,
				tlasPool,
				uboPool
			});
			rayGenDescriptorSets_[i].allocate();

			rayGenDescriptorSets_[i].setDebugName(
				"RTChunkPassVk-RayGen::DescriptorSet frame " + std::to_string(i)
			);
		}

		// MISS DS + POOL
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(RTChunkMissBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eMissKHR;

			vk::DescriptorSetLayoutBinding nightTexBinding{};
			nightTexBinding.binding = TO_API_FORM(RTChunkMissBinding::NightSkyboxTex);
			nightTexBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			nightTexBinding.descriptorCount = 1;
			nightTexBinding.stageFlags = vk::ShaderStageFlagBits::eMissKHR;
			
			vk::DescriptorSetLayoutBinding dayTexBinding{};
			dayTexBinding.binding = TO_API_FORM(RTChunkMissBinding::DaySkyboxTex);
			dayTexBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			dayTexBinding.descriptorCount = 1;
			dayTexBinding.stageFlags = vk::ShaderStageFlagBits::eMissKHR;

			missDescriptorSets_[i].createLayout({
				uboBinding,
				nightTexBinding,
				dayTexBinding
			});

			vk::DescriptorPoolSize uboPool;
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize nightTexPool;
			nightTexPool.type = vk::DescriptorType::eCombinedImageSampler;
			nightTexPool.descriptorCount = 1;

			vk::DescriptorPoolSize dayTexPool;
			dayTexPool.type = vk::DescriptorType::eCombinedImageSampler;
			dayTexPool.descriptorCount = 1;

			missDescriptorSets_[i].createPool({
				uboPool,
				nightTexPool,
				dayTexPool
			});
			missDescriptorSets_[i].allocate();

			missDescriptorSets_[i].setDebugName(
				"RTChunkPassVk-Miss::DescriptorSet frame " + std::to_string(i)
			);
		}

		// CLOSEST HIT DS + POOL (OPAQUE)
		{
			vk::DescriptorSetLayoutBinding tlasBinding{};
			tlasBinding.binding = TO_API_FORM(RTOpaqueClosestHitBinding::TLAS);
			tlasBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
			tlasBinding.descriptorCount = 1;
			tlasBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			vk::DescriptorSetLayoutBinding chunkInfoBinding{};
			chunkInfoBinding.binding = TO_API_FORM(RTOpaqueClosestHitBinding::ChunkInfo);
			chunkInfoBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
			chunkInfoBinding.descriptorCount = 1;
			chunkInfoBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR |
				vk::ShaderStageFlagBits::eAnyHitKHR;

			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(RTOpaqueClosestHitBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			vk::DescriptorSetLayoutBinding atlasBinding{};
			atlasBinding.binding = TO_API_FORM(RTOpaqueClosestHitBinding::AtlasTex);
			atlasBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			atlasBinding.descriptorCount = 1;
			atlasBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR |
				vk::ShaderStageFlagBits::eAnyHitKHR;

			closestHitOpaqueDescriptorSets_[i].createLayout({
				tlasBinding,
				chunkInfoBinding,
				uboBinding,
				atlasBinding
			});

			vk::DescriptorPoolSize tlasPool{};
			tlasPool.type = vk::DescriptorType::eAccelerationStructureKHR;
			tlasPool.descriptorCount = 1;

			vk::DescriptorPoolSize chunkInfoPool{};
			chunkInfoPool.type = vk::DescriptorType::eStorageBuffer;
			chunkInfoPool.descriptorCount = 1;

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize atlasPool{};
			atlasPool.type = vk::DescriptorType::eCombinedImageSampler;
			atlasPool.descriptorCount = 1;

			closestHitOpaqueDescriptorSets_[i].createPool({
				tlasPool,
				chunkInfoPool,
				uboPool,
				atlasPool
			});
			closestHitOpaqueDescriptorSets_[i].allocate();

			closestHitOpaqueDescriptorSets_[i].setDebugName(
				"RTChunkPassVk-ClosestHitOpaque::DescriptorSet frame " + std::to_string(i)
			);
		}

		// CLOSEST HIT DS + POOL (WATER)
		{
			vk::DescriptorSetLayoutBinding tlasBinding{};
			tlasBinding.binding = TO_API_FORM(RTWaterClosestHitBinding::TLAS);
			tlasBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
			tlasBinding.descriptorCount = 1;
			tlasBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			vk::DescriptorSetLayoutBinding waterInfoBinding{};
			waterInfoBinding.binding = TO_API_FORM(RTWaterClosestHitBinding::WaterInfo);
			waterInfoBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
			waterInfoBinding.descriptorCount = 1;
			waterInfoBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(RTWaterClosestHitBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			vk::DescriptorSetLayoutBinding dudvBinding{};
			dudvBinding.binding = TO_API_FORM(RTWaterClosestHitBinding::DudvTex);
			dudvBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			dudvBinding.descriptorCount = 1;
			dudvBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			vk::DescriptorSetLayoutBinding normalBinding{};
			normalBinding.binding = TO_API_FORM(RTWaterClosestHitBinding::NormalTex);
			normalBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			normalBinding.descriptorCount = 1;
			normalBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			closestHitWaterDescriptorSets_[i].createLayout({
				tlasBinding,
				waterInfoBinding,
				uboBinding,
				dudvBinding,
				normalBinding
			});

			vk::DescriptorPoolSize tlasPool{};
			tlasPool.type = vk::DescriptorType::eAccelerationStructureKHR;
			tlasPool.descriptorCount = 1;

			vk::DescriptorPoolSize waterInfoPool{};
			waterInfoPool.type = vk::DescriptorType::eStorageBuffer;
			waterInfoPool.descriptorCount = 1;

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize dudvPool{};
			dudvPool.type = vk::DescriptorType::eCombinedImageSampler;
			dudvPool.descriptorCount = 1;

			vk::DescriptorPoolSize normalPool{};
			normalPool.type = vk::DescriptorType::eCombinedImageSampler;
			normalPool.descriptorCount = 1;

			closestHitWaterDescriptorSets_[i].createPool({
				tlasPool,
				waterInfoPool,
				uboPool,
				dudvPool,
				normalPool
			});
			closestHitWaterDescriptorSets_[i].allocate();

			closestHitWaterDescriptorSets_[i].setDebugName(
				"RTChunkPassVk-ClosestHitWater::DescriptorSet frame " + std::to_string(i)
			);
		}
	} // end for
} // end of createDescriptorSet()

void RayTracingWorldPassVk::createPipeline()
{
	RayTracingPipelineDescVk desc{};
	desc.rayGenShader = shader_->rayGenShader();
	desc.missShader = shader_->missShader();

	const std::vector<HitGroupShaderModules>& hitGroupShaderModules = shader_->hitGroupShaders();
	desc.hitGroups.reserve(hitGroupShaderModules.size());
	for (const HitGroupShaderModules& hg : hitGroupShaderModules)
	{
		desc.hitGroups.push_back({
			hg.closestHitShaderModule.get(),
			hg.anyHitShaderModule.get()
			});
	} // end for
	
	desc.setLayouts =
	{
		{0, rayGenDescriptorSets_[0].getLayout()},
		{1, missDescriptorSets_[0].getLayout()},
		{2, closestHitOpaqueDescriptorSets_[0].getLayout()},
		{3, closestHitWaterDescriptorSets_[0].getLayout()}
	};

	desc.maxRecursionDepth = 3;

	pipeline_.create(desc);

	pipeline_.setDebugName("RayTracingWorldPassVk::Pipeline");
} // end of createPipeline()

void RayTracingWorldPassVk::createSBT()
{
	sbt_.create(
		pipeline_.getPipeline(),
		4,
		0,
		1,
		{ 2, 3 }
	);
} // end of createSBT()

void RayTracingWorldPassVk::buildOpaqueRTInstancesFromDrawList(
	const ChunkDrawList& opaqueDrawList,
	std::vector<vk::AccelerationStructureInstanceKHR>& out
)
{
	out.clear();

	for (const auto& item : opaqueDrawList.items)
	{
		if (!item.gpu)
			continue;

		auto* chunkGpuVk = dynamic_cast<ChunkMeshGPUVk*>(item.gpu.get());
		if (!chunkGpuVk)
			continue;

		if (!chunkGpuVk->getOpaqueBLAS().valid())
			continue;

		const auto& chunkVerts = chunkGpuVk->getOpaqueRTVerticesCPU();
		const auto& chunkIndices = chunkGpuVk->getOpaqueRTIndicesCPU();

		if (chunkVerts.empty() || chunkIndices.empty())
			continue;

		vk::AccelerationStructureInstanceKHR inst{};

		inst.transform.matrix[0][0] = 1.0f;
		inst.transform.matrix[0][1] = 0.0f;
		inst.transform.matrix[0][2] = 0.0f;
		inst.transform.matrix[0][3] = item.chunkOrigin.x;

		inst.transform.matrix[1][0] = 0.0f;
		inst.transform.matrix[1][1] = 1.0f;
		inst.transform.matrix[1][2] = 0.0f;
		inst.transform.matrix[1][3] = item.chunkOrigin.y;

		inst.transform.matrix[2][0] = 0.0f;
		inst.transform.matrix[2][1] = 0.0f;
		inst.transform.matrix[2][2] = 1.0f;
		inst.transform.matrix[2][3] = item.chunkOrigin.z;

		inst.instanceCustomIndex = static_cast<uint32_t>(out.size());
		inst.mask = 0x01;
		inst.instanceShaderBindingTableRecordOffset = 0;
		inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		inst.accelerationStructureReference = chunkGpuVk->getOpaqueBLAS().deviceAddress();

		out.push_back(inst);
	} // end for
} // end of buildOpaqueRTInstancesFromDrawList()

void RayTracingWorldPassVk::buildWaterRTInstancesFromDrawList(
	const ChunkDrawList& waterDrawList,
	std::vector<vk::AccelerationStructureInstanceKHR>& out
)
{
	out.clear();

	for (const auto& item : waterDrawList.items)
	{
		if (!item.gpu)
			continue;

		auto* chunkGpuVk = dynamic_cast<ChunkMeshGPUVk*>(item.gpu.get());
		if (!chunkGpuVk)
			continue;

		if (!chunkGpuVk->getWaterBLAS().valid())
			continue;

		const auto& waterVerts = chunkGpuVk->getWaterRTVerticesCPU();
		const auto& waterIndices = chunkGpuVk->getWaterRTIndicesCPU();

		if (waterVerts.empty() || waterIndices.empty())
			continue;

		vk::AccelerationStructureInstanceKHR inst{};

		inst.transform.matrix[0][0] = 1.0f;
		inst.transform.matrix[0][1] = 0.0f;
		inst.transform.matrix[0][2] = 0.0f;
		inst.transform.matrix[0][3] = item.chunkOrigin.x;

		inst.transform.matrix[1][0] = 0.0f;
		inst.transform.matrix[1][1] = 1.0f;
		inst.transform.matrix[1][2] = 0.0f;
		inst.transform.matrix[1][3] = item.chunkOrigin.y;

		inst.transform.matrix[2][0] = 0.0f;
		inst.transform.matrix[2][1] = 0.0f;
		inst.transform.matrix[2][2] = 1.0f;
		inst.transform.matrix[2][3] = item.chunkOrigin.z;

		inst.instanceCustomIndex = static_cast<uint32_t>(out.size());
		inst.mask = 0x02;
		inst.instanceShaderBindingTableRecordOffset = 1;
		inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		inst.accelerationStructureReference = chunkGpuVk->getWaterBLAS().deviceAddress();

		out.push_back(inst);
	} // end for
} // end of buildWaterRTInstancesFromDrawList()

void RayTracingWorldPassVk::buildPackedOpaqueRTSceneFromDrawList(
	const ChunkDrawList& opaqueDrawList,
	RTPackedSceneCPU& out
)
{
	out.opaqueChunkInfos.clear();

	for (const auto& item : opaqueDrawList.items)
	{
		if (!item.gpu)
			continue;

		auto* chunkGpuVk = dynamic_cast<ChunkMeshGPUVk*>(item.gpu.get());
		if (!chunkGpuVk)
			continue;

		if (!chunkGpuVk->getOpaqueBLAS().valid())
			continue;

		const std::vector<World::RTVertex>& chunkVerts = chunkGpuVk->getOpaqueRTVerticesCPU();
		const std::vector<uint32_t>& chunkIndices = chunkGpuVk->getOpaqueRTIndicesCPU();

		if (chunkVerts.empty() || chunkIndices.empty())
			continue;

		World::RTChunkInfo info{};
		info.vertexAddress = chunkGpuVk->getOpaqueRTVertexAddress();
		info.indexAddress = chunkGpuVk->getOpaqueRTIndexAddress();
		info.countsPad.x = static_cast<uint32_t>(chunkVerts.size());
		info.countsPad.y = static_cast<uint32_t>(chunkIndices.size());
		info.chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

		out.opaqueChunkInfos.push_back(info);
	} // end for
} // end of buildPackedOpaqueRTSceneFromDrawList()

void RayTracingWorldPassVk::buildPackedWaterRTSceneFromDrawList(
	const ChunkDrawList& waterDrawList,
	RTPackedSceneCPU& out
)
{
	out.waterChunkInfos.clear();

	for (const auto& item : waterDrawList.items)
	{
		if (!item.gpu)
			continue;

		auto* chunkGpuVk = dynamic_cast<ChunkMeshGPUVk*>(item.gpu.get());
		if (!chunkGpuVk)
			continue;

		if (!chunkGpuVk->getWaterBLAS().valid())
			continue;

		const std::vector<World::RTVertex>& chunkVerts = chunkGpuVk->getWaterRTVerticesCPU();
		const std::vector<uint32_t>& chunkIndices = chunkGpuVk->getWaterRTIndicesCPU();

		if (chunkVerts.empty() || chunkIndices.empty())
			continue;

		World::RTChunkInfo info{};
		info.vertexAddress = chunkGpuVk->getWaterRTVertexAddress();
		info.indexAddress = chunkGpuVk->getWaterRTIndexAddress();
		info.countsPad.x = static_cast<uint32_t>(chunkVerts.size());
		info.countsPad.y = static_cast<uint32_t>(chunkIndices.size());
		info.chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

		out.waterChunkInfos.push_back(info);
	} // end for
} // end of buildPackedOpaqueRTSceneFromDrawList()

void RayTracingWorldPassVk::uploadPackedRTScene(
	vk::CommandBuffer cmd,
	uint32_t frameIndex,
	const RTPackedSceneCPU& cpuScene
)
{
	std::vector<BufferVk> stagingBuffers;
	stagingBuffers.reserve(2);

	packedRTOpaqueInfoBufferSize_[frameIndex] =
		sizeof(World::RTChunkInfo) * cpuScene.opaqueChunkInfos.size();

	packedRTWaterInfoBufferSize_[frameIndex] =
		sizeof(World::RTChunkInfo) * cpuScene.waterChunkInfos.size();

	// packed opaque info buffer
	if (packedRTOpaqueInfoBufferSize_[frameIndex] > 0)
	{
		BufferVk staging(vk_);
		staging.create(
			packedRTOpaqueInfoBufferSize_[frameIndex],
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);

		staging.upload(
			cpuScene.opaqueChunkInfos.data(),
			packedRTOpaqueInfoBufferSize_[frameIndex]
		);

		if (!packedRTOpaqueInfoBuffer_[frameIndex].getBuffer() ||
			packedRTOpaqueInfoBufferSize_[frameIndex] > packedRTOpaqueInfoBufferCapacity_[frameIndex])
		{
			if (packedRTOpaqueInfoBuffer_[frameIndex].valid())
			{
				vk_.retireBuffer(frameIndex, std::move(packedRTOpaqueInfoBuffer_[frameIndex]));
			}

			packedRTOpaqueInfoBuffer_[frameIndex] = BufferVk(vk_);
			packedRTOpaqueInfoBuffer_[frameIndex].create(
				packedRTOpaqueInfoBufferSize_[frameIndex],
				vk::BufferUsageFlagBits::eTransferDst |
				vk::BufferUsageFlagBits::eStorageBuffer,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);

			packedRTOpaqueInfoBufferCapacity_[frameIndex] = packedRTOpaqueInfoBufferSize_[frameIndex];
		}

		stagingBuffers.push_back(std::move(staging));

		vk_.recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			packedRTOpaqueInfoBuffer_[frameIndex].getBuffer(),
			packedRTOpaqueInfoBufferSize_[frameIndex]
		);
	}

	// packed water info buffer
	if (packedRTWaterInfoBufferSize_[frameIndex] > 0)
	{
		BufferVk staging(vk_);
		staging.create(
			packedRTWaterInfoBufferSize_[frameIndex],
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);

		staging.upload(
			cpuScene.waterChunkInfos.data(),
			packedRTWaterInfoBufferSize_[frameIndex]
		);

		if (!packedRTWaterInfoBuffer_[frameIndex].getBuffer() ||
			packedRTWaterInfoBufferSize_[frameIndex] > packedRTWaterInfoBufferCapacity_[frameIndex])
		{
			if (packedRTWaterInfoBuffer_[frameIndex].valid())
			{
				vk_.retireBuffer(frameIndex, std::move(packedRTWaterInfoBuffer_[frameIndex]));
			}

			packedRTWaterInfoBuffer_[frameIndex] = BufferVk(vk_);
			packedRTWaterInfoBuffer_[frameIndex].create(
				packedRTWaterInfoBufferSize_[frameIndex],
				vk::BufferUsageFlagBits::eTransferDst |
				vk::BufferUsageFlagBits::eStorageBuffer,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);

			packedRTWaterInfoBufferCapacity_[frameIndex] = packedRTWaterInfoBufferSize_[frameIndex];
		}

		stagingBuffers.push_back(std::move(staging));

		vk_.recordCopyBuffer(
			cmd,
			stagingBuffers.back().getBuffer(),
			packedRTWaterInfoBuffer_[frameIndex].getBuffer(),
			packedRTWaterInfoBufferSize_[frameIndex]
		);
	}

	for (auto& staging : stagingBuffers)
	{
		vk_.retireBuffer(frameIndex, std::move(staging));
	} // end for

	std::vector<vk::BufferMemoryBarrier> barriers;

	auto addBarrier = [&](BufferVk& buffer, vk::DeviceSize size)
		{
			if (!buffer.getBuffer() || size == 0)
				return;

			vk::BufferMemoryBarrier b{};
			b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			b.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.buffer = buffer.getBuffer();
			b.offset = 0;
			b.size = size;

			barriers.push_back(b);
		};

	addBarrier(packedRTOpaqueInfoBuffer_[frameIndex], packedRTOpaqueInfoBufferSize_[frameIndex]);
	addBarrier(packedRTWaterInfoBuffer_[frameIndex], packedRTWaterInfoBufferSize_[frameIndex]);

	if (!barriers.empty())
	{
		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			{},
			0, nullptr,
			static_cast<uint32_t>(barriers.size()), barriers.data(),
			0, nullptr
		);
	}
} // end of uploadPackedRTScene()

void RayTracingWorldPassVk::buildRTSceneKeys(
	const ChunkDrawList& rtDrawList,
	std::vector<uint64_t>& out
)
{
	out.clear();
	out.reserve(rtDrawList.items.size());

	for (const auto& item : rtDrawList.items)
	{
		if (!item.gpu)
			continue;

		auto* chunkGpuVk = dynamic_cast<ChunkMeshGPUVk*>(item.gpu.get());
		if (!chunkGpuVk)
			continue;

		bool hasOpaque =
			chunkGpuVk->getOpaqueBLAS().valid() &&
			!chunkGpuVk->getOpaqueRTVerticesCPU().empty() &&
			!chunkGpuVk->getOpaqueRTIndicesCPU().empty();

		bool hasWater =
			chunkGpuVk->getOpaqueBLAS().valid() &&
			!chunkGpuVk->getWaterRTVerticesCPU().empty() &&
			!chunkGpuVk->getWaterRTIndicesCPU().empty();

		if (!hasOpaque && !hasWater)
			continue;

		const uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(
			static_cast<int32_t>(item.chunkOrigin.x)));
		const uint64_t y = static_cast<uint64_t>(static_cast<uint32_t>(
			static_cast<int32_t>(item.chunkOrigin.y)));
		const uint64_t z = static_cast<uint64_t>(static_cast<uint32_t>(
			static_cast<int32_t>(item.chunkOrigin.z)));

		const uint64_t posKey =
			(x << 42) ^ (y << 21) ^ z;

		// mix position with geometry version
		const uint64_t versionKey =
			item.geometryVersion * 0x9E3779B185EBCA87ull;
		out.push_back(posKey ^ versionKey);
	} // end for

	std::sort(out.begin(), out.end());
} // end of buildRTSceneKeys()