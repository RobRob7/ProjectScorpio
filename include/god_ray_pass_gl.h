#ifndef GOD_RAY_PASS_GL_H
#define GOD_RAY_PASS_GL_H

#include "constants.h"
#include "bindings.h"

#include "ubo_gl.h"

#include <cstdint>
#include <memory>
#include <array>

class ComputeShader;
struct RenderSettings;

class GodRayPassGL
{
public:
    GodRayPassGL(const RenderSettings& rs);
    ~GodRayPassGL();

    void init();
    void resize(int w, int h);

    void clearColorImage(const std::array<float, 4>& color);

    void render(God_Ray_Constants::GodRayPassUBO& ubo);

    void setInput(
        uint32_t inputDepth,
        uint32_t inputShadowMap
    )
    {
        inputDepthImage_ = inputDepth;
        inputShadowMapImage_ = inputShadowMap;
    } // end of setInput()

    uint32_t& getOutputImage() { return outputImage_; }

private:
    void syncSettings();
    void destroyAttachment();
    void destroyGL();
    void createAttachment();
private:
    const RenderSettings& rs_;

    uint32_t factor_{};
    int sourceWidth_{};
    int sourceHeight_{};
    int width_{};
    int height_{};

    uint32_t numWorkGroups_{ God_Ray_Constants::WORK_GROUPS };
    uint32_t workGroupX_{};
    uint32_t workGroupY_{};

    uint32_t inputDepthImage_{};
    uint32_t inputShadowMapImage_{};

    uint32_t outputImage_{};
    GLenum outImageFormat_{ GL_RGBA16F };

    std::unique_ptr<ComputeShader> computeShader_;

    UBOGL uboBuffer_{ TO_API_FORM(GodRayPassBinding::UBO) };
};

#endif
