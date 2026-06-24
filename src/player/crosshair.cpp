#include "crosshair.h"

#include "constants.h"
#include "shader.h"

#include <glad/glad.h>

#include <memory>

using namespace Crosshair_Constants;

//--- PUBLIC ---//
Crosshair::Crosshair() = default;

Crosshair::~Crosshair()
{
	destroyGL();
} // end of destructor

void Crosshair::init()
{
	destroyGL();

	crosshairShader_ = std::make_unique<Shader>("crosshair/crosshair.vert", "crosshair/crosshair.frag");

	glCreateVertexArrays(1, &vao_);
	glCreateBuffers(1, &vbo_);

	glNamedBufferData(vbo_, sizeof(VERTICES), VERTICES, GL_STATIC_DRAW);

	// attach buffer to vao
	glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(float) * 2);

	glEnableVertexArrayAttrib(vao_, 0);
	glVertexArrayAttribFormat(vao_, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao_, 0, 0);
} // end of init()

void Crosshair::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12
)
{
	if (!crosshairShader_ || vao_ == 0)
		return;

	GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST);

	crosshairShader_->use();
	glBindVertexArray(vao_);
	glDrawArrays(GL_LINES, 0, 4);

	if (wasDepth) 
		glEnable(GL_DEPTH_TEST);
	else 
		glDisable(GL_DEPTH_TEST);
} // end of render()


//--- PRIVATE ---//
void Crosshair::destroyGL()
{
	if (vao_)
	{
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}
	if (vbo_)
	{
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}
} // end of destroyGL()