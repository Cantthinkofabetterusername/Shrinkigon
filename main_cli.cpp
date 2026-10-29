#include <iostream>
#include <string>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Exporter.hpp>

#include "mesh_data.h"
#include "optimization_algorithms.h"

std::vector<MeshData> loadMeshCLI(const std::string& path) {
    std::vector<MeshData> meshes;
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "[Error] Fallo al leer modelo: " << importer.GetErrorString() << "\n";
        return meshes;
    }

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* ai_mesh = scene->mMeshes[m];
        MeshData meshData;

        meshData.hasTexture = false;
        meshData.textureID = 0;

        for (unsigned int i = 0; i < ai_mesh->mNumVertices; ++i) {
            Vertex v;
            v.pos = { ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };

            if (ai_mesh->HasNormals()) {
                v.norm = { ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };
            } else {
                v.norm = { 0.0f, 1.0f, 0.0f };
            }

            v.u = 0.0f; v.v = 0.0f;
            meshData.vertices.push_back(v);
        }

        for (unsigned int f = 0; f < ai_mesh->mNumFaces; ++f) {
            aiFace face = ai_mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                meshData.indices.push_back(face.mIndices[j]);
            }
        }
        meshes.push_back(meshData);
    }
    return meshes;
}

bool saveMeshCLI(const std::vector<MeshData>& meshes, const std::string& path) {
    aiScene scene;
    scene.mRootNode = new aiNode();
    scene.mRootNode->mName = aiString("RootNode");

    scene.mNumMeshes = meshes.size();
    scene.mMeshes = new aiMesh*[scene.mNumMeshes];
    scene.mNumMaterials = 1;
    scene.mMaterials = new aiMaterial*[1];
    scene.mMaterials[0] = new aiMaterial();

    scene.mRootNode->mNumMeshes = scene.mNumMeshes;
    scene.mRootNode->mMeshes = new unsigned int[scene.mNumMeshes];

    for (unsigned int m = 0; m < meshes.size(); ++m) {
        const MeshData& source = meshes[m];
        aiMesh* mesh = new aiMesh();
        mesh->mMaterialIndex = 0;
        mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

        mesh->mNumVertices = source.vertices.size();
        mesh->mVertices = new aiVector3D[mesh->mNumVertices];
        mesh->mNormals = new aiVector3D[mesh->mNumVertices];

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            mesh->mVertices[i] = aiVector3D(source.vertices[i].pos.x, source.vertices[i].pos.y, source.vertices[i].pos.z);
            mesh->mNormals[i] = aiVector3D(source.vertices[i].norm.x, source.vertices[i].norm.y, source.vertices[i].norm.z);
        }

        mesh->mNumFaces = source.indices.size() / 3;
        mesh->mFaces = new aiFace[mesh->mNumFaces];

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            aiFace& face = mesh->mFaces[i];
            face.mNumIndices = 3;
            face.mIndices = new unsigned int[3];
            face.mIndices[0] = source.indices[i * 3];
            face.mIndices[1] = source.indices[i * 3 + 1];
            face.mIndices[2] = source.indices[i * 3 + 2];
        }

        scene.mMeshes[m] = mesh;
        scene.mRootNode->mMeshes[m] = m;
    }

    Assimp::Exporter exporter;
    return (exporter.Export(&scene, "obj", path) == AI_SUCCESS);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Uso: ./speedigon_cli <input.obj> <output.obj> <target_triangles>\n";
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    int targetTriangles = std::stoi(argv[3]);

    std::cout << "[Speedigon] Leyendo malla: " << inputFile << "...\n";
    std::vector<MeshData> meshes = loadMeshCLI(inputFile);

    if (meshes.empty()) {
        std::cerr << "[Error] No se pudo cargar el modelo.\n";
        return 1;
    }

    DecimationConfig config;
    config.targetTriangleCount = targetTriangles;
    config.cancelFlag = nullptr;

    std::cout << "[Speedigon] Reduciendo poligonos...\n";
    for (auto& mesh : meshes) {
        mesh = MeshOptimizer::incrementalDecimation(mesh, config);
    }

    std::cout << "[Speedigon] Escribiendo salida...\n";
    if (saveMeshCLI(meshes, outputFile)) {
        std::cout << "[Speedigon] Proceso finalizado exitosamente.\n";
    } else {
        std::cerr << "[Error] Fallo al guardar archivo de salida.\n";
        return 1;
    }

    return 0;
}