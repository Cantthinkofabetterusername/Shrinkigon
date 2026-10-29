#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "mesh_data.h"
#include "optimization_algorithms.h"
#include "localization.h"
#include "theme.h"
#include "scene_renderer.h"
#include "settings_ui.h"
#include "cloud_client.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>
#include <dirent.h>

#include <OpenGL/gl.h>
#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui_opengl21_renderer.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Exporter.hpp>

namespace fs = std::filesystem;

struct Character {
    GLuint textureID;
    int width;
    int height;
    int bearingX;
    int bearingY;
    unsigned int advance;
};

std::map<char, Character> Characters;

struct Edge {
    unsigned int u, v;

    bool operator<(const Edge& o) const {
        if (u != o.u) return u < o.u;
        return v < o.v;
    }
};

std::vector<MeshData> loadedMeshes;
std::vector<Vec3> loadedVertices;
std::vector<Vec3> loadedNormals;
std::vector<unsigned int> loadedIndices;

Vec3 modelCenter = { 0.0f, 0.0f, 0.0f };
float modelScale = 1.0f;

unsigned int totalVertexCount = 0;
unsigned int totalEdgeCount = 0;
float modelMemoryKB = 0.0f;

unsigned int originalVertexCount = 0;
unsigned int originalEdgeCount = 0;
float originalMemoryKB = 0.0f;

unsigned int optimizedVertexCount = 0;
unsigned int optimizedEdgeCount = 0;
float optimizedMemoryKB = 0.0f;

bool modelLoaded = false;
bool optimizationCompleted = false;
std::vector<MeshData> originalMeshes;
std::string optimizationNote = "";
float modelMinY = 0.0f;
SettingsUI settingsUI;
bool saveCompleted = false;
bool saveFailed = false;
std::string saveErrorMessage = "";

std::string originalFolderPath = "";
std::string originalModelPath = "";

bool isDirectory(const std::string& path) {
    struct stat s;

    if (stat(path.c_str(), &s) == 0) {
        return (s.st_mode & S_IFDIR) != 0;
    }

    return false;
}

std::string findModelInPath(const std::string& path) {
    if (!isDirectory(path)) {
        return path;
    }

    std::vector<std::string> extensions = {
        ".gltf",
        ".glb",
        ".obj",
        ".fbx",
        ".dae",
        ".blend",
        ".stl",
        ".ply"
    };

    DIR* dir = opendir(path.c_str());

    if (!dir) {
        return path;
    }

    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        if (name == "." || name == "..") {
            continue;
        }

        std::string fullPath =
            path + "/" + name;

        if (!isDirectory(fullPath)) {
            std::string lowerName = name;

            std::transform(
                lowerName.begin(),
                lowerName.end(),
                lowerName.begin(),
                ::tolower
            );

            for (const auto& ext : extensions) {
                if (
                    lowerName.size() >= ext.size() &&
                    lowerName.compare(
                        lowerName.size() - ext.size(),
                        ext.size(),
                        ext
                    ) == 0
                ) {
                    closedir(dir);
                    return fullPath;
                }
            }
        }
    }

    closedir(dir);

    return path;
}

std::string openMacFileDialog() {
    std::string result = "";

    std::string script =
        std::string(
            "osascript -e 'POSIX path of "
            "(choose folder with prompt \""
        ) +
        tr("dialog.select_folder") +
        "\")' 2>/dev/null";

    FILE* pipe = popen(script.c_str(), "r");

    if (pipe) {
        char buffer[512];

        while (
            fgets(
                buffer,
                sizeof(buffer),
                pipe
            ) != nullptr
        ) {
            result += buffer;
        }

        pclose(pipe);

        while (
            !result.empty() &&
            (
                result.back() == '\n' ||
                result.back() == '\r'
            )
        ) {
            result.pop_back();
        }
    }

    return result;
}

GLuint loadTexture(
    const std::string& baseDir,
    const std::string& texPath,
    const aiScene* scene
) {
    if (texPath.empty()) {
        return 0;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    unsigned char* data = nullptr;

    if (texPath[0] == '*') {
        int texIdx =
            std::atoi(texPath.c_str() + 1);

        if (
            texIdx >= 0 &&
            texIdx < static_cast<int>(scene->mNumTextures)
        ) {
            aiTexture* tex =
                scene->mTextures[texIdx];

            if (tex->mHeight == 0) {
                data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(
                        tex->pcData
                    ),
                    tex->mWidth,
                    &width,
                    &height,
                    &channels,
                    4
                );
            } else {
                data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(
                        tex->pcData
                    ),
                    tex->mWidth *
                    tex->mHeight *
                    sizeof(aiTexel),
                    &width,
                    &height,
                    &channels,
                    4
                );
            }
        }
    } else {
        std::string cleanPath = texPath;

        std::replace(
            cleanPath.begin(),
            cleanPath.end(),
            '\\',
            '/'
        );

        std::string filename = cleanPath;

        size_t lastSlash =
            cleanPath.find_last_of('/');

        if (lastSlash != std::string::npos) {
            filename =
                cleanPath.substr(
                    lastSlash + 1
                );
        }

        std::vector<std::string> candidates = {
            baseDir + "/" + cleanPath,
            baseDir + "/textures/" + filename,
            baseDir + "/" + filename,
            cleanPath
        };

        for (const auto& candidate : candidates) {
            data = stbi_load(
                candidate.c_str(),
                &width,
                &height,
                &channels,
                4
            );

            if (data) {
                break;
            }
        }
    }

    if (!data) {
        return 0;
    }

    GLuint texID = 0;

    glGenTextures(
        1,
        &texID
    );

    glBindTexture(
        GL_TEXTURE_2D,
        texID
    );

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_REPEAT
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_REPEAT
    );

    stbi_image_free(data);

    return texID;
}

void clearLoadedModel() {
    for (auto& mesh : loadedMeshes) {
        if (
            mesh.hasTexture &&
            mesh.textureID != 0
        ) {
            glDeleteTextures(
                1,
                &mesh.textureID
            );
        }
    }

    loadedMeshes.clear();
    originalMeshes.clear();
    loadedVertices.clear();
    loadedNormals.clear();
    loadedIndices.clear();

    totalVertexCount = 0;
    totalEdgeCount = 0;
    modelMemoryKB = 0.0f;

    modelLoaded = false;
}

float calculateMeshMemoryKB() {
    size_t memBytes =
        loadedVertices.size() *
        sizeof(Vec3) *
        2 +
        loadedIndices.size() *
        sizeof(unsigned int);

    return static_cast<float>(
        memBytes
    ) / 1024.0f;
}

void rebuildLoadedMeshStatistics() {
    loadedVertices.clear();
    loadedNormals.clear();
    loadedIndices.clear();

    std::set<Edge> uniqueEdges;

    Vec3 minPos = {
        1e9f,
        1e9f,
        1e9f
    };

    Vec3 maxPos = {
        -1e9f,
        -1e9f,
        -1e9f
    };

    totalVertexCount = 0;

    for (const auto& mesh : loadedMeshes) {
        unsigned int vertexOffset =
            static_cast<unsigned int>(
                loadedVertices.size()
            );

        totalVertexCount +=
            static_cast<unsigned int>(
                mesh.vertices.size()
            );

        for (const auto& vertex : mesh.vertices) {
            loadedVertices.push_back(
                vertex.pos
            );

            loadedNormals.push_back(
                vertex.norm
            );

            minPos.x =
                std::min(
                    minPos.x,
                    vertex.pos.x
                );

            minPos.y =
                std::min(
                    minPos.y,
                    vertex.pos.y
                );

            minPos.z =
                std::min(
                    minPos.z,
                    vertex.pos.z
                );

            maxPos.x =
                std::max(
                    maxPos.x,
                    vertex.pos.x
                );

            maxPos.y =
                std::max(
                    maxPos.y,
                    vertex.pos.y
                );

            maxPos.z =
                std::max(
                    maxPos.z,
                    vertex.pos.z
                );
        }

        for (size_t i = 0;
             i + 2 < mesh.indices.size();
             i += 3) {

            unsigned int i0 =
                vertexOffset +
                mesh.indices[i];

            unsigned int i1 =
                vertexOffset +
                mesh.indices[i + 1];

            unsigned int i2 =
                vertexOffset +
                mesh.indices[i + 2];

            loadedIndices.push_back(i0);
            loadedIndices.push_back(i1);
            loadedIndices.push_back(i2);

            Edge e0 = {
                std::min(i0, i1),
                std::max(i0, i1)
            };

            Edge e1 = {
                std::min(i1, i2),
                std::max(i1, i2)
            };

            Edge e2 = {
                std::min(i2, i0),
                std::max(i2, i0)
            };

            uniqueEdges.insert(e0);
            uniqueEdges.insert(e1);
            uniqueEdges.insert(e2);
        }
    }

    totalEdgeCount =
        static_cast<unsigned int>(
            uniqueEdges.size()
        );

    if (totalVertexCount > 0) {
        modelCenter.x =
            (minPos.x + maxPos.x) *
            0.5f;

        modelCenter.y =
            (minPos.y + maxPos.y) *
            0.5f;

        modelCenter.z =
            (minPos.z + maxPos.z) *
            0.5f;

        float dx =
            maxPos.x - minPos.x;

        float dy =
            maxPos.y - minPos.y;

        float dz =
            maxPos.z - minPos.z;

        float maxDim =
            std::max(
                dx,
                std::max(dy, dz)
            );

        if (maxDim > 0.00001f) {
            modelScale =
                3.0f / maxDim;
        } else {
            modelScale = 1.0f;
        }
    }

    if (totalVertexCount > 0) {
        modelMinY = minPos.y;
    }

    modelMemoryKB =
        calculateMeshMemoryKB();
}

bool loadModelFile(
    const std::string& inputPath
) {
    std::string actualPath =
        findModelInPath(inputPath);

    if (actualPath.empty()) {
        return false;
    }

    Assimp::Importer importer;

    const aiScene* scene =
        importer.ReadFile(
            actualPath,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_FlipUVs |
            aiProcess_PreTransformVertices
        );

    if (
        !scene ||
        scene->mFlags &
        AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode
    ) {
        return false;
    }

    clearLoadedModel();

    std::string baseDir = "";

    size_t lastSlash =
        actualPath.find_last_of(
            "/\\"
        );

    if (lastSlash != std::string::npos) {
        baseDir =
            actualPath.substr(
                0,
                lastSlash
            );
    } else {
        baseDir = ".";
    }

    std::vector<GLuint> materialTextures(
        scene->mNumMaterials,
        0
    );

    std::vector<Vec3> materialColors(
        scene->mNumMaterials,
        {
            0.8f,
            0.8f,
            0.8f
        }
    );

    std::vector<std::string> materialTexturePaths(
        scene->mNumMaterials
    );

    for (
        unsigned int i = 0;
        i < scene->mNumMaterials;
        ++i
    ) {
        aiMaterial* mat =
            scene->mMaterials[i];

        aiColor3D color(
            0.8f,
            0.8f,
            0.8f
        );

        if (
            mat->Get(
                AI_MATKEY_COLOR_DIFFUSE,
                color
            ) == AI_SUCCESS
        ) {
            materialColors[i] = {
                color.r,
                color.g,
                color.b
            };
        }

        aiString texPath;

        if (
            mat->GetTexture(
                aiTextureType_DIFFUSE,
                0,
                &texPath
            ) == AI_SUCCESS ||
            mat->GetTexture(
                aiTextureType_BASE_COLOR,
                0,
                &texPath
            ) == AI_SUCCESS
        ) {
            materialTexturePaths[i] =
                texPath.C_Str();

            materialTextures[i] =
                loadTexture(
                    baseDir,
                    texPath.C_Str(),
                    scene
                );
        }
    }

    std::set<Edge> uniqueEdges;

    Vec3 minPos = {
        1e9f,
        1e9f,
        1e9f
    };

    Vec3 maxPos = {
        -1e9f,
        -1e9f,
        -1e9f
    };

    for (
        unsigned int m = 0;
        m < scene->mNumMeshes;
        ++m
    ) {
        aiMesh* mesh =
            scene->mMeshes[m];

        MeshData meshData;

        if (
            mesh->mMaterialIndex <
            scene->mNumMaterials
        ) {
            meshData.diffuseColor =
                materialColors[
                    mesh->mMaterialIndex
                ];

            meshData.textureID =
                materialTextures[
                    mesh->mMaterialIndex
                ];

            meshData.hasTexture =
                meshData.textureID != 0;

            meshData.texturePath =
                materialTexturePaths[
                    mesh->mMaterialIndex
                ];
        }

        unsigned int vertexOffset =
            static_cast<unsigned int>(
                loadedVertices.size()
            );

        for (
            unsigned int i = 0;
            i < mesh->mNumVertices;
            ++i
        ) {
            Vec3 pos = {
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            };

            Vec3 norm = {
                0.0f,
                1.0f,
                0.0f
            };

            if (mesh->HasNormals()) {
                norm = {
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z
                };
            }

            float u = 0.0f;
            float v = 0.0f;

            if (
                mesh->HasTextureCoords(0)
            ) {
                u =
                    mesh->mTextureCoords[0][i].x;

                v =
                    mesh->mTextureCoords[0][i].y;
            }

            Vertex vertex = {
                pos,
                norm,
                u,
                v
            };

            meshData.vertices.push_back(
                vertex
            );

            minPos.x =
                std::min(
                    minPos.x,
                    pos.x
                );

            minPos.y =
                std::min(
                    minPos.y,
                    pos.y
                );

            minPos.z =
                std::min(
                    minPos.z,
                    pos.z
                );

            maxPos.x =
                std::max(
                    maxPos.x,
                    pos.x
                );

            maxPos.y =
                std::max(
                    maxPos.y,
                    pos.y
                );

            maxPos.z =
                std::max(
                    maxPos.z,
                    pos.z
                );
        }

        for (
            unsigned int f = 0;
            f < mesh->mNumFaces;
            ++f
        ) {
            aiFace face =
                mesh->mFaces[f];

            for (
                unsigned int j = 0;
                j < face.mNumIndices;
                ++j
            ) {
                meshData.indices.push_back(
                    face.mIndices[j]
                );
            }

            if (
                face.mNumIndices >= 3
            ) {
                for (
                    unsigned int j = 0;
                    j < face.mNumIndices;
                    ++j
                ) {
                    unsigned int idx1 =
                        vertexOffset +
                        face.mIndices[j];

                    unsigned int idx2 =
                        vertexOffset +
                        face.mIndices[
                            (j + 1) %
                            face.mNumIndices
                        ];

                    Edge e = {
                        std::min(
                            idx1,
                            idx2
                        ),
                        std::max(
                            idx1,
                            idx2
                        )
                    };

                    uniqueEdges.insert(e);
                }
            }
        }

        loadedMeshes.push_back(
            meshData
        );
    }

    rebuildLoadedMeshStatistics();

    modelLoaded = true;

    return true;
}

void updateOptimizedStatistics() {
    rebuildLoadedMeshStatistics();

    optimizedVertexCount =
        totalVertexCount;

    optimizedEdgeCount =
        totalEdgeCount;

    optimizedMemoryKB =
        modelMemoryKB;
}

constexpr int kOptimizationTimeLimitSeconds = 60;

struct OptimizationJob {
    bool running = false;
    bool timedOut = false;
    std::atomic<bool> cancel{false};
    std::chrono::steady_clock::time_point startTime;
    std::vector<MeshData> input;
    std::vector<MeshData> output;
    std::string noteKey;
    bool usedCloud = false;
    DecimationConfig config;
    std::future<void> future;
};

OptimizationJob optimizationJob;

int optimizationElapsedSeconds() {
    if (!optimizationJob.running) {
        return 0;
    }

    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() -
            optimizationJob.startTime
        ).count()
    );
}

constexpr unsigned int kLocalTriangleLimit = 100000;

unsigned int countLoadedTriangles() {
    unsigned int total = 0;

    for (const auto& mesh : loadedMeshes) {
        total += static_cast<unsigned int>(
            mesh.indices.size() / 3
        );
    }

    return total;
}

bool startOptimization(
    const DecimationConfig& config
) {
    OptimizationJob& job = optimizationJob;

    unsigned int triangleCount =
        countLoadedTriangles();

    if (
        !settingsUI.isSpeedigonActive() &&
        triangleCount > kLocalTriangleLimit
    ) {
        settingsUI.showLimitNotice(
            triangleCount,
            kLocalTriangleLimit
        );

        return false;
    }

    job.input = loadedMeshes;
    job.output.clear();
    job.noteKey.clear();
    job.timedOut = false;
    job.cancel = false;
    job.usedCloud = settingsUI.isSpeedigonActive();
    job.config = config;
    job.config.cancelFlag = &job.cancel;
    job.startTime = std::chrono::steady_clock::now();
    job.running = true;

    bool useCloud = job.usedCloud;
    std::string token = settingsUI.token();

    job.future = std::async(
        std::launch::async,
        [&job, useCloud, token]() {
            bool finished = false;

            if (useCloud) {
                std::vector<MeshData> remoteResult;

                CloudResponse response =
                    CloudClient::optimizeRemote(
                        token,
                        job.input,
                        job.config,
                        remoteResult,
                        &job.cancel
                    );

                if (response.ok) {
                    job.output = std::move(remoteResult);
                    job.noteKey = "opt.note.cloud_ok";
                    finished = true;
                } else {
                    job.noteKey = "opt.note.cloud_fallback";
                }
            }

            if (!finished && !job.cancel.load()) {
                job.output = job.input;

                for (auto& mesh : job.output) {
                    if (job.cancel.load()) {
                        break;
                    }

                    mesh =
                        MeshOptimizer::incrementalDecimation(
                            mesh,
                            job.config
                        );
                }
            }

            if (job.cancel.load()) {
                job.timedOut = true;
            }
        }
    );

    return true;
}

int pollOptimization() {
    OptimizationJob& job = optimizationJob;

    if (!job.running) {
        return 0;
    }

    if (
        optimizationElapsedSeconds() >= kOptimizationTimeLimitSeconds &&
        !job.cancel.load()
    ) {
        job.cancel = true;
    }

    if (
        job.future.wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready
    ) {
        return 0;
    }

    job.future.get();
    job.running = false;

    int result = 1;

    if (job.timedOut) {
        settingsUI.showTimeoutNotice(
            job.usedCloud
        );

        result = 2;
    } else {
        originalMeshes = std::move(job.input);
        loadedMeshes = std::move(job.output);
        optimizationNote = job.noteKey;

        updateOptimizedStatistics();

        optimizationCompleted = true;
        saveCompleted = false;
        saveFailed = false;
    }

    job.input.clear();
    job.output.clear();

    return result;
}

void restoreOriginalModel() {
    if (originalMeshes.empty()) {
        return;
    }

    loadedMeshes = originalMeshes;
    originalMeshes.clear();

    updateOptimizedStatistics();

    optimizationCompleted = false;
    saveCompleted = false;
    saveFailed = false;
    optimizationNote.clear();
}

bool failSave(
    const std::string& message
) {
    saveErrorMessage = message;

    std::fprintf(
        stderr,
        "Save failed: %s\n",
        message.c_str()
    );

    return false;
}

bool saveOptimizedModel(
    const std::string& folderPath
) {
    saveErrorMessage.clear();

    if (
        !modelLoaded ||
        loadedMeshes.empty()
    ) {
        return failSave(
            "no model loaded"
        );
    }

    std::string normalizedPath = folderPath;

    while (
        normalizedPath.size() > 1 &&
        (
            normalizedPath.back() == '/' ||
            normalizedPath.back() == '\\'
        )
    ) {
        normalizedPath.pop_back();
    }

    fs::path inputFolder(normalizedPath);

    if (
        !fs::exists(inputFolder) ||
        !fs::is_directory(inputFolder)
    ) {
        return failSave(
            "invalid folder: " +
            normalizedPath
        );
    }

    fs::path outputFolder =
        inputFolder.parent_path() /
        (
            inputFolder.filename().string() +
            "_shrinkigonized"
        );

    try {
        if (fs::exists(outputFolder)) {
            fs::remove_all(outputFolder);
        }

        fs::create_directories(outputFolder);

        for (
            auto it = fs::recursive_directory_iterator(inputFolder);
            it != fs::recursive_directory_iterator();
            ++it
        ) {
            const fs::path current = it->path();

            if (
                it->is_directory() &&
                current.filename().string().find(
                    "_shrinkigonized"
                ) != std::string::npos
            ) {
                it.disable_recursion_pending();
                continue;
            }

            fs::path target =
                outputFolder /
                fs::relative(current, inputFolder);

            if (it->is_directory()) {
                fs::create_directories(target);
            } else {
                fs::create_directories(target.parent_path());

                fs::copy_file(
                    current,
                    target,
                    fs::copy_options::overwrite_existing
                );
            }
        }
    }
    catch (const std::exception& error) {
        return failSave(
            std::string(
                "copying folder: "
            ) + error.what()
        );
    }
    catch (...) {
        return failSave(
            "copying folder: unknown error"
        );
    }

    std::string originalModel =
        originalModelPath;

    fs::path originalModelPathFs(
        originalModel
    );

    std::string extension =
        originalModelPathFs.extension().string();

    std::string modelFilename =
        originalModelPathFs.filename().string();

    fs::path outputModel =
        outputFolder /
        modelFilename;

    Assimp::Exporter exporter;

    std::string lowerExtension = extension;

    std::transform(
        lowerExtension.begin(),
        lowerExtension.end(),
        lowerExtension.begin(),
        ::tolower
    );

    bool unflipV =
        lowerExtension != ".gltf" &&
        lowerExtension != ".glb";

    aiScene scene;

    scene.mRootNode =
        new aiNode();

    scene.mRootNode->mName =
        aiString("RootNode");

    scene.mNumMeshes =
        static_cast<unsigned int>(
            loadedMeshes.size()
        );

    scene.mMeshes =
        new aiMesh*[scene.mNumMeshes];

    scene.mNumMaterials =
        static_cast<unsigned int>(
            loadedMeshes.size()
        );

    scene.mMaterials =
        new aiMaterial*[scene.mNumMaterials];

    scene.mRootNode->mNumMeshes =
        scene.mNumMeshes;

    scene.mRootNode->mMeshes =
        new unsigned int[
            scene.mNumMeshes
        ];

    for (
        unsigned int m = 0;
        m < scene.mNumMeshes;
        ++m
    ) {
        const MeshData& source =
            loadedMeshes[m];

        aiMesh* mesh =
            new aiMesh();

        mesh->mMaterialIndex = m;

        mesh->mPrimitiveTypes =
            aiPrimitiveType_TRIANGLE;

        mesh->mNumVertices =
            static_cast<unsigned int>(
                source.vertices.size()
            );

        mesh->mVertices =
            new aiVector3D[
                mesh->mNumVertices
            ];

        mesh->mNormals =
            new aiVector3D[
                mesh->mNumVertices
            ];

        if (
            source.hasTexture
        ) {
            mesh->mTextureCoords[0] =
                new aiVector3D[
                    mesh->mNumVertices
                ];

            mesh->mNumUVComponents[0] = 2;
        }

        for (
            unsigned int i = 0;
            i < mesh->mNumVertices;
            ++i
        ) {
            const Vertex& vertex =
                source.vertices[i];

            mesh->mVertices[i] =
                aiVector3D(
                    vertex.pos.x,
                    vertex.pos.y,
                    vertex.pos.z
                );

            mesh->mNormals[i] =
                aiVector3D(
                    vertex.norm.x,
                    vertex.norm.y,
                    vertex.norm.z
                );

            if (
                source.hasTexture
            ) {
                mesh->mTextureCoords[0][i] =
                    aiVector3D(
                        vertex.u,
                        unflipV
                            ? 1.0f - vertex.v
                            : vertex.v,
                        0.0f
                    );
            }
        }

        mesh->mNumFaces =
            static_cast<unsigned int>(
                source.indices.size() / 3
            );

        mesh->mFaces =
            new aiFace[
                mesh->mNumFaces
            ];

        for (
            unsigned int i = 0;
            i < mesh->mNumFaces;
            ++i
        ) {
            aiFace& face =
                mesh->mFaces[i];

            face.mNumIndices = 3;

            face.mIndices =
                new unsigned int[3];

            face.mIndices[0] =
                source.indices[i * 3];

            face.mIndices[1] =
                source.indices[i * 3 + 1];

            face.mIndices[2] =
                source.indices[i * 3 + 2];
        }

        scene.mMeshes[m] =
            mesh;

        scene.mRootNode->mMeshes[m] =
            m;

        aiMaterial* material =
            new aiMaterial();

        aiColor3D diffuse(
            source.diffuseColor.x,
            source.diffuseColor.y,
            source.diffuseColor.z
        );

        material->AddProperty(
            &diffuse,
            1,
            AI_MATKEY_COLOR_DIFFUSE
        );

        aiString materialName(
            "material_" +
            std::to_string(m)
        );

        material->AddProperty(
            &materialName,
            AI_MATKEY_NAME
        );

        if (
            source.hasTexture &&
            !source.texturePath.empty() &&
            source.texturePath[0] != '*'
        ) {
            aiString texturePath(
                source.texturePath
            );

            material->AddProperty(
                &texturePath,
                AI_MATKEY_TEXTURE_DIFFUSE(0)
            );
        }

        scene.mMaterials[m] =
            material;
    }

    const aiExportFormatDesc* format =
        nullptr;

    for (
        size_t i = 0;
        i < exporter.GetExportFormatCount();
        ++i
    ) {
        const aiExportFormatDesc* current =
            exporter.GetExportFormatDescription(i);

        if (
            current &&
            extension.size() > 1 &&
            (
                extension.substr(1) ==
                current->fileExtension
            )
        ) {
            format = current;
            break;
        }
    }

    if (!format) {
        for (
            size_t i = 0;
            i < exporter.GetExportFormatCount();
            ++i
        ) {
            const aiExportFormatDesc* current =
                exporter.GetExportFormatDescription(i);

            if (
                current &&
                std::string(current->fileExtension) == "obj"
            ) {
                format = current;
                break;
            }
        }

        if (!format) {
            return failSave(
                "no export format for " +
                extension
            );
        }

        outputModel.replace_extension(".obj");
    }

    aiReturn result = AI_FAILURE;

    try {
        result =
            exporter.Export(
                &scene,
                format->id,
                outputModel.string()
            );
    }
    catch (const std::exception& error) {
        return failSave(
            std::string(
                "exporter: "
            ) + error.what()
        );
    }
    catch (...) {
        return failSave(
            "exporter: unknown error"
        );
    }

    if (result != AI_SUCCESS) {
        const char* errorText =
            exporter.GetErrorString();

        return failSave(
            std::string(
                "exporter ("
            ) +
            format->id +
            "): " +
            (
                errorText &&
                errorText[0]
                ? errorText
                : "no details"
            )
        );
    }

    if (!fs::exists(outputModel)) {
        return failSave(
            "exporter reported success but file not found: " +
            outputModel.string()
        );
    }

    return true;
}

void renderText(
    const std::string& text,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b,
    float a,
    int windowWidth,
    int windowHeight
) {
    float screenX =
        (x + 1.0f) *
        0.5f *
        windowWidth;

    float screenY =
        (y + 1.0f) *
        0.5f *
        windowHeight;

    float totalWidth = 0.0f;

    for (char c : text) {
        if (
            Characters.find(c) !=
            Characters.end()
        ) {
            totalWidth +=
                (
                    Characters[c].advance >>
                    6
                ) *
                scale;
        }
    }

    float startX =
        screenX -
        totalWidth / 2.0f;

    float startY =
        screenY;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glColor4f(
        r,
        g,
        b,
        a
    );

    for (char c : text) {
        if (
            Characters.find(c) ==
            Characters.end()
        ) {
            continue;
        }

        Character ch =
            Characters[c];

        float xpos =
            startX +
            ch.bearingX *
            scale;

        float ypos =
            startY -
            (
                ch.height -
                ch.bearingY
            ) *
            scale;

        float w =
            ch.width *
            scale;

        float h =
            ch.height *
            scale;

        glBindTexture(
            GL_TEXTURE_2D,
            ch.textureID
        );

        glBegin(GL_QUADS);

        glTexCoord2f(
            0.0f,
            1.0f
        );

        glVertex2f(
            xpos,
            ypos
        );

        glTexCoord2f(
            1.0f,
            1.0f
        );

        glVertex2f(
            xpos + w,
            ypos
        );

        glTexCoord2f(
            1.0f,
            0.0f
        );

        glVertex2f(
            xpos + w,
            ypos + h
        );

        glTexCoord2f(
            0.0f,
            0.0f
        );

        glVertex2f(
            xpos,
            ypos + h
        );

        glEnd();

        startX +=
            (
                ch.advance >>
                6
            ) *
            scale;
    }

    glDisable(GL_TEXTURE_2D);
}

void framebuffer_size_callback(
    GLFWwindow* window,
    int width,
    int height
) {
    glViewport(
        0,
        0,
        width,
        height
    );
}

float bezier_curve(
    float t,
    float point1,
    float point2,
    float point3,
    float point4
) {
    return
        pow(1 - t, 3) *
        point1 +

        3 *
        pow(1 - t, 2) *
        t *
        point2 +

        3 *
        (1 - t) *
        pow(t, 2) *
        point3 +

        pow(t, 3) *
        point4;
}

int main() {
    if (!glfwInit()) {
        std::cerr <<
            "Failed to initialize GLFW!"
            << std::endl;

        return -1;
    }

    glfwWindowHint(
        GLFW_CONTEXT_VERSION_MAJOR,
        2
    );

    glfwWindowHint(
        GLFW_CONTEXT_VERSION_MINOR,
        1
    );

    GLFWwindow* window =
        glfwCreateWindow(
            1600,
            900,
            "Shrinkigon",
            nullptr,
            nullptr
        );

    if (window == nullptr) {
        std::cerr <<
            "Failed to create GLFW window!"
            << std::endl;

        glfwTerminate();

        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(
        window,
        framebuffer_size_callback
    );

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io =
        ImGui::GetIO();

    io.Fonts->AddFontFromFileTTF(
        "fonts/Geo-Regular.ttf",
        24.0f
    );

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(
        window,
        true
    );

    ImGui_ImplOpenGL21_Init();

    FT_Library ft;

    if (FT_Init_FreeType(&ft)) {
        std::cerr <<
            "Failed to initialize FreeType!"
            << std::endl;

        glfwTerminate();

        return -1;
    }

    FT_Face face;

    if (
        FT_New_Face(
            ft,
            "fonts/CaacupeOne-Regular.ttf",
            0,
            &face
        )
    ) {
        std::cerr <<
            "Failed to load font!"
            << std::endl;

        FT_Done_FreeType(ft);
        glfwTerminate();

        return -1;
    }

    FT_Set_Pixel_Sizes(
        face,
        0,
        128
    );

    glPixelStorei(
        GL_UNPACK_ALIGNMENT,
        1
    );

    for (
        unsigned char c = 0;
        c < 128;
        c++
    ) {
        if (
            FT_Load_Char(
                face,
                c,
                FT_LOAD_RENDER
            )
        ) {
            continue;
        }

        GLuint texture;

        glGenTextures(
            1,
            &texture
        );

        glBindTexture(
            GL_TEXTURE_2D,
            texture
        );

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_ALPHA,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_ALPHA,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            GL_CLAMP_TO_EDGE
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            GL_CLAMP_TO_EDGE
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_LINEAR
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_LINEAR
        );

        Character character = {
            texture,
            static_cast<int>(
                face->glyph->bitmap.width
            ),
            static_cast<int>(
                face->glyph->bitmap.rows
            ),
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<unsigned int>(
                face->glyph->advance.x
            )
        };

        Characters.insert(
            std::pair<char, Character>(
                c,
                character
            )
        );
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glClearColor(
        1.0f,
        1.0f,
        1.0f,
        1.0f
    );

    settingsUI.init();

    float t = 0.00f;
    float alpha = 1.00f;

    bool button1_pressed = false;
    bool button2_pressed = false;

    static char filePath[256] = "";

    bool isLoading = false;
    int loadingFrames = 0;

    float camRotX = 20.0f;
    float camRotY = 45.0f;
    float camZoom = 5.0f;
    float camPanX = 0.0f;
    float camPanY = 0.0f;

    ImVec2 previewMin(0, 0);
    ImVec2 previewSize(0, 0);

    DecimationConfig optimizationConfig;

    ImGuiStyle& style =
        ImGui::GetStyle();

    style.ItemSpacing =
        ImVec2(8, 8);

    style.FramePadding =
        ImVec2(10, 6);

    style.Colors[
        ImGuiCol_WindowBg
    ] =
        ImVec4(
            0.10f,
            0.10f,
            0.12f,
            1.0f
        );

    style.Colors[
        ImGuiCol_Button
    ] =
        ImVec4(
            0.5f,
            0.5f,
            0.5f,
            1.0f
        );

    style.Colors[
        ImGuiCol_ButtonHovered
    ] =
        ImVec4(
            0.6f,
            0.6f,
            0.6f,
            1.0f
        );

    style.Colors[
        ImGuiCol_ButtonActive
    ] =
        ImVec4(
            0.7f,
            0.7f,
            0.7f,
            1.0f
        );

    style.Colors[
        ImGuiCol_FrameBg
    ] =
        ImVec4(
            0.5f,
            0.5f,
            0.5f,
            1.0f
        );

    style.Colors[
        ImGuiCol_FrameBgHovered
    ] =
        ImVec4(
            0.6f,
            0.6f,
            0.6f,
            1.0f
        );

    style.Colors[
        ImGuiCol_TitleBg
    ] =
        ImVec4(
            0.14f,
            0.14f,
            0.16f,
            1.0f
        );

    style.Colors[
        ImGuiCol_TitleBgActive
    ] =
        ImVec4(
            0.18f,
            0.18f,
            0.20f,
            1.0f
        );

    captureBaseStyle();

    while (
        !glfwWindowShouldClose(window)
    ) {
        ImGui_ImplOpenGL21_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        settingsUI.update();

        if (pollOptimization() == 2) {
            button1_pressed =
                true;
        }

        applyTheme(
            settingsUI.isSpeedigonActive()
        );

        const Theme& theme =
            getTheme(
                settingsUI.isSpeedigonActive()
            );

        glClear(
            GL_COLOR_BUFFER_BIT
        );

        int width;
        int height;

        glfwGetFramebufferSize(
            window,
            &width,
            &height
        );

        glMatrixMode(
            GL_PROJECTION
        );

        glLoadIdentity();

        glOrtho(
            0,
            width,
            0,
            height,
            -1,
            1
        );

        glMatrixMode(
            GL_MODELVIEW
        );

        glLoadIdentity();

        if (isLoading) {
            loadingFrames++;

            if (loadingFrames > 2) {
                if (
                    strlen(filePath) > 0
                ) {
                    if (!button2_pressed) {
                        if (
                            loadModelFile(
                                filePath
                            )
                        ) {
                            originalFolderPath =
                                filePath;

                            originalModelPath =
                                findModelInPath(
                                    filePath
                                );

                            originalVertexCount =
                                totalVertexCount;

                            originalEdgeCount =
                                totalEdgeCount;

                            originalMemoryKB =
                                modelMemoryKB;

                            optimizationCompleted =
                                false;


                            saveCompleted =
                                false;


                            saveFailed =
                                false;
                        }
                    } else {
                        if (
                            !startOptimization(
                                optimizationConfig
                            )
                        ) {
                            button1_pressed =
                                true;
                        }

                        button2_pressed =
                            false;
                    }
                }

                isLoading = false;
            }
        }

        if (t <= 1.00f) {
            float ypos =
                bezier_curve(
                    t,
                    -2.1f,
                    -1.6f,
                    1.5f,
                    0.3f
                );

            renderText(
                tr("welcome.1"),
                0.0f,
                ypos,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                width,
                height
            );

            t += 0.01f;
        }
        else if (
            t > 1.00f &&
            t <= 2.00f
        ) {
            float ypos =
                bezier_curve(
                    t - 1,
                    -2.1f,
                    -1.6f,
                    1.1f,
                    -0.1f
                );

            renderText(
                tr("welcome.1"),
                0.0f,
                0.3f,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                width,
                height
            );

            renderText(
                tr("welcome.2"),
                0.0f,
                ypos,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                width,
                height
            );

            t += 0.01f;
        }
        else if (
            t > 2.00f &&
            t <= 3.00f
        ) {
            float ypos =
                bezier_curve(
                    t - 2,
                    -2.1f,
                    -1.6f,
                    0.7f,
                    -0.5f
                );

            renderText(
                tr("welcome.1"),
                0.0f,
                0.3f,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                width,
                height
            );

            renderText(
                tr("welcome.2"),
                0.0f,
                -0.1f,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                width,
                height
            );

            renderText(
                "Shrinkigon!",
                0.0f,
                ypos,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                width,
                height
            );

            t += 0.01f;
        }
        else if (
            t > 3.00f &&
            alpha >= 0.00f
        ) {
            renderText(
                tr("welcome.1"),
                0.0f,
                0.3f,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                alpha,
                width,
                height
            );

            renderText(
                tr("welcome.2"),
                0.0f,
                -0.1f,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                alpha,
                width,
                height
            );

            renderText(
                "Shrinkigon!",
                0.0f,
                -0.5f,
                2.0f,
                0.0f,
                0.0f,
                0.0f,
                alpha,
                width,
                height
            );

            alpha -= 0.005f;
        }
        else {
            glClearColor(
                theme.windowBg.x,
                theme.windowBg.y,
                theme.windowBg.z,
                1.0f
            );

            ImGui::SetNextWindowPos(
                ImVec2(0, 0)
            );

            ImGui::SetNextWindowSize(
                ImGui::GetIO().DisplaySize
            );

            ImGui::Begin(
                "Shrinkigon",
                nullptr,
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoBackground
            );

            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                theme.sidebarBg
            );

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                theme.text
            );

            ImGui::BeginChild(
                "Sidebar",
                ImVec2(330, 0)
            );

            settingsUI.drawSidebarButton();

            ImGui::Text(
                "%s", tr("asset.current")
            );

            ImGui::Separator();

            ImGui::InputText(
                "##Path",
                filePath,
                IM_ARRAYSIZE(filePath)
            );

            if (
                ImGui::Button(
                    tr("btn.upload")
                ) &&
                !optimizationJob.running
            ) {
                std::string selectedPath =
                    openMacFileDialog();

                if (
                    !selectedPath.empty()
                ) {
                    strncpy(
                        filePath,
                        selectedPath.c_str(),
                        sizeof(filePath) - 1
                    );

                    filePath[
                        sizeof(filePath) - 1
                    ] = '\0';

                    button1_pressed =
                        true;

                    button2_pressed =
                        false;

                    optimizationCompleted =
                        false;


                    saveCompleted =
                        false;


                    saveFailed =
                        false;

                    isLoading =
                        true;

                    loadingFrames =
                        0;
                }
            }

            if (button1_pressed) {
                ImGui::Spacing();

                ImGui::Text(
                    "%s", tr("opt.params")
                );

                ImGui::Separator();

                ImGui::InputInt(
                    tr("opt.target"),
                    &optimizationConfig.targetTriangleCount
                );

                ImGui::InputFloat(
                    tr("opt.reduction"),
                    &optimizationConfig.reductionPercentage,
                    1.0f,
                    10.0f,
                    "%.1f%%"
                );

                if (
                    optimizationConfig.reductionPercentage < 0.0f
                ) {
                    optimizationConfig.reductionPercentage =
                        0.0f;
                }

                if (
                    optimizationConfig.reductionPercentage > 100.0f
                ) {
                    optimizationConfig.reductionPercentage =
                        100.0f;
                }

                ImGui::InputFloat(
                    tr("opt.max_error"),
                    &optimizationConfig.maxDeformationError
                );

                ImGui::Checkbox(
                    tr("opt.prevent_flip"),
                    &optimizationConfig.preventNormalFlipping
                );

                ImGui::Checkbox(
                    tr("opt.preserve_edges"),
                    &optimizationConfig.preserveHardEdges
                );

                if (
                    optimizationConfig.preserveHardEdges
                ) {
                    ImGui::InputFloat(
                        tr("opt.edge_angle"),
                        &optimizationConfig.hardEdgeAngleThreshold
                    );
                }

                ImGui::Checkbox(
                    tr("opt.lock_boundary"),
                    &optimizationConfig.lockBoundaryEdges
                );

                ImGui::Checkbox(
                    tr("opt.preserve_uv"),
                    &optimizationConfig.preserveUVMapping
                );

                ImGui::Spacing();

                if (
                    ImGui::Button(
                        tr("btn.optimize")
                    )
                ) {
                    button1_pressed =
                        false;

                    button2_pressed =
                        true;

                    isLoading =
                        true;

                    loadingFrames =
                        0;
                }
            }

            if (optimizationJob.running) {
                ImGui::Spacing();

                ImGui::Text(
                    "%s %d / %d s",
                    tr("opt.running"),
                    optimizationElapsedSeconds(),
                    kOptimizationTimeLimitSeconds
                );
            }

            if (
                optimizationCompleted
            ) {
                ImGui::Spacing();

                if (!optimizationNote.empty()) {
                    ImGui::TextWrapped(
                        "%s",
                        tr(optimizationNote.c_str())
                    );
                }

                if (
                    ImGui::Button(
                        tr("btn.remesh")
                    )
                ) {
                    restoreOriginalModel();

                    button1_pressed =
                        true;
                }

                if (
                    ImGui::Button(
                        tr("btn.save")
                    )
                ) {
                    saveCompleted =
                        saveOptimizedModel(
                            originalFolderPath
                        );

                    saveFailed =
                        !saveCompleted;
                }

                if (saveCompleted) {
                    ImGui::Text(
                        "%s", tr("save.ok")
                    );
                }

                if (saveFailed) {
                    ImGui::TextWrapped(
                        "%s: %s", tr("save.failed"),
                        saveErrorMessage.c_str()
                    );
                }
            }

            ImGui::EndChild();

            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::BeginChild(
                "Main Area",
                ImVec2(0, 0)
            );

            ImVec4 previewBg =
                ImVec4(
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f
                );

            ImVec4 previewText =
                theme.text;

            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                previewBg
            );

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                previewText
            );

            ImGui::BeginChild(
                "Preview",
                ImVec2(0, 700)
            );

            previewMin =
                ImGui::GetWindowPos();

            previewSize =
                ImGui::GetWindowSize();

            if (
                ImGui::IsWindowHovered()
            ) {
                if (
                    ImGui::IsMouseDragging(
                        ImGuiMouseButton_Left
                    )
                ) {
                    ImVec2 delta =
                        ImGui::GetIO().MouseDelta;

                    camRotY +=
                        delta.x *
                        0.5f;

                    camRotX +=
                        delta.y *
                        0.5f;
                }

                if (
                    ImGui::IsMouseDragging(
                        ImGuiMouseButton_Right
                    ) ||
                    ImGui::IsMouseDragging(
                        ImGuiMouseButton_Middle
                    )
                ) {
                    ImVec2 delta =
                        ImGui::GetIO().MouseDelta;

                    camPanX +=
                        delta.x *
                        0.005f *
                        camZoom;

                    camPanY -=
                        delta.y *
                        0.005f *
                        camZoom;
                }

                if (
                    ImGui::GetIO().MouseWheel !=
                    0.0f
                ) {
                    camZoom -=
                        ImGui::GetIO().MouseWheel *
                        0.3f;

                    if (
                        camZoom < 0.1f
                    ) {
                        camZoom =
                            0.1f;
                    }
                }
            }

            ImGui::EndChild();

            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                theme.infoBg
            );

            ImGui::PushStyleColor(
                ImGuiCol_Text,
                theme.infoText
            );

            ImGui::BeginChild(
                "Information",
                ImVec2(0, 0)
            );

            ImGui::Text(
                " %s", tr("info.title")
            );

            ImGui::Separator();

            if (isLoading) {
                ImGui::Text(
                    " %s", tr("info.loading")
                );
            }
            else if (modelLoaded) {
                ImGui::Text(
                    "%s", tr("info.original")
                );

                ImGui::Text(
                    " %s: %u", tr("info.vertices"),
                    originalVertexCount
                );

                ImGui::Text(
                    " %s: %u", tr("info.edges"),
                    originalEdgeCount
                );

                if (
                    originalMemoryKB >=
                    1024.0f
                ) {
                    ImGui::Text(
                        " %s: %.2f MB", tr("info.memory"),
                        originalMemoryKB /
                        1024.0f
                    );
                } else {
                    ImGui::Text(
                        " %s: %.2f KB", tr("info.memory"),
                        originalMemoryKB
                    );
                }

                if (
                    optimizationCompleted
                ) {
                    ImGui::Separator();

                    ImGui::Text(
                        "%s", tr("info.optimized")
                    );

                    ImGui::Text(
                        " %s: %u", tr("info.vertices"),
                        optimizedVertexCount
                    );

                    ImGui::Text(
                        " %s: %u", tr("info.edges"),
                        optimizedEdgeCount
                    );

                    if (
                        optimizedMemoryKB >=
                        1024.0f
                    ) {
                        ImGui::Text(
                            " %s: %.2f MB", tr("info.memory"),
                            optimizedMemoryKB /
                            1024.0f
                        );
                    } else {
                        ImGui::Text(
                            " %s: %.2f KB", tr("info.memory"),
                            optimizedMemoryKB
                        );
                    }

                    unsigned int removedVertices =
                        originalVertexCount >
                        optimizedVertexCount
                        ? originalVertexCount -
                          optimizedVertexCount
                        : 0;

                    unsigned int removedEdges =
                        originalEdgeCount >
                        optimizedEdgeCount
                        ? originalEdgeCount -
                          optimizedEdgeCount
                        : 0;

                    float reducedMemory =
                        originalMemoryKB >
                        optimizedMemoryKB
                        ? originalMemoryKB -
                          optimizedMemoryKB
                        : 0.0f;

                    float vertexReduction =
                        originalVertexCount > 0
                        ? static_cast<float>(
                            removedVertices
                        ) /
                        originalVertexCount *
                        100.0f
                        : 0.0f;

                    float edgeReduction =
                        originalEdgeCount > 0
                        ? static_cast<float>(
                            removedEdges
                        ) /
                        originalEdgeCount *
                        100.0f
                        : 0.0f;

                    float memoryReduction =
                        originalMemoryKB > 0.0f
                        ? reducedMemory /
                          originalMemoryKB *
                          100.0f
                        : 0.0f;

                    ImGui::Separator();

                    ImGui::Text(
                        "%s", tr("info.reduction")
                    );

                    ImGui::Text(
                        " %s: %u (%.2f%%)", tr("info.vertices_removed"),
                        removedVertices,
                        vertexReduction
                    );

                    ImGui::Text(
                        " %s: %u (%.2f%%)", tr("info.edges_removed"),
                        removedEdges,
                        edgeReduction
                    );

                    ImGui::Text(
                        " %s: %.2f KB (%.2f%%)", tr("info.memory_reduced"),
                        reducedMemory,
                        memoryReduction
                    );
                }
            }
            else if (button1_pressed) {
                ImGui::Text(
                    " %s", tr("info.invalid")
                );
            }

            ImGui::EndChild();

            ImGui::PopStyleColor(2);

            ImGui::EndChild();

            ImGui::End();

            settingsUI.drawWindows();
        }

        ImGui::Render();

        if (
            t > 3.00f &&
            alpha <= 0.00f &&
            previewSize.x > 0 &&
            previewSize.y > 0
        ) {
            int winW;
            int winH;

            glfwGetWindowSize(
                window,
                &winW,
                &winH
            );

            float scaleX =
                static_cast<float>(width) /
                winW;

            float scaleY =
                static_cast<float>(height) /
                winH;

            SceneViewport viewport;

            viewport.x =
                static_cast<GLint>(
                    previewMin.x *
                    scaleX
                );

            viewport.y =
                static_cast<GLint>(
                    height -
                    (
                        previewMin.y +
                        previewSize.y
                    ) *
                    scaleY
                );

            viewport.width =
                static_cast<GLsizei>(
                    previewSize.x *
                    scaleX
                );

            viewport.height =
                static_cast<GLsizei>(
                    previewSize.y *
                    scaleY
                );

            viewport.framebufferWidth = width;
            viewport.framebufferHeight = height;

            SceneCamera camera;

            camera.rotX = camRotX;
            camera.rotY = camRotY;
            camera.zoom = camZoom;
            camera.panX = camPanX;
            camera.panY = camPanY;

            SceneModelInfo modelInfo;

            modelInfo.loaded = modelLoaded;
            modelInfo.center = modelCenter;
            modelInfo.scale = modelScale;
            modelInfo.minY = modelMinY;

            renderScene(
                viewport,
                camera,
                modelInfo,
                loadedMeshes,
                theme
            );
        }

        ImGui_ImplOpenGL21_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    optimizationJob.cancel = true;

    if (optimizationJob.future.valid()) {
        optimizationJob.future.wait();
    }

    clearLoadedModel();

    for (
        auto const& pair :
        Characters
    ) {
        glDeleteTextures(
            1,
            &pair.second.textureID
        );
    }

    ImGui_ImplOpenGL21_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwTerminate();

    return 0;
}