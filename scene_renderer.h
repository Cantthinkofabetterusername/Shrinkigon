#ifndef SCENE_RENDERER_H
#define SCENE_RENDERER_H

#include <OpenGL/gl.h>

#include <vector>

#include "mesh_data.h"
#include "theme.h"

struct SceneCamera {
    float rotX = 20.0f;
    float rotY = 45.0f;
    float zoom = 5.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

struct SceneModelInfo {
    bool loaded = false;
    Vec3 center = { 0.0f, 0.0f, 0.0f };
    float scale = 1.0f;
    float minY = 0.0f;
};

struct SceneViewport {
    GLint x = 0;
    GLint y = 0;
    GLsizei width = 0;
    GLsizei height = 0;
    GLsizei framebufferWidth = 0;
    GLsizei framebufferHeight = 0;
};

void renderScene(
    const SceneViewport& viewport,
    const SceneCamera& camera,
    const SceneModelInfo& model,
    const std::vector<MeshData>& meshes,
    const Theme& theme
);

#endif
