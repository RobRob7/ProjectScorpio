#include "cubemap_gl.h"

#include "constants.h"
#include "bindings.h"

#include "texture_gl.h"
#include "shader.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>

using namespace Cubemap_Constants;

//--- PUBLIC ---//
CubemapGL::CubemapGL(const std::array<std::string_view, 6>& textures)
	: faces_(textures)
{
} // end of constructor

// destructor
CubemapGL::~CubemapGL()
{
	destroyGL();
} // end of destructor

void CubemapGL::init()
{
	destroyGL();

	shader_ = std::make_unique<Shader>(
		"cubemap/cubemap.vert", 
		"cubemap/cubemap.frag"
	);

	cubemapTextureNight_ = std::make_unique<TextureGL>(faces_);
	cubemapTextureDay_ = std::make_unique<TextureGL>(DAY_FACES);

	// VAO + VBO
	glCreateVertexArrays(1, &vao_);
	glCreateBuffers(1, &vbo_);

	// upload vertex data
	glNamedBufferData(vbo_, SKYBOX_VERTICES.size() * sizeof(float), SKYBOX_VERTICES.data(), GL_STATIC_DRAW);

	// attach vbo to vao
	glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, 3 * sizeof(float));

	// pos attribute
	glEnableVertexArrayAttrib(vao_, 0);
	glVertexArrayAttribFormat(vao_, 0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(vao_, 0, 0);

	// UBO
	ubo_.init<sizeof(CubemapUBO)>();
} // end of init()

// render CubemapGL
void CubemapGL::render(
	const FrameContext* frameVk,
	const FrameContextDX12* frameDX12,
	const glm::mat4& view,
	const glm::mat4& projection,
	const glm::vec3& sunDir,
	const float time
)
{
	if (!shader_ || !cubemapTextureNight_ || !cubemapTextureDay_ || vao_ == 0)
		return;

	// bind ubo
	ubo_.bind();

	// bind textures
	glBindTextureUnit(TO_API_FORM(CubemapBinding::NightSkyboxTex), cubemapTextureNight_->ID());
	glBindTextureUnit(TO_API_FORM(CubemapBinding::DaySkyboxTex), cubemapTextureDay_->ID());

	// remove translation from camera view
	glm::mat4 viewStrippedTranslation = glm::mat4(glm::mat3(view));

	if (time > 0.0f)
	{
		float speed = 0.005f;

		glm::mat4 skyRot = glm::rotate(glm::mat4(1.0f),
			time * speed,
			glm::vec3(0.0f, 1.0f, 0.0f));
		viewStrippedTranslation = viewStrippedTranslation * glm::mat4(glm::mat3(skyRot));
	}

	GLint prevFunc;
	glGetIntegerv(GL_DEPTH_FUNC, &prevFunc);
	GLboolean prevMask;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &prevMask);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	shader_->use();
	CubemapUBO cubemapUBO{};
	cubemapUBO.u_dayNightMix = glm::clamp((sunDir.y + 0.15f) / 0.30f, 0.0f, 1.0f);
	cubemapUBO.u_view = viewStrippedTranslation;
	cubemapUBO.u_proj = projection;
	ubo_.update(&cubemapUBO, sizeof(CubemapUBO));

	glBindVertexArray(vao_);

	glDrawArrays(GL_TRIANGLES, 0, 36);

	glDepthMask(prevMask);
	glDepthFunc(prevFunc);
} // end of render()


//--- PRIVATE ---//
void CubemapGL::destroyGL()
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