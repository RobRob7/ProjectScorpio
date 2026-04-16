#include "chunk_manager.h"

#include "vulkan_main.h"

#include "chunk_draw_list.h"
#include "chunk_mesh.h"
#include "chunk_entry.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <utility>
#include <cfloat>

//--- HELPER ---//
struct Plane
{
	glm::vec3 n; // normal
	float d;     // plane: dot(n, x) + d >= 0 is inside
};

struct Frustum
{
	Plane p[6]; // L, R, B, T, N, F
};

static Plane NormalizePlane(const Plane& pl)
{
	float len = glm::length(pl.n);
	if (len <= 1e-8f) return pl;
	return { pl.n / len, pl.d / len };
} // end of NormalizePlane()

static Frustum ExtractFrustumPlanes(const glm::mat4& VP)
{
	// GLM is column-major; to get row r: (VP[0][r], VP[1][r], VP[2][r], VP[3][r])
	auto row = [&](int r) {
		return glm::vec4(VP[0][r], VP[1][r], VP[2][r], VP[3][r]);
		};

	glm::vec4 r0 = row(0);
	glm::vec4 r1 = row(1);
	glm::vec4 r2 = row(2);
	glm::vec4 r3 = row(3);

	auto makePlane = [&](const glm::vec4& v) {
		return NormalizePlane(Plane{ glm::vec3(v), v.w });
		};

	Frustum f;
	f.p[0] = makePlane(r3 + r0); // Left
	f.p[1] = makePlane(r3 - r0); // Right
	f.p[2] = makePlane(r3 + r1); // Bottom
	f.p[3] = makePlane(r3 - r1); // Top
	f.p[4] = makePlane(r3 + r2); // Near
	f.p[5] = makePlane(r3 - r2); // Far
	return f;
} // end of ExtractFrustumPlanes()

struct AABB
{
	glm::vec3 min;
	glm::vec3 max;
};

static glm::vec3 PositiveVertex(const AABB& b, const glm::vec3& n)
{
	return glm::vec3(
		(n.x >= 0.0f) ? b.max.x : b.min.x,
		(n.y >= 0.0f) ? b.max.y : b.min.y,
		(n.z >= 0.0f) ? b.max.z : b.min.z
	);
} // end of PositiveVertex()

static bool IntersectsFrustum(const AABB& box, const Frustum& f)
{
	for (int i = 0; i < 6; ++i)
	{
		const Plane& p = f.p[i];
		glm::vec3 v = PositiveVertex(box, p.n);

		// if the “most inside” corner is still outside, whole AABB is outside
		if (glm::dot(p.n, v) + p.d < 0.0f)
			return false;
	} // end for
	return true;
} // end of IntersectsFrustum()

static AABB ChunkWorldAABB(int chunkX, int chunkZ)
{
	glm::vec3 base = glm::vec3(chunkX * CHUNK_SIZE, 0.0f, chunkZ * CHUNK_SIZE);
	glm::vec3 size = glm::vec3(CHUNK_SIZE, CHUNK_SIZE_Y, CHUNK_SIZE);

	return { base, base + size };
} // end of ChunkWorldAABB()


//--- PUBLIC ---//
ChunkManager::ChunkManager(int viewRadiusInChunks)
	: viewRadius_(viewRadiusInChunks), lastBlockUsed_(BlockID::Dirt)
{
} // end of constructor

ChunkManager::~ChunkManager() = default;

void ChunkManager::init(VulkanMain* vk)
{
	vk_ = vk;
} // end of init()

void ChunkManager::update(const glm::vec3& cameraPos)
{
	lastCameraPos_ = cameraPos;

	// find current chunk camera is in
	int cameraChunkX = static_cast<int>(std::floor(cameraPos.x / CHUNK_SIZE));
	int cameraChunkZ = static_cast<int>(std::floor(cameraPos.z / CHUNK_SIZE));

	for (int dz = -viewRadius_; dz <= viewRadius_; ++dz)
	{
		for (int dx = -viewRadius_; dx <= viewRadius_; ++dx)
		{
			// check chunk coordinate (current)
			ChunkCoord coord{ cameraChunkX + dx, cameraChunkZ + dz };

			// check if we are in "new" chunk
			if (chunks_.find(coord) == chunks_.end())
			{
				if (queuedChunks_.insert(coord).second)
				{
					// add to pending chunks
					pendingChunks_.push(coord);
				}
			}
		} // end for
	} // end for

	// identify chunks out of range
	for (auto it = chunks_.begin(); it != chunks_.end();)
	{
		int dx = it->first.x - cameraChunkX;
		int dz = it->first.z - cameraChunkZ;
		if (std::abs(dx) > viewRadius_ || std::abs(dz) > viewRadius_)
		{
			// save to file if chunk is dirty
			if (it->second->cpu->getChunk().m_dirty)
			{
				saveWorld_.saveChunkToFile(it->second->cpu->getChunk(), "HelloWorld");
				it->second->cpu->getChunk().m_dirty = false;
			}

			it = chunks_.erase(it);
		}
		else
		{
			++it;
		}
	} // end for

	const int maxNewChunksPerFrame = 2;
	int built = 0;
	// load up to maxNewChunksPerFrame
	while (!pendingChunks_.empty() && built < maxNewChunksPerFrame)
	{
		ChunkCoord coord = pendingChunks_.front();
		pendingChunks_.pop();
		queuedChunks_.erase(coord);

		// chunk already loaded
		if (chunks_.find(coord) != chunks_.end())
		{
			continue;
		}

		// create chunk and upload
		std::unique_ptr<ChunkEntry> entry = std::make_unique<ChunkEntry>(coord.x, coord.z, vk_);

		auto& chunk = entry->cpu->getChunk();
		bool loaded = saveWorld_.loadChunkFromFile(chunk, coord.x, coord.z, "HelloWorld");
		if (loaded)
		{
		}

		entry->rebuildAndUpload();
		chunks_.emplace(coord, std::move(entry));
		++built;
	} // end while

} // end of update()

bool ChunkManager::buildVisibleChunkBounds(
	glm::vec3& outMin,
	glm::vec3& outMax,
	int paddingChunks
)
{
	if (chunks_.empty())
	{
		return false;
	}

	bool hasAny = false;

	glm::vec3 minWS(FLT_MAX);
	glm::vec3 maxWS(-FLT_MAX);

	const float maxDistSq = static_cast<float>(viewRadius_ * viewRadius_);
	const glm::vec3 camXZ = { lastCameraPos_.x, 0.0f, lastCameraPos_.z };

	for (const auto& [coord, entry] : chunks_)
	{
		const int cx = coord.x;
		const int cz = coord.z;

		// distance culling
		if (enableDistanceCulling_)
		{
			glm::vec3 chunkCenter{
				static_cast<float>(cx * CHUNK_SIZE + CHUNK_SIZE / 2),
				0.0f,
				static_cast<float>(cz * CHUNK_SIZE + CHUNK_SIZE / 2)
			};

			glm::vec3 delta = chunkCenter - camXZ;
			float distSq = delta.x * delta.x + delta.z * delta.z;

			if (distSq > maxDistSq * CHUNK_SIZE * CHUNK_SIZE)
			{
				continue;
			}
		}

		// AABB
		glm::vec3 chunkMin{
			static_cast<float>(cx * CHUNK_SIZE),
			0.0f,
			static_cast<float>(cz * CHUNK_SIZE)
		};

		glm::vec3 chunkMax{
			static_cast<float>(cx * CHUNK_SIZE + CHUNK_SIZE),
			static_cast<float>(CHUNK_SIZE_Y),
			static_cast<float>(cz * CHUNK_SIZE + CHUNK_SIZE)
		};

		minWS = glm::min(minWS, chunkMin);
		maxWS = glm::max(maxWS, chunkMax);

		hasAny = true;
	} // end for

	if (!hasAny)
	{
		return false;
	}

	// padding (for shadow stability)
	float pad = static_cast<float>(paddingChunks * CHUNK_SIZE);

	minWS.x -= pad;
	minWS.z -= pad;
	maxWS.x += pad;
	maxWS.z += pad;

	minWS.y -= 2.0f;
	maxWS.y += 2.0f;

	outMin = minWS;
	outMax = maxWS;

	return true;
} // end of buildVisibleChunkBounds()

void ChunkManager::buildOpaqueDrawList(
	const glm::mat4& view, 
	const glm::mat4& proj, 
	ChunkDrawList& out
)
{
	out.clear();

	int camChunkX = static_cast<int>(std::floor(lastCameraPos_.x / CHUNK_SIZE));
	int camChunkZ = static_cast<int>(std::floor(lastCameraPos_.z / CHUNK_SIZE));
	int maxDist2 = viewRadius_ * viewRadius_;

	frameBlocksRendered_ = 0;
	frameChunksRendered_ = 0;

	// get frustum planes
	Frustum fr = ExtractFrustumPlanes(proj * view);
	for (auto& [coord, entry] : chunks_)
	{
		ChunkMesh* cpu = entry->cpu.get();

		// skip empty meshes
		if (cpu->opaqueIndexCount() <= 0) continue;

		// chunkX, chunkZ
		int chunkX = cpu->getChunk().m_chunkX;
		int chunkZ = cpu->getChunk().m_chunkZ;

		// distance culling
		int dx = chunkX - camChunkX;
		int dz = chunkZ - camChunkZ;
		int dist2 = dx * dx + dz * dz;
		if (enableDistanceCulling_ && dist2 > maxDist2)
		{
			continue;
		}

		// set AABB
		AABB box = ChunkWorldAABB(chunkX, chunkZ);
		if (enableFrustumCulling_ && !IntersectsFrustum(box, fr))
		{
			continue;
		}

		// chunk/block count
		frameChunksRendered_++;
		frameBlocksRendered_ += cpu->getRenderedBlockCount();

		ChunkDrawItem item;
		item.chunkOrigin = glm::vec3(chunkX * CHUNK_SIZE, 0.0f, chunkZ * CHUNK_SIZE);
		item.gpu = entry->gpu.get();
		item.opaqueIndexCount = static_cast<uint32_t>(cpu->opaqueIndexCount());
		item.waterIndexCount = static_cast<uint32_t>(std::max(0, cpu->waterIndexCount()));
		item.renderedBlockCount = cpu->getRenderedBlockCount();

		out.items.push_back(item);
	} // end for
} // end of buildOpaqueDrawList()

void ChunkManager::buildWaterDrawList(
	const glm::mat4& view, 
	const glm::mat4& proj, 
	ChunkDrawList& out
)
{
	out.clear();

	int camChunkX = static_cast<int>(std::floor(lastCameraPos_.x / CHUNK_SIZE));
	int camChunkZ = static_cast<int>(std::floor(lastCameraPos_.z / CHUNK_SIZE));
	int maxDist2 = viewRadius_ * viewRadius_;

	// get frustum planes
	Frustum fr = ExtractFrustumPlanes(proj * view);
	for (auto& [coord, entry] : chunks_)
	{
		ChunkMesh* cpu = entry->cpu.get();

		// skip empty meshes
		if (cpu->waterIndexCount() <= 0) continue;

		// chunkX, chunkZ
		int chunkX = cpu->getChunk().m_chunkX;
		int chunkZ = cpu->getChunk().m_chunkZ;

		// distance culling
		int dx = chunkX - camChunkX;
		int dz = chunkZ - camChunkZ;
		int dist2 = dx * dx + dz * dz;
		if (enableDistanceCulling_ && dist2 > maxDist2)
		{
			continue;
		}

		// set AABB
		AABB box = ChunkWorldAABB(chunkX, chunkZ);
		if (enableFrustumCulling_ && !IntersectsFrustum(box, fr))
		{
			continue;
		}

		ChunkDrawItem item;
		item.chunkOrigin = glm::vec3(chunkX * CHUNK_SIZE, 0.0f, chunkZ * CHUNK_SIZE);
		item.gpu = entry->gpu.get();
		item.waterIndexCount = static_cast<uint32_t>(std::max(0, cpu->waterIndexCount()));

		out.items.push_back(item);
	} // end for
} // end of buildWaterDrawList()

void ChunkManager::buildTLASInstances(std::vector<vk::AccelerationStructureInstanceKHR>& out)
{
	out.clear();

	for (auto& [coord, entry] : chunks_)
	{
		if (!entry || !entry->gpu)
		{
			continue;
		}

		auto* gpuVk = dynamic_cast<ChunkMeshGPUVk*>(entry->gpu.get());

		if (!gpuVk->hasOpaqueBLAS())
		{
			continue;
		}

		const float tx = static_cast<float>(coord.x * CHUNK_SIZE);
		const float ty = 0.0f;
		const float tz = static_cast<float>(coord.z * CHUNK_SIZE);

		vk::AccelerationStructureInstanceKHR inst{};

		inst.transform.matrix[0][0] = 1.0f;
		inst.transform.matrix[0][1] = 0.0f;
		inst.transform.matrix[0][2] = 0.0f;
		inst.transform.matrix[0][3] = tx;

		inst.transform.matrix[1][0] = 0.0f;
		inst.transform.matrix[1][1] = 1.0f;
		inst.transform.matrix[1][2] = 0.0f;
		inst.transform.matrix[1][3] = ty;

		inst.transform.matrix[2][0] = 0.0f;
		inst.transform.matrix[2][1] = 0.0f;
		inst.transform.matrix[2][2] = 1.0f;
		inst.transform.matrix[2][3] = tz;

		inst.instanceCustomIndex = 0;
		inst.mask = 0xFF;
		inst.instanceShaderBindingTableRecordOffset = 0;
		inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		inst.accelerationStructureReference = gpuVk->getOpaqueBLASAddress();

		out.push_back(inst);
	} // end for
} // end of buildTLASInstances()

BlockID ChunkManager::getBlock(int wx, int wy, int wz) const
{
	int chunkX = static_cast<int>(std::floor(wx / static_cast<float>(CHUNK_SIZE)));
	int chunkZ = static_cast<int>(std::floor(wz / static_cast<float>(CHUNK_SIZE)));

	ChunkCoord coord{ chunkX, chunkZ };
	auto it = chunks_.find(coord);
	if (it == chunks_.end())
	{
		return BlockID::Air;
	}

	int localX = wx - chunkX * CHUNK_SIZE;
	int localY = wy;
	int localZ = wz - chunkZ * CHUNK_SIZE;

	if (localX < 0 || localX >= CHUNK_SIZE ||
		localY < 0 || localY >= CHUNK_SIZE_Y ||
		localZ < 0 || localZ >= CHUNK_SIZE)
	{
		return BlockID::Air;
	}

	return it->second->cpu->getBlock(localX, localY, localZ);
} // end of getBlock()

void ChunkManager::setBlock(int wx, int wy, int wz, BlockID id)
{
	int chunkX = static_cast<int>(std::floor(wx / static_cast<float>(CHUNK_SIZE)));
	int chunkZ = static_cast<int>(std::floor(wz / static_cast<float>(CHUNK_SIZE)));

	ChunkCoord coord{ chunkX, chunkZ };
	auto it = chunks_.find(coord);
	if (it == chunks_.end())
	{
		return;
	}

	int localX = wx - chunkX * CHUNK_SIZE;
	int localY = wy;
	int localZ = wz - chunkZ * CHUNK_SIZE;

	if (localY < 0 || localY >= CHUNK_SIZE_Y)
	{
		return;
	}

	it->second->cpu->setBlock(localX, localY, localZ, id);

	vk_->waitIdle();
	it->second->rebuildAndUpload();

	// mark chunk as modified
	it->second->cpu->getChunk().m_dirty = true;
} // end of setBlock()

void ChunkManager::setLastBlockUsed(BlockID block)
{
	lastBlockUsed_ = block;
} // end of setLastBlockUsed()

int ChunkManager::getViewRadius() const
{
	return viewRadius_;
} // end of getViewRadius()

void ChunkManager::setViewRadius(int r)
{
	viewRadius_ = std::clamp(r, MIN_RADIUS, MAX_RADIUS);
} // end of setViewRadius()

const glm::vec3& ChunkManager::getLastCameraPos() const
{
	return lastCameraPos_;
} // end of getLastCameraPos()

float ChunkManager::getAmbientStrength() const
{
	return ambientStrength_;
} // end of getAmbientStrength()

void ChunkManager::setAmbientStrength(float strength)
{
	ambientStrength_ = std::clamp(strength, MIN_AMBSTR, MAX_AMBSTR);
} // end of setAmbientStrength()

void ChunkManager::placeOrRemoveBlock(bool shouldPlace, const glm::vec3& origin, const glm::vec3& dir)
{
	BlockHit hit = raycastBlocks(origin, dir);
	// place op
	if (shouldPlace)
	{
		if (hit.hit)
		{
			// DISALLOW block place in water
			if (getBlock(hit.block.x, hit.block.y, hit.block.z) == BlockID::Water) return;


			glm::ivec3 placePos = hit.block + hit.normal;
			setBlock(placePos.x, placePos.y, placePos.z, lastBlockUsed_);
			// add block to counter
			++frameBlocksRendered_;
		}
	}
	// destroy op
	else
	{
		if (hit.hit)
		{
			// DISALLOW water deletion by player (direct water block or blocks next to water)
			if (getBlock(hit.block.x, hit.block.y, hit.block.z) == BlockID::Water
				|| getBlock(hit.block.x - 1, hit.block.y, hit.block.z) == BlockID::Water
				|| getBlock(hit.block.x + 1, hit.block.y, hit.block.z) == BlockID::Water
				|| getBlock(hit.block.x, hit.block.y, hit.block.z - 1) == BlockID::Water
				|| getBlock(hit.block.x, hit.block.y, hit.block.z + 1) == BlockID::Water
				//|| getBlock(hit.block.x, hit.block.y - 1, hit.block.z) == BlockID::Water
				|| getBlock(hit.block.x, hit.block.y + 1, hit.block.z) == BlockID::Water) return;

			setBlock(hit.block.x, hit.block.y, hit.block.z, BlockID::Air);
			// remove block from counter
			--frameBlocksRendered_;
		}
	}
} // end of placeOrRemoveBlock()

void ChunkManager::saveWorld()
{
	// save all loaded chunks that have been modified
	for (auto& [coord, chunkMesh] : chunks_)
	{
		ChunkData& chunk = chunkMesh->cpu->getChunk();

		if (!chunk.m_dirty)
		{
			continue;
		}
		saveWorld_.saveChunkToFile(chunk, "HelloWorld");
		chunk.m_dirty = false;

	} // end for
} // end of saveWorld()

uint32_t ChunkManager::getFrameChunksRendered() const
{
	return frameChunksRendered_;
} // end of getFrameChunksRendered()

uint32_t ChunkManager::getFrameBlocksRendered() const
{
	return frameBlocksRendered_;
} // end of getFrameBlocksRendered()

bool ChunkManager::statusFrustumCulling() const
{
	return enableFrustumCulling_;
} // end of statusFrustumCulling()

void ChunkManager::enableFrustumCulling(bool enable)
{
	enableFrustumCulling_ = enable;
} // end of enableFrustumCulling()

bool ChunkManager::statusDistanceCulling() const
{
	return enableDistanceCulling_;
} // end of statusDistanceCulling()

void ChunkManager::enableDistanceCulling(bool enable)
{
	enableDistanceCulling_ = enable;
} // end of enableDistanceCulling()


//--- PRIVATE ---//
BlockHit ChunkManager::raycastBlocks(const glm::vec3& origin, const glm::vec3& dir) const
{
	BlockHit hit;

	glm::vec3 rayDir = glm::normalize(dir);

	// invalid direction
	if (glm::dot(rayDir, rayDir) < 1e-8f) {
		return hit;
	}

	// start slightly in front of the camera to avoid hitting inside the player cell
	glm::vec3 start = origin + rayDir * 0.001f;

	int x = static_cast<int>(std::floor(start.x));
	int y = static_cast<int>(std::floor(start.y));
	int z = static_cast<int>(std::floor(start.z));

	// step on each axis
	int stepX = (rayDir.x > 0.0f) ? 1 : (rayDir.x < 0.0f ? -1 : 0);
	int stepY = (rayDir.y > 0.0f) ? 1 : (rayDir.y < 0.0f ? -1 : 0);
	int stepZ = (rayDir.z > 0.0f) ? 1 : (rayDir.z < 0.0f ? -1 : 0);

	// distance to first voxel boundary on each axis
	auto initAxis = [&](float originCoord, float dirCoord, int gridCoord, int step) {
		if (step == 0) {
			return std::make_pair(std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
		}

		float nextBoundary = (step > 0) ? (gridCoord + 1.0f) : static_cast<float>(gridCoord);
		float tMax = (nextBoundary - originCoord) / dirCoord;
		float tDelta = std::abs(1.0f / dirCoord);
		return std::make_pair(tMax, tDelta);
		};

	auto [tMaxX, tDeltaX] = initAxis(start.x, rayDir.x, x, stepX);
	auto [tMaxY, tDeltaY] = initAxis(start.y, rayDir.y, y, stepY);
	auto [tMaxZ, tDeltaZ] = initAxis(start.z, rayDir.z, z, stepZ);

	float t = 0.0f;
	glm::ivec3 normal(0);

	while (t <= maxDistanceRay_)
	{
		// check current cell
		BlockID id = getBlock(x, y, z);
		if (id != BlockID::Air)
		{
			hit.hit = true;
			hit.block = glm::ivec3(x, y, z);
			hit.normal = normal;
			return hit;
		}

		// step to next voxel
		if (tMaxX < tMaxY && tMaxX < tMaxZ)
		{
			x += stepX;
			t = tMaxX;
			tMaxX += tDeltaX;
			normal = glm::ivec3(-stepX, 0, 0);
		}
		else if (tMaxY < tMaxZ)
		{
			y += stepY;
			t = tMaxY;
			tMaxY += tDeltaY;
			normal = glm::ivec3(0, -stepY, 0);
		}
		else
		{
			z += stepZ;
			t = tMaxZ;
			tMaxZ += tDeltaZ;
			normal = glm::ivec3(0, 0, -stepZ);
		}
	}

	return hit;
} // end of raycastBlocks()