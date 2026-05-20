#include "fog_pass.h"

#include "constants.h"
#include "compute_shader.h"

#include <glad/glad.h>

#include <memory>
#include <stdexcept>

//--- PUBLIC ---//
FogPass::FogPass() = default;

FogPass::~FogPass()
{
	destroyGL();
} // end of destructor

void FogPass::init()
{
	destroyGL();

	computeShader_ = std::make_unique<ComputeShader>(
		"fogpass/fog.comp"
	);

	uboBuffer_.init<sizeof(Fog_Constants::FogPassUBO)>();
} // end of init()

void FogPass::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;

	int newWidth = (w + resFactor_ - 1) / resFactor_;
	int newHeight = (h + resFactor_ - 1) / resFactor_;

	if (newWidth == width_ && newHeight == height_)
		return;

	width_ = newWidth;
	height_ = newHeight;

	workGroupX_ = (width_ + (numWorkGroups_ - 1)) / numWorkGroups_;
	workGroupY_ = (height_ + (numWorkGroups_ - 1)) / numWorkGroups_;

	if (outputTex_)
	{
		glDeleteTextures(1, &outputTex_);
		outputTex_ = 0;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &outputTex_);
	glTextureStorage2D(outputTex_, 1, GL_RGBA16F, width_, height_);
	glTextureParameteri(outputTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(outputTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(outputTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(outputTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
} // end of resize()

void FogPass::render(
	uint32_t sceneDepthTex,
	uint32_t shadowMapTex,
	Fog_Constants::FogPassUBO& ubo
)
{
	if (!computeShader_ || 
		!sceneDepthTex ||
		!shadowMapTex ||
		!outputTex_)
		return;

	computeShader_->use();

	glBindTextureUnit(TO_API_FORM(FogPassBinding::ForwardDepthTex), sceneDepthTex);
	glBindTextureUnit(TO_API_FORM(FogPassBinding::ShadowMapTex), shadowMapTex);
	glBindImageTexture(
		TO_API_FORM(FogPassBinding::OutColorTex),
		outputTex_,
		0,
		GL_FALSE,
		0,
		GL_WRITE_ONLY,
		GL_RGBA16F
	);
	
	uboBuffer_.update(&ubo, sizeof(ubo));
	uboBuffer_.bind();

	glDispatchCompute(
		workGroupX_,
		workGroupY_,
		1
	);

	glMemoryBarrier(
		GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | 
		GL_TEXTURE_FETCH_BARRIER_BIT
	);
} // end of render()


//--- PRIVATE ---//
void FogPass::destroyGL()
{
	if (outputTex_)
	{
		glDeleteTextures(1, &outputTex_);
		outputTex_ = 0;
	}
} // end of destroyGL()