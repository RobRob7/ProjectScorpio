#ifndef CHUNK_MESH_DATA_H
#define CHUNK_MESH_DATA_H

#include "constants.h"

#include <vector>
#include <cstdint>

struct ChunkMeshData
{
    // opaque
    std::vector<World::Vertex> opaqueVertices;
    std::vector<World::RTVertex> opaqueRTVertices;
    std::vector<uint32_t> opaqueIndices;
    int32_t opaqueIndexCount = 0;

    // water
    std::vector<World::VertexWater> waterVertices;
    std::vector<uint32_t> waterIndices;
    int32_t waterIndexCount = 0;

    uint32_t renderedBlockCount = 0;
};

#endif
