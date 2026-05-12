#ifndef FOG_PASS_H
#define FOG_PASS_H

#include "constants.h"
#include "bindings.h"

#include "ubo_gl.h"

#include <cstdint>
#include <memory>

class ComputeShader;
struct RenderSettings;

class FogPass
{
public:
    FogPass();
    ~FogPass();

    void init();
    void resize(int w, int h);

    void render(
        uint32_t sceneDepthTex,
        uint32_t shadowMapTex,
        Fog_Constants::FogPassUBO& ubo
    );

    const uint32_t& getOutputTex() const { return outputTex_; }

private:
    void destroyGL();
private:
    int width_{};
    int height_{};

    uint32_t resFactor_{ Fog_Constants::RES_FACTOR };

    uint32_t outputTex_{};

    std::unique_ptr<ComputeShader> computeShader_;

    UBOGL uboBuffer_{ TO_API_FORM(FogPassBinding::UBO) };
};

#endif
