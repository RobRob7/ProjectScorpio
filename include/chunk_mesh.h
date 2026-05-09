#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H

#include "chunk_data.h"
#include "chunk_mesh_data.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <optional>

class IChunkMeshGPU;

inline constexpr std::array<uint32_t, 6> FACE_INDICES = { 0, 1, 2, 0, 2, 3 };

inline constexpr std::array<glm::vec3, 4> FACE_POS_X = { {
    {1, 0, 0},
    {1, 1, 0},
    {1, 1, 1},
    {1, 0, 1}
} };

// -X (left) face
inline constexpr std::array<glm::vec3, 4> FACE_NEG_X = { {
    {0, 0, 1},
    {0, 1, 1},
    {0, 1, 0},
    {0, 0, 0}
} };

// +Y (top) face
inline constexpr std::array<glm::vec3, 4> FACE_POS_Y = { {
    {0, 1, 0},
    {1, 1, 0},
    {1, 1, 1},
    {0, 1, 1}
} };

// -Y (bottom) face
inline constexpr std::array<glm::vec3, 4> FACE_NEG_Y = { {
    {0, 0, 1},
    {1, 0, 1},
    {1, 0, 0},
    {0, 0, 0}
} };

// +Z (front) face
inline constexpr std::array<glm::vec3, 4> FACE_POS_Z = { {
    {0, 0, 1},
    {0, 1, 1},
    {1, 1, 1},
    {1, 0, 1}
} };

// -Z (back) face
inline constexpr std::array<glm::vec3, 4> FACE_NEG_Z = { {
    {1, 0, 0},
    {1, 1, 0},
    {0, 1, 0},
    {0, 0, 0}
} };

inline constexpr int ATLAS_COLS = 32;
inline constexpr int ATLAS_ROWS = 32;

enum class FaceDir : int {
    PosX, NegX,
    PosY, NegY,
    PosZ, NegZ
};

// helper for when texture is flipped vertically
inline int tileYFromTop(int rowFromTop) 
{
    return ATLAS_ROWS - 1 - rowFromTop;
}

struct ChunkCoord
{
    int x;
    int z;

    bool operator==(const ChunkCoord& other) const noexcept
    {
        return x == other.x && z == other.z;
    }
};

struct ChunkCoordHash
{
    size_t operator()(const ChunkCoord& c) const noexcept
    {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.z) << 1);
    }
};


class ChunkMesh
{
public:
    ChunkMesh(
        int chunkX, 
        int chunkZ, 
        bool autoBuild = true
    );
    ~ChunkMesh();

    void setBlock(int x, int y, int z, BlockID id);
    BlockID getBlock(int x, int y, int z) const;
    ChunkData& getChunk();
    void rebuild();

    const ChunkMeshData& data() const { return data_; }
    uint32_t getRenderedBlockCount() const { return data_.renderedBlockCount; }
    int32_t opaqueIndexCount() const { return data_.opaqueIndexCount; }
    int32_t waterIndexCount() const { return data_.waterIndexCount; }
private:
    ChunkData chunkData_;
    ChunkMeshData data_;
private:
	void buildChunkMesh();
	bool isTransparent(int x, int y, int z);
    uint32_t computeRenderedBlockCount();

    // atlas
    void getBlockTile(BlockID id, int& tileX, int& tileY, std::optional<FaceDir> face);
};

#endif
