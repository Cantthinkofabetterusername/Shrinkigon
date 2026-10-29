#ifndef MESH_DATA_H
#define MESH_DATA_H

#include <vector>
#include <string>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
typedef unsigned int GLuint;
#endif

struct Vec3 {
    float x, y, z;
};

struct Vertex {
    Vec3 pos;
    Vec3 norm;
    float u, v;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    GLuint textureID = 0;
    bool hasTexture = false;
    Vec3 diffuseColor = { 0.8f, 0.8f, 0.8f };
    std::string texturePath;
};

#endif