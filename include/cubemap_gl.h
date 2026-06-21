#ifndef CUBEMAP_GL_H
#define CUBEMAP_GL_H

#include "bindings.h"

#include "i_cubemap.h"

#include "ubo_gl.h"

#include <glm/glm.hpp>

#include <string_view>
#include <array>
#include <cstdint>
#include <memory>

class TextureGL;
class Shader;

class CubemapGL final : public ICubemap
{
public:
    CubemapGL(const std::array<std::string_view, 6>& textures = Cubemap_Constants::DEFAULT_FACES);
    ~CubemapGL() override;

    void init() override;

    void render(
        const FrameContext* frameVk,
        const FrameContextDX12* frameDX12,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& sunDir,
        const float time
    ) override;

private:
    void destroyGL();
private:
    std::unique_ptr<Shader> shader_;

    std::unique_ptr<TextureGL> cubemapTextureNight_;
    std::unique_ptr<TextureGL> cubemapTextureDay_;

    uint32_t vao_{};
    uint32_t vbo_{};
    UBOGL ubo_{ TO_API_FORM(CubemapBinding::UBO) };

    std::array<std::string_view, 6> faces_;
};

#endif
