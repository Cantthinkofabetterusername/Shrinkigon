#ifndef MESH_CHUNKING_H
#define MESH_CHUNKING_H

#include <atomic>
#include <functional>
#include <vector>

#include "mesh_data.h"

using MeshDecimator = std::function<MeshData(const MeshData&)>;

std::vector<MeshData> splitMeshIntoChunks(const MeshData& mesh, int chunkCount);

MeshData mergeChunks(const std::vector<MeshData>& chunks, const MeshData& original);

MeshData decimateInChunks(
    const MeshData& mesh,
    int chunkCount,
    unsigned int workerCount,
    const MeshDecimator& decimate,
    const std::atomic<bool>* cancelFlag
);

#endif
