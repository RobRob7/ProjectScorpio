#include "chunk_pass_dx12.h"

#include "frame_context_vk.h"
#include "dx12_main.h"
#include "image_dx12.h"

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
#include <algorithm>

using namespace Chunk_Constants;
using namespace Gbuffer_Constants;
using namespace Shadow_Map_Constants;

//--- PUBLIC ---//
ChunkPassDX12::ChunkPassDX12(
	DX12Main& dx,
	RenderSettings& rs
)
	: dx_(&dx),
	rs_(&rs),
	atlas_(dx),
	opaquePipeline_(dx),
	opaqueGBufferPipeline_(dx),
	opaqueShadowPipeline_(dx)
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	opaqueUBOBuffers_.reserve(frames);
	reflUBOBuffers_.reserve(frames);
	refrUBOBuffers_.reserve(frames);

	opaqueDescriptorSets_.reserve(frames);
	reflectionDescriptorSets_.reserve(frames);
	refractionDescriptorSets_.reserve(frames);

	opaqueGBufferUBOBuffers_.reserve(frames);
	opaqueGBufferDescriptorSets_.reserve(frames);

	opaqueShadowUBOBuffers_.reserve(frames);
	opaqueShadowDescriptorSets_.reserve(frames);

	for (uint32_t i = 0; i < frames; ++i)
	{
		opaqueUBOBuffers_.emplace_back(*dx_);
		reflUBOBuffers_.emplace_back(*dx_);
		refrUBOBuffers_.emplace_back(*dx_);

		opaqueDescriptorSets_.emplace_back(*dx_);
		reflectionDescriptorSets_.emplace_back(*dx_);
		refractionDescriptorSets_.emplace_back(*dx_);

		opaqueGBufferUBOBuffers_.emplace_back(*dx_);
		opaqueGBufferDescriptorSets_.emplace_back(*dx_);

		opaqueShadowUBOBuffers_.emplace_back(*dx_);
		opaqueShadowDescriptorSets_.emplace_back(*dx_);
	} // end for

} // end of constructor

ChunkPassDX12::~ChunkPassDX12() = default;

void ChunkPassDX12::init(
	RenderTargetFormatsDX12 defaultFormats,
	RenderTargetFormatsDX12 gbufferFormats,
	RenderTargetFormatsDX12 shadowFormats
)
{
	opaqueShader_ = std::make_unique<ShaderDX12>(
		"hlsl/chunk/chunk.vert.cso",
		"hlsl/chunk/chunk.frag.cso"
	);
	(void)gbufferFormats;
	(void)shadowFormats;
	//opaqueGBufferShader_ = std::make_unique<ShaderDX12>(
	//	"hlsl/gbuffer/gbuffer.vert.cso",
	//	"hlsl/gbuffer/gbuffer.frag.cso"
	//);
	//opaqueShadowShader_ = std::make_unique<ShaderDX12>(
	//	"hlsl/shadowmappass/shadowmappass.vert.cso",
	//	"hlsl/shadowmappass/shadowmappass.frag.cso"
	//);

	atlas_.loadFromFile("blocks_padded.png", true);
	atlas_.setDebugName(L"ChunkPassDX12-AtlasTexture");

	createResources();
	createDescriptorSets();
	createPipelines(
		defaultFormats,
		gbufferFormats,
		shadowFormats
	);
} // end of init()

void ChunkPassDX12::resize()
{
	updateDescriptorSet(dx_->currentFrameIndex());
} // end of resize()

void ChunkPassDX12::renderOpaque(
	RenderTargetDX12 renderTarget,
	const RenderInputs& in,
	const FrameContextDX12& frame,
	const glm::mat4& view,
	const glm::mat4& proj,
	const glm::mat4& lightSpaceMatrix,
	const uint32_t waterPassWidth,
	const uint32_t waterPassHeight
)
{
	updateSingleDescriptorSet(frame.frameIndex, renderTarget);

	in.world->buildOpaqueDrawList(view, proj);

	ID3D12GraphicsCommandList* cmd = frame.cmd;

	// default
	if (renderTarget == RenderTargetDX12::Default)
	{
		const uint32_t width = frame.width;
		const uint32_t height = frame.height;

		cmd->SetName({ L"ChunkPassDX12-Default::cmd" });

		DescriptorSetDX12& set = opaqueDescriptorSets_[frame.frameIndex];

		chunkUBOData_ = {};

		chunkUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

		chunkUBOData_.u_useSSAO = rs_->useSSAO ? 1 : 0;
		chunkUBOData_.u_useShadowMap = rs_->useShadowMap ? 1 : 0;

		chunkUBOData_.u_view = view;
		chunkUBOData_.u_proj = proj;
		chunkUBOData_.u_screenSize = glm::vec2(width, height);
		chunkUBOData_.u_ambientStrength = in.world->getAmbientStrength();

		chunkUBOData_.u_viewPos = in.camera->getCameraPosition();

		chunkUBOData_.u_lightDir = in.light->getDirection();
		chunkUBOData_.u_lightColor = in.light->getLightColor();

		opaqueUBOBuffers_[frame.frameIndex].upload(&chunkUBOData_, sizeof(chunkUBOData_));

		ID3D12DescriptorHeap* heaps[] =
		{
			set.getDescriptorHeap()
		};

		cmd->SetDescriptorHeaps(1, heaps);
		cmd->SetGraphicsRootSignature(opaquePipeline_.getRootSignature());
		cmd->SetPipelineState(opaquePipeline_.getPipeline());

		cmd->SetGraphicsRootDescriptorTable(
			set.getDescriptorTableRootIndex(),
			set.getTableGPUHandle()
		);

		const ChunkDrawList& list = in.world->getOpaqueDrawList();
		for (const auto& item : list.items)
		{
			ChunkPushConstants pc{};
			pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

			set.setGraphicsPushConstants(
				cmd,
				0,
				pc
			);

			item.gpu->drawOpaque(nullptr, &frame);
		} // end for
	}
	//// reflection
	//else if (renderTarget == RenderTargetDX12::WaterReflection)
	//{
	//	cmd.beginDebugUtilsLabelEXT({ "ChunkPassDX12-Reflection::cmd" });

	//	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

	//	vk::DescriptorSet set = reflectionDescriptorSets_[frame.frameIndex].getSet();

	//	// build reflected view matrix
	//	const float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
	//	Camera camera = *in.camera;
	//	glm::vec3 reflectedPos = camera.getCameraPosition();
	//	reflectedPos.y = 2.0f * waterHeight - reflectedPos.y;
	//	camera.setCameraPosition(reflectedPos);

	//	// set clip plane (clip everything below water)
	//	glm::vec4 clipPlane{ 0, 1, 0, -waterHeight };

	//	chunkUBOData_ = {};

	//	chunkUBOData_.u_clipPlane = clipPlane;
	//	chunkUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

	//	chunkUBOData_.u_useSSAO = 0;
	//	chunkUBOData_.u_useShadowMap = rs_.useShadowMap ? 1 : 0;

	//	chunkUBOData_.u_view = view;
	//	chunkUBOData_.u_proj = proj;
	//	chunkUBOData_.u_screenSize = glm::vec2(waterPassWidth, waterPassHeight);
	//	chunkUBOData_.u_ambientStrength = in.world->getAmbientStrength();

	//	chunkUBOData_.u_viewPos = camera.getCameraPosition();

	//	chunkUBOData_.u_lightDir = in.light->getDirection();
	//	chunkUBOData_.u_lightColor = in.light->getLightColor();

	//	reflUBOBuffers_[frame.frameIndex].upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

	//	cmd.bindDescriptorSets(
	//		vk::PipelineBindPoint::eGraphics,
	//		opaquePipeline_.getLayout(),
	//		0,
	//		1, &set,
	//		0, nullptr
	//	);

	//	const ChunkDrawList& list = in.world->getOpaqueDrawList();
	//	for (const auto& item : list.items)
	//	{
	//		ChunkPushConstants pc{};
	//		pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

	//		cmd.pushConstants(
	//			opaquePipeline_.getLayout(),
	//			vk::ShaderStageFlagBits::eVertex,
	//			0,
	//			sizeof(ChunkPushConstants),
	//			&pc
	//		);

	//		item.gpu->drawOpaque(&frame, nullptr);
	//	} // end for

	//	cmd.endDebugUtilsLabelEXT();
	//}
	//// refraction
	//else if (renderTarget == RenderTargetDX12::WaterRefraction)
	//{
	//	cmd.beginDebugUtilsLabelEXT({ "ChunkPassDX12-Refraction::cmd" });

	//	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaquePipeline_.getPipeline());

	//	vk::DescriptorSet set = refractionDescriptorSets_[frame.frameIndex].getSet();

	//	// set clip plane (clip everything above water)
	//	float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
	//	glm::vec4 clipPlane{ 0, -1, 0, waterHeight };

	//	chunkUBOData_ = {};

	//	chunkUBOData_.u_clipPlane = clipPlane;
	//	chunkUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

	//	chunkUBOData_.u_useSSAO = 0;
	//	chunkUBOData_.u_useShadowMap = rs_.useShadowMap ? 1 : 0;

	//	chunkUBOData_.u_view = view;
	//	chunkUBOData_.u_proj = proj;
	//	chunkUBOData_.u_screenSize = glm::vec2(waterPassWidth, waterPassHeight);
	//	chunkUBOData_.u_ambientStrength = in.world->getAmbientStrength();

	//	chunkUBOData_.u_viewPos = in.camera->getCameraPosition();

	//	chunkUBOData_.u_lightDir = in.light->getDirection();
	//	chunkUBOData_.u_lightColor = in.light->getLightColor();

	//	refrUBOBuffers_[frame.frameIndex].upload(&chunkUBOData_, sizeof(chunkUBOData_), 0);

	//	cmd.bindDescriptorSets(
	//		vk::PipelineBindPoint::eGraphics,
	//		opaquePipeline_.getLayout(),
	//		0,
	//		1, &set,
	//		0, nullptr
	//	);

	//	const ChunkDrawList& list = in.world->getOpaqueDrawList();
	//	for (const auto& item : list.items)
	//	{
	//		ChunkPushConstants pc{};
	//		pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

	//		cmd.pushConstants(
	//			opaquePipeline_.getLayout(),
	//			vk::ShaderStageFlagBits::eVertex,
	//			0,
	//			sizeof(ChunkPushConstants),
	//			&pc
	//		);

	//		item.gpu->drawOpaque(&frame, nullptr);
	//	} // end for

	//	cmd.endDebugUtilsLabelEXT();
	//}
	//// gbuffer
	//else if (renderTarget == RenderTargetDX12::GBuffer)
	//{
	//	cmd.beginDebugUtilsLabelEXT({ "ChunkPassDX12-GBuffer::cmd" });

	//	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaqueGBufferPipeline_.getPipeline());

	//	vk::DescriptorSet set = opaqueGBufferDescriptorSets_[frame.frameIndex].getSet();

	//	gbufferUBOData_ = {};

	//	gbufferUBOData_.u_view = view;
	//	gbufferUBOData_.u_proj = proj;

	//	opaqueGBufferUBOBuffers_[frame.frameIndex].upload(&gbufferUBOData_, sizeof(gbufferUBOData_), 0);

	//	cmd.bindDescriptorSets(
	//		vk::PipelineBindPoint::eGraphics,
	//		opaqueGBufferPipeline_.getLayout(),
	//		0,
	//		1, &set,
	//		0, nullptr
	//	);

	//	const ChunkDrawList& list = in.world->getOpaqueDrawList();
	//	for (const auto& item : list.items)
	//	{
	//		ChunkPushConstants pc{};
	//		pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

	//		cmd.pushConstants(
	//			opaqueGBufferPipeline_.getLayout(),
	//			vk::ShaderStageFlagBits::eVertex,
	//			0,
	//			sizeof(ChunkPushConstants),
	//			&pc
	//		);

	//		item.gpu->drawOpaque(&frame, nullptr);
	//	} // end for

	//	cmd.endDebugUtilsLabelEXT();
	//}
	//// shadow
	//else if (renderTarget == RenderTargetDX12::Shadow)
	//{
	//	cmd.beginDebugUtilsLabelEXT({ "ChunkPassDX12-Shadow::cmd" });

	//	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, opaqueShadowPipeline_.getPipeline());

	//	vk::DescriptorSet set = opaqueShadowDescriptorSets_[frame.frameIndex].getSet();

	//	shadowUBOData_ = {};

	//	shadowUBOData_.u_lightSpaceMatrix = lightSpaceMatrix;

	//	opaqueShadowUBOBuffers_[frame.frameIndex].upload(&shadowUBOData_, sizeof(shadowUBOData_), 0);

	//	cmd.bindDescriptorSets(
	//		vk::PipelineBindPoint::eGraphics,
	//		opaqueShadowPipeline_.getLayout(),
	//		0,
	//		1, &set,
	//		0, nullptr
	//	);

	//	const ChunkDrawList& list = in.world->getOpaqueDrawList();
	//	for (const auto& item : list.items)
	//	{
	//		ChunkPushConstants pc{};
	//		pc.u_chunkOrigin = glm::vec4(item.chunkOrigin, 0.0f);

	//		cmd.pushConstants(
	//			opaqueShadowPipeline_.getLayout(),
	//			vk::ShaderStageFlagBits::eVertex,
	//			0,
	//			sizeof(ChunkPushConstants),
	//			&pc
	//		);

	//		item.gpu->drawOpaque(&frame, nullptr);
	//	} // end for

	//	cmd.endDebugUtilsLabelEXT();
	//}
} // end of renderOpaque()


//--- PRIVATE ---//
void ChunkPassDX12::updateDescriptorSet(uint32_t frameIndex)
{
	// OPAQUE REGULAR
	{
		DescriptorSetDX12& set = opaqueDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (opaqueUBOBuffers_[frameIndex].valid())
		{
			set.writeUniformBuffer(
				TO_API_FORM(ChunkBinding::UBO),
				opaqueUBOBuffers_[frameIndex],
				sizeof(ChunkOpaqueUBO)
			);
		}

		if (atlas_.valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::AtlasTex),
				atlas_.image()
			);
		}

		if (ssaoBlurTex_ && ssaoBlurTex_->valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::SSAOTex),
				*ssaoBlurTex_
			);
		}

		if (shadowMapTex_ && shadowMapTex_->valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::ShadowTex),
				*shadowMapTex_
			);
		}
	}

	// REFLECTION
	{
		DescriptorSetDX12& set = reflectionDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (reflUBOBuffers_[frameIndex].valid())
		{
			set.writeUniformBuffer(
				TO_API_FORM(ChunkBinding::UBO),
				reflUBOBuffers_[frameIndex],
				sizeof(ChunkOpaqueUBO)
			);
		}

		if (atlas_.valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::AtlasTex),
				atlas_.image()
			);
		}

		if (ssaoBlurTex_ && ssaoBlurTex_->valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::SSAOTex),
				*ssaoBlurTex_
			);
		}

		if (shadowMapTex_ && shadowMapTex_->valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::ShadowTex),
				*shadowMapTex_
			);
		}
	}

	// REFRACTION
	{
		DescriptorSetDX12& set = refractionDescriptorSets_[frameIndex];
		if (!set.valid())
		{
			return;
		}

		if (refrUBOBuffers_[frameIndex].valid())
		{
			set.writeUniformBuffer(
				TO_API_FORM(ChunkBinding::UBO),
				refrUBOBuffers_[frameIndex],
				sizeof(ChunkOpaqueUBO)
			);
		}

		if (atlas_.valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::AtlasTex),
				atlas_.image()
			);
		}

		if (ssaoBlurTex_ && ssaoBlurTex_->valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::SSAOTex),
				*ssaoBlurTex_
			);
		}

		if (shadowMapTex_ && shadowMapTex_->valid())
		{
			set.writeTextureSRV(
				TO_API_FORM(ChunkBinding::ShadowTex),
				*shadowMapTex_
			);
		}
	}

	//// GBUFFER
	//{
	//	DescriptorSetDX12& set = opaqueGBufferDescriptorSets_[frameIndex];
	//	if (!set.valid())
	//	{
	//		return;
	//	}

	//	if (opaqueGBufferUBOBuffers_[frameIndex].valid())
	//	{
	//		set.writeUniformBuffer(
	//			TO_API_FORM(GbufferBinding::UBO),
	//			opaqueGBufferUBOBuffers_[frameIndex],
	//			sizeof(GbufferUBO)
	//		);
	//	}
	//}

	//// SHADOW
	//{
	//	DescriptorSetDX12& set = opaqueShadowDescriptorSets_[frameIndex];
	//	if (!set.valid())
	//	{
	//		return;
	//	}

	//	if (opaqueShadowUBOBuffers_[frameIndex].valid())
	//	{
	//		set.writeUniformBuffer(
	//			TO_API_FORM(ShadowMapPassBinding::UBO),
	//			opaqueShadowUBOBuffers_[frameIndex],
	//			sizeof(ShadowMapPassUBO)
	//		);
	//	}
	//}
} // end of updateDescriptorSet()

void ChunkPassDX12::updateSingleDescriptorSet(uint32_t frameIndex, RenderTargetDX12 renderTarget)
{
	if (renderTarget == RenderTargetDX12::Default)
	{
		// OPAQUE DEFAULT
		{
			DescriptorSetDX12& set = opaqueDescriptorSets_[frameIndex];
			if (!set.valid())
			{
				return;
			}

			if (opaqueUBOBuffers_[frameIndex].valid())
			{
				set.writeUniformBuffer(
					TO_API_FORM(ChunkBinding::UBO),
					opaqueUBOBuffers_[frameIndex],
					sizeof(ChunkOpaqueUBO)
				);
			}

			if (atlas_.valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::AtlasTex),
					atlas_.image()
				);
			}

			if (ssaoBlurTex_ && ssaoBlurTex_->valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::SSAOTex),
					*ssaoBlurTex_
				);
			}

			if (shadowMapTex_ && shadowMapTex_->valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::ShadowTex),
					*shadowMapTex_
				);
			}
		}
	}
	else if (renderTarget == RenderTargetDX12::WaterReflection)
	{
		// REFLECTION
		{
			DescriptorSetDX12& set = reflectionDescriptorSets_[frameIndex];
			if (!set.valid())
			{
				return;
			}

			if (reflUBOBuffers_[frameIndex].valid())
			{
				set.writeUniformBuffer(
					TO_API_FORM(ChunkBinding::UBO),
					reflUBOBuffers_[frameIndex],
					sizeof(ChunkOpaqueUBO)
				);
			}

			if (atlas_.valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::AtlasTex),
					atlas_.image()
				);
			}

			if (ssaoBlurTex_ && ssaoBlurTex_->valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::SSAOTex),
					*ssaoBlurTex_
				);
			}

			if (shadowMapTex_ && shadowMapTex_->valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::ShadowTex),
					*shadowMapTex_
				);
			}
		}
	}
	else if (renderTarget == RenderTargetDX12::WaterRefraction)
	{
		// REFRACTION
		{
			DescriptorSetDX12& set = refractionDescriptorSets_[frameIndex];
			if (!set.valid())
			{
				return;
			}

			if (refrUBOBuffers_[frameIndex].valid())
			{
				set.writeUniformBuffer(
					TO_API_FORM(ChunkBinding::UBO),
					refrUBOBuffers_[frameIndex],
					sizeof(ChunkOpaqueUBO)
				);
			}

			if (atlas_.valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::AtlasTex),
					atlas_.image()
				);
			}

			if (ssaoBlurTex_ && ssaoBlurTex_->valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::SSAOTex),
					*ssaoBlurTex_
				);
			}

			if (shadowMapTex_ && shadowMapTex_->valid())
			{
				set.writeTextureSRV(
					TO_API_FORM(ChunkBinding::ShadowTex),
					*shadowMapTex_
				);
			}
		}
	}
	//else if (renderTarget == RenderTargetDX12::GBuffer)
	//{
	//	// GBUFFER
	//	{
	//		DescriptorSetDX12& set = opaqueGBufferDescriptorSets_[frameIndex];
	//		if (!set.valid())
	//		{
	//			return;
	//		}

	//		if (opaqueGBufferUBOBuffers_[frameIndex].valid())
	//		{
	//			set.writeUniformBuffer(
	//				TO_API_FORM(GbufferBinding::UBO),
	//				opaqueGBufferUBOBuffers_[frameIndex],
	//				sizeof(GbufferUBO)
	//			);
	//		}
	//	}
	//}
	//else if (renderTarget == RenderTargetDX12::Shadow)
	//{
	//	// SHADOW
	//	{
	//		DescriptorSetDX12& set = opaqueShadowDescriptorSets_[frameIndex];
	//		if (!set.valid())
	//		{
	//			return;
	//		}

	//		if (opaqueShadowUBOBuffers_[frameIndex].valid())
	//		{
	//			set.writeUniformBuffer(
	//				TO_API_FORM(ShadowMapPassBinding::UBO),
	//				opaqueShadowUBOBuffers_[frameIndex],
	//				sizeof(ShadowMapPassUBO)
	//			);
	//		}
	//	}
	//}
} // end of updateSingleDescriptorSet()

void ChunkPassDX12::createResources()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		// opaque
		opaqueUBOBuffers_[i].create(
			sizeof(ChunkOpaqueUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);

		// reflection
		reflUBOBuffers_[i].create(
			sizeof(ChunkOpaqueUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);

		// refraction
		refrUBOBuffers_[i].create(
			sizeof(ChunkOpaqueUBO),
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_FLAG_NONE,
			true
		);


		//// gbuffer
		//opaqueGBufferUBOBuffers_[i].create(
		//	sizeof(GbufferUBO),
		//	D3D12_HEAP_TYPE_UPLOAD,
		//	D3D12_RESOURCE_STATE_GENERIC_READ,
		//	D3D12_RESOURCE_FLAG_NONE,
		//	true
		//);

		//// shadow
		//opaqueShadowUBOBuffers_[i].create(
		//	sizeof(ShadowMapPassUBO),
		//	D3D12_HEAP_TYPE_UPLOAD,
		//	D3D12_RESOURCE_STATE_GENERIC_READ,
		//	D3D12_RESOURCE_FLAG_NONE,
		//	true
		//);
	} // end for
} // end of createResources()

void ChunkPassDX12::createDescriptorSets()
{
	const uint32_t frames = dx_->getMaxFramesInFlight();

	for (uint32_t i = 0; i < frames; ++i)
	{
		// opaque
		{
			DescriptorBindingDX12 uboBinding{
				.binding = TO_API_FORM(ChunkBinding::UBO),
				.type = DescriptorTypeDX12::UniformBuffer,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_ALL
			};
			DescriptorBindingDX12 atlasBinding{
				.binding = TO_API_FORM(ChunkBinding::AtlasTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};
			DescriptorBindingDX12 ssaoBinding{
				.binding = TO_API_FORM(ChunkBinding::SSAOTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};
			DescriptorBindingDX12 shadowMapBinding{
				.binding = TO_API_FORM(ChunkBinding::ShadowTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};
			PushConstantRangeDX12 chunkPushConstants{
				.binding = 0,
				.num32BitValues = sizeof(ChunkPushConstants) / 4,
				.registerSpace = 1,
				.visibility = D3D12_SHADER_VISIBILITY_VERTEX
			};

			opaqueDescriptorSets_[i].createLayout(
				{
					uboBinding,
					atlasBinding,
					ssaoBinding,
					shadowMapBinding
				},
				{
					chunkPushConstants
				}
			);

			opaqueDescriptorSets_[i].createPool(4);
			opaqueDescriptorSets_[i].allocate();

			opaqueDescriptorSets_[i].setDebugName(
				L"ChunkPassDX12-Opaque::DescriptorSet frame " + std::to_wstring(i)
			);
		}

		// reflection
		{
			DescriptorBindingDX12 uboBinding{
				.binding = TO_API_FORM(ChunkBinding::UBO),
				.type = DescriptorTypeDX12::UniformBuffer,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_ALL
			};
			DescriptorBindingDX12 atlasBinding{
				.binding = TO_API_FORM(ChunkBinding::AtlasTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};
			DescriptorBindingDX12 ssaoBinding{
				.binding = TO_API_FORM(ChunkBinding::SSAOTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};
			DescriptorBindingDX12 shadowMapBinding{
				.binding = TO_API_FORM(ChunkBinding::ShadowTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};

			reflectionDescriptorSets_[i].createLayout({
				uboBinding,
				atlasBinding,
				ssaoBinding,
				shadowMapBinding
				});

			reflectionDescriptorSets_[i].createPool(4);
			reflectionDescriptorSets_[i].allocate();

			reflectionDescriptorSets_[i].setDebugName(
				L"ChunkPassDX12-Reflection::DescriptorSet frame " + std::to_wstring(i)
			);
		}

		// refraction
		{
			DescriptorBindingDX12 uboBinding{
				.binding = TO_API_FORM(ChunkBinding::UBO),
				.type = DescriptorTypeDX12::UniformBuffer,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_ALL
			};
			DescriptorBindingDX12 atlasBinding{
				.binding = TO_API_FORM(ChunkBinding::AtlasTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};
			DescriptorBindingDX12 ssaoBinding{
				.binding = TO_API_FORM(ChunkBinding::SSAOTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};
			DescriptorBindingDX12 shadowMapBinding{
				.binding = TO_API_FORM(ChunkBinding::ShadowTex),
				.type = DescriptorTypeDX12::TextureSRV,
				.count = 1,
				.visibility = D3D12_SHADER_VISIBILITY_PIXEL
			};

			refractionDescriptorSets_[i].createLayout({
				uboBinding,
				atlasBinding,
				ssaoBinding,
				shadowMapBinding
				});

			refractionDescriptorSets_[i].createPool(4);
			refractionDescriptorSets_[i].allocate();

			refractionDescriptorSets_[i].setDebugName(
				L"ChunkPassDX12-Refraction::DescriptorSet frame " + std::to_wstring(i)
			);
		}

		//// gbuffer
		//{
		//	DescriptorBindingDX12 uboBinding{
		//		.binding = TO_API_FORM(GbufferBinding::UBO),
		//		.type = DescriptorTypeDX12::UniformBuffer,
		//		.count = 1,
		//		.visibility = D3D12_SHADER_VISIBILITY_ALL
		//	};

		//	opaqueGBufferDescriptorSets_[i].createLayout({
		//		uboBinding
		//		});

		//	opaqueGBufferDescriptorSets_[i].createPool(1);
		//	opaqueGBufferDescriptorSets_[i].allocate();

		//	opaqueGBufferDescriptorSets_[i].setDebugName(
		//		L"ChunkPassDX12-GBuffer::DescriptorSet frame " + std::to_wstring(i)
		//	);
		//}

		//// shadow
		//{
		//	DescriptorBindingDX12 uboBinding{
		//		.binding = TO_API_FORM(ShadowMapPassBinding::UBO),
		//		.type = DescriptorTypeDX12::UniformBuffer,
		//		.count = 1,
		//		.visibility = D3D12_SHADER_VISIBILITY_ALL
		//	};

		//	opaqueShadowDescriptorSets_[i].createLayout({
		//		uboBinding
		//		});

		//	opaqueShadowDescriptorSets_[i].createPool(1);
		//	opaqueShadowDescriptorSets_[i].allocate();

		//	opaqueShadowDescriptorSets_[i].setDebugName(
		//		L"ChunkPassDX12-Shadow::DescriptorSet frame " + std::to_wstring(i)
		//	);
		//}
	} // end for
} // end of createDescriptorSets()

void ChunkPassDX12::createPipelines(
	RenderTargetFormatsDX12 defaultFormats,
	RenderTargetFormatsDX12 gbufferFormats,
	RenderTargetFormatsDX12 shadowFormats
)
{
	// opaque
	{
		GraphicsPipelineDescDX12 desc{
			.vertShader = opaqueShader_->vertShader(),
			.fragShader = opaqueShader_->fragShader(),

			.rootSignature = opaqueDescriptorSets_[0].getRootSignature(),

			.inputElements =
			{
				D3D12_INPUT_ELEMENT_DESC{
					.SemanticName = "POSITION",
					.SemanticIndex = 0,
					.Format = DXGI_FORMAT_R32_UINT,
					.InputSlot = 0,
					.AlignedByteOffset = 0,
					.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
					.InstanceDataStepRate = 0
				}
			},

			.cullMode = D3D12_CULL_MODE_BACK,
			.frontCCW = FALSE,

			.depthTestEnable = TRUE,
			.depthWriteEnable = TRUE,
			.depthCompareFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,

			.colorFormat = defaultFormats.colorFormat,
			.depthFormat = defaultFormats.depthFormat
		};

			opaquePipeline_.create(desc);
			opaquePipeline_.setDebugName(L"CubemapDX12-Default::Pipeline");
	}
	
	//// gbuffer
	//{
	//	GraphicsPipelineDescDX12 desc{
	//	.vertShader = opaqueGBufferShader_->vertShader(),
	//	.fragShader = opaqueGBufferShader_->fragShader(),

	//	.rootSignature = opaqueGBufferDescriptorSets_[0].getRootSignature(),

	//	.inputElements =
	//	{
	//		D3D12_INPUT_ELEMENT_DESC{
	//			.SemanticName = "POSITION",
	//			.SemanticIndex = 0,
	//			.Format = DXGI_FORMAT_R32_UINT,
	//			.InputSlot = 0,
	//			.AlignedByteOffset = 0,
	//			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
	//			.InstanceDataStepRate = 0
	//		}
	//	},

	//	.cullMode = D3D12_CULL_MODE_BACK,
	//	.frontCCW = FALSE,

	//	.depthTestEnable = TRUE,
	//	.depthWriteEnable = TRUE,
	//	.depthCompareFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,

	//	.colorFormat = gbufferFormats.colorFormat,
	//	.depthFormat = gbufferFormats.depthFormat,
	//	};

	//	opaqueGBufferPipeline_.create(desc);
	//	opaqueGBufferPipeline_.setDebugName(L"ChunkPassDX12-GBuffer::Pipeline");
	//}

	//// shadow
	//{
	//	GraphicsPipelineDescDX12 desc{
	//	.vertShader = opaqueShadowShader_->vertShader(),
	//	.fragShader = opaqueShadowShader_->fragShader(),

	//	.rootSignature = opaqueShadowDescriptorSets_[0].getRootSignature(),

	//	.inputElements =
	//	{
	//		D3D12_INPUT_ELEMENT_DESC{
	//			.SemanticName = "POSITION",
	//			.SemanticIndex = 0,
	//			.Format = DXGI_FORMAT_R32_UINT,
	//			.InputSlot = 0,
	//			.AlignedByteOffset = 0,
	//			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
	//			.InstanceDataStepRate = 0
	//		}
	//	},

	//	.cullMode = D3D12_CULL_MODE_BACK,
	//	.frontCCW = FALSE,

	//	.depthTestEnable = TRUE,
	//	.depthWriteEnable = TRUE,
	//	.depthCompareFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,

	//	.colorFormat = shadowFormats.colorFormat,
	//	.depthFormat = shadowFormats.depthFormat,
	//	};

	//	opaqueShadowPipeline_.create(desc);
	//	opaqueShadowPipeline_.setDebugName(L"ChunkPassDX12-Shadow::Pipeline");
	//}
} // end of createPipelines()
