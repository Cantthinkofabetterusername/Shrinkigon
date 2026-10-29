#include "scene_renderer.h"

#include <cmath>

namespace {

void drawSky(const Theme& theme) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glColor3fv(theme.skyBottom);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glColor3fv(theme.skyTop);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void drawBaseplate(const Theme& theme, float topY) {
    const int tiles = 20;
    const float tileSize = 1.0f;
    const float half = tiles * tileSize * 0.5f;
    const float thickness = 0.5f;
    const float bottomY = topY - thickness;

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < tiles; ++i) {
        for (int j = 0; j < tiles; ++j) {
            if (((i + j) & 1) == 0) {
                glColor3fv(theme.tileA);
            } else {
                glColor3fv(theme.tileB);
            }

            float x0 = -half + i * tileSize;
            float z0 = -half + j * tileSize;

            glVertex3f(x0, topY, z0);
            glVertex3f(x0, topY, z0 + tileSize);
            glVertex3f(x0 + tileSize, topY, z0 + tileSize);
            glVertex3f(x0 + tileSize, topY, z0);
        }
    }

    glColor3fv(theme.plateSide);

    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-half, topY, half);
    glVertex3f(-half, bottomY, half);
    glVertex3f(half, bottomY, half);
    glVertex3f(half, topY, half);

    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-half, topY, -half);
    glVertex3f(half, topY, -half);
    glVertex3f(half, bottomY, -half);
    glVertex3f(-half, bottomY, -half);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(half, topY, -half);
    glVertex3f(half, topY, half);
    glVertex3f(half, bottomY, half);
    glVertex3f(half, bottomY, -half);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-half, topY, -half);
    glVertex3f(-half, bottomY, -half);
    glVertex3f(-half, bottomY, half);
    glVertex3f(-half, topY, half);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-half, bottomY, -half);
    glVertex3f(half, bottomY, -half);
    glVertex3f(half, bottomY, half);
    glVertex3f(-half, bottomY, half);

    glEnd();

    glDisable(GL_LIGHTING);

    glLineWidth(1.5f);
    glColor3fv(theme.gridLine);

    float lineY = topY + 0.004f;

    glBegin(GL_LINES);

    for (int i = 0; i <= tiles; ++i) {
        float p = -half + i * tileSize;

        glVertex3f(p, lineY, -half);
        glVertex3f(p, lineY, half);
        glVertex3f(-half, lineY, p);
        glVertex3f(half, lineY, p);
    }

    glEnd();

    glLineWidth(1.0f);
    glDisable(GL_COLOR_MATERIAL);
}

void drawMeshes(const std::vector<MeshData>& meshes) {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    for (const auto& mesh : meshes) {
        if (mesh.hasTexture && mesh.textureID != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, mesh.textureID);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_COLOR_MATERIAL);
            glColor3f(mesh.diffuseColor.x, mesh.diffuseColor.y, mesh.diffuseColor.z);
        }

        glBegin(GL_TRIANGLES);

        for (unsigned int idx : mesh.indices) {
            const auto& vertex = mesh.vertices[idx];

            if (mesh.hasTexture) {
                glTexCoord2f(vertex.u, vertex.v);
            }

            glNormal3f(vertex.norm.x, vertex.norm.y, vertex.norm.z);
            glVertex3f(vertex.pos.x, vertex.pos.y, vertex.pos.z);
        }

        glEnd();

        if (mesh.hasTexture) {
            glDisable(GL_TEXTURE_2D);
        } else {
            glDisable(GL_COLOR_MATERIAL);
        }
    }
}

}

void renderScene(
    const SceneViewport& viewport,
    const SceneCamera& camera,
    const SceneModelInfo& model,
    const std::vector<MeshData>& meshes,
    const Theme& theme
) {
    if (viewport.width <= 0 || viewport.height <= 0) {
        return;
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

    glClearColor(theme.skyBottom[0], theme.skyBottom[1], theme.skyBottom[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSky(theme);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    float aspect = static_cast<float>(viewport.width) / static_cast<float>(viewport.height);
    float nearZ = 0.1f;
    float farZ = 100.0f;
    float top = nearZ * std::tan(22.5f * 3.14159265f / 180.0f);
    float right = top * aspect;

    glFrustum(-right, right, -top, top, nearZ, farZ);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glTranslatef(camera.panX, camera.panY, -camera.zoom);
    glRotatef(camera.rotX, 1.0f, 0.0f, 0.0f);
    glRotatef(camera.rotY, 0.0f, 1.0f, 0.0f);

    GLfloat lightPos[] = { 1.0f, 1.0f, 2.0f, 0.0f };
    GLfloat lightDiffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    GLfloat lightAmbient[] = { 0.4f, 0.4f, 0.4f, 1.0f };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHT0);

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);

    float floorY = 0.0f;

    if (model.loaded) {
        floorY = (model.minY - model.center.y) * model.scale;
    }

    drawBaseplate(theme, floorY - 0.004f);

    if (model.loaded) {
        glScalef(model.scale, model.scale, model.scale);
        glTranslatef(-model.center.x, -model.center.y, -model.center.z);

        drawMeshes(meshes);
    }

    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHTING);
    glDisable(GL_NORMALIZE);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, viewport.framebufferWidth, viewport.framebufferHeight);
    glClearColor(theme.windowBg.x, theme.windowBg.y, theme.windowBg.z, 1.0f);
}
