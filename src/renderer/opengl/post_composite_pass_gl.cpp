#include "post_composite_pass_gl.h"

#include "shader.h"

#include "bindings.h"

#include <stdexcept>

//--- PUBLIC ---//
PostCompositePassGL::PostCompositePassGL() = default;

PostCompositePassGL::~PostCompositePassGL()
{
	destroyGL();
} // end of destructor

void PostCompositePassGL::init()
{
	destroyGL();

	shader_ = std::make_unique<Shader>(
		"compositepass/post_compositepass.vert",
		"compositepass/post_compositepass.frag"
	);

	glCreateVertexArrays(1, &fsVao_);
} // end of init()

void PostCompositePassGL::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (w == width_ && h == height_) return;

	width_ = w;
	height_ = h;

	destroyAttachment();
	createAttachment();
} // end of resize()

void PostCompositePassGL::render()
{
	if (!shader_ ||
		!fogColorImage_ ||
		!godRayColorImage_ ||
		!sceneColorImage_||
		!fsVao_)
		return;

	glBindTextureUnit(TO_API_FORM(PostCompositePassBinding::FogTex), *fogColorImage_);
	glBindTextureUnit(TO_API_FORM(PostCompositePassBinding::GodRayTex), *godRayColorImage_);
	glBindTextureUnit(TO_API_FORM(PostCompositePassBinding::SceneColorTex), *sceneColorImage_);

	const GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
	glViewport(0, 0, width_, height_);
	glDisable(GL_DEPTH_TEST);

	shader_->use();
	glBindVertexArray(fsVao_);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);

	if (prevDepth) glEnable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
} // end of render()


//--- PRIVATE ---//
void PostCompositePassGL::destroyAttachment()
{
	if (fbo_)
	{
		glDeleteFramebuffers(1, &fbo_);
		fbo_ = 0;
	}

	if (postColorImage_)
	{
		glDeleteTextures(1, &postColorImage_);
		postColorImage_ = 0;
	}
} // end of destroyAttachment()

void PostCompositePassGL::destroyGL()
{
	destroyAttachment();

	if (fsVao_)
	{
		glDeleteVertexArrays(1, &fsVao_);
		fsVao_ = 0;
	}
} // end of destroyGL()

void PostCompositePassGL::createAttachment()
{
	glCreateFramebuffers(1, &fbo_);
	
	glCreateTextures(GL_TEXTURE_2D, 1, &postColorImage_);
	glTextureStorage2D(postColorImage_, 1, postColorFormat_, width_, height_);
	glTextureParameteri(postColorImage_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(postColorImage_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(postColorImage_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(postColorImage_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// attach
	glNamedFramebufferTexture(fbo_, GL_COLOR_ATTACHMENT0, postColorImage_, 0);

	const GLenum drawBuff = GL_COLOR_ATTACHMENT0;
	glNamedFramebufferDrawBuffers(fbo_, 1, &drawBuff);

	if (glCheckNamedFramebufferStatus(fbo_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("Post Composite Pass FBO incomplete!");
	}
} // end of createAttachment()