#include "optimization_algorithms.h"

#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <OpenMesh/Core/Geometry/VectorT.hh>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <queue>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct CustomMeshTraits : public OpenMesh::DefaultTraits {
    VertexAttributes(
        OpenMesh::Attributes::Status |
        OpenMesh::Attributes::Normal
    );

    EdgeAttributes(
        OpenMesh::Attributes::Status
    );

    FaceAttributes(
        OpenMesh::Attributes::Status |
        OpenMesh::Attributes::Normal
    );

    HalfedgeAttributes(
        OpenMesh::Attributes::PrevHalfedge |
        OpenMesh::Attributes::Status |
        OpenMesh::Attributes::TexCoord2D
    );
};

using OMesh = OpenMesh::TriMesh_ArrayKernelT<CustomMeshTraits>;
using Vec3d = OpenMesh::Vec3d;
using Vec2f = OpenMesh::Vec2f;

constexpr double kPi = 3.14159265358979323846;
constexpr float kUVSeamEpsilonSquared = 1e-10f;
constexpr double kMinNormalDot = 0.3;
constexpr double kMaxUVStretch = 3.0;

struct Quadric {
    double m[4][4]{};

    Quadric() {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m[i][j] = 0.0;
            }
        }
    }

    Quadric operator+(const Quadric& other) const {
        Quadric result;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = m[i][j] + other.m[i][j];
            }
        }

        return result;
    }

    double evaluate(const Vec3d& p) const {
        double v[4] = { p[0], p[1], p[2], 1.0 };
        double result = 0.0;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result += v[i] * m[i][j] * v[j];
            }
        }

        return result;
    }
};

struct EdgeCandidate {
    OMesh::EdgeHandle edge;
    OMesh::VertexHandle v0;
    OMesh::VertexHandle v1;
    Vec3d position;
    Vec2f uv;
    double cost;
    std::size_t version0;
    std::size_t version1;
};

struct EdgeCandidateCompare {
    bool operator()(
        const EdgeCandidate& a,
        const EdgeCandidate& b
    ) const {
        return a.cost > b.cost;
    }
};

using CandidateQueue = std::priority_queue<
    EdgeCandidate,
    std::vector<EdgeCandidate>,
    EdgeCandidateCompare
>;

Vec3d toVec3d(const OMesh::Point& point) {
    return Vec3d(
        static_cast<double>(point[0]),
        static_cast<double>(point[1]),
        static_cast<double>(point[2])
    );
}

OMesh::Point toPoint(const Vec3d& v) {
    return OMesh::Point(
        static_cast<float>(v[0]),
        static_cast<float>(v[1]),
        static_cast<float>(v[2])
    );
}

double determinant3x3(
    double a00, double a01, double a02,
    double a10, double a11, double a12,
    double a20, double a21, double a22
) {
    return
        a00 * (a11 * a22 - a12 * a21) -
        a01 * (a10 * a22 - a12 * a20) +
        a02 * (a10 * a21 - a11 * a20);
}

bool faceGeometry(
    OMesh& mesh,
    OMesh::FaceHandle fh,
    Vec3d& unitNormal,
    double& area,
    Vec3d& anchor
) {
    Vec3d p[3];
    int count = 0;

    for (auto vh : mesh.fv_range(fh)) {
        if (count < 3) {
            p[count] = toVec3d(mesh.point(vh));
        }

        ++count;
    }

    if (count != 3) {
        return false;
    }

    Vec3d c = (p[1] - p[0]) % (p[2] - p[0]);
    double length = c.length();

    if (length < 1e-18) {
        return false;
    }

    unitNormal = c * (1.0 / length);
    area = 0.5 * length;
    anchor = p[0];

    return true;
}

Quadric calculateVertexQuadric(
    OMesh& mesh,
    OMesh::VertexHandle vh
) {
    Quadric result;

    for (auto fh : mesh.vf_range(vh)) {
        if (mesh.status(fh).deleted()) {
            continue;
        }

        Vec3d normal;
        Vec3d anchor;
        double area = 0.0;

        if (!faceGeometry(mesh, fh, normal, area, anchor)) {
            continue;
        }

        double d = -(normal[0] * anchor[0] +
                     normal[1] * anchor[1] +
                     normal[2] * anchor[2]);

        double plane[4] = { normal[0], normal[1], normal[2], d };

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] += area * plane[i] * plane[j];
            }
        }
    }

    return result;
}

std::vector<Quadric> calculateAllQuadrics(OMesh& mesh) {
    std::vector<Quadric> quadrics(mesh.n_vertices());

    for (auto vh : mesh.vertices()) {
        if (mesh.status(vh).deleted()) {
            continue;
        }

        quadrics[vh.idx()] = calculateVertexQuadric(mesh, vh);
    }

    return quadrics;
}

bool solveOptimalPosition(
    const Quadric& q,
    Vec3d& result
) {
    double a00 = q.m[0][0];
    double a01 = q.m[0][1];
    double a02 = q.m[0][2];

    double a10 = q.m[1][0];
    double a11 = q.m[1][1];
    double a12 = q.m[1][2];

    double a20 = q.m[2][0];
    double a21 = q.m[2][1];
    double a22 = q.m[2][2];

    double det = determinant3x3(
        a00, a01, a02,
        a10, a11, a12,
        a20, a21, a22
    );

    if (std::abs(det) < 1e-12) {
        return false;
    }

    double b0 = -q.m[0][3];
    double b1 = -q.m[1][3];
    double b2 = -q.m[2][3];

    double detX = determinant3x3(
        b0, a01, a02,
        b1, a11, a12,
        b2, a21, a22
    );

    double detY = determinant3x3(
        a00, b0, a02,
        a10, b1, a12,
        a20, b2, a22
    );

    double detZ = determinant3x3(
        a00, a01, b0,
        a10, a11, b1,
        a20, a21, b2
    );

    result = Vec3d(detX / det, detY / det, detZ / det);

    return true;
}

bool faceContainsVertex(
    OMesh& mesh,
    OMesh::FaceHandle fh,
    OMesh::VertexHandle vh
) {
    for (auto fvh : mesh.fv_range(fh)) {
        if (fvh == vh) {
            return true;
        }
    }

    return false;
}

bool wouldFlipNormals(
    OMesh& mesh,
    OMesh::VertexHandle v0,
    OMesh::VertexHandle v1,
    const Vec3d& target,
    double minDot
) {
    std::unordered_set<int> checkedFaces;

    for (auto fh : mesh.vf_range(v0)) {
        if (!mesh.status(fh).deleted()) {
            checkedFaces.insert(fh.idx());
        }
    }

    for (auto fh : mesh.vf_range(v1)) {
        if (!mesh.status(fh).deleted()) {
            checkedFaces.insert(fh.idx());
        }
    }

    for (int faceIndex : checkedFaces) {
        OMesh::FaceHandle fh(faceIndex);

        if (faceContainsVertex(mesh, fh, v0) &&
            faceContainsVertex(mesh, fh, v1)) {
            continue;
        }

        Vec3d oldPoints[3];
        Vec3d newPoints[3];
        int count = 0;

        for (auto vh : mesh.fv_range(fh)) {
            if (count >= 3) {
                break;
            }

            oldPoints[count] = toVec3d(mesh.point(vh));
            newPoints[count] =
                (vh == v0 || vh == v1)
                    ? target
                    : oldPoints[count];

            ++count;
        }

        if (count != 3) {
            continue;
        }

        Vec3d oldNormal =
            (oldPoints[1] - oldPoints[0]) %
            (oldPoints[2] - oldPoints[0]);

        if (oldNormal.length() < 1e-18) {
            continue;
        }

        oldNormal.normalize();

        Vec3d newNormal =
            (newPoints[1] - newPoints[0]) %
            (newPoints[2] - newPoints[0]);

        if (newNormal.length() < 1e-18) {
            return true;
        }

        newNormal.normalize();

        if (OpenMesh::dot(oldNormal, newNormal) < minDot) {
            return true;
        }
    }

    return false;
}

double edgeNormalAngleDegrees(
    OMesh& mesh,
    OMesh::EdgeHandle eh
) {
    auto heh0 = mesh.halfedge_handle(eh, 0);
    auto heh1 = mesh.halfedge_handle(eh, 1);

    auto fh0 = mesh.face_handle(heh0);
    auto fh1 = mesh.face_handle(heh1);

    if (!fh0.is_valid() || !fh1.is_valid()) {
        return 0.0;
    }

    Vec3d n0;
    Vec3d n1;
    Vec3d anchor;
    double area = 0.0;

    if (!faceGeometry(mesh, fh0, n0, area, anchor) ||
        !faceGeometry(mesh, fh1, n1, area, anchor)) {
        return 0.0;
    }

    double dot = OpenMesh::dot(n0, n1);
    dot = std::max(-1.0, std::min(1.0, dot));

    return std::acos(dot) * 180.0 / kPi;
}

bool isHardEdge(
    OMesh& mesh,
    OMesh::EdgeHandle eh,
    const DecimationConfig& config
) {
    if (!config.preserveHardEdges) {
        return false;
    }

    return edgeNormalAngleDegrees(mesh, eh) >
           static_cast<double>(config.hardEdgeAngleThreshold);
}

bool isUVSeamEdge(
    OMesh& mesh,
    OMesh::EdgeHandle eh
) {
    auto h0 = mesh.halfedge_handle(eh, 0);
    auto h1 = mesh.halfedge_handle(eh, 1);

    if (!mesh.face_handle(h0).is_valid() ||
        !mesh.face_handle(h1).is_valid()) {
        return false;
    }

    Vec2f toB0 = mesh.texcoord2D(h0);
    Vec2f toA0 = mesh.texcoord2D(mesh.prev_halfedge_handle(h0));
    Vec2f toA1 = mesh.texcoord2D(h1);
    Vec2f toB1 = mesh.texcoord2D(mesh.prev_halfedge_handle(h1));

    return (toB0 - toB1).sqrnorm() > kUVSeamEpsilonSquared ||
           (toA0 - toA1).sqrnorm() > kUVSeamEpsilonSquared;
}

bool isSeamVertex(
    OMesh& mesh,
    OMesh::VertexHandle vh
) {
    for (auto eh : mesh.ve_range(vh)) {
        if (mesh.status(eh).deleted()) {
            continue;
        }

        if (isUVSeamEdge(mesh, eh)) {
            return true;
        }
    }

    return false;
}

bool isLockedVertex(
    OMesh& mesh,
    OMesh::VertexHandle vh,
    const DecimationConfig& config
) {
    if (config.lockBoundaryEdges && mesh.is_boundary(vh)) {
        return true;
    }

    if (config.preserveUVMapping && isSeamVertex(mesh, vh)) {
        return true;
    }

    return false;
}

Vec2f vertexUV(
    OMesh& mesh,
    OMesh::VertexHandle vh
) {
    for (auto heh : mesh.vih_range(vh)) {
        if (mesh.face_handle(heh).is_valid()) {
            return mesh.texcoord2D(heh);
        }
    }

    return Vec2f(0.0f, 0.0f);
}

void setVertexUV(
    OMesh& mesh,
    OMesh::VertexHandle vh,
    const Vec2f& uv
) {
    for (auto heh : mesh.vih_range(vh)) {
        mesh.set_texcoord2D(heh, uv);
    }
}

void assignFaceCornerUVs(
    OMesh& mesh,
    OMesh::FaceHandle fh,
    const std::vector<std::pair<OMesh::VertexHandle, Vec2f>>& corners
) {
    if (!fh.is_valid()) {
        return;
    }

    for (auto heh : mesh.fh_range(fh)) {
        auto to = mesh.to_vertex_handle(heh);

        for (const auto& corner : corners) {
            if (corner.first == to) {
                mesh.set_texcoord2D(heh, corner.second);
                break;
            }
        }
    }
}

double signedUVArea(const Vec2f uv[3]) {
    double ax = static_cast<double>(uv[1][0]) - static_cast<double>(uv[0][0]);
    double ay = static_cast<double>(uv[1][1]) - static_cast<double>(uv[0][1]);
    double bx = static_cast<double>(uv[2][0]) - static_cast<double>(uv[0][0]);
    double by = static_cast<double>(uv[2][1]) - static_cast<double>(uv[0][1]);

    return 0.5 * (ax * by - ay * bx);
}

bool wouldDistortUV(
    OMesh& mesh,
    OMesh::VertexHandle v0,
    OMesh::VertexHandle v1,
    const Vec3d& targetPosition,
    const Vec2f& targetUV
) {
    std::unordered_set<int> checkedFaces;

    for (auto fh : mesh.vf_range(v0)) {
        if (!mesh.status(fh).deleted()) {
            checkedFaces.insert(fh.idx());
        }
    }

    for (auto fh : mesh.vf_range(v1)) {
        if (!mesh.status(fh).deleted()) {
            checkedFaces.insert(fh.idx());
        }
    }

    for (int faceIndex : checkedFaces) {
        OMesh::FaceHandle fh(faceIndex);

        if (faceContainsVertex(mesh, fh, v0) &&
            faceContainsVertex(mesh, fh, v1)) {
            continue;
        }

        OMesh::VertexHandle corners[3];
        Vec3d oldP[3];
        Vec3d newP[3];
        Vec2f oldUV[3];
        Vec2f newUV[3];
        int count = 0;

        for (auto heh : mesh.fh_range(fh)) {
            if (count >= 3) {
                ++count;
                break;
            }

            corners[count] = mesh.to_vertex_handle(heh);
            oldP[count] = toVec3d(mesh.point(corners[count]));
            oldUV[count] = mesh.texcoord2D(heh);

            if (corners[count] == v0 || corners[count] == v1) {
                newP[count] = targetPosition;
                newUV[count] = targetUV;
            } else {
                newP[count] = oldP[count];
                newUV[count] = oldUV[count];
            }

            ++count;
        }

        if (count != 3) {
            continue;
        }

        double oldArea =
            0.5 * ((oldP[1] - oldP[0]) % (oldP[2] - oldP[0])).length();

        double newArea =
            0.5 * ((newP[1] - newP[0]) % (newP[2] - newP[0])).length();

        double oldUVArea = signedUVArea(oldUV);
        double newUVArea = signedUVArea(newUV);

        if (oldArea < 1e-18 || std::abs(oldUVArea) < 1e-14) {
            continue;
        }

        if (newArea < 1e-18) {
            return true;
        }

        if (oldUVArea * newUVArea <= 0.0) {
            return true;
        }

        double oldDensity = std::abs(oldUVArea) / oldArea;
        double newDensity = std::abs(newUVArea) / newArea;
        double ratio = newDensity / oldDensity;

        if (ratio > kMaxUVStretch || ratio < 1.0 / kMaxUVStretch) {
            return true;
        }
    }

    return false;
}

bool candidateIsAllowed(
    OMesh& mesh,
    const EdgeCandidate& candidate,
    const DecimationConfig& config
) {
    auto eh = candidate.edge;

    if (!eh.is_valid()) {
        return false;
    }

    if (mesh.status(eh).deleted()) {
        return false;
    }

    if (!mesh.is_collapse_ok(mesh.halfedge_handle(eh, 0)) &&
        !mesh.is_collapse_ok(mesh.halfedge_handle(eh, 1))) {
        return false;
    }

    if (config.lockBoundaryEdges && mesh.is_boundary(eh)) {
        return false;
    }

    if (isHardEdge(mesh, eh, config)) {
        return false;
    }

    if (config.preventNormalFlipping) {
        if (wouldFlipNormals(
            mesh,
            candidate.v0,
            candidate.v1,
            candidate.position,
            kMinNormalDot
        )) {
            return false;
        }
    }

    if (config.preserveUVMapping) {
        if (wouldDistortUV(
            mesh,
            candidate.v0,
            candidate.v1,
            candidate.position,
            candidate.uv
        )) {
            return false;
        }
    }

    return true;
}

bool evaluateEdge(
    OMesh& mesh,
    OMesh::EdgeHandle eh,
    const std::vector<Quadric>& quadrics,
    const std::vector<std::size_t>& versions,
    EdgeCandidate& result,
    const DecimationConfig& config
) {
    if (!eh.is_valid() || mesh.status(eh).deleted()) {
        return false;
    }

    auto heh0 = mesh.halfedge_handle(eh, 0);
    auto heh1 = mesh.halfedge_handle(eh, 1);

    if (!heh0.is_valid() || !heh1.is_valid()) {
        return false;
    }

    auto v0 = mesh.from_vertex_handle(heh0);
    auto v1 = mesh.to_vertex_handle(heh0);

    if (!v0.is_valid() || !v1.is_valid()) {
        return false;
    }

    if (mesh.status(v0).deleted() || mesh.status(v1).deleted()) {
        return false;
    }

    if (isLockedVertex(mesh, v0, config) ||
        isLockedVertex(mesh, v1, config)) {
        return false;
    }

    Quadric q = quadrics[v0.idx()] + quadrics[v1.idx()];

    Vec3d p0 = toVec3d(mesh.point(v0));
    Vec3d p1 = toVec3d(mesh.point(v1));
    Vec3d pm = (p0 + p1) * 0.5;

    double edgeLength = (p1 - p0).length();

    bool b0 = mesh.is_boundary(v0);
    bool b1 = mesh.is_boundary(v1);

    std::vector<Vec3d> candidates;

    if (b0 && b1) {
        if (!mesh.is_boundary(eh)) {
            return false;
        }

        candidates.push_back(p0);
        candidates.push_back(p1);
    } else if (b0) {
        candidates.push_back(p0);
    } else if (b1) {
        candidates.push_back(p1);
    } else {
        Vec3d optimal;

        if (!config.preserveUVMapping &&
            solveOptimalPosition(q, optimal) &&
            (optimal - pm).length() <= edgeLength) {
            candidates.push_back(optimal);
        }

        candidates.push_back(p0);
        candidates.push_back(p1);
        candidates.push_back(pm);

        if (config.preserveUVMapping) {
            candidates.push_back(p0 * 0.75 + p1 * 0.25);
            candidates.push_back(p0 * 0.25 + p1 * 0.75);
        }
    }

    double bestCost = std::numeric_limits<double>::max();
    Vec3d bestPosition = pm;

    for (const auto& position : candidates) {
        double cost = q.evaluate(position);

        if (cost < bestCost) {
            bestCost = cost;
            bestPosition = position;
        }
    }

    double t = 0.5;

    if (edgeLength > 1e-18) {
        t = OpenMesh::dot(bestPosition - p0, p1 - p0) /
            (edgeLength * edgeLength);
        t = std::max(0.0, std::min(1.0, t));
    }

    Vec2f uv0 = vertexUV(mesh, v0);
    Vec2f uv1 = vertexUV(mesh, v1);

    result.edge = eh;
    result.v0 = v0;
    result.v1 = v1;
    result.position = bestPosition;
    result.uv = uv0 + (uv1 - uv0) * static_cast<float>(t);
    result.cost = std::max(0.0, bestCost);
    result.version0 = versions[v0.idx()];
    result.version1 = versions[v1.idx()];

    return candidateIsAllowed(mesh, result, config);
}

unsigned int countActiveTriangles(OMesh& mesh) {
    unsigned int count = 0;

    for (auto fh : mesh.faces()) {
        if (!mesh.status(fh).deleted()) {
            ++count;
        }
    }

    return count;
}

void addEdgeCandidate(
    OMesh& mesh,
    OMesh::EdgeHandle eh,
    const std::vector<Quadric>& quadrics,
    const std::vector<std::size_t>& versions,
    CandidateQueue& queue,
    const DecimationConfig& config
) {
    EdgeCandidate candidate;

    if (evaluateEdge(
        mesh,
        eh,
        quadrics,
        versions,
        candidate,
        config
    )) {
        queue.push(candidate);
    }
}

OMesh meshDataToOpenMesh(const MeshData& input) {
    OMesh mesh;

    mesh.request_vertex_status();
    mesh.request_edge_status();
    mesh.request_face_status();
    mesh.request_halfedge_status();
    mesh.request_halfedge_texcoords2D();
    mesh.request_vertex_normals();
    mesh.request_face_normals();

    if (input.vertices.empty()) {
        return mesh;
    }

    double minX = std::numeric_limits<double>::max();
    double minY = minX;
    double minZ = minX;
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = maxX;
    double maxZ = maxX;

    for (const auto& vertex : input.vertices) {
        minX = std::min(minX, static_cast<double>(vertex.pos.x));
        minY = std::min(minY, static_cast<double>(vertex.pos.y));
        minZ = std::min(minZ, static_cast<double>(vertex.pos.z));
        maxX = std::max(maxX, static_cast<double>(vertex.pos.x));
        maxY = std::max(maxY, static_cast<double>(vertex.pos.y));
        maxZ = std::max(maxZ, static_cast<double>(vertex.pos.z));
    }

    double diagonal = std::sqrt(
        (maxX - minX) * (maxX - minX) +
        (maxY - minY) * (maxY - minY) +
        (maxZ - minZ) * (maxZ - minZ)
    );

    double epsilon = std::max(diagonal * 1e-6, 1e-9);

    std::map<std::array<long long, 3>, OMesh::VertexHandle> weldMap;
    std::vector<OMesh::VertexHandle> handles(input.vertices.size());

    for (std::size_t i = 0; i < input.vertices.size(); ++i) {
        const auto& p = input.vertices[i].pos;

        std::array<long long, 3> key = {
            std::llround(static_cast<double>(p.x) / epsilon),
            std::llround(static_cast<double>(p.y) / epsilon),
            std::llround(static_cast<double>(p.z) / epsilon)
        };

        auto it = weldMap.find(key);

        if (it == weldMap.end()) {
            auto vh = mesh.add_vertex(OMesh::Point(p.x, p.y, p.z));
            it = weldMap.emplace(key, vh).first;
        }

        handles[i] = it->second;
    }

    for (std::size_t i = 0; i + 2 < input.indices.size(); i += 3) {
        unsigned int source[3] = {
            input.indices[i],
            input.indices[i + 1],
            input.indices[i + 2]
        };

        if (source[0] >= handles.size() ||
            source[1] >= handles.size() ||
            source[2] >= handles.size()) {
            continue;
        }

        OMesh::VertexHandle faceVertices[3] = {
            handles[source[0]],
            handles[source[1]],
            handles[source[2]]
        };

        if (faceVertices[0] == faceVertices[1] ||
            faceVertices[1] == faceVertices[2] ||
            faceVertices[2] == faceVertices[0]) {
            continue;
        }

        OMesh::FaceHandle fh = mesh.add_face(
            std::vector<OMesh::VertexHandle>(
                faceVertices,
                faceVertices + 3
            )
        );

        if (!fh.is_valid()) {
            for (int k = 0; k < 3; ++k) {
                faceVertices[k] =
                    mesh.add_vertex(mesh.point(faceVertices[k]));
            }

            fh = mesh.add_face(
                std::vector<OMesh::VertexHandle>(
                    faceVertices,
                    faceVertices + 3
                )
            );

            if (!fh.is_valid()) {
                continue;
            }
        }

        std::vector<std::pair<OMesh::VertexHandle, Vec2f>> corners;

        for (int k = 0; k < 3; ++k) {
            corners.emplace_back(
                faceVertices[k],
                Vec2f(
                    input.vertices[source[k]].u,
                    input.vertices[source[k]].v
                )
            );
        }

        assignFaceCornerUVs(mesh, fh, corners);
    }

    mesh.update_normals();

    return mesh;
}

MeshData openMeshToMeshData(
    OMesh& mesh,
    const MeshData& original
) {
    MeshData result;

    result.textureID = original.textureID;
    result.hasTexture = original.hasTexture;
    result.diffuseColor = original.diffuseColor;
    result.texturePath = original.texturePath;

    mesh.update_normals();

    std::map<std::tuple<int, float, float>, unsigned int> vertexMap;

    for (auto fh : mesh.faces()) {
        if (mesh.status(fh).deleted()) {
            continue;
        }

        unsigned int faceIndices[3];
        int count = 0;

        for (auto heh : mesh.fh_range(fh)) {
            if (count >= 3) {
                ++count;
                break;
            }

            auto vh = mesh.to_vertex_handle(heh);
            Vec2f uv = mesh.texcoord2D(heh);

            auto key = std::make_tuple(vh.idx(), uv[0], uv[1]);
            auto it = vertexMap.find(key);

            if (it == vertexMap.end()) {
                auto point = mesh.point(vh);
                auto normal = mesh.normal(vh);

                Vertex vertex;

                vertex.pos = { point[0], point[1], point[2] };
                vertex.norm = { normal[0], normal[1], normal[2] };
                vertex.u = uv[0];
                vertex.v = uv[1];

                unsigned int newIndex =
                    static_cast<unsigned int>(result.vertices.size());

                result.vertices.push_back(vertex);
                it = vertexMap.emplace(key, newIndex).first;
            }

            faceIndices[count] = it->second;
            ++count;
        }

        if (count != 3) {
            continue;
        }

        if (faceIndices[0] == faceIndices[1] ||
            faceIndices[1] == faceIndices[2] ||
            faceIndices[2] == faceIndices[0]) {
            continue;
        }

        result.indices.push_back(faceIndices[0]);
        result.indices.push_back(faceIndices[1]);
        result.indices.push_back(faceIndices[2]);
    }

    return result;
}

}

MeshData MeshOptimizer::incrementalDecimation(
    const MeshData& inputMesh,
    const DecimationConfig& config
) {
    if (inputMesh.vertices.empty() ||
        inputMesh.indices.size() < 3) {
        return inputMesh;
    }

    OMesh mesh = meshDataToOpenMesh(inputMesh);

    unsigned int originalTriangles = countActiveTriangles(mesh);

    if (originalTriangles == 0) {
        return inputMesh;
    }

    float percentage =
        std::max(0.0f, std::min(100.0f, config.reductionPercentage));

    unsigned int targetTriangles =
        static_cast<unsigned int>(
            std::floor(
                static_cast<double>(originalTriangles) *
                (1.0 - static_cast<double>(percentage) / 100.0)
            )
        );

    if (targetTriangles < 1) {
        targetTriangles = 1;
    }

    if (targetTriangles >= originalTriangles) {
        return inputMesh;
    }

    std::vector<Quadric> quadrics = calculateAllQuadrics(mesh);
    std::vector<std::size_t> versions(mesh.n_vertices(), 0);
    CandidateQueue queue;

    for (auto eh : mesh.edges()) {
        if (mesh.status(eh).deleted()) {
            continue;
        }

        if (config.cancelFlag && config.cancelFlag->load()) {
            return inputMesh;
        }

        addEdgeCandidate(mesh, eh, quadrics, versions, queue, config);
    }

    unsigned int currentTriangles = originalTriangles;

    while (currentTriangles > targetTriangles && !queue.empty()) {
        if (config.cancelFlag && config.cancelFlag->load()) {
            return inputMesh;
        }

        EdgeCandidate candidate = queue.top();
        queue.pop();

        if (!candidate.edge.is_valid() ||
            mesh.status(candidate.edge).deleted()) {
            continue;
        }

        if (!candidate.v0.is_valid() ||
            !candidate.v1.is_valid() ||
            mesh.status(candidate.v0).deleted() ||
            mesh.status(candidate.v1).deleted()) {
            continue;
        }

        if (candidate.version0 != versions[candidate.v0.idx()] ||
            candidate.version1 != versions[candidate.v1.idx()]) {
            continue;
        }

        auto heh0 = mesh.halfedge_handle(candidate.edge, 0);
        auto heh1 = mesh.halfedge_handle(candidate.edge, 1);

        OMesh::HalfedgeHandle collapseHalfedge;

        if (mesh.is_collapse_ok(heh0)) {
            collapseHalfedge = heh0;
        } else if (mesh.is_collapse_ok(heh1)) {
            collapseHalfedge = heh1;
        } else {
            continue;
        }

        auto fromVertex = mesh.from_vertex_handle(collapseHalfedge);
        auto toVertex = mesh.to_vertex_handle(collapseHalfedge);

        unsigned int removedFaces = 0;

        if (mesh.face_handle(heh0).is_valid()) {
            ++removedFaces;
        }

        if (mesh.face_handle(heh1).is_valid()) {
            ++removedFaces;
        }

        Quadric combinedQuadric =
            quadrics[fromVertex.idx()] + quadrics[toVertex.idx()];

        std::unordered_set<int> ring;

        for (auto neighbor : mesh.vv_range(fromVertex)) {
            if (!mesh.status(neighbor).deleted()) {
                ring.insert(neighbor.idx());
            }
        }

        for (auto neighbor : mesh.vv_range(toVertex)) {
            if (!mesh.status(neighbor).deleted()) {
                ring.insert(neighbor.idx());
            }
        }

        ring.erase(fromVertex.idx());
        ring.insert(toVertex.idx());

        mesh.collapse(collapseHalfedge);

        if (!mesh.status(fromVertex).deleted()) {
            continue;
        }

        mesh.set_point(toVertex, toPoint(candidate.position));
        setVertexUV(mesh, toVertex, candidate.uv);

        quadrics[toVertex.idx()] = combinedQuadric;

        currentTriangles =
            currentTriangles > removedFaces
                ? currentTriangles - removedFaces
                : 0;

        std::unordered_set<int> edgesToUpdate;

        for (int index : ring) {
            OMesh::VertexHandle vh(index);

            if (mesh.status(vh).deleted()) {
                continue;
            }

            versions[vh.idx()]++;

            for (auto eh : mesh.ve_range(vh)) {
                if (!mesh.status(eh).deleted()) {
                    edgesToUpdate.insert(eh.idx());
                }
            }
        }

        for (int edgeIndex : edgesToUpdate) {
            addEdgeCandidate(
                mesh,
                OMesh::EdgeHandle(edgeIndex),
                quadrics,
                versions,
                queue,
                config
            );
        }
    }

    mesh.garbage_collection();
    mesh.update_normals();

    return openMeshToMeshData(mesh, inputMesh);
}