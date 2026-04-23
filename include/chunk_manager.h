#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include "constants.h"

#include "save.h"

#include "chunk_draw_list.h"
#include "chunk_mesh.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <cstdint>
#include <vector>
#include <algorithm>

struct ChunkEntry;
struct ChunkDrawList;
class VulkanMain;

using namespace World;

struct BlockHit
{
	bool hit = false;
	glm::ivec3 block{};
	glm::ivec3 normal{};
};

class ChunkManager
{
public:
	ChunkManager(int viewRadiusInChunks = 15);
	~ChunkManager();

	void init(VulkanMain* vk);
	void updateDynamic(const glm::vec3& cameraPos);

	bool buildVisibleChunkBounds(
		glm::vec3& outMin,
		glm::vec3& outMax,
		int paddingChunks = 1
	);

	void buildOpaqueDrawList(
		const glm::mat4& view, 
		const glm::mat4& proj, 
		ChunkDrawList& out
	);
	void buildOpaqueDrawList(
		const glm::mat4& view, 
		const glm::mat4& proj
	);

	void buildWaterDrawList(
		const glm::mat4& view, 
		const glm::mat4& proj, 
		ChunkDrawList& out
	);

	BlockID getBlock(int wx, int wy, int wz) const;
	void setBlock(int wx, int wy, int wz, BlockID id);
	void placeOrRemoveBlock(bool shouldPlace, const glm::vec3& origin, const glm::vec3& dir);
	void saveWorld();

	void setLastBlockUsed(BlockID block) { lastBlockUsed_ = block; }
	int getViewRadius() const { return viewRadius_; }
	void setViewRadius(int r) { viewRadius_ = std::clamp(r, MIN_RADIUS, MAX_RADIUS); }

	const glm::vec3& getLastCameraPos() const { return lastCameraPos_; }

	float getAmbientStrength() const { return ambientStrength_; }
	void setAmbientStrength(float strength) { ambientStrength_ = std::clamp(strength, MIN_AMBSTR, MAX_AMBSTR); }

	uint32_t getFrameChunksRendered() const { return frameChunksRendered_; }
	uint32_t getFrameBlocksRendered() const { return frameBlocksRendered_; }

	bool statusFrustumCulling() const { return enableFrustumCulling_; }
	void enableFrustumCulling(bool enable) { enableFrustumCulling_ = enable; }

	bool statusDistanceCulling() const { return enableDistanceCulling_; }
	void enableDistanceCulling(bool enable) { enableDistanceCulling_ = enable; }

	const ChunkDrawList& getChunkDrawList() const { return chunkDrawList_; }

private:
	BlockHit raycastBlocks(const glm::vec3& origin, const glm::vec3& dir) const;
private:
	float ambientStrength_{ MIN_AMBSTR };
	Save saveWorld_;

	// culling toggles 
	bool enableFrustumCulling_ = true;
	bool enableDistanceCulling_ = true;

	// count
	uint32_t frameChunksRendered_{ 0 };
	uint32_t frameBlocksRendered_{ 0 };

	glm::vec3 lastCameraPos_{};

	int streamCenterX_{ 0 };
	int streamCenterZ_{ 0 };
	bool streamCenterInitialized_{ false };
	int streamRecenterThreshold_{ 0 };

	int viewRadius_;
	std::unordered_map<ChunkCoord, std::unique_ptr<ChunkEntry>, ChunkCoordHash> chunks_;
	std::queue<ChunkCoord> pendingChunks_;
	std::unordered_set<ChunkCoord, ChunkCoordHash> queuedChunks_;

	VulkanMain* vk_ = nullptr;

	ChunkDrawList chunkDrawList_{};

	// raycast data
	BlockID lastBlockUsed_;
	static constexpr float maxDistanceRay_ = 5.0f;
};

#endif
