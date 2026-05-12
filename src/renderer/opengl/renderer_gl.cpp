#include "renderer_gl.h"

#include "chunk_pass_gl.h"

#include "chunk_manager.h"
#include "camera.h"
#include "light_gl.h"
#include "cubemap_gl.h"
#include "crosshair.h"

#include "gbuffer_pass.h"
#include "shadow_map_pass_gl.h"
#include "debug_pass.h"
#include "ssao_pass.h"
#include "fxaa_pass.h"
#include "present_pass.h"
#include "water_pass.h"
#include "fog_pass.h"
#include "post_composite_pass_gl.h"

#include "render_inputs.h"

#include <glad/glad.h>

#include <stdexcept>
#include <memory>

//--- PUBLIC ---//
RendererGL::RendererGL() = default;

RendererGL::~RendererGL()
{
    destroyGL();
} // end of destructor

void RendererGL::init()
{
    destroyGL();

    if (!renderSettings_) renderSettings_ = std::make_unique<RenderSettings>();

    if (!gbuffer_)              gbuffer_ = std::make_unique<GBufferPass>();
    if (!shadowMapPass_)        shadowMapPass_ = std::make_unique<ShadowMapPassGL>();
    if (!debugPass_)            debugPass_ = std::make_unique<DebugPass>();
    if (!ssaoPass_)             ssaoPass_ = std::make_unique<SSAOPass>();
    if (!fxaaPass_)             fxaaPass_ = std::make_unique<FXAAPass>();
    if (!fogPass_)              fogPass_ = std::make_unique<FogPass>();
    if (!compositePassPost_)    compositePassPost_ = std::make_unique<PostCompositePassGL>();
    if (!presentPass_)          presentPass_ = std::make_unique<PresentPass>();
    if (!waterPass_)            waterPass_ = std::make_unique<WaterPass>();

    if (!chunkPass_)            chunkPass_ = std::make_unique<ChunkPassGL>();

    chunkPass_->init();

	gbuffer_->init();
    shadowMapPass_->init();
	debugPass_->init();
    ssaoPass_->init();

    glCreateFramebuffers(1, &forwardFBO_);
    glCreateTextures(GL_TEXTURE_2D, 1, &forwardColorTex_);
    glCreateTextures(GL_TEXTURE_2D, 1, &forwardDepthTex_);

    waterPass_->init();
    fxaaPass_->init();
    fogPass_->init();
    compositePassPost_->init();
    presentPass_->init();
} // end of init()

void RendererGL::resize(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (w == width_ && h == height_) return;

    width_ = w;
    height_ = h;

    gbuffer_->resize(width_, height_);
    ssaoPass_->resize(width_, height_);
    fxaaPass_->resize(width_, height_);
    waterPass_->resize(width_, height_);
    fogPass_->resize(width_, height_);
    compositePassPost_->resize(width_, height_);
    presentPass_->resize(width_, height_);

    resizeForwardTargets();
} // end of resize()

void RendererGL::renderFrame(
    const RenderInputs& in,
    const FrameContext* frame,
    UI* ui
)
{
    if (!in.world || !in.camera || !in.light || !in.skybox || !in.crosshair) return;

    glEnable(GL_FRAMEBUFFER_SRGB);

    in.world->updateDynamic(in.camera->getCameraPosition());

    // update light/sun
    in.light->updateLight(
        in.time, 
        in.camera->getCameraPosition(),
        renderSettings_->sunPaused
    );

    // update opaque + water shader
    chunkPass_->updateShader(in, *renderSettings_, width_, height_);
    waterPass_->updateShader(in, *renderSettings_, width_, height_);

    const glm::mat4 view = in.camera->getViewMatrix();
    const float aspect = (height_ > 0)
        ? (static_cast<float>(width_) / static_cast<float>(height_))
        : 1.0f;
    const glm::mat4 proj = in.camera->getProjectionMatrix(aspect);


    // ----------------- PASSES ----------------- //
    // gbuffer pass
    gbuffer_->render(
        *chunkPass_, 
        in, 
        view, 
        proj
    );

    // shadow map pass
    shadowMapPass_->renderOffscreen(
        *chunkPass_, 
        in
    );

    // ssao pass
    if (renderSettings_->useSSAO)
    {
        ssaoPass_->render(
            gbuffer_->getNormalTexture(), 
            gbuffer_->getDepthTexture(), 
            proj
        );
    }

    // debug pass
    if (renderSettings_->debugMode != DebugMode::None)
    {
        debugPass_->render(
            gbuffer_->getNormalTexture(),
            gbuffer_->getDepthTexture(),
            shadowMapPass_->getDepthTexture(),
            in.camera->getNearPlane(),
            in.camera->getFarPlane(),
            static_cast<int>(renderSettings_->debugMode)
        );
        return;
    }

    // water pass
    waterPass_->renderOffscreen(
        *renderSettings_,
        shadowMapPass_.get(),
        *chunkPass_, 
        in
    );
    // --------------- END PASSES --------------- //


    // ----------------- FORWARD RENDER ----------------- //
    glBindFramebuffer(GL_FRAMEBUFFER, forwardFBO_);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // render objects (non-UI)
    chunkPass_->renderOpaque(
        ssaoPass_->aoBlurTexture(), 
        shadowMapPass_->getDepthTexture(), 
        in, 
        view, 
        proj, 
        shadowMapPass_->getLightSpaceMatrix(),
        width_, 
        height_
    );

    waterPass_->renderWater(
        *renderSettings_,
        shadowMapPass_.get(),
        in, 
        view, 
        proj, 
        width_, 
        height_
    );

    in.skybox->render(
        nullptr, 
        view, 
        proj, 
        in.light->getDirection(),
        in.time
    );

    in.light->render(
        nullptr,
        view,
        proj
    );
    // --------------- END FORWARD RENDER --------------- //


    // ----------------- POST-PROCESSING ----------------- //
    uint32_t finalSceneDepth = forwardDepthTex_;
    uint32_t postBaseColor = forwardColorTex_;
    uint32_t postColor{};

    // FOG
    if (renderSettings_->useFog)
    {
        Fog_Constants::FogPassUBO fogUBO{};
        fogUBO.u_useVolFog = renderSettings_->fogSettings.volumetricFog;
        fogUBO.u_invViewProj = glm::inverse(proj * view);
        fogUBO.u_lightSpaceMatrix = shadowMapPass_->getLightSpaceMatrix();
        fogUBO.u_cameraPos = glm::vec4(in.camera->getCameraPosition(), 1.0f);
        fogUBO.u_nearFar = { in.camera->getNearPlane(), in.camera->getFarPlane() };
        fogUBO.u_fogStartEnd = { renderSettings_->fogSettings.start, renderSettings_->fogSettings.end };
        fogUBO.u_fogColor = in.light->getLightColor();
        fogUBO.u_lightDir = in.light->getDirection();
        fogUBO.u_maxDistance = renderSettings_->fogSettings.maxDistance;
        fogUBO.u_ambStr = in.world->getAmbientStrength();
        fogUBO.u_stepSize = renderSettings_->fogSettings.stepSize;
        fogUBO.u_scatteringDensity = renderSettings_->fogSettings.scatteringDensity;
        fogUBO.u_absorptionDensity = renderSettings_->fogSettings.absorptionDensity;

        fogPass_->render(
            forwardDepthTex_,
            shadowMapPass_->getDepthTexture(),
            fogUBO
        );
        
        compositePassPost_->setInput(
            postBaseColor,
            fogPass_->getOutputTex()
        );
        compositePassPost_->render();

        postBaseColor = compositePassPost_->getOutColorImage();
    }

    // FXAA
    if (renderSettings_->useFXAA)
    {
        fxaaPass_->render(postBaseColor);
        postBaseColor = fxaaPass_->getOutputTex();
    }

    postColor = postBaseColor;
    // --------------- END POST-PROCESSING --------------- //


    // ----------------- PRESENT PASS ----------------- //
    if (presentPass_)
    {
        presentPass_->render(postColor);
    }
    // --------------- END PRESENT PASS --------------- //


    // ----------------- UI ELEMENTS ----------------- //
    if (in.crosshair)
    {
        in.crosshair->render(nullptr);
    }
    // --------------- END UI ELEMENTS --------------- //
} // end of renderFrame()

RenderSettings& RendererGL::settings()
{
    return *renderSettings_;
} // end of settings()


//--- PRIVATE ---//
void RendererGL::destroyGL()
{
    if (forwardFBO_)
    {
        glDeleteFramebuffers(1, &forwardFBO_);
        forwardFBO_ = 0;
    }

    if (forwardColorTex_)
    {
        glDeleteTextures(1, &forwardColorTex_);
        forwardColorTex_ = 0;
    }

    if (forwardDepthTex_)
    {
        glDeleteTextures(1, &forwardDepthTex_);
        forwardDepthTex_ = 0;
    }
} // end of destroyGL()

void RendererGL::resizeForwardTargets()
{
    // recreate textures
    if (forwardColorTex_)
    {
        glDeleteTextures(1, &forwardColorTex_);
        forwardColorTex_ = 0;
    }
    if (forwardDepthTex_)
    {
        glDeleteTextures(1, &forwardDepthTex_);
        forwardDepthTex_ = 0;
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &forwardColorTex_);

    glTextureStorage2D(forwardColorTex_, 1, GL_RGBA8, width_, height_);
    glTextureParameteri(forwardColorTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(forwardColorTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(forwardColorTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(forwardColorTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &forwardDepthTex_);

    glTextureStorage2D(forwardDepthTex_, 1, GL_DEPTH_COMPONENT24, width_, height_);
    glTextureParameteri(forwardDepthTex_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(forwardDepthTex_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(forwardDepthTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(forwardDepthTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glNamedFramebufferTexture(forwardFBO_, GL_COLOR_ATTACHMENT0, forwardColorTex_, 0);
    glNamedFramebufferTexture(forwardFBO_, GL_DEPTH_ATTACHMENT, forwardDepthTex_, 0);

    GLenum drawBuf = GL_COLOR_ATTACHMENT0;
    glNamedFramebufferDrawBuffers(forwardFBO_, 1, &drawBuf);

    // check FBO
    if (glCheckNamedFramebufferStatus(forwardFBO_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("FBO 'forwardFBO_' is incomplete!");
    }
} // end of resizeForwardTargets()
