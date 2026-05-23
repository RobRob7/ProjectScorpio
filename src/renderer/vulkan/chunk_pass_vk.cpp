#include "chunk_pass_vk.h"

#include "buffer_vk.h"
#include "descriptor_set_vk.h"
#include "graphics_pipeline_vk.h"

#include "frame_context_vk.h"
#include "vulkan_main.h"
#include "image_vk.h"

#include "bindings.h"

#include "chunk_draw_list.h"
#include "i_chunk_mesh_gpu.h"

#include "render_inputs.h"
#include "render_settings.h"
#include "chunk_manager.h"
#include "camera.h"
#include "i_light.h"

#include <memory>
#include <cstddef>

using namespace Chunk_Constants;
using namespace Gbuffer_Constants;
using namespace Shadow_Map_Constants;

//--- PUBLIC ---//
ChunkPassVk::ChunkPassVk(
	VulkanMain& vk,
	RenderSettings& rs,
	const ImageVk& ssaoBlurImage,
	const ImageVk& shadowMapImage
)
	: vk_(vk),
	rs_(rs),
	ssaoBlurImage_(ssaoBlurImage),
	shadowMapImage_(shadowMapImage),
	atlas_(vk),
	opaquePipeline_(vk),
	opaqueGBufferPipeline_(vk),
	opaqueShadowPipeline_(vk)
{
	opaqueUBOBuffers_.reserve(vk_.getMaxFramesInFlight());
	reflUBOBuffers_.reserve(vk_.getMaxFramesInFlight());
	refrUBOBuffers_.reserve(vk_.getMaxFramesInFlight());

	opaqueDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	reflectionDescriptorSets_.reserve(vk_.getMaxFramesInFlight());
	refractionDescriptorSets_.reserve(vk_.getMaxFramesInFlight());

	opaqueGBufferUBOBuffers_.reserve(vk_.getMaxFramesInFlight());
	opaqueGBufferDescriptorSets_.reserve(vk_.getMaxFramesInFlight());

	opaqueShadowUBOBuffers_.reserve(vk_.getMaxFramesInFlight());
	opaqueShadowDescriptorSets_.reserve(vk_.getMaxFramesInFlight());

	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		opaqueUBOBuffers_.emplace_back(vk_);
		reflUBOBuffers_.emplace_back(vk_);
		refrUBOBuffers_.emplace_back(vk_);

		opaqueDescriptorSets_.emplace_back(vk_);
		reflectionDescriptorSets_.emplace_back(vk_);
		refractionDescriptorSets_.emplace_back(vk_);

		opaqueGBufferUBOBuffers_.emplace_back(vk_);
		opaqueGBufferDescriptorSets_.emplace_back(vk_);

		opaqueShadowUBOBuffers_.emplace_back(vk_);
		opaqueShadowDescriptorSets_.emplace_back(vk_);
	} // end for

} // end of constructor

ChunkPassVk::~ChunkPassVk() = default;

void ChunkPassVk::init(
	RenderTargetFormatsVk defaultFormats,
	RenderTargetFormatsVk gbufferFormats,
	RenderTargetFormatsVk shadowFormats
)
{
	opaqueShader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(), 
		"chunk/chunk.vert.spv",
		"chunk/chunk.frag.spv"
	);
	opaqueGBufferShader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"gbuffer/gbuffer.vert.spv",
		"gbuffer/gbuffer.frag.spv"
	);
	opaqueShadowShader_ = std::make_unique<ShaderModuleVk>(
		vk_.getDevice(),
		"shadowmappass/shadowmappass.vert.spv",
		"shadowmappass/shadowmappass.frag.spv"
	);

	atlas_.loadFromFile("blocks_padded.png", true);
	atlas_.setDebugName("ChunkPassVk-AtlasTexture");

	createResources();
	createDescriptorSets();
	createPipelines(
		defaultFormats,
		gbufferFormats,
		shadowFormats
	);
} // end of init()

void ChunkPassVk::resize()
{
	refreshTexBinding();
} // end of resize()

void ChunkPassVk::renderOpaque(
	RenderTargetVk renderTarget,
	const RenderInputs& in,
	const FrameContext& frame,
	const glm::mat4& view,
	const glm::mat4& proj,
	const glm::mat4& lightSpaceMatrix,
	const uint32_t waterPassWidth,
	const uint32_t waterPassHeight
)
{
	in.world->buildOpaqueDrawList(view, proj);

	vk::CommandBuffer cmd = frame.cmd;

	// default
	if (renderTarget == RenderTargetVk::Default)
	{
		vk::Extent2D extent = frame.extent;

		cmd.beginDebugUtilsLabelEXT({ "ChunkPassVk-Default::cmd" });

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

		vk::DescriptorSet set = opaqueDescriptorSets_[frame.frameIndex].getSet();

		chunkUBOData_ = {};

		chunkUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

		chunkUBOData_.u_useSSAO = rs_.useSSAO ? 1 : 0;
		chunkUBOData_.u_useShadowMap = rs_.useShadowMap ? 1 : 0;

		chunkUBOData_.u_view = view;
		chunkUBOData_.u_proj = proj;
		chunkUBOData_.u_screenSize = glm::vec2(extent.width, extent.height);
		chunkUBOData_.u_ambientStrength = in.world->getAmbientStrength();

		chunkUBOData_.u_viewPos = in.camera->getCameraPosition();

		chunkUBOData_.u_lightDir = in.light->getDirection();
		chunkUBOData_.u_lightColor = in.light->getLightColor();

		opaqueUBOBuffers_[frame.frameIndex].upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaquePipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getOpaqueDrawList();
		for (const auto& item : list.items)
		{
			ChunkPushConstants pc{};
			pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

			cmd.pushConstants(
				opaquePipeline_.getLayout(),
				vk::ShaderStageFlagBits::eVertex,
				0,
				sizeof(ChunkPushConstants),
				&pc
			);

			item.gpu->drawOpaque(cmd);
		} // end for

		cmd.endDebugUtilsLabelEXT();
	}
	// reflection
	else if (renderTarget == RenderTargetVk::WaterReflection)
	{
		cmd.beginDebugUtilsLabelEXT({ "ChunkPassVk-Reflection::cmd" });

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

		vk::DescriptorSet set = reflectionDescriptorSets_[frame.frameIndex].getSet();

		// build reflected view matrix
		const float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
		Camera camera = *in.camera;
		glm::vec3 reflectedPos = camera.getCameraPosition();
		reflectedPos.y = 2.0f * waterHeight - reflectedPos.y;
		camera.setCameraPosition(reflectedPos);

		// set clip plane (clip everything below water)
		glm::vec4 clipPlane{ 0, 1, 0, -waterHeight };

		chunkUBOData_ = {};

		chunkUBOData_.u_clipPlane = clipPlane;
		chunkUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

		chunkUBOData_.u_useSSAO = 0;
		chunkUBOData_.u_useShadowMap = rs_.useShadowMap ? 1 : 0;

		chunkUBOData_.u_view = view;
		chunkUBOData_.u_proj = proj;
		chunkUBOData_.u_screenSize = glm::vec2(waterPassWidth, waterPassHeight);
		chunkUBOData_.u_ambientStrength = in.world->getAmbientStrength();

		chunkUBOData_.u_viewPos = camera.getCameraPosition();

		chunkUBOData_.u_lightDir = in.light->getDirection();
		chunkUBOData_.u_lightColor = in.light->getLightColor();

		reflUBOBuffers_[frame.frameIndex].upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaquePipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getOpaqueDrawList();
		for (const auto& item : list.items)
		{
			ChunkPushConstants pc{};
			pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

			cmd.pushConstants(
				opaquePipeline_.getLayout(),
				vk::ShaderStageFlagBits::eVertex,
				0,
				sizeof(ChunkPushConstants),
				&pc
			);

			item.gpu->drawOpaque(cmd);
		} // end for

		cmd.endDebugUtilsLabelEXT();
	}
	// refraction
	else if (renderTarget == RenderTargetVk::WaterRefraction)
	{
		cmd.beginDebugUtilsLabelEXT({ "ChunkPassVk-Refraction::cmd" });

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

		vk::DescriptorSet set = refractionDescriptorSets_[frame.frameIndex].getSet();

		// set clip plane (clip everything above water)
		float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
		glm::vec4 clipPlane{ 0, -1, 0, waterHeight };

		chunkUBOData_ = {};

		chunkUBOData_.u_clipPlane = clipPlane;
		chunkUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

		chunkUBOData_.u_useSSAO = 0;
		chunkUBOData_.u_useShadowMap = rs_.useShadowMap ? 1 : 0;

		chunkUBOData_.u_view = view;
		chunkUBOData_.u_proj = proj;
		chunkUBOData_.u_screenSize = glm::vec2(waterPassWidth, waterPassHeight);
		chunkUBOData_.u_ambientStrength = in.world->getAmbientStrength();

		chunkUBOData_.u_viewPos = in.camera->getCameraPosition();

		chunkUBOData_.u_lightDir = in.light->getDirection();
		chunkUBOData_.u_lightColor = in.light->getLightColor();

		refrUBOBuffers_[frame.frameIndex].upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaquePipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getOpaqueDrawList();
		for (const auto& item : list.items)
		{
			ChunkPushConstants pc{};
			pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

			cmd.pushConstants(
				opaquePipeline_.getLayout(),
				vk::ShaderStageFlagBits::eVertex,
				0,
				sizeof(ChunkPushConstants),
				&pc
			);

			item.gpu->drawOpaque(cmd);
		} // end for

		cmd.endDebugUtilsLabelEXT();
	}
	// gbuffer
	else if (renderTarget == RenderTargetVk::GBuffer)
	{
		cmd.beginDebugUtilsLabelEXT({ "ChunkPassVk-GBuffer::cmd" });

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaqueGBufferPipeline_.getPipeline());

		vk::DescriptorSet set = opaqueGBufferDescriptorSets_[frame.frameIndex].getSet();

		gbufferUBOData_ = {};

		gbufferUBOData_.u_view = view;
		gbufferUBOData_.u_proj = proj;

		opaqueGBufferUBOBuffers_[frame.frameIndex].upload(&gbufferUBOData_, sizeof(gbufferUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaqueGBufferPipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getOpaqueDrawList();
		for (const auto& item : list.items)
		{
			ChunkPushConstants pc{};
			pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

			cmd.pushConstants(
				opaqueGBufferPipeline_.getLayout(),
				vk::ShaderStageFlagBits::eVertex,
				0,
				sizeof(ChunkPushConstants),
				&pc
			);

			item.gpu->drawOpaque(cmd);
		} // end for

		cmd.endDebugUtilsLabelEXT();
	}
	// shadow
	else if (renderTarget == RenderTargetVk::Shadow)
	{
		cmd.beginDebugUtilsLabelEXT({ "ChunkPassVk-Shadow::cmd" });

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaqueShadowPipeline_.getPipeline());

		vk::DescriptorSet set = opaqueShadowDescriptorSets_[frame.frameIndex].getSet();

		shadowUBOData_ = {};

		shadowUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

		opaqueShadowUBOBuffers_[frame.frameIndex].upload(&shadowUBOData_, sizeof(shadowUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaqueShadowPipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getOpaqueDrawList();
		for (const auto& item : list.items)
		{
			ChunkPushConstants pc{};
			pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

			cmd.pushConstants(
				opaqueShadowPipeline_.getLayout(),
				vk::ShaderStageFlagBits::eVertex,
				0,
				sizeof(ChunkPushConstants),
				&pc
			);

			item.gpu->drawOpaque(cmd);
		} // end for

		cmd.endDebugUtilsLabelEXT();
	}
} // end of renderOpaque()


//--- PRIVATE ---//
void ChunkPassVk::refreshTexBinding()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		// opaque
		opaqueDescriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(ChunkBinding::SSAOTex),
			ssaoBlurImage_.view(),
			ssaoBlurImage_.sampler()
		);

		opaqueDescriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(ChunkBinding::ShadowTex),
			shadowMapImage_.view(),
			shadowMapImage_.sampler()
		);

		// reflection
		reflectionDescriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(ChunkBinding::SSAOTex),
			ssaoBlurImage_.view(),
			ssaoBlurImage_.sampler()
		);

		reflectionDescriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(ChunkBinding::ShadowTex),
			shadowMapImage_.view(),
			shadowMapImage_.sampler()
		);

		// refraction
		refractionDescriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(ChunkBinding::SSAOTex),
			ssaoBlurImage_.view(),
			ssaoBlurImage_.sampler()
		);

		refractionDescriptorSets_[i].writeCombinedImageSampler(
			TO_API_FORM(ChunkBinding::ShadowTex),
			shadowMapImage_.view(),
			shadowMapImage_.sampler()
		);
	} // end for
} // end of refreshTexBinding()

void ChunkPassVk::createResources()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		// opaque
		opaqueUBOBuffers_[i].create(
			sizeof(ChunkOpaqueUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		// reflection
		reflUBOBuffers_[i].create(
			sizeof(ChunkOpaqueUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		// refraction
		refrUBOBuffers_[i].create(
			sizeof(ChunkOpaqueUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);


		// gbuffer
		opaqueGBufferUBOBuffers_[i].create(
			sizeof(GbufferUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		// shadow
		opaqueShadowUBOBuffers_[i].create(
			sizeof(ShadowMapPassUBO),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	} // end for
} // end of createResources()

void ChunkPassVk::createDescriptorSets()
{
	for (uint32_t i = 0; i < vk_.getMaxFramesInFlight(); ++i)
	{
		// opaque
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(ChunkBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding atlasBinding{};
			atlasBinding.binding = TO_API_FORM(ChunkBinding::AtlasTex);
			atlasBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			atlasBinding.descriptorCount = 1;
			atlasBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding ssaoBinding{};
			ssaoBinding.binding = TO_API_FORM(ChunkBinding::SSAOTex);
			ssaoBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			ssaoBinding.descriptorCount = 1;
			ssaoBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding shadowMapBinding{};
			shadowMapBinding.binding = TO_API_FORM(ChunkBinding::ShadowTex);
			shadowMapBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			shadowMapBinding.descriptorCount = 1;
			shadowMapBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			opaqueDescriptorSets_[i].createLayout({
				uboBinding,
				atlasBinding,
				ssaoBinding,
				shadowMapBinding
				});

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize atlasPool{};
			atlasPool.type = vk::DescriptorType::eCombinedImageSampler;
			atlasPool.descriptorCount = 1;

			vk::DescriptorPoolSize ssaoPool{};
			ssaoPool.type = vk::DescriptorType::eCombinedImageSampler;
			ssaoPool.descriptorCount = 1;

			vk::DescriptorPoolSize shadowMapPool{};
			shadowMapPool.type = vk::DescriptorType::eCombinedImageSampler;
			shadowMapPool.descriptorCount = 1;

			opaqueDescriptorSets_[i].createPool({
				uboPool,
				atlasPool,
				ssaoPool,
				shadowMapPool
				});
			opaqueDescriptorSets_[i].allocate();

			opaqueDescriptorSets_[i].setDebugName(
				"ChunkPassVK-Opaque::DescriptorSet frame " + std::to_string(i)
			);

			opaqueDescriptorSets_[i].writeUniformBuffer(
				TO_API_FORM(ChunkBinding::UBO),
				opaqueUBOBuffers_[i].getBuffer(),
				sizeof(ChunkOpaqueUBO)
			);

			opaqueDescriptorSets_[i].writeCombinedImageSampler(
				TO_API_FORM(ChunkBinding::AtlasTex),
				atlas_.view(),
				atlas_.sampler()
			);
		}

		// reflection
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(ChunkBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding atlasBinding{};
			atlasBinding.binding = TO_API_FORM(ChunkBinding::AtlasTex);
			atlasBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			atlasBinding.descriptorCount = 1;
			atlasBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding ssaoBinding{};
			ssaoBinding.binding = TO_API_FORM(ChunkBinding::SSAOTex);
			ssaoBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			ssaoBinding.descriptorCount = 1;
			ssaoBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding shadowMapBinding{};
			shadowMapBinding.binding = TO_API_FORM(ChunkBinding::ShadowTex);
			shadowMapBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			shadowMapBinding.descriptorCount = 1;
			shadowMapBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			reflectionDescriptorSets_[i].createLayout({
				uboBinding,
				atlasBinding,
				ssaoBinding,
				shadowMapBinding
				});

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize atlasPool{};
			atlasPool.type = vk::DescriptorType::eCombinedImageSampler;
			atlasPool.descriptorCount = 1;

			vk::DescriptorPoolSize ssaoPool{};
			ssaoPool.type = vk::DescriptorType::eCombinedImageSampler;
			ssaoPool.descriptorCount = 1;

			vk::DescriptorPoolSize shadowMapPool{};
			shadowMapPool.type = vk::DescriptorType::eCombinedImageSampler;
			shadowMapPool.descriptorCount = 1;

			reflectionDescriptorSets_[i].createPool({
				uboPool,
				atlasPool,
				ssaoPool,
				shadowMapPool
				});
			reflectionDescriptorSets_[i].allocate();

			reflectionDescriptorSets_[i].setDebugName(
				"ChunkPassVK-Reflection::DescriptorSet frame " + std::to_string(i)
			);

			reflectionDescriptorSets_[i].writeUniformBuffer(
				TO_API_FORM(ChunkBinding::UBO),
				reflUBOBuffers_[i].getBuffer(),
				sizeof(ChunkOpaqueUBO)
			);

			reflectionDescriptorSets_[i].writeCombinedImageSampler(
				TO_API_FORM(ChunkBinding::AtlasTex),
				atlas_.view(),
				atlas_.sampler()
			);
		}

		// reflection
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(ChunkBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding atlasBinding{};
			atlasBinding.binding = TO_API_FORM(ChunkBinding::AtlasTex);
			atlasBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			atlasBinding.descriptorCount = 1;
			atlasBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding ssaoBinding{};
			ssaoBinding.binding = TO_API_FORM(ChunkBinding::SSAOTex);
			ssaoBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			ssaoBinding.descriptorCount = 1;
			ssaoBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding shadowMapBinding{};
			shadowMapBinding.binding = TO_API_FORM(ChunkBinding::ShadowTex);
			shadowMapBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			shadowMapBinding.descriptorCount = 1;
			shadowMapBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

			refractionDescriptorSets_[i].createLayout({
				uboBinding,
				atlasBinding,
				ssaoBinding,
				shadowMapBinding
				});

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			vk::DescriptorPoolSize atlasPool{};
			atlasPool.type = vk::DescriptorType::eCombinedImageSampler;
			atlasPool.descriptorCount = 1;

			vk::DescriptorPoolSize ssaoPool{};
			ssaoPool.type = vk::DescriptorType::eCombinedImageSampler;
			ssaoPool.descriptorCount = 1;

			vk::DescriptorPoolSize shadowMapPool{};
			shadowMapPool.type = vk::DescriptorType::eCombinedImageSampler;
			shadowMapPool.descriptorCount = 1;

			refractionDescriptorSets_[i].createPool({
				uboPool,
				atlasPool,
				ssaoPool,
				shadowMapPool
				});
			refractionDescriptorSets_[i].allocate();

			refractionDescriptorSets_[i].setDebugName(
				"ChunkPassVK-Refraction::DescriptorSet frame " + std::to_string(i)
			);

			refractionDescriptorSets_[i].writeUniformBuffer(
				TO_API_FORM(ChunkBinding::UBO),
				refrUBOBuffers_[i].getBuffer(),
				sizeof(ChunkOpaqueUBO)
			);

			refractionDescriptorSets_[i].writeCombinedImageSampler(
				TO_API_FORM(ChunkBinding::AtlasTex),
				atlas_.view(),
				atlas_.sampler()
			);
		}

		// gbuffer
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(GbufferBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			opaqueGBufferDescriptorSets_[i].createLayout({uboBinding});

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			opaqueGBufferDescriptorSets_[i].createPool({uboPool});
			opaqueGBufferDescriptorSets_[i].allocate();

			opaqueGBufferDescriptorSets_[i].setDebugName(
				"ChunkPassVK-GBuffer::DescriptorSet frame " + std::to_string(i)
			);

			opaqueGBufferDescriptorSets_[i].writeUniformBuffer(
				TO_API_FORM(GbufferBinding::UBO),
				opaqueGBufferUBOBuffers_[i].getBuffer(),
				sizeof(GbufferUBO)
			);
		}


		// shadow
		{
			vk::DescriptorSetLayoutBinding uboBinding{};
			uboBinding.binding = TO_API_FORM(ShadowMapPassBinding::UBO);
			uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			uboBinding.descriptorCount = 1;
			uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

			opaqueShadowDescriptorSets_[i].createLayout({
				uboBinding
				});

			vk::DescriptorPoolSize uboPool{};
			uboPool.type = vk::DescriptorType::eUniformBuffer;
			uboPool.descriptorCount = 1;

			opaqueShadowDescriptorSets_[i].createPool({
				uboPool
				});
			opaqueShadowDescriptorSets_[i].allocate();

			opaqueShadowDescriptorSets_[i].setDebugName(
				"ChunkPassVK-Shadow::DescriptorSet frame " + std::to_string(i)
			);

			opaqueShadowDescriptorSets_[i].writeUniformBuffer(
				TO_API_FORM(ShadowMapPassBinding::UBO),
				opaqueShadowUBOBuffers_[i].getBuffer(),
				sizeof(ShadowMapPassUBO)
			);
		}
	} // end for
} // end of createDescriptorSets()

void ChunkPassVk::createPipelines(
	RenderTargetFormatsVk defaultFormats,
	RenderTargetFormatsVk gbufferFormats,
	RenderTargetFormatsVk shadowFormats
)
{
	// opaque
	{
		GraphicsPipelineDescVk desc{};
		desc.vertShader = opaqueShader_->vertShader();
		desc.fragShader = opaqueShader_->fragShader();

		vk::PushConstantRange pushRange{};
		pushRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
		pushRange.offset = 0;
		pushRange.size = sizeof(ChunkPushConstants);
		desc.pushConstantRanges = { pushRange };

		desc.setLayouts = { opaqueDescriptorSets_[0].getLayout()};

		desc.colorFormat = defaultFormats.colorFormat;
		desc.depthFormat = defaultFormats.depthFormat;

		desc.cullMode = vk::CullModeFlagBits::eBack;
		desc.frontFace = vk::FrontFace::eClockwise;
		desc.depthTestEnable = true;
		desc.depthWriteEnable = true;
		desc.depthCompareOp = vk::CompareOp::eLessOrEqual;

		vk::VertexInputBindingDescription binding{};
		binding.binding = 0;
		binding.stride = sizeof(Vertex);
		binding.inputRate = vk::VertexInputRate::eVertex;

		vk::VertexInputAttributeDescription attr{};
		attr.location = 0;
		attr.binding = 0;
		attr.format = vk::Format::eR32Uint;
		attr.offset = offsetof(Vertex, sample);

		desc.vertexBinding = binding;
		desc.vertexAttributes = { attr };

		opaquePipeline_.create(desc);

		opaquePipeline_.setDebugName("ChunkPassVk-Opaque::Pipeline");
	}
	
	// gbuffer
	{
		GraphicsPipelineDescVk desc{};
		desc.vertShader = opaqueGBufferShader_->vertShader();
		desc.fragShader = opaqueGBufferShader_->fragShader();

		vk::PushConstantRange pushRange{};
		pushRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
		pushRange.offset = 0;
		pushRange.size = sizeof(ChunkPushConstants);
		desc.pushConstantRanges = { pushRange };

		desc.setLayouts = { opaqueGBufferDescriptorSets_[0].getLayout()};

		desc.colorFormat = gbufferFormats.colorFormat;
		desc.depthFormat = gbufferFormats.depthFormat;

		desc.cullMode = vk::CullModeFlagBits::eBack;
		desc.frontFace = vk::FrontFace::eClockwise;
		desc.depthTestEnable = true;
		desc.depthWriteEnable = true;
		desc.depthCompareOp = vk::CompareOp::eLessOrEqual;

		vk::VertexInputBindingDescription binding{};
		binding.binding = 0;
		binding.stride = sizeof(Vertex);
		binding.inputRate = vk::VertexInputRate::eVertex;

		vk::VertexInputAttributeDescription attr{};
		attr.location = 0;
		attr.binding = 0;
		attr.format = vk::Format::eR32Uint;
		attr.offset = offsetof(Vertex, sample);

		desc.vertexBinding = binding;
		desc.vertexAttributes = { attr };

		opaqueGBufferPipeline_.create(desc);

		opaqueGBufferPipeline_.setDebugName("ChunkPassVk-GBuffer::Pipeline");
	}

	// shadow
	{
		GraphicsPipelineDescVk desc{};
		desc.vertShader = opaqueShadowShader_->vertShader();
		desc.fragShader = opaqueShadowShader_->fragShader();

		vk::PushConstantRange pushRange{};
		pushRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
		pushRange.offset = 0;
		pushRange.size = sizeof(Chunk_Constants::ChunkPushConstants);
		desc.pushConstantRanges = { pushRange };

		vk::VertexInputBindingDescription binding{};
		binding.binding = 0;
		binding.stride = sizeof(Vertex);
		binding.inputRate = vk::VertexInputRate::eVertex;

		vk::VertexInputAttributeDescription attr{};
		attr.location = 0;
		attr.binding = 0;
		attr.format = vk::Format::eR32Uint;
		attr.offset = offsetof(Vertex, sample);

		desc.vertexBinding = binding;
		desc.vertexAttributes = { attr };

		desc.setLayouts = { opaqueShadowDescriptorSets_[0].getLayout()};

		desc.depthFormat = shadowFormats.depthFormat;
		desc.depthTestEnable = true;
		desc.depthWriteEnable = true;
		desc.depthCompareOp = vk::CompareOp::eLess;

		desc.cullMode = vk::CullModeFlagBits::eFront;
		desc.frontFace = vk::FrontFace::eClockwise;

		opaqueShadowPipeline_.create(desc);

		opaqueShadowPipeline_.setDebugName("ChunkPassVk-Shadow::Pipeline");
	}
} // end of createPipelines()
