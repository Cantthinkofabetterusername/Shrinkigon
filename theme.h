#ifndef THEME_H
#define THEME_H

#include "imgui.h"

struct Theme {
    ImVec4 windowBg;
    ImVec4 sidebarBg;
    ImVec4 text;
    ImVec4 infoBg;
    ImVec4 infoText;
    float skyTop[3];
    float skyBottom[3];
    float tileA[3];
    float tileB[3];
    float gridLine[3];
    float plateSide[3];
};

const Theme& getTheme(bool neon);
void captureBaseStyle();
void applyTheme(bool neon);

#endif
