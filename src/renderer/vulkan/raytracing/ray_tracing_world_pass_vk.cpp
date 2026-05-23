#include "ray_tracing_world_pass_vk.h"

#include "bindings.h"

#include "chunk_manager.h"
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

//--- PUBLIC ---//
RayTracingWorldPassVk::RayTracingWorldPassVk(
	VulkanMain& vk,
	const std::vector<AccelerationStructureVk>& tlas,
	const std::vector<BufferVk>& packedRTOpaqueInfoBuffer,
	const std::vector<vk::DeviceSize>& packedRTOpaqueInfoBufferSize,
	const std::vector<BufferVk>& packedRTWaterInfoBuffer,
	const std::vector<vk::DeviceSize>& packedRTWaterInfoBufferSize
)
	: vk_(vk),
	tlas_(tlas),
	packedRTOpaqueInfoBuffer_(packedRTOpaqueInfoBuffer),
	packedRTOpaqueInfoBufferSize_(packedRTOpaqueInfoBufferSize),
	packedRTWaterInfoBuffer_(packedRTWaterInfoBuffer),
	packedRTWaterInfoBufferSize_(packedRTWaterInfoBufferSize),
	atlas_(vk),
	dudvTex_(vk),
	normalTex_(vk),
	outColorImage_(vk),
	outDepthImage_(vk),
	pipeline_(vk),
	sbt_(vk)
{
	rayGenUBOs_.reserve(vk_.getMaxFramesInFlight());
	missUBOs_.reserve(vk_.getMaxFramesInFlight());
	closestHitOpaqueUBOs_.reserve(vk_.getMaxFramesInFlight());
	closestHitWaterUBOs_.reserve(vk_.getMaxFramesInFlight());

	rayGenDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	missDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	closestHitOpaqueDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	closestHitWaterDescriptorSets_.reserve(vk_.getMaxFramesInFlight());

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		rayGenUBOs_.emplace_back(vk_);
		missUBOs_.emplace_back(vk_);
		closestHitOpaqueUBOs_.emplace_back(vk_);
		closestHitWaterUBOs_.emplace_back(vk_);

		rayGenDescriptorSets_.emplace_back(vk_);
		missDescriptorSets_.emplace_back(vk_);
		closestHitOpaqueDescriptorSets_.emplace_back(vk_);
		closestHitWaterDescriptorSets_.emplace_back(vk_);
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
	atlas_.setDebugName("RayTracingWorldPassVk-AtlasTexture");

	dudvTex_.loadFromFile("dudv.png", true);
	dudvTex_.setDebugName("RayTracingWorldPassVk-DuDvTexture");

	normalTex_.loadFromFile("waternormal.png", true);
	normalTex_.setDebugName("RayTracingWorldPassVk-NormalTexture");

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
		!rtaoTex_->valid()
		)
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

	closestHitOpaqueData_.u_lightDir = glm::vec4(in.light->getDirection(), 0.0f);
	closestHitOpaqueData_.u_lightColor = glm::vec4(in.light->getLightColor(), 0.0f);
	closestHitOpaqueData_.u_ambStr = in.world->getAmbientStrength();
	closestHitOpaqueUBOs_[frame.frameIndex].upload(&closestHitOpaqueData_, sizeof(closestHitOpaqueData_));
	
	closestHitWaterData_.u_lightDir = glm::vec4(in.light->getDirection(), 0.0f);
	closestHitWaterData_.u_lightColor = glm::vec4(in.light->getLightColor(), 0.0f);
	closestHitWaterData_.u_time = in.time;
	closestHitWaterUBOs_[frame.frameIndex].upload(&closestHitWaterData_, sizeof(closestHitWaterData_));

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
				sizeof(RT_Chunk_Constants::RayGenUBO)
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
				sizeof(RT_Chunk_Constants::MissUBO)
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

		if (closestHitOpaqueUBOs_[frameIndex].getBuffer())
		{
			set.writeUniformBuffer(
				TO_API_FORM(RTOpaqueClosestHitBinding::UBO),
				closestHitOpaqueUBOs_[frameIndex].getBuffer(),
				sizeof(RT_Chunk_Constants::ClosestHitUBO)
			);
		}

		if (atlas_.valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(RTOpaqueClosestHitBinding::AtlasTex),
				atlas_.view(),
				atlas_.sampler()
			);
		}

		if (rtaoTex_ && rtaoTex_->valid())
		{
			set.writeCombinedImageSampler(
				TO_API_FORM(RTOpaqueClosestHitBinding::RTAOTex),
				rtaoTex_->view(),
				rtaoTex_->sampler()
			);
		}
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

		if (closestHitWaterUBOs_[frameIndex].getBuffer())
		{
			set.writeUniformBuffer(
				TO_API_FORM(RTWaterClosestHitBinding::UBO),
				closestHitWaterUBOs_[frameIndex].getBuffer(),
				sizeof(RT_Water_Constants::ClosestHitUBO)
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

	outColorImage_.setDebugName("RayTracingWorldPassVk-ColorImage");

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

	outDepthImage_.setDebugName("RayTracingWorldPassVk-DepthImage");
} // end of createOutputImages()

void RayTracingWorldPassVk::createResources()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		rayGenUBOs_[i].create(
			sizeof(RT_Chunk_Constants::RayGenUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);

		missUBOs_[i].create(
			sizeof(RT_Chunk_Constants::MissUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);
	
		closestHitOpaqueUBOs_[i].create(
			sizeof(RT_Chunk_Constants::ClosestHitUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent
		);

		closestHitWaterUBOs_[i].create(
			sizeof(RT_Water_Constants::ClosestHitUBO),
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

			vk::DescriptorSetLayoutBinding rtaoTexBinding{};
			rtaoTexBinding.binding = TO_API_FORM(RTOpaqueClosestHitBinding::RTAOTex);
			rtaoTexBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			rtaoTexBinding.descriptorCount = 1;
			rtaoTexBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

			closestHitOpaqueDescriptorSets_[i].createLayout({
				tlasBinding,
				chunkInfoBinding,
				uboBinding,
				atlasBinding,
				rtaoTexBinding
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

			vk::DescriptorPoolSize rtaoTexPool{};
			rtaoTexPool.type = vk::DescriptorType::eCombinedImageSampler;
			rtaoTexPool.descriptorCount = 1;

			closestHitOpaqueDescriptorSets_[i].createPool({
				tlasPool,
				chunkInfoPool,
				uboPool,
				atlasPool,
				rtaoTexPool
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
