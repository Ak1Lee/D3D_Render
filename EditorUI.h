#pragma once
#include "imgui/imgui.h"

class DXRender;

class EditorUI
{
public:
    EditorUI(DXRender* InRenderer);
    ~EditorUI();

    void Draw();

private:
    void DrawPerformanceSection();
    void DrawCameraSection();
    void DrawLightSection();
    void DrawMaterialSection();
    void DrawGeometrySection();

    DXRender* Renderer;
};
