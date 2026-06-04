#include "ssao_pass.h"

#include "constants.h"
#include "bindings.h"

#include "render_settings.h"

#include "shader.h"

#include <glad/glad.h>

#include "glm/glm.hpp"

#include <vector>
#include <stdexcept>
#include <random>

//--- PUBLIC ---//
SSAOPass::SSAOPass(const RenderSettings& rs)
	: rs_(rs)
{
	factor_ = rs.resScale.SSAO;
} // end of constructor

SSAOPass::~SSAOPass()
{
	destroyGL();
} // end of destructor

void SSAOPass::init()
{
	ssaoShader_ = std::make_unique<Shader>("ssaopass/ssao.vert", "ssaopass/ssao.frag");
	blurShader_ = std::make_unique<Shader>("ssaopass/ssaoblur.vert", "ssaopass/ssaoblur.frag");

	ssaoRawSamplesUBOBuffer_.init<sizeof(SSAO_Constants::SSAORawSamplesUBO)>();
	ssaoRawUBOBuffer_.init<sizeof(SSAO_Constants::SSAORawUBO)>();
	ssaoBlurUBOBuffer_.init<sizeof(SSAO_Constants::SSAOBlurUBO)>();

	glCreateVertexArrays(1, &fsVao_);

	createKernel();
	createNoise();
} // end of init()

void SSAOPass::resize(int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (w == width_ && h == height_) return;

	destroyTargets();
	width_ = w / factor_;
	height_ = h / factor_;
	createTargets();
} // end of resize()

void SSAOPass::destroyGL()
{
	destroyTargets();

	if (noiseTexture_)
	{
		glDeleteTextures(1, &noiseTexture_);
		noiseTexture_ = 0;
	}
	if (fsVao_)
	{
		glDeleteVertexArrays(1, &fsVao_);
		fsVao_ = 0;
	}

	width_ = 0;
	height_ = 0;
} // end of destroyGL()

void SSAOPass::render(
	SSAO_Constants::SSAORawUBO& rawUBO,
	SSAO_Constants::SSAOBlurUBO& blurUBO,
	uint32_t normalTex,
	uint32_t depthTex,
	const glm::mat4& proj
)
{
	if (!ssaoShader_ || !blurShader_) return;
	if (!fboRaw_ || !fboBlur_) return;

	// raw SSAO
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboRaw_);
		glViewport(0, 0, width_, height_);
		glDisable(GL_DEPTH_TEST);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ssaoShader_->use();

		glBindTextureUnit(TO_API_FORM(SSAORawBinding::GNormalTex), normalTex);
		glBindTextureUnit(TO_API_FORM(SSAORawBinding::GDepthTex), depthTex);
		glBindTextureUnit(TO_API_FORM(SSAORawBinding::NoiseTex), noiseTexture_);

		using namespace SSAO_Constants;
		rawUBO.u_proj = proj;
		rawUBO.u_invProj = glm::inverse(proj);
		rawUBO.u_bias = BIAS;
		rawUBO.u_noiseScale = glm::vec2(
			static_cast<float>(width_) / static_cast<float>(K_NOISE_SIZE),
			static_cast<float>(height_) / static_cast<float>(K_NOISE_SIZE));
		ssaoRawUBOBuffer_.update(&rawUBO, sizeof(rawUBO));
		ssaoRawUBOBuffer_.bind();

		glBindVertexArray(fsVao_);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}


	// blur SSAO
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboBlur_);
		glViewport(0, 0, width_, height_);
		glClear(GL_COLOR_BUFFER_BIT);

		blurShader_->use();

		glBindTextureUnit(TO_API_FORM(SSAOBlurBinding::SSAORawTex), aoRaw_);

		blurUBO.u_texelSize = glm::vec2(1.0f / width_, 1.0f / height_);
		ssaoBlurUBOBuffer_.update(&blurUBO, sizeof(blurUBO));
		ssaoBlurUBOBuffer_.bind();

		glBindVertexArray(fsVao_);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	// restore GL state
	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
} // end of render()

uint32_t SSAOPass::aoRawTexture() const
{
	return aoRaw_;
} // end of aoRawTexture()

uint32_t SSAOPass::aoBlurTexture() const
{
	return aoBlur_;
} // end of aoBlurTexture()


//--- PRIVATE ---//
void SSAOPass::createTargets()
{
	glCreateFramebuffers(1, &fboRaw_);
	glCreateFramebuffers(1, &fboBlur_);

	glCreateTextures(GL_TEXTURE_2D, 1, &aoRaw_);
	glTextureStorage2D(aoRaw_, 1, GL_R8, width_, height_);
	glTextureParameteri(aoRaw_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(aoRaw_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(aoRaw_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(aoRaw_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glCreateTextures(GL_TEXTURE_2D, 1, &aoBlur_);
	glTextureStorage2D(aoBlur_, 1, GL_R8, width_, height_);
	glTextureParameteri(aoBlur_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(aoBlur_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(aoBlur_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(aoBlur_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glNamedFramebufferTexture(fboRaw_, GL_COLOR_ATTACHMENT0, aoRaw_, 0);
	glNamedFramebufferTexture(fboBlur_, GL_COLOR_ATTACHMENT0, aoBlur_, 0);

	const GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
	glNamedFramebufferDrawBuffers(fboRaw_, 1, bufs);
	glNamedFramebufferDrawBuffers(fboBlur_, 1, bufs);

	if (glCheckNamedFramebufferStatus(fboRaw_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("SSAO raw FBO incomplete!");
	}
	if (glCheckNamedFramebufferStatus(fboBlur_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("SSAO blur FBO incomplete!");
	}
} // end of createTargets()

void SSAOPass::destroyTargets()
{
	if (fboRaw_)
	{
		glDeleteFramebuffers(1, &fboRaw_);
		fboRaw_ = 0;
	}
	if (fboBlur_)
	{
		glDeleteFramebuffers(1, &fboBlur_);
		fboBlur_ = 0;
	}
	if (aoRaw_)
	{
		glDeleteTextures(1, &aoRaw_);
		aoRaw_ = 0;
	}
	if (aoBlur_)
	{
		glDeleteTextures(1, &aoBlur_);
		aoBlur_ = 0;
	}
} // end of destroyTargets()

void SSAOPass::createNoise()
{
	using namespace SSAO_Constants;

	std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

	std::vector<glm::vec4> noise;
	noise.reserve(K_NOISE_SIZE * K_NOISE_SIZE);

	for (int i = 0; i < K_NOISE_SIZE * K_NOISE_SIZE; ++i)
	{
		glm::vec3 n(dist(rng), dist(rng), 0.0f);
		noise.emplace_back(n, 1.0f);
	} // end for

	glCreateTextures(GL_TEXTURE_2D, 1, &noiseTexture_);
	glTextureStorage2D(noiseTexture_, 1, GL_RGB16F, K_NOISE_SIZE, K_NOISE_SIZE);
	glTextureSubImage2D(noiseTexture_, 0, 0, 0, K_NOISE_SIZE, K_NOISE_SIZE, GL_RGB, GL_FLOAT, noise.data());

	glTextureParameteri(noiseTexture_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(noiseTexture_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(noiseTexture_, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(noiseTexture_, GL_TEXTURE_WRAP_T, GL_REPEAT);
} // end of createNoise()

void SSAOPass::createKernel()
{
	using namespace SSAO_Constants;

	std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist01{ 0.0f,1.0f };

	for (int i = 0; i < MAX_KERNEL_SIZE; ++i)
	{
		// hemisphere around +z (tangent space)
		glm::vec4 s{
			dist01(rng) * 2.0f - 1.0f,
			dist01(rng) * 2.0f - 1.0f,
			dist01(rng),
			0.0f
		};
		s = glm::normalize(s);
		s *= dist01(rng);

		// bias samples toward the origin
		float scale = static_cast<float>(i) / static_cast<float>(KERNEL_SIZE);
		scale = 0.1f + (scale * scale) * (1.0f - 0.1f);
		s *= scale;

		samples_[i] = s;
	} // end for

	// upload kernel
	for (int i = 0; i < MAX_KERNEL_SIZE; ++i)
	{
		ssaoRawSamplesUBO_.u_samples[i] = samples_[i];
	} // end for

	ssaoRawSamplesUBOBuffer_.update(&ssaoRawSamplesUBO_, sizeof(ssaoRawSamplesUBO_));
	ssaoRawSamplesUBOBuffer_.bind();
} // end of createKernel()