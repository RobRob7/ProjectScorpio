#include "shadow_map_pass_dx12.h"

#include "constants.h"
#include "render_target_dx12.h"
#include "render_inputs.h"
#include "chunk_manager.h"

#include "frame_context_dx12.h"
#include "dx12_main.h"

#include "light_dx12.h"
#include "chunk_pass_dx12.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

//--- PUBLIC ---//
ShadowMapPassDX12::ShadowMapPassDX12(DX12Main& dx)
	: dx_(&dx),
	depthImage_(dx)
{
} // end of constructor

ShadowMapPassDX12::~ShadowMapPassDX12() = default;

void ShadowMapPassDX12::init()
{
	createAttachments();
} // end of init()

void ShadowMapPassDX12::render(
	ChunkPassDX12& chunk,
	const RenderInputs& in,
	const FrameContextDX12& frame
)
{
	ID3D12GraphicsCommandList* cmd = frame.cmd;
	cmd->SetName({ L"ShadowMapPassDX12::cmd" });

	dx_->beginGPUEvent(cmd, L"ShadowMapPassDX12::render");
	{
		depthImage_.transitionToDepthWrite(cmd);

		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(width_);
		viewport.Height = static_cast<float>(height_);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT scissor{};
		scissor.left = 0;
		scissor.top = 0;
		scissor.right = static_cast<LONG>(width_);
		scissor.bottom = static_cast<LONG>(height_);

		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissor);

		D3D12_CPU_DESCRIPTOR_HANDLE depthDSV{ depthImage_.dsvCPU() };

		cmd->OMSetRenderTargets(
			0,
			nullptr,
			FALSE,
			&depthDSV
		);

		cmd->ClearDepthStencilView(
			depthDSV,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f,
			0,
			0,
			nullptr
		);

		// configure light space transform
		glm::vec3 minWS, maxWS;
		if (!in.world->buildVisibleChunkBounds(minWS, maxWS))
		{
			dx_->endGPUEvent(cmd);

			depthImage_.transitionToShaderRead(cmd, true);

			return;
		}
		buildLightSpaceBounds(in, minWS, maxWS);

		chunk.renderOpaque(
			RenderTargetDX12::Shadow,
			in,
			frame,
			lightView_,
			lightProj_,
			lightSpaceMatrix_
		);
	}

	depthImage_.transitionToShaderRead(cmd, true);

	dx_->endGPUEvent(cmd);
} // end of render()


//--- PRIVATE ---//
void ShadowMapPassDX12::buildLightSpaceBounds(
	const RenderInputs& in,
	const glm::vec3& minWS,
	const glm::vec3& maxWS
)
{
	glm::vec3 centerWS = 0.5f * (minWS + maxWS);

	glm::vec3 lightDir = in.light->getDirection();

	float lightDistance = 200.0f;
	glm::vec3 lightPos = centerWS - lightDir * lightDistance;

	// build light view
	lightView_ = glm::lookAt(
		lightPos,
		centerWS,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	// build the 8 corners of the visible world-space bounds
	glm::vec3 corners[8] =
	{
		{minWS.x, minWS.y, minWS.z},
		{maxWS.x, minWS.y, minWS.z},
		{minWS.x, maxWS.y, minWS.z},
		{maxWS.x, maxWS.y, minWS.z},
		{minWS.x, minWS.y, maxWS.z},
		{maxWS.x, minWS.y, maxWS.z},
		{minWS.x, maxWS.y, maxWS.z},
		{maxWS.x, maxWS.y, maxWS.z}
	};

	// transform bounds into light space and fit min/max
	glm::vec3 minLS(FLT_MAX);
	glm::vec3 maxLS(-FLT_MAX);

	for (const glm::vec3& c : corners)
	{
		glm::vec4 ls = lightView_ * glm::vec4(c, 1.0f);
		glm::vec3 p(ls);

		minLS = glm::min(minLS, p);
		maxLS = glm::max(maxLS, p);
	} // end for

	// stable shadow mapping
	// padding
	const float xyPad = 8.0f;
	const float zPad = 16.0f;

	float widthLS = maxLS.x - minLS.x;
	float heightLS = maxLS.y - minLS.y;

	float extent = std::max(widthLS, heightLS);
	extent += xyPad * 2.0f;
	extent = std::ceil(extent / CHUNK_SIZE) * CHUNK_SIZE;

	glm::vec3 centerLS = 0.5f * (minLS + maxLS);

	float texelSize = extent / 
		static_cast<float>(std::max(1, static_cast<int>(width_)));

	centerLS.x = std::round(centerLS.x / texelSize) * texelSize;
	centerLS.y = std::round(centerLS.y / texelSize) * texelSize;

	// rebuild snapped X/Y bounds
	minLS.x = centerLS.x - extent * 0.5f;
	maxLS.x = centerLS.x + extent * 0.5f;
	minLS.y = centerLS.y - extent * 0.5f;
	maxLS.y = centerLS.y + extent * 0.5f;

	float nearPlane = -maxLS.z - zPad;
	float farPlane = -minLS.z + zPad;

	// near/far plane clamp
	nearPlane = std::max(0.1f, nearPlane);
	farPlane = std::max(nearPlane + 1.0f, farPlane);

	// build fitted ortho projection
	lightProj_ = glm::orthoRH_ZO(
		minLS.x, maxLS.x,
		minLS.y, maxLS.y,
		nearPlane, farPlane
	);

	lightSpaceMatrix_ = lightProj_ * lightView_;
} // end of buildLightSpaceBounds()

void ShadowMapPassDX12::createAttachments()
{
	depthImage_.createSampledDepthImage(
		width_,
		height_
	);

	depthImage_.setDebugName(L"ShadowMapPassDX12-DepthImage");
} // end of createAttachments()
