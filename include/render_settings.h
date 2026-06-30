#ifndef RENDER_SETTINGS_H
#define RENDER_SETTINGS_H

#include "constants.h"

#include <glm/glm.hpp>

#include <cstdint>

struct PassResolutionScale
{
	// HYBRID
	uint32_t FOG{ 2 };
	uint32_t GOD_RAYS{ 4 };
	uint32_t FXAA{ 1 };

	// RT
	uint32_t RT_WORLD{ 1 };
	uint32_t RTAO{ 1 };
	uint32_t RT_SHADOW{ 1 };
	
	// NON-RT
	uint32_t WATER{ 2 };
	uint32_t SSAO{ 2 };
};

enum class DebugMode : int
{
	None = 0,		// '1' key
	Normals = 1,	// '2' key
	Depth = 2,		// '3' key
	ShadowMap = 3,	// '4' key
	rtDepth = 4,	// '5' key
};

struct AOSettings
{
	float radius{SSAO_Constants::RADIUS};
	int samples{SSAO_Constants::KERNEL_SIZE};
};

struct FogSettings
{
	float maxDistance{ 100.0f };
	float stepSize{ 0.150f };

	float scatteringDensity{ 0.015f };
	float absorptionDensity{ 0.003f };
};

struct GodRaySettings
{
	float maxDistance{ 100.0f };
	float stepSize{ 0.150f };
};

struct RenderSettings
{
	// debug view mode
	DebugMode debugMode{ DebugMode::None };

	// pass resolution scales
	PassResolutionScale resScale{};

	// display options
	bool enableVsync{ true };

	// graphics options
	bool useVRS{ false };

	bool useRT{ false };
	bool useRTAO{ true };
	bool useRTShadow{ true };

	bool useShadowMap{ true };
	bool useSSAO{ true };
	bool useFXAA{ false };
	bool useFog{ false };
	bool useGodRays{ false };

	FogSettings fogSettings;
	GodRaySettings godRaySettings;
	AOSettings aoSettings;

	// sun controls
	bool sunPaused{ false };
};

#endif
