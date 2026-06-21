#include "water_pass.h"

#include "bindings.h"

#include "constants.h"

#include "chunk_draw_list.h"

#include "i_chunk_mesh_gpu.h"

#include "render_inputs.h"
#include "render_settings.h"
#include "shader.h"

#include "shadow_map_pass_gl.h"
#include "chunk_pass_gl.h"
#include "chunk_manager.h"
#include "camera.h"
#include "light_gl.h"
#include "cubemap_gl.h"
#include "texture_gl.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <memory>

using namespace Chunk_Constants;

//--- PUBLIC ---//
WaterPass::WaterPass(const RenderSettings& rs)
    : rs_(rs)
{
    factor_ = rs.resScale.WATER;
} // end of constructor

WaterPass::~WaterPass()
{
	destroyGL();
} // end of destructor

void WaterPass::init()
{
    shader_ = std::make_unique<Shader>("water/water.vert", "water/water.frag");
    ubo_.init<sizeof(ChunkWaterUBO)>();

    dudvTex_ = std::make_unique<TextureGL>("dudv.png");
    dudvTex_->setWrapRepeat();
    dudvTex_->setNoMipmapsLinear();

    normalTex_ = std::make_unique<TextureGL>("waternormal.png");
    normalTex_->setWrapRepeat();
    normalTex_->setNoMipmapsLinear();
} // end of init()

void WaterPass::resize(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (w == fullW_ && h == fullH_) return;
    
    destroyTargets();
    fullW_ = w;
    fullH_ = h;
    width_  = w / factor_;
    height_ = h / factor_;
    createTargets();
} // end of resize()

void WaterPass::updateShader(
    const RenderInputs& in,
    const RenderSettings& rs,
    const int w, const int h
)
{
    // update uniforms of water shader
    shader_->use();
    waterUBO_.u_useShadowMap = rs.useShadowMap ? 1 : 0;
    waterUBO_.u_time = in.time;
    waterUBO_.u_near = in.camera->getNearPlane();
    waterUBO_.u_far = in.camera->getFarPlane();
    waterUBO_.u_screenSize = glm::vec2{ w, h };
    waterUBO_.u_viewPos = in.camera->getCameraPosition();
    waterUBO_.u_lightDir = in.light->getDirection();
    waterUBO_.u_lightColor = in.light->getLightColor();
    waterUBO_.u_ambientStrength = in.world->getAmbientStrength();
    ubo_.update(&waterUBO_, sizeof(waterUBO_));
} // end of updateShader()

void WaterPass::destroyGL()
{
    destroyTargets();

    width_ = 0;
    height_ = 0;
} // end of destroyGL()

void WaterPass::renderOffscreen(
    const RenderSettings& rs,
    ShadowMapPassGL* shadowMap,
    ChunkPassGL& chunk,
    const RenderInputs& in
)
{
    waterPass(rs, shadowMap, chunk, in);
} // end of renderOffscreen()

void WaterPass::renderWater(
    const RenderSettings& rs,
    ShadowMapPassGL* shadowMap,
    const RenderInputs& in,
    const glm::mat4& view,
    const glm::mat4& proj,
    int width, int height
)
{
    // bind ubo
    ubo_.bind();

    // bind textures
    glBindTextureUnit(TO_API_FORM(WaterBinding::ReflColorTex), getReflColorTex());
    glBindTextureUnit(TO_API_FORM(WaterBinding::RefrColorTex), getRefrColorTex());
    glBindTextureUnit(TO_API_FORM(WaterBinding::RefrDepthTex), getRefrDepthTex());
    glBindTextureUnit(TO_API_FORM(WaterBinding::DudvTex), getDuDVTex());
    glBindTextureUnit(TO_API_FORM(WaterBinding::NormalTex), getNormalTex());

    ChunkDrawList list;
    in.world->buildWaterDrawList(view, proj, list);

    shader_->use();
    waterUBO_.u_useShadowMap = rs.useShadowMap ? 1 : 0;
    waterUBO_.u_lightSpaceMatrix = shadowMap->getLightSpaceMatrix();
    waterUBO_.u_view = view;
    waterUBO_.u_proj = proj;
    waterUBO_.u_screenSize = glm::vec2{ width, height };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);

    for (const auto& item : list.items)
    {
        glm::mat4 model = glm::translate(
            glm::mat4(1.0f),
            item.chunkOrigin);
        waterUBO_.u_model = model;
        ubo_.update(&waterUBO_, sizeof(waterUBO_));
        item.gpu->drawWater({});
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
} // end of renderWater()

uint32_t WaterPass::getReflColorTex() const
{
    return reflColorTex_;
} // end of getReflColorTex()

uint32_t WaterPass::getRefrColorTex() const
{
    return refrColorTex_;
} // end of getRefrColorTex()

uint32_t WaterPass::getRefrDepthTex() const
{
    return refrDepthTex_;
} // end of getRefrDepthTex()

uint32_t WaterPass::getDuDVTex() const
{
    return dudvTex_->ID();
} // end of getDuDVTex()

uint32_t WaterPass::getNormalTex() const
{
    return normalTex_->ID();
} // end of getNormalTex()


//--- PRIVATE ---//
void WaterPass::createTargets()
{
    glCreateFramebuffers(1, &reflFBO_);

    glCreateFramebuffers(1, &refrFBO_);

    // reflection
    glCreateTextures(GL_TEXTURE_2D, 1, &reflColorTex_);
    glTextureStorage2D(reflColorTex_, 1, GL_RGBA8, width_, height_);
    glTextureParameteri(reflColorTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(reflColorTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(reflColorTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(reflColorTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCreateRenderbuffers(1, &reflDepthRBO_);
    glNamedRenderbufferStorage(reflDepthRBO_, GL_DEPTH_COMPONENT24, width_, height_);

    glNamedFramebufferTexture(reflFBO_, GL_COLOR_ATTACHMENT0, reflColorTex_, 0);
    glNamedFramebufferRenderbuffer(reflFBO_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflDepthRBO_);

    GLenum drawBuf = GL_COLOR_ATTACHMENT0;
    glNamedFramebufferDrawBuffers(reflFBO_, 1, &drawBuf);

    // refraction
    glCreateTextures(GL_TEXTURE_2D, 1, &refrColorTex_);
    glTextureStorage2D(refrColorTex_, 1, GL_RGBA8, width_, height_);
    glTextureParameteri(refrColorTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(refrColorTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(refrColorTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(refrColorTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCreateTextures(GL_TEXTURE_2D, 1, &refrDepthTex_);
    glTextureStorage2D(refrDepthTex_, 1, GL_DEPTH_COMPONENT24, width_, height_);
    glTextureParameteri(refrDepthTex_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(refrDepthTex_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(refrDepthTex_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(refrDepthTex_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glNamedFramebufferTexture(refrFBO_, GL_COLOR_ATTACHMENT0, refrColorTex_, 0);
    glNamedFramebufferTexture(refrFBO_, GL_DEPTH_ATTACHMENT, refrDepthTex_, 0);

    drawBuf = GL_COLOR_ATTACHMENT0;
    glNamedFramebufferDrawBuffers(refrFBO_, 1, &drawBuf);

    // check FBOs
    if (glCheckNamedFramebufferStatus(reflFBO_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("FBO 'reflFBO_' is incomplete!");
    }
    if (glCheckNamedFramebufferStatus(refrFBO_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("FBO 'refrFBO_' is incomplete!");
    }
} // end of createTargets()

void WaterPass::destroyTargets()
{
    if (reflFBO_)
    {
        glDeleteFramebuffers(1, &reflFBO_);
        reflFBO_ = 0;
    }
    if (reflColorTex_)
    {
        glDeleteTextures(1, &reflColorTex_);
        reflColorTex_ = 0;
    }
    if (reflDepthRBO_)
    {
        glDeleteRenderbuffers(1, &reflDepthRBO_);
        reflDepthRBO_ = 0;
    }
    if (refrFBO_)
    {
        glDeleteFramebuffers(1, &refrFBO_);
        refrFBO_ = 0;
    }
    if (refrColorTex_)
    {
        glDeleteTextures(1, &refrColorTex_);
        refrColorTex_ = 0;
    }
    if (refrDepthTex_)
    {
        glDeleteTextures(1, &refrDepthTex_);
        refrDepthTex_ = 0;
    }
} // end of destroyTargets()

void WaterPass::waterPass(
    const RenderSettings& rs,
    ShadowMapPassGL* shadowMap,
    ChunkPassGL& chunk, 
    const RenderInputs& in
)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CLIP_DISTANCE0);
    waterReflectionPass(rs, shadowMap, chunk, in);
    waterRefractionPass(rs, shadowMap, chunk, in);

    // restore framebuffer + viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fullW_, fullH_);

    glDisable(GL_CLIP_DISTANCE0);
} // end of waterPass()

void WaterPass::waterReflectionPass(
    const RenderSettings& rs,
    ShadowMapPassGL* shadowMap,
    ChunkPassGL& chunk, 
    const RenderInputs& in
) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, reflFBO_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // build reflected view matrix
    float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
    Camera camera = *in.camera;
    glm::vec3 reflectedPos = camera.getCameraPosition();
    reflectedPos.y = 2.0f * waterHeight - reflectedPos.y;
    camera.setCameraPosition(reflectedPos);
    camera.invertPitch();
    glm::mat4 reflView = camera.getViewMatrix();

    // set clip plane (clip everything below water)
    glm::vec4 clipPlane{ 0, 1, 0, -(waterHeight)};
    auto& opaqueShader = chunk.getOpaqueShader();
    auto& chunkOpaqueUBO = chunk.getOpaqueUBO();
    auto chunkOpaqueUBOCopy = chunkOpaqueUBO;

    opaqueShader.use();
    chunkOpaqueUBO.u_useShadowMap = rs.useShadowMap ? 1 : 0;
    chunkOpaqueUBO.u_clipPlane = clipPlane;
    chunkOpaqueUBO.u_useSSAO = 0;

    const float aspect = (fullH_ > 0)
        ? (static_cast<float>(fullW_) / static_cast<float>(fullH_))
        : 1.0f;
    const glm::mat4 proj = in.camera->getProjectionMatrix(aspect);

    chunkOpaqueUBO.u_viewPos = camera.getCameraPosition();
    chunkOpaqueUBO.u_lightDir = in.light->getDirection();
    chunkOpaqueUBO.u_lightColor = in.light->getLightColor();

    // render objects (non-UI)
    chunk.renderOpaque(
        0,
        shadowMap->getDepthTexture(),
        in,
        reflView,
        proj,
        shadowMap->getLightSpaceMatrix(),
        width_,
        height_
    );
    in.skybox->render(
        nullptr,
        nullptr,
        reflView, 
        proj,
        in.light->getDirection(),
        in.time
    );
    in.light->render(
        nullptr,
        reflView,
        proj
    );

    // restore opaque UBO
    chunkOpaqueUBO = chunkOpaqueUBOCopy;
} // end of waterReflectionPass()

void WaterPass::waterRefractionPass(
    const RenderSettings& rs,
    ShadowMapPassGL* shadowMap,
    ChunkPassGL& chunk, 
    const RenderInputs& in
) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, refrFBO_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // set clip plane (clip everything above water)
    float waterHeight = static_cast<float>(World::SEA_LEVEL) + 0.9f;
    glm::vec4 clipPlane{ 0, -1, 0, (waterHeight) };
    auto& opaqueShader = chunk.getOpaqueShader();
    auto& chunkOpaqueUBO = chunk.getOpaqueUBO();
    auto chunkOpaqueUBOCopy = chunkOpaqueUBO;

    opaqueShader.use();
    chunkOpaqueUBO.u_useShadowMap = rs.useShadowMap ? 1 : 0;
    chunkOpaqueUBO.u_clipPlane = clipPlane;
    chunkOpaqueUBO.u_useSSAO = 0;

    const glm::mat4 view = in.camera->getViewMatrix();

    const float aspect = (fullH_ > 0)
        ? (static_cast<float>(fullW_) / static_cast<float>(fullH_))
        : 1.0f;
    const glm::mat4 proj = in.camera->getProjectionMatrix(aspect);

    chunkOpaqueUBO.u_viewPos = in.camera->getCameraPosition();
    chunkOpaqueUBO.u_lightDir = in.light->getDirection();
    chunkOpaqueUBO.u_lightColor = in.light->getLightColor();

    // render objects (non-UI)
    chunk.renderOpaque(
        0,
        shadowMap->getDepthTexture(),
        in,
        view,
        proj,
        shadowMap->getLightSpaceMatrix(),
        width_,
        height_
    );

    // restore opaque UBO
    chunkOpaqueUBO = chunkOpaqueUBOCopy;
} // end of waterRefractionPass()