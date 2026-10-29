#ifndef OPTIMIZATION_ALGORITHMS_H
#define OPTIMIZATION_ALGORITHMS_H

#include <atomic>

#include "mesh_data.h"

struct DecimationConfig {
    int targetTriangleCount = 1000;
    float reductionPercentage = 50.0f;
    float maxDeformationError = 0.1f;
    bool preventNormalFlipping = true;
    bool preserveHardEdges = true;
    float hardEdgeAngleThreshold = 45.0f;
    bool lockBoundaryEdges = true;
    bool preserveUVMapping = true;
    const std::atomic<bool>* cancelFlag = nullptr;
};

class MeshOptimizer {
public:
    static MeshData incrementalDecimation(
        const MeshData& inputMesh,
        const DecimationConfig& config
    );
};

#endif