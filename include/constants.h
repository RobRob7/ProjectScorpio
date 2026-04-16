#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <glm/glm.hpp>

#include <cstdint>
#include <array>
#include <string_view>

enum class Backend
{
	OpenGL,
	Vulkan,
	DX12
};

namespace RayTracing
{
	struct RTCameraUBO
	{
		glm::mat4 invView;
		glm::mat4 invProj;
		glm::vec4 cameraPos;
	};
};

namespace World
{
	const int WATER_TEX_FACTOR = 2;

	const int MIN_GROUND = 100.0;
	const int MAX_TERRAIN = MIN_GROUND;
	const int SEA_LEVEL = MIN_GROUND + 40;

	const int CHUNK_SIZE = 15;
	const int CHUNK_SIZE_Y = 256;

	const int MIN_RADIUS = 5;
	const int MAX_RADIUS = 100;
	const float MIN_AMBSTR = 0.03f;
	const float MAX_AMBSTR = 0.5f;

	// blocks
	enum class BlockID : uint8_t
	{
		Air,
		Dirt,
		Grass,
		Stone,
		Tree_Leaf,
		Tree_Trunk,
		Glow_Block,
		Sand,
		Water
	};

	// world opaque vertices
	// LAYOUT (32u bits)
	// 0  - 1   : UV corner index
	// 2  - 6   : tileY
	// 7  - 11  : tileX
	// 12 - 14  : normal index
	// 15 - 18  : x pos
	// 19 - 27  : y pos
	// 28 - 31  : z pos
	struct Vertex
	{
		uint32_t sample;
	};

	struct RTVertex
	{
		glm::vec3 position;
	};

	struct VertexWater
	{
		glm::vec3 pos;
	};
};

namespace Light_Constants
{
	const float MIN_COLOR = 0.0f;
	const float MAX_COLOR = 1.0f;

	const float MIN_SPEED = 0.0f;
	const float MAX_SPEED = 0.5f;


	struct LightUBO
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 color;
	};

	inline constexpr std::array<float, 108> CUBE_VERTICES = {
		// Back face
		 0.5f,  0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,

		 // Front face
		 -0.5f, -0.5f,  0.5f,
		  0.5f, -0.5f,  0.5f,
		  0.5f,  0.5f,  0.5f,
		  0.5f,  0.5f,  0.5f,
		 -0.5f,  0.5f,  0.5f,
		 -0.5f, -0.5f,  0.5f,

		 // Left face
		 -0.5f,  0.5f,  0.5f,
		 -0.5f,  0.5f, -0.5f,
		 -0.5f, -0.5f, -0.5f,
		 -0.5f, -0.5f, -0.5f,
		 -0.5f, -0.5f,  0.5f,
		 -0.5f,  0.5f,  0.5f,

		 // Right face
		  0.5f, -0.5f, -0.5f,
		  0.5f,  0.5f, -0.5f,
		  0.5f,  0.5f,  0.5f,
		  0.5f,  0.5f,  0.5f,
		  0.5f, -0.5f,  0.5f,
		  0.5f, -0.5f, -0.5f,

		  // Bottom face
		  -0.5f, -0.5f, -0.5f,
		   0.5f, -0.5f, -0.5f,
		   0.5f, -0.5f,  0.5f,
		   0.5f, -0.5f,  0.5f,
		  -0.5f, -0.5f,  0.5f,
		  -0.5f, -0.5f, -0.5f,

		  // Top face
		   0.5f,  0.5f,  0.5f,
		   0.5f,  0.5f, -0.5f,
		  -0.5f,  0.5f, -0.5f,
		  -0.5f,  0.5f, -0.5f,
		  -0.5f,  0.5f,  0.5f,
		   0.5f,  0.5f,  0.5f
	};

	struct VertexLight
	{
		glm::vec3 pos;
	};
};

namespace Cubemap_Constants
{
	struct CubemapUBO
	{
		glm::mat4 u_view;
		glm::mat4 u_proj;

		glm::vec3 _pad0;
		float u_dayNightMix = 1.0f;
	};

	const std::array<float, 108> SKYBOX_VERTICES =
	{
		// right (+x)
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,

		// left (-x)
		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		// top (+y)
		-1.0f,  1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		// bottom (-y)
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f,  1.0f,

		// front (+z)
		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		// back (-z)
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
	};

	// cubemap default
	const std::array<std::string_view, 6> DEFAULT_FACES = { {
		"texture/cubemap/space_alt/right.png",
		"texture/cubemap/space_alt/left.png",
		"texture/cubemap/space_alt/top.png",
		"texture/cubemap/space_alt/bottom.png",
		"texture/cubemap/space_alt/front.png",
		"texture/cubemap/space_alt/back.png"
	} };

	// cubemap day
	const std::array<std::string_view, 6> DAY_FACES = { {
		"texture/cubemap/clear_sky/px.png",
		"texture/cubemap/clear_sky/nx.png",
		"texture/cubemap/clear_sky/py.png",
		"texture/cubemap/clear_sky/ny.png",
		"texture/cubemap/clear_sky/pz.png",
		"texture/cubemap/clear_sky/nz.png"
	} };

	struct VertexCubemap
	{
		glm::vec3 pos;
	};
};

namespace Chunk_Constants
{
	struct ChunkOpaqueUBO
	{
		// vert
		glm::mat4 u_lightSpaceMatrix;

		glm::vec3 u_chunkOrigin;
		float _pad0;

		glm::mat4 u_view;
		glm::mat4 u_proj;

		// frag
		glm::vec4 u_clipPlane = glm::vec4(0.0f);

		glm::vec3 u_viewPos;
		float _pad1;

		glm::vec3 u_lightDir;
		float _pad2;

		glm::vec3 u_lightColor;
		float u_ambientStrength;

		glm::vec2 u_screenSize;
		int32_t u_useSSAO = 0;
		int32_t u_useShadowMap = 0;
	};

	struct ChunkPushConstants
	{
		glm::vec4 u_chunkOrigin{ 0.0f };
	};

	struct ChunkWaterUBO
	{
		// vert
		glm::mat4 u_lightSpaceMatrix;

		glm::mat4 u_model;
		glm::mat4 u_view;
		glm::mat4 u_proj;

		glm::vec4 u_tileScale_pad = glm::vec4{ 0.02f, 0.0f, 0.0f, 0.0f };

		// frag
		float u_time;
		float u_distortStrength = 8.0f;
		float u_waveSpeed = 0.04f;
		int32_t u_useShadowMap = 0;

		float u_near;
		float u_far;
		glm::vec2 u_screenSize;

		glm::vec3 u_viewPos;
		int32_t _pad0;

		glm::vec3 u_lightDir;
		int32_t _pad1;

		glm::vec3 u_lightColor;
		float u_ambientStrength;
	};

	struct ChunkWaterPushConstants
	{
		glm::mat4 u_model;
	};
};

namespace Gbuffer_Constants
{
	struct GbufferUBO
	{
		glm::mat4 u_view;
		glm::mat4 u_proj;

		glm::vec3 u_chunkOrigin;
		float _pad0;
	};
};

namespace Debug_Constants
{
	struct DebugPassUBO
	{
		int32_t u_mode;
		float u_near;
		float u_far;
		float _pad0;
	};
};

namespace Shadow_Map_Constants
{
	const int SHADOW_RESOLUTION = 4096;

	struct ShadowMapPassUBO
	{
		glm::mat4 u_lightSpaceMatrix;

		glm::vec3 u_chunkOrigin;
		float _pad0;
	};
};

namespace SSAO_Constants
{
	const int MAX_KERNEL_SIZE = 64;

	const int K_NOISE_SIZE = 4;
	const float RADIUS = 5.0f;
	const float BIAS = 0.05f;
	const int KERNEL_SIZE = 64;

	struct SSAOBlurUBO
	{
		glm::vec2 u_texelSize;
		glm::vec2 _pad0;
	};

	struct SSAORawUBO
	{
		glm::mat4 u_proj;
		glm::mat4 u_invProj;

		glm::vec2 u_noiseScale;
		float u_radius;
		float u_bias;

		int32_t u_kernelSize;
		float _pad0;
		glm::vec2 _pad1;

		glm::vec4 u_samples[MAX_KERNEL_SIZE];
	};
};

namespace FXAA_Constants
{
	const float EDGE_SHARP_QUALITY{ 8.0f };
	const float EDGE_THRESH_MAX{ 0.125f };
	const float EDGE_THRESH_MIN{ 0.0625f };

	struct FXAAPassUBO
	{
		glm::vec2 u_inverseScreenSize;
		float u_edgeSharpnessQuality;
		float u_edgeThresholdMax;

		float u_edgeThresholdMin;
		float _pad0;
		glm::vec2 _pad1;
	};
};

namespace Fog_Constants
{
	const glm::vec3 FOG_COLOR{ 1.0f, 1.0f, 1.0f };
	const float FOG_START{ 25.0f };
	const float FOG_END{ 125.0f };

	struct FogPassUBO
	{
		float u_near;
		float u_far;
		glm::vec2 _pad0;

		glm::vec3 u_fogColor;
		float _pad1;

		float u_fogStart;
		float u_fogEnd;
		float u_ambStr;
		float _pad2;
	};
};

namespace Crosshair_Constants
{
	const float SIZE{ 0.004f };

	const glm::vec2 CENTER{ 0.0f, 0.0f };

	const float VERTICES[] = {
		// h
		CENTER.x - SIZE, CENTER.y,
		CENTER.x + SIZE, CENTER.y,

		// v
		CENTER.x, CENTER.y - SIZE,
		CENTER.x, CENTER.y + SIZE
	};
};

#endif
