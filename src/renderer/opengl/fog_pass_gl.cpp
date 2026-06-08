#include "fog_pass_gl.h"

#include "render_settings.h"

#include "constants.h"
#include "compute_shader.h"

#include <glad/glad.h>

#include <memory>
#include <stdexcept>
#include <algorithm>

//--- PUBLIC ---//
FogPassGL::FogPassGL(const RenderSettings& rs)
	: rs_(rs)
{
	factor_ = std::max(1u, rs_.resScale.FOG);
} // end of constructor

FogPassGL::~FogPassGL()
{
	destroyGL();
} // end of destructor

void FogPassGL::init()
{
	destroyGL();

	computeShader_ = std::make_unique<ComputeShader>(
		"fogpass/fog.comp"
	);

	uboBuffer_.init<sizeof(Fog_Constants::FogPassUBO)>();
} // end of init()

void FogPassGL::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;

	sourceWidth_ = w;
	sourceHeight_ = h;

	uint32_t newFactor = std::max(1u, rs_.resScale.FOG);

	int newWidth = (w + newFactor - 1) / newFactor;
	int newHeight = (h + newFactor - 1) / newFactor;

	if (newWidth == width_ &&
		newHeight == height_ &&
		newFactor == factor_)
	{
		return;
	}

	factor_ = newFactor;
	width_ = newWidth;
	height_ = newHeight;

	workGroupX_ = (width_ + (numWorkGroups_ - 1)) / numWorkGroups_;
	workGroupY_ = (height_ + (numWorkGroups_ - 1)) / numWorkGroups_;

	destroyAttachment();
	createAttachment();
} // end of resize()

void FogPassGL::clearColorImage(const std::array<float, 4>& color)
{
	syncSettings();

	if (!outputImage_) return;

	glClearTexImage(
		outputImage_,
		0,
		GL_RGBA,
		GL_FLOAT,
		color.data()
	);

	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
} // end of clearColorImage()

void FogPassGL::render(Fog_Constants::FogPassUBO& ubo)
{
	syncSettings();

	if (!computeShader_ || 
		!inputDepthImage_ ||
		!outputImage_)
		return;

	computeShader_->use();

	glBindTextureUnit(TO_API_FORM(FogPassBinding::ForwardDepthTex), inputDepthImage_);
	glBindImageTexture(
		TO_API_FORM(FogPassBinding::OutColorTex),
		outputImage_,
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
void FogPassGL::syncSettings()
{
	resize(sourceWidth_, sourceHeight_);
} // end of syncSettings()

void FogPassGL::destroyAttachment()
{
	if (outputImage_)
	{
		glDeleteTextures(1, &outputImage_);
		outputImage_ = 0;
	}
} // end of destroyAttachment()

void FogPassGL::destroyGL()
{
	destroyAttachment();
} // end of destroyGL()

void FogPassGL::createAttachment()
{
	glCreateTextures(GL_TEXTURE_2D, 1, &outputImage_);
	glTextureStorage2D(outputImage_, 1, outImageFormat_, width_, height_);
	glTextureParameteri(outputImage_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(outputImage_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(outputImage_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(outputImage_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
} // end of createAttachment()