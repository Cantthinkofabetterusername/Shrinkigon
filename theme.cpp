#include "theme.h"

namespace {

const Theme kDefaultTheme = {
    ImVec4(0.10f, 0.10f, 0.12f, 1.0f),
    ImVec4(0.10f, 0.10f, 0.12f, 1.0f),
    ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
    ImVec4(0.0f, 0.0f, 0.0f, 1.0f),
    ImVec4(0.1f, 0.8f, 0.1f, 1.0f),
    { 0.32f, 0.54f, 0.86f },
    { 0.80f, 0.89f, 0.97f },
    { 0.46f, 0.47f, 0.50f },
    { 0.40f, 0.41f, 0.44f },
    { 0.28f, 0.29f, 0.32f },
    { 0.24f, 0.25f, 0.28f }
};

const Theme kNeonTheme = {
    ImVec4(0.03f, 0.0f, 0.08f, 1.0f),
    ImVec4(0.05f, 0.0f, 0.12f, 1.0f),
    ImVec4(0.20f, 1.0f, 0.95f, 1.0f),
    ImVec4(0.0f, 0.0f, 0.05f, 1.0f),
    ImVec4(0.22f, 1.0f, 0.08f, 1.0f),
    { 0.02f, 0.0f, 0.10f },
    { 0.38f, 0.0f, 0.52f },
    { 0.06f, 0.0f, 0.14f },
    { 0.10f, 0.0f, 0.22f },
    { 0.0f, 0.95f, 1.0f },
    { 0.85f, 0.0f, 0.75f }
};

ImVec4 baseColors[ImGuiCol_COUNT];
bool wasBlue[ImGuiCol_COUNT];
bool baseCaptured = false;
bool applied = false;
bool lastNeon = false;

bool isBlue(const ImVec4& c) {
    return c.z > c.x + 0.15f && c.z >= c.y;
}

}

const Theme& getTheme(bool neon) {
    return neon ? kNeonTheme : kDefaultTheme;
}

void captureBaseStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        ImVec4 color = style.Colors[i];
        wasBlue[i] = isBlue(color);

        if (wasBlue[i]) {
            float luma = 0.299f * color.x + 0.587f * color.y + 0.114f * color.z;
            color = ImVec4(luma, luma, luma, color.w);
        }

        baseColors[i] = color;
    }

    baseColors[ImGuiCol_CheckMark] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    baseColors[ImGuiCol_TextSelectedBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.30f);
    baseColors[ImGuiCol_Tab] = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);
    baseColors[ImGuiCol_TabHovered] = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
    baseColors[ImGuiCol_TabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
    baseColors[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    baseColors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    baseCaptured = true;
    applied = false;
}

void applyTheme(bool neon) {
    if (!baseCaptured) {
        return;
    }

    if (applied && lastNeon == neon) {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();

    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        style.Colors[i] = baseColors[i];
    }

    if (neon) {
        bool touched[ImGuiCol_COUNT] = {};

        auto set = [&](int index, ImVec4 color) {
            style.Colors[index] = color;
            touched[index] = true;
        };

        const ImVec4 cyan(0.20f, 1.0f, 0.95f, 1.0f);
        const ImVec4 magenta(1.0f, 0.10f, 0.80f, 1.0f);
        const ImVec4 green(0.22f, 1.0f, 0.08f, 1.0f);

        set(ImGuiCol_Text, cyan);
        set(ImGuiCol_WindowBg, ImVec4(0.03f, 0.0f, 0.08f, 1.0f));
        set(ImGuiCol_PopupBg, ImVec4(0.06f, 0.0f, 0.14f, 1.0f));
        set(ImGuiCol_Border, magenta);
        set(ImGuiCol_FrameBg, ImVec4(0.14f, 0.0f, 0.26f, 1.0f));
        set(ImGuiCol_FrameBgHovered, ImVec4(0.26f, 0.0f, 0.42f, 1.0f));
        set(ImGuiCol_FrameBgActive, ImVec4(0.40f, 0.0f, 0.60f, 1.0f));
        set(ImGuiCol_TitleBg, ImVec4(0.20f, 0.0f, 0.34f, 1.0f));
        set(ImGuiCol_TitleBgActive, ImVec4(0.55f, 0.0f, 0.70f, 1.0f));
        set(ImGuiCol_TitleBgCollapsed, ImVec4(0.12f, 0.0f, 0.22f, 1.0f));
        set(ImGuiCol_CheckMark, green);
        set(ImGuiCol_SliderGrab, magenta);
        set(ImGuiCol_SliderGrabActive, cyan);
        set(ImGuiCol_Button, ImVec4(0.35f, 0.0f, 0.55f, 1.0f));
        set(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.0f, 0.85f, 1.0f));
        set(ImGuiCol_ButtonActive, magenta);
        set(ImGuiCol_Header, ImVec4(0.45f, 0.0f, 0.65f, 0.7f));
        set(ImGuiCol_HeaderHovered, ImVec4(0.65f, 0.0f, 0.85f, 0.8f));
        set(ImGuiCol_HeaderActive, magenta);
        set(ImGuiCol_Separator, ImVec4(1.0f, 0.10f, 0.80f, 0.7f));
        set(ImGuiCol_ScrollbarBg, ImVec4(0.05f, 0.0f, 0.12f, 1.0f));
        set(ImGuiCol_ScrollbarGrab, ImVec4(0.55f, 0.0f, 0.70f, 1.0f));
        set(ImGuiCol_Tab, ImVec4(0.30f, 0.0f, 0.48f, 1.0f));
        set(ImGuiCol_TabHovered, ImVec4(0.65f, 0.0f, 0.85f, 1.0f));
        set(ImGuiCol_TabActive, ImVec4(0.85f, 0.0f, 0.75f, 1.0f));

        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            if (wasBlue[i] && !touched[i]) {
                style.Colors[i] = ImVec4(magenta.x, magenta.y, magenta.z, baseColors[i].w);
            }
        }
    }

    applied = true;
    lastNeon = neon;
}