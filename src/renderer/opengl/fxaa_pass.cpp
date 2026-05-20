#include "fxaa_pass.h"

#include "constants.h"
#include "bindings.h"

#include "shader.h"

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <stdexcept>

using namespace FXAA_Constants;

//--- PUBLIC ---//
FXAAPass::FXAAPass() = default;

FXAAPass::~FXAAPass()
{
	destroyGL();
} // end of destructor

void FXAAPass::init()
{
	destroyGL();

	shader_ = std::make_unique<Shader>("fxaapass/fxaa.vert", "fxaapass/fxaa.frag");

	ubo_.init<sizeof(FXAAPassUBO)>();

	glCreateVertexArrays(1, &fsVao_);
} // end of init()

void FXAAPass::resize(int w, int h)
{
	if (!shader_)
		return;
	if (w <= 0 || h <= 0) 
		return;
	if (w == width_ && h == height_) 
		return;

	destroyTargets();
	width_ = w;
	height_ = h;
	createTargets();
} // end of resize()

void FXAAPass::render(uint32_t sceneColorTex)
{
	if (!shader_ || 
		!sceneColorTex || 
		width_ <= 0 || 
		height_ <= 0 || 
		fsVao_ == 0 || 
		fxaaFBO_ == 0)
		return;

	glBindTextureUnit(TO_API_FORM(FXAAPassBinding::ForwardColorTex), sceneColorTex);

	const GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);

	glViewport(0, 0, width_, height_);

	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, fxaaFBO_);
	glBindVertexArray(fsVao_);

	shader_->use();
	fxaaPassUBO_.u_inverseScreenSize = glm::vec2(1.0f / static_cast<float>(width_), 1.0f / static_cast<float>(height_));
	fxaaPassUBO_.u_edgeSharpnessQuality = EDGE_SHARP_QUALITY;
	fxaaPassUBO_.u_edgeThresholdMax = EDGE_THRESH_MAX;
	fxaaPassUBO_.u_edgeThresholdMin = EDGE_THRESH_MIN;
	ubo_.update(&fxaaPassUBO_, sizeof(fxaaPassUBO_));
	ubo_.bind();

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (prevDepth) glEnable(GL_DEPTH_TEST);
} // end of render()

uint32_t FXAAPass::getOutputTex() const
{
	return fxaaColorTex_;
} // end of getOutputTex()


//--- PRIVATE ---//
void FXAAPass::createTargets()
{
	glCreateFramebuffers(1, &fxaaFBO_);
	glCreateTextures(GL_TEXTURE_2D, 1, &fxaaColorTex_);

	glTextureStorage2D(fxaaColorTex_, 1, GL_RGBA16F, width_, height_);
	glTextureParameteri(fxaaColorTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(fxaaColorTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(fxaaColorTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(fxaaColorTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glNamedFramebufferTexture(fxaaFBO_, GL_COLOR_ATTACHMENT0, fxaaColorTex_, 0);
	GLenum buf = GL_COLOR_ATTACHMENT0;
	glNamedFramebufferDrawBuffers(fxaaFBO_, 1, &buf);

	if (glCheckNamedFramebufferStatus(fxaaFBO_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("FXAA FBO incomplete!");
	}
} // end of createTargets()

void FXAAPass::destroyTargets()
{
	if (fxaaFBO_)
	{
		glDeleteFramebuffers(1, &fxaaFBO_);
		fxaaFBO_ = 0;
	}
	if (fxaaColorTex_)
	{
		glDeleteTextures(1, &fxaaColorTex_);
		fxaaColorTex_ = 0;
	}
} // end of destroyTargets()

void FXAAPass::destroyGL()
{
	destroyTargets();

	if (fsVao_)
	{
		glDeleteVertexArrays(1, &fsVao_);
		fsVao_ = 0;
	}

	width_ = 0;
	height_ = 0;
} // end of destroyGL()
