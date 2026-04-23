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
	opaqueUBOBuffer_(vk),
	reflUBOBuffer_(vk),
	refrUBOBuffer_(vk),
	opaqueDescriptorSet_(vk),
	reflectionDescriptorSet_(vk),
	refractionDescriptorSet_(vk),
	opaquePipeline_(vk),
	opaqueGBufferUBOBuffer_(vk),
	opaqueGBufferDescriptorSet_(vk),
	opaqueGBufferPipeline_(vk),
	opaqueShadowUBOBuffer_(vk),
	opaqueShadowDescriptorSet_(vk),
	opaqueShadowPipeline_(vk)
{
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

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

		vk::DescriptorSet set = opaqueDescriptorSet_.getSet();

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
		chunkUBOData_.u_lightColor = in.light->getColor();

		opaqueUBOBuffer_.upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaquePipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getChunkDrawList();
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
	}
	// reflection
	else if (renderTarget == RenderTargetVk::WaterReflection)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

		vk::DescriptorSet set = reflectionDescriptorSet_.getSet();

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
		chunkUBOData_.u_lightColor = in.light->getColor();

		reflUBOBuffer_.upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaquePipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getChunkDrawList();
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
	}
	// refraction
	else if (renderTarget == RenderTargetVk::WaterRefraction)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

		vk::DescriptorSet set = refractionDescriptorSet_.getSet();

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
		chunkUBOData_.u_lightColor = in.light->getColor();

		refrUBOBuffer_.upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaquePipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getChunkDrawList();
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
	}
	// gbuffer
	else if (renderTarget == RenderTargetVk::GBuffer)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaqueGBufferPipeline_.getPipeline());

		vk::DescriptorSet set = opaqueGBufferDescriptorSet_.getSet();

		gbufferUBOData_ = {};

		gbufferUBOData_.u_view = view;
		gbufferUBOData_.u_proj = proj;

		opaqueGBufferUBOBuffer_.upload(&gbufferUBOData_, sizeof(gbufferUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaqueGBufferPipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getChunkDrawList();
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
	}
	// shadow
	else if (renderTarget == RenderTargetVk::Shadow)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaqueShadowPipeline_.getPipeline());

		vk::DescriptorSet set = opaqueShadowDescriptorSet_.getSet();

		shadowUBOData_ = {};

		shadowUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

		opaqueShadowUBOBuffer_.upload(&shadowUBOData_, sizeof(shadowUBOData_), 0);

		cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			opaqueShadowPipeline_.getLayout(),
			0,
			1, &set,
			0, nullptr
		);

		const ChunkDrawList& list = in.world->getChunkDrawList();
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
	}
} // end of renderOpaque()


//--- PRIVATE ---//
void ChunkPassVk::refreshTexBinding()
{
	// opaque
	opaqueDescriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(ChunkBinding::SSAOTex),
		ssaoBlurImage_.view(),
		ssaoBlurImage_.sampler()
	);

	opaqueDescriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(ChunkBinding::ShadowTex),
		shadowMapImage_.view(),
		shadowMapImage_.sampler()
	);

	// reflection
	reflectionDescriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(ChunkBinding::SSAOTex),
		ssaoBlurImage_.view(),
		ssaoBlurImage_.sampler()
	);

	reflectionDescriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(ChunkBinding::ShadowTex),
		shadowMapImage_.view(),
		shadowMapImage_.sampler()
	);

	// refraction
	refractionDescriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(ChunkBinding::SSAOTex),
		ssaoBlurImage_.view(),
		ssaoBlurImage_.sampler()
	);

	refractionDescriptorSet_.writeCombinedImageSampler(
		TO_API_FORM(ChunkBinding::ShadowTex),
		shadowMapImage_.view(),
		shadowMapImage_.sampler()
	);

} // end of refreshTexBinding()

void ChunkPassVk::createResources()
{
	// opaque
	opaqueUBOBuffer_.create(
		sizeof(ChunkOpaqueUBO),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// reflection
	reflUBOBuffer_.create(
		sizeof(ChunkOpaqueUBO),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// refraction
	refrUBOBuffer_.create(
		sizeof(ChunkOpaqueUBO),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);


	// gbuffer
	opaqueGBufferUBOBuffer_.create(
		sizeof(GbufferUBO),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	// shadow
	opaqueShadowUBOBuffer_.create(
		sizeof(ShadowMapPassUBO),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
} // end of createResources()

void ChunkPassVk::createDescriptorSets()
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

		opaqueDescriptorSet_.createLayout({
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

		opaqueDescriptorSet_.createPool({
			uboPool,
			atlasPool,
			ssaoPool,
			shadowMapPool
			});
		opaqueDescriptorSet_.allocate();

		opaqueDescriptorSet_.writeUniformBuffer(
			TO_API_FORM(ChunkBinding::UBO),
			opaqueUBOBuffer_.getBuffer(),
			sizeof(ChunkOpaqueUBO)
		);

		opaqueDescriptorSet_.writeCombinedImageSampler(
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

		reflectionDescriptorSet_.createLayout({
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

		reflectionDescriptorSet_.createPool({
			uboPool,
			atlasPool,
			ssaoPool,
			shadowMapPool
			});
		reflectionDescriptorSet_.allocate();

		reflectionDescriptorSet_.writeUniformBuffer(
			TO_API_FORM(ChunkBinding::UBO),
			reflUBOBuffer_.getBuffer(),
			sizeof(ChunkOpaqueUBO)
		);

		reflectionDescriptorSet_.writeCombinedImageSampler(
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

		refractionDescriptorSet_.createLayout({
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

		refractionDescriptorSet_.createPool({
			uboPool,
			atlasPool,
			ssaoPool,
			shadowMapPool
			});
		refractionDescriptorSet_.allocate();

		refractionDescriptorSet_.writeUniformBuffer(
			TO_API_FORM(ChunkBinding::UBO),
			refrUBOBuffer_.getBuffer(),
			sizeof(ChunkOpaqueUBO)
		);

		refractionDescriptorSet_.writeCombinedImageSampler(
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

		opaqueGBufferDescriptorSet_.createLayout({ uboBinding });

		vk::DescriptorPoolSize uboPool{};
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		opaqueGBufferDescriptorSet_.createPool({ uboPool });
		opaqueGBufferDescriptorSet_.allocate();

		opaqueGBufferDescriptorSet_.writeUniformBuffer(
			TO_API_FORM(GbufferBinding::UBO),
			opaqueGBufferUBOBuffer_.getBuffer(),
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

		opaqueShadowDescriptorSet_.createLayout({ 
			uboBinding 
			});

		vk::DescriptorPoolSize uboPool{};
		uboPool.type = vk::DescriptorType::eUniformBuffer;
		uboPool.descriptorCount = 1;

		opaqueShadowDescriptorSet_.createPool({
			uboPool 
			});
		opaqueShadowDescriptorSet_.allocate();

		opaqueShadowDescriptorSet_.writeUniformBuffer(
			TO_API_FORM(ShadowMapPassBinding::UBO),
			opaqueShadowUBOBuffer_.getBuffer(),
			sizeof(ShadowMapPassUBO)
		);
	}
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

		desc.setLayouts = { opaqueDescriptorSet_.getLayout() };

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

		desc.setLayouts = { opaqueGBufferDescriptorSet_.getLayout() };

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

		desc.setLayouts = { opaqueShadowDescriptorSet_.getLayout() };

		desc.depthFormat = shadowFormats.depthFormat;
		desc.depthTestEnable = true;
		desc.depthWriteEnable = true;
		desc.depthCompareOp = vk::CompareOp::eLess;

		desc.cullMode = vk::CullModeFlagBits::eFront;
		desc.frontFace = vk::FrontFace::eClockwise;

		opaqueShadowPipeline_.create(desc);
	}
} // end of createPipelines()
