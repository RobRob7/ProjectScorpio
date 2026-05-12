#include "chunk_pass_gl.h"

#include "bindings.h"
#include "ubo_gl.h"

#include "chunk_draw_list.h"

#include "i_chunk_mesh_gpu.h"

#include "shader.h"
#include "texture.h"
#include "camera.h"
#include "light_gl.h"

#include "render_inputs.h"
#include "render_settings.h"
#include "chunk_manager.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>

//--- PUBLIC ---//
ChunkPassGL::~ChunkPassGL() = default;

void ChunkPassGL::init()
{
	opaqueShader_ = std::make_unique<Shader>("chunk/chunk.vert", "chunk/chunk.frag");

	atlas_ = std::make_unique<Texture>("blocks_padded.png", true);

    uboOpaque_.init<sizeof(ChunkOpaqueUBO)>();
} // end of init()

void ChunkPassGL::updateShader(
    const RenderInputs& in,
    const RenderSettings& rs,
    const int w, const int h
)
{
    // update uniforms of opaque shader
    opaqueShader_->use();
    chunkOpaqueUBO_.u_useShadowMap = rs.useShadowMap ? 1 : 0;
    chunkOpaqueUBO_.u_ambientStrength = in.world->getAmbientStrength();
    chunkOpaqueUBO_.u_viewPos = in.camera->getCameraPosition();
    chunkOpaqueUBO_.u_lightDir = in.light->getDirection();
    chunkOpaqueUBO_.u_lightColor = in.light->getLightColor();
    // ssao
    chunkOpaqueUBO_.u_screenSize = glm::vec2{ w, h };
    chunkOpaqueUBO_.u_useSSAO = rs.useSSAO ? 1 : 0;
    uboOpaque_.update(&chunkOpaqueUBO_, sizeof(chunkOpaqueUBO_));
} // end of updateShader()

void ChunkPassGL::renderOpaque(
    uint32_t ssaoTex,
    const RenderInputs& in,
    const glm::mat4& view,
    const glm::mat4& proj,
    int width, int height
)
{
    // bind ubo
    uboOpaque_.bind();

    // bind textures
    glBindTextureUnit(TO_API_FORM(ChunkBinding::AtlasTex), atlas_->ID());
    glBindTextureUnit(TO_API_FORM(ChunkBinding::SSAOTex), ssaoTex);

    ChunkDrawList list;
    in.world->buildOpaqueDrawList(view, proj, list);

    opaqueShader_->use();
    chunkOpaqueUBO_.u_view = view;
    chunkOpaqueUBO_.u_proj = proj;
    chunkOpaqueUBO_.u_screenSize = glm::vec2{ width, height };

    for (const auto& item : list.items)
    {
        chunkOpaqueUBO_.u_chunkOrigin = item.chunkOrigin;
        uboOpaque_.update(&chunkOpaqueUBO_, sizeof(chunkOpaqueUBO_));
        item.gpu->drawOpaque({});
    }
} // end of renderOpaque()

void ChunkPassGL::renderOpaque(
    uint32_t ssaoTex,
    uint32_t shadowTex,
	const RenderInputs& in,
	const glm::mat4& view,
	const glm::mat4& proj,
    const glm::mat4& lightSpaceMatrix,
	int width, int height
)
{
    // bind ubo
    uboOpaque_.bind();

    // bind textures
    glBindTextureUnit(TO_API_FORM(ChunkBinding::AtlasTex), atlas_->ID());
    glBindTextureUnit(TO_API_FORM(ChunkBinding::SSAOTex), ssaoTex);
    glBindTextureUnit(TO_API_FORM(ChunkBinding::ShadowTex), shadowTex);

    ChunkDrawList list;
    in.world->buildOpaqueDrawList(view, proj, list);

    opaqueShader_->use();
    chunkOpaqueUBO_.u_lightSpaceMatrix = lightSpaceMatrix;
    chunkOpaqueUBO_.u_view = view;
    chunkOpaqueUBO_.u_proj = proj;
    chunkOpaqueUBO_.u_screenSize = glm::vec2{ width, height };

    for (const auto& item : list.items)
    {
        chunkOpaqueUBO_.u_chunkOrigin = item.chunkOrigin;
        uboOpaque_.update(&chunkOpaqueUBO_, sizeof(chunkOpaqueUBO_));
        item.gpu->drawOpaque({});
    }
} // end of renderOpaque()

void ChunkPassGL::renderOpaqueOffscreen(
    UBOGL& uboGL,
    void* ubo,
    uint32_t uboSize,
    glm::vec3& chunkOrigin,
    const RenderInputs& in,
    const glm::mat4& view,
    const glm::mat4& proj
)
{
    ChunkDrawList list;
    in.world->buildOpaqueDrawList(view, proj, list);

    for (const auto& item : list.items)
    {
        chunkOrigin = item.chunkOrigin;
        uboGL.update(ubo, uboSize);
        item.gpu->drawOpaque({});
    }
} // end of renderOpaque()
