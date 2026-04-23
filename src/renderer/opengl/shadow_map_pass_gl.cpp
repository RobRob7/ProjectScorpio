#include "shadow_map_pass_gl.h"

#include "constants.h"

#include "i_light.h"
#include "chunk_manager.h"
#include "chunk_pass_gl.h"
#include "shader.h"
#include "render_inputs.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <memory>

//--- PUBLIC ---//
ShadowMapPassGL::ShadowMapPassGL() = default;

ShadowMapPassGL::~ShadowMapPassGL()
{
	destroyGL();
} // end of destructor

void ShadowMapPassGL::init()
{
	destroyGL();

	shader_ = std::make_unique<Shader>(
		"shadowmappass/shadowmappass.vert", 
		"shadowmappass/shadowmappass.frag"
	);

	uboGL_.init<sizeof(ShadowMapPassUBO)>();

	createTargets();
} // end of init()

void ShadowMapPassGL::renderOffscreen(
	ChunkPassGL& chunk,
	const RenderInputs& in
)
{
	if (!shader_ || !fbo_ || width_ <= 0 || height_ <= 0) return;

	// bind ubo
	uboGL_.bind();

	glViewport(0, 0, width_, height_);

	glEnable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

	glClear(GL_DEPTH_BUFFER_BIT);

	// configure light space transform
	glm::vec3 minWS, maxWS;
	if (!in.world->buildVisibleChunkBounds(minWS, maxWS))
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}
	buildLightSpaceBounds(in, minWS, maxWS);

	shader_->use();
	uboData_.u_lightSpaceMatrix = lightSpaceMatrix_;
	uboGL_.update(&uboData_, sizeof(uboData_));

	chunk.renderOpaqueOffscreen(
		uboGL_, 
		&uboData_, 
		sizeof(uboData_), 
		uboData_.u_chunkOrigin,
		in, 
		lightView_, 
		lightProj_
	);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
} // end of render()


//--- PRIVATE ---//
void ShadowMapPassGL::buildLightSpaceBounds(
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

	float texelSize = extent / static_cast<float>(std::max(1, width_));

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
	lightProj_ = glm::ortho(
		minLS.x, maxLS.x,
		minLS.y, maxLS.y,
		nearPlane, farPlane
	);
	lightProj_[1][1] *= -1.0f;

	lightSpaceMatrix_ = lightProj_ * lightView_;
} // end of buildLightSpaceBounds()

void ShadowMapPassGL::createTargets()
{
	glCreateFramebuffers(1, &fbo_);

	// depth texture
	glCreateTextures(GL_TEXTURE_2D, 1, &depthTex_);
	glTextureStorage2D(depthTex_, 1, GL_DEPTH_COMPONENT24, width_, height_);
	glTextureParameteri(depthTex_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(depthTex_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(depthTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTextureParameteri(depthTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTextureParameterfv(depthTex_, GL_TEXTURE_BORDER_COLOR, borderColor);

	// attach
	glNamedFramebufferTexture(fbo_, GL_DEPTH_ATTACHMENT, depthTex_, 0);
	glNamedFramebufferDrawBuffer(fbo_, GL_NONE);
	glNamedFramebufferReadBuffer(fbo_, GL_NONE);

	if (glCheckNamedFramebufferStatus(fbo_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("Shadow map FBO incomplete!");
	}
} // end of createTargets()

void ShadowMapPassGL::destroyTargets()
{
	if (fbo_)
	{
		glDeleteFramebuffers(1, &fbo_);
		fbo_ = 0;
	}

	if (depthTex_)
	{
		glDeleteTextures(1, &depthTex_);
		depthTex_ = 0;
	}
} // end of destroyTargets()

void ShadowMapPassGL::destroyGL()
{
	destroyTargets();
} // end of destroyGL()