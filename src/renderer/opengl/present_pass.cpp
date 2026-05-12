#include "present_pass.h"

#include "bindings.h"

#include "shader.h"

#include <glad/glad.h>

//--- PUBLIC ---//
PresentPass::PresentPass() = default;

PresentPass::~PresentPass()
{
	destroyGL();
} // end of destructor

void PresentPass::init()
{
	destroyGL();

	shader_ = std::make_unique<Shader>("presentpass/present.vert", "presentpass/present.frag");

	glCreateVertexArrays(1, &fsVao_);
} // end of init()

void PresentPass::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (w == width_ && h == height_) return;

	width_ = w;
	height_ = h;
} // end of resize()

void PresentPass::render(uint32_t finalColorTex)
{
	if (!shader_ || 
		!finalColorTex || 
		width_ <= 0 || 
		height_ <= 0 || 
		fsVao_ == 0)
		return;

	// bind textures
	glBindTextureUnit(TO_API_FORM(PresentPassBinding::ForwardColorTex), finalColorTex);

	const GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);

	glViewport(0, 0, width_, height_);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width_, height_);
	glDisable(GL_DEPTH_TEST);

	shader_->use();
	glBindVertexArray(fsVao_);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);

	if (prevDepth) glEnable(GL_DEPTH_TEST);
} // end of render()


//--- PRIVATE ---//
void PresentPass::destroyGL()
{
	if (fsVao_)
	{
		glDeleteVertexArrays(1, &fsVao_);
		fsVao_ = 0;
	}
} // end of destroyGL()