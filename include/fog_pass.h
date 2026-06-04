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
    FogPass(const RenderSettings& rs);
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
    const RenderSettings& rs_;

    uint32_t factor_{};
    int width_{};
    int height_{};

    uint32_t numWorkGroups_{ Fog_Constants::WORK_GROUPS };
    uint32_t workGroupX_{};
    uint32_t workGroupY_{};

    uint32_t outputTex_{};

    std::unique_ptr<ComputeShader> computeShader_;

    UBOGL uboBuffer_{ TO_API_FORM(FogPassBinding::UBO) };
};

#endif
