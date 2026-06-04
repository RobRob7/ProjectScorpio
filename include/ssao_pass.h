#ifndef SSAO_PASS_H
#define SSAO_PASS_H

#include "constants.h"

#include "bindings.h"

#include "ubo_gl.h"

#include <glm/glm.hpp>

#include <memory>
#include <array>

class Shader;
struct RenderSettings;

class SSAOPass
{
public:
	SSAOPass(const RenderSettings& rs);
	~SSAOPass();

	void init();
	void resize(int w, int h);
	void destroyGL();

	void render(
		SSAO_Constants::SSAORawUBO& rawUBO,
		SSAO_Constants::SSAOBlurUBO& blurUBO,
		uint32_t normalTex, 
		uint32_t depthTex, 
		const glm::mat4& proj
	);

	uint32_t aoRawTexture() const;
	uint32_t aoBlurTexture() const;

private:
	void createTargets();
	void destroyTargets();
	void createNoise();
	void createKernel();
private:
	const RenderSettings& rs_;

	uint32_t factor_{};
	uint32_t width_{};
	uint32_t height_{};

	uint32_t fboRaw_{};
	uint32_t fboBlur_{};
	uint32_t aoRaw_{};
	uint32_t aoBlur_{};
	uint32_t noiseTexture_{};
	
	uint32_t fsVao_{};

	std::unique_ptr<Shader> ssaoShader_;
	std::unique_ptr<Shader> blurShader_;

	SSAO_Constants::SSAORawSamplesUBO ssaoRawSamplesUBO_{};
	UBOGL ssaoRawSamplesUBOBuffer_{ TO_API_FORM(SSAORawBinding::SamplesUBO) };
	UBOGL ssaoRawUBOBuffer_{ TO_API_FORM(SSAORawBinding::UBO) };
	UBOGL ssaoBlurUBOBuffer_{ TO_API_FORM(SSAOBlurBinding::UBO) };

	std::array<glm::vec4, SSAO_Constants::MAX_KERNEL_SIZE> samples_{};
};

#endif
