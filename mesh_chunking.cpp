#include "mesh_chunking.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <utility>

namespace {

std::uint32_t floatBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    return bits;
}

struct VertexKey {
    std::array<std::uint32_t, 5> bits;

    bool operator==(const VertexKey& other) const {
        return bits == other.bits;
    }
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& key) const {
        std::uint64_t hash = 1469598103934665603ull;

        for (std::uint32_t value : key.bits) {
            hash ^= value;
            hash *= 1099511628211ull;
        }

        return static_cast<std::size_t>(hash);
    }
};

struct PositionKey {
    std::array<std::uint32_t, 3> bits;

    bool operator==(const PositionKey& other) const {
        return bits == other.bits;
    }
};

struct PositionKeyHash {
    std::size_t operator()(const PositionKey& key) const {
        std::uint64_t hash = 1469598103934665603ull;

        for (std::uint32_t value : key.bits) {
            hash ^= value;
            hash *= 1099511628211ull;
        }

        return static_cast<std::size_t>(hash);
    }
};

void bisect(
    std::vector<std::uint32_t>& ids,
    std::size_t begin,
    std::size_t end,
    int parts,
    const std::vector<std::array<float, 3>>& centroids,
    std::vector<std::pair<std::size_t, std::size_t>>& ranges
) {
    if (parts <= 1 || end - begin < 2) {
        ranges.emplace_back(begin, end);
        return;
    }

    std::array<float, 3> low = { 1e30f, 1e30f, 1e30f };
    std::array<float, 3> high = { -1e30f, -1e30f, -1e30f };

    for (std::size_t i = begin; i < end; ++i) {
        const auto& c = centroids[ids[i]];

        for (int axis = 0; axis < 3; ++axis) {
            low[axis] = std::min(low[axis], c[axis]);
            high[axis] = std::max(high[axis], c[axis]);
        }
    }

    int axis = 0;

    for (int a = 1; a < 3; ++a) {
        if (high[a] - low[a] > high[axis] - low[axis]) {
            axis = a;
        }
    }

    int leftParts = parts / 2;
    std::size_t mid = begin + (end - begin) * static_cast<std::size_t>(leftParts) / static_cast<std::size_t>(parts);

    if (mid <= begin || mid >= end) {
        ranges.emplace_back(begin, end);
        return;
    }

    std::nth_element(
        ids.begin() + static_cast<std::ptrdiff_t>(begin),
        ids.begin() + static_cast<std::ptrdiff_t>(mid),
        ids.begin() + static_cast<std::ptrdiff_t>(end),
        [&](std::uint32_t a, std::uint32_t b) {
            return centroids[a][axis] < centroids[b][axis];
        }
    );

    bisect(ids, begin, mid, leftParts, centroids, ranges);
    bisect(ids, mid, end, parts - leftParts, centroids, ranges);
}

}

std::vector<MeshData> splitMeshIntoChunks(const MeshData& mesh, int chunkCount) {
    std::vector<MeshData> chunks;

    const std::size_t faceCount = mesh.indices.size() / 3;

    if (chunkCount <= 1 || faceCount < 2) {
        chunks.push_back(mesh);
        return chunks;
    }

    std::vector<std::array<float, 3>> centroids(faceCount);

    for (std::size_t f = 0; f < faceCount; ++f) {
        std::array<float, 3> sum = { 0.0f, 0.0f, 0.0f };

        for (int k = 0; k < 3; ++k) {
            const Vertex& vertex = mesh.vertices[mesh.indices[3 * f + k]];

            sum[0] += vertex.pos.x;
            sum[1] += vertex.pos.y;
            sum[2] += vertex.pos.z;
        }

        centroids[f] = { sum[0] / 3.0f, sum[1] / 3.0f, sum[2] / 3.0f };
    }

    std::vector<std::uint32_t> ids(faceCount);

    for (std::size_t f = 0; f < faceCount; ++f) {
        ids[f] = static_cast<std::uint32_t>(f);
    }

    std::vector<std::pair<std::size_t, std::size_t>> ranges;

    bisect(ids, 0, faceCount, chunkCount, centroids, ranges);

    std::vector<std::int64_t> remap(mesh.vertices.size(), -1);

    for (const auto& range : ranges) {
        MeshData chunk;

        chunk.textureID = mesh.textureID;
        chunk.hasTexture = mesh.hasTexture;
        chunk.diffuseColor = mesh.diffuseColor;
        chunk.texturePath = mesh.texturePath;

        std::vector<std::uint32_t> touched;

        for (std::size_t i = range.first; i < range.second; ++i) {
            std::uint32_t face = ids[i];

            for (int k = 0; k < 3; ++k) {
                std::uint32_t original = mesh.indices[3 * face + k];

                if (remap[original] < 0) {
                    remap[original] = static_cast<std::int64_t>(chunk.vertices.size());
                    chunk.vertices.push_back(mesh.vertices[original]);
                    touched.push_back(original);
                }

                chunk.indices.push_back(static_cast<unsigned int>(remap[original]));
            }
        }

        for (std::uint32_t original : touched) {
            remap[original] = -1;
        }

        chunks.push_back(std::move(chunk));
    }

    return chunks;
}

MeshData mergeChunks(const std::vector<MeshData>& chunks, const MeshData& original) {
    MeshData result;

    result.textureID = original.textureID;
    result.hasTexture = original.hasTexture;
    result.diffuseColor = original.diffuseColor;
    result.texturePath = original.texturePath;

    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> vertexMap;
    std::unordered_map<PositionKey, std::uint32_t, PositionKeyHash> positionMap;
    std::vector<std::uint32_t> positionOfVertex;
    std::vector<std::array<double, 3>> normalSums;

    for (const auto& chunk : chunks) {
        std::vector<std::uint32_t> local(chunk.vertices.size());

        for (std::size_t i = 0; i < chunk.vertices.size(); ++i) {
            const Vertex& vertex = chunk.vertices[i];

            VertexKey key = {{
                floatBits(vertex.pos.x),
                floatBits(vertex.pos.y),
                floatBits(vertex.pos.z),
                floatBits(vertex.u),
                floatBits(vertex.v)
            }};

            auto it = vertexMap.find(key);

            if (it == vertexMap.end()) {
                std::uint32_t index = static_cast<std::uint32_t>(result.vertices.size());

                result.vertices.push_back(vertex);
                it = vertexMap.emplace(key, index).first;

                PositionKey positionKey = {{ key.bits[0], key.bits[1], key.bits[2] }};
                auto pit = positionMap.find(positionKey);

                if (pit == positionMap.end()) {
                    pit = positionMap.emplace(positionKey, static_cast<std::uint32_t>(normalSums.size())).first;
                    normalSums.push_back({ 0.0, 0.0, 0.0 });
                }

                positionOfVertex.push_back(pit->second);
            }

            local[i] = it->second;
        }

        for (std::size_t i = 0; i + 2 < chunk.indices.size(); i += 3) {
            std::uint32_t a = local[chunk.indices[i]];
            std::uint32_t b = local[chunk.indices[i + 1]];
            std::uint32_t c = local[chunk.indices[i + 2]];

            if (a == b || b == c || c == a) {
                continue;
            }

            result.indices.push_back(a);
            result.indices.push_back(b);
            result.indices.push_back(c);

            const Vec3& pa = result.vertices[a].pos;
            const Vec3& pb = result.vertices[b].pos;
            const Vec3& pc = result.vertices[c].pos;

            double ux = static_cast<double>(pb.x) - pa.x;
            double uy = static_cast<double>(pb.y) - pa.y;
            double uz = static_cast<double>(pb.z) - pa.z;
            double vx = static_cast<double>(pc.x) - pa.x;
            double vy = static_cast<double>(pc.y) - pa.y;
            double vz = static_cast<double>(pc.z) - pa.z;

            double nx = uy * vz - uz * vy;
            double ny = uz * vx - ux * vz;
            double nz = ux * vy - uy * vx;
            double length = std::sqrt(nx * nx + ny * ny + nz * nz);

            if (length < 1e-18) {
                continue;
            }

            nx /= length;
            ny /= length;
            nz /= length;

            for (std::uint32_t vertex : { a, b, c }) {
                auto& sum = normalSums[positionOfVertex[vertex]];

                sum[0] += nx;
                sum[1] += ny;
                sum[2] += nz;
            }
        }
    }

    for (std::size_t i = 0; i < result.vertices.size(); ++i) {
        const auto& sum = normalSums[positionOfVertex[i]];
        double length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);

        if (length > 1e-12) {
            result.vertices[i].norm = {
                static_cast<float>(sum[0] / length),
                static_cast<float>(sum[1] / length),
                static_cast<float>(sum[2] / length)
            };
        }
    }

    return result;
}

MeshData decimateInChunks(
    const MeshData& mesh,
    int chunkCount,
    unsigned int workerCount,
    const MeshDecimator& decimate,
    const std::atomic<bool>* cancelFlag
) {
    std::vector<MeshData> chunks = splitMeshIntoChunks(mesh, chunkCount);
    std::vector<MeshData> results(chunks.size());
    std::atomic<std::size_t> next{0};

    unsigned int workers = std::max(1u, std::min<unsigned int>(workerCount, static_cast<unsigned int>(chunks.size())));
    std::vector<std::thread> pool;

    for (unsigned int w = 0; w < workers; ++w) {
        pool.emplace_back([&]() {
            while (true) {
                if (cancelFlag && cancelFlag->load()) {
                    break;
                }

                std::size_t index = next.fetch_add(1);

                if (index >= chunks.size()) {
                    break;
                }

                results[index] = decimate(chunks[index]);
            }
        });
    }

    for (auto& worker : pool) {
        worker.join();
    }

    if (cancelFlag && cancelFlag->load()) {
        return mesh;
    }

    return mergeChunks(results, mesh);
}
