#ifndef SHADOW_MAP_PASS_DX12_H
#define SHADOW_MAP_PASS_DX12_H

#include "constants.h"
#include "render_target_dx12.h"

#include "image_dx12.h"

#include <glm/glm.hpp>

class DX12Main;
class ShaderDX12;
class ChunkPassDX12;
struct RenderInputs;
struct FrameContextDX12;

class ShadowMapPassDX12
{
public:
	explicit ShadowMapPassDX12(DX12Main& dx);
	~ShadowMapPassDX12();

	void init();

	void render(
		ChunkPassDX12& chunk,
		const RenderInputs& in,
		const FrameContextDX12& frame
	);

	ImageDX12& getDepthImage() { return depthImage_; }
	const ImageDX12& getDepthImage() const { return depthImage_; }

	const glm::mat4& getLightSpaceMatrix() const { return lightSpaceMatrix_; }

private:
	void buildLightSpaceBounds(
		const RenderInputs& in,
		const glm::vec3& minWS,
		const glm::vec3& maxWS
	);
	void createAttachments();
private:
	DX12Main* dx_{ nullptr };

	uint32_t width_{ Shadow_Map_Constants::SHADOW_RESOLUTION };
	uint32_t height_{ Shadow_Map_Constants::SHADOW_RESOLUTION };

	glm::mat4 lightSpaceMatrix_{};
	glm::mat4 lightView_{};
	glm::mat4 lightProj_{};

	ImageDX12 depthImage_;
	DXGI_FORMAT depthFormat_{ DXGI_FORMAT_D32_FLOAT };
};

#endif
