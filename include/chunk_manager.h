#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include <vulkan/vulkan.hpp>

#include "constants.h"

#include "save.h"

#include "chunk_mesh.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <cstdint>
#include <vector>

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

	void update(const glm::vec3& cameraPos);

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
	void buildWaterDrawList(
		const glm::mat4& view, 
		const glm::mat4& proj, 
		ChunkDrawList& out
	);

	void buildTLASInstances(std::vector<vk::AccelerationStructureInstanceKHR>& out);

	BlockID getBlock(int wx, int wy, int wz) const;
	void setBlock(int wx, int wy, int wz, BlockID id);
	void setLastBlockUsed(BlockID block);
	int getViewRadius() const;
	void setViewRadius(int r);

	const glm::vec3& getLastCameraPos() const;

	float getAmbientStrength() const;
	void setAmbientStrength(float strength);

	void placeOrRemoveBlock(bool shouldPlace, const glm::vec3& origin, const glm::vec3& dir);

	void saveWorld();

	uint32_t getFrameChunksRendered() const;
	uint32_t getFrameBlocksRendered() const;

	bool statusFrustumCulling() const;
	void enableFrustumCulling(bool enable);

	bool statusDistanceCulling() const;
	void enableDistanceCulling(bool enable);

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

	int viewRadius_;
	std::unordered_map<ChunkCoord, std::unique_ptr<ChunkEntry>, ChunkCoordHash> chunks_;
	std::queue<ChunkCoord> pendingChunks_;
	std::unordered_set<ChunkCoord, ChunkCoordHash> queuedChunks_;

	VulkanMain* vk_ = nullptr;

	// raycast data
	BlockID lastBlockUsed_;
	static constexpr float maxDistanceRay_ = 5.0f;
private:
	BlockHit raycastBlocks(const glm::vec3& origin, const glm::vec3& dir) const;
};

#endif
