#ifndef FOG_PASS_GL_H
#define FOG_PASS_GL_H

#include "constants.h"
#include "bindings.h"

#include "ubo_gl.h"

#include <cstdint>
#include <memory>
#include <array>

class ComputeShader;
struct RenderSettings;

class FogPassGL
{
public:
    FogPassGL(const RenderSettings& rs);
    ~FogPassGL();

    void init();
    void resize(int w, int h);

    void clearColorImage(const std::array<float, 4>& color);

    void render(Fog_Constants::FogPassUBO& ubo);

    void setInput(uint32_t inputDepth)
    {
        inputDepthImage_ = inputDepth;
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

    uint32_t numWorkGroups_{ Fog_Constants::WORK_GROUPS };
    uint32_t workGroupX_{};
    uint32_t workGroupY_{};

    uint32_t inputDepthImage_{};

    uint32_t outputImage_{};
    GLenum outImageFormat_{ GL_RGBA16F };

    std::unique_ptr<ComputeShader> computeShader_;

    UBOGL uboBuffer_{ TO_API_FORM(FogPassBinding::UBO) };
};

#endif
