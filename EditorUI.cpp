#include "EditorUI.h"
#include "render.h"
#include "DXMaterial.h"
#include <string>

EditorUI::EditorUI(DXRender* InRenderer) : Renderer(InRenderer)
{
}

EditorUI::~EditorUI()
{
}

void EditorUI::Draw()
{
    ImGui::Begin("Settings");

    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawPerformanceSection();
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawCameraSection();
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawLightSection();
    }

    if (ImGui::CollapsingHeader("Geometry"))
    {
        DrawGeometrySection();
    }

    ImGui::End();
}

void EditorUI::DrawPerformanceSection()
{
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    bool& bEnableMultiThreadRecord = Renderer->GetMultiThreadRecordingFlag();
    ImGui::Checkbox("Multi-Thread Recording", &bEnableMultiThreadRecord);
    ImGui::Text("Record Time: %.3f ms (%s)",
        Renderer->GetLastRecordTimeMs(),
        bEnableMultiThreadRecord ? "MT" : "ST");
}

void EditorUI::DrawCameraSection()
{
    auto& MainCamera = Renderer->GetMainCamera();
    auto MainCameraPos = MainCamera.GetPosition();
    float MainCameraPosFloat3[3] = { MainCameraPos.x, MainCameraPos.y, MainCameraPos.z };
    if (ImGui::DragFloat3("Camera Position", MainCameraPosFloat3, 0.1f))
    {
        MainCamera.SetPosition(MainCameraPosFloat3[0], MainCameraPosFloat3[1], MainCameraPosFloat3[2]);
    }
}

void EditorUI::DrawLightSection()
{
    auto& lightData = Renderer->GetLightConstants();
    float lightDir[3] = { lightData.LightDirection.x, lightData.LightDirection.y, lightData.LightDirection.z };
    if (ImGui::DragFloat3("Light Dir", lightDir, 0.02f))
    {
        lightData.LightDirection = { lightDir[0], lightDir[1], lightDir[2] };
    }

    ImGui::SliderFloat("Light Size (PCSS)", &lightData.LightSize, 0.0f, 5.0f);

    // Legacy global roughness
    auto& matData = Renderer->GetMaterialConstants();
    ImGui::DragFloat("Global Roughness", &matData.Roughness, 0.02f, 0.05f, 1.0f);
}



void EditorUI::DrawGeometrySection()
{
    const auto& meshes = Renderer->GetMeshList();

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        MeshBase* mesh = meshes[i];
        
        // Convert wstring to string for ImGui
        std::wstring wname = mesh->GetName();
        std::string name(wname.length(), 0);
        for (size_t j = 0; j < wname.length(); ++j)
            name[j] = static_cast<char>(wname[j]);

        // Append index to name to avoid ImGui ID collision if names are identical
        std::string label = name + "##" + std::to_string(i);

        ImGui::PushID(i);
        if (ImGui::TreeNode(label.c_str()))
        {
            bool bVisible = mesh->IsVisible();
            if (ImGui::Checkbox("Visible", &bVisible))
            {
                mesh->SetVisible(bVisible);
            }

            auto pos = mesh->GetPosition();
            float posFloat[3] = { pos.x, pos.y, pos.z };
            if (ImGui::DragFloat3("Position", posFloat, 0.1f))
            {
                mesh->SetPosition(posFloat[0], posFloat[1], posFloat[2]);
            }

            auto angle = mesh->GetAngle();
            float angleFloat[3] = { angle.x, angle.y, angle.z };
            if (ImGui::DragFloat3("Rotation", angleFloat, 0.1f))
            {
                mesh->SetAngle(angleFloat[0], angleFloat[1], angleFloat[2]);
            }

            auto scale = mesh->GetScale();
            float scaleFloat[3] = { scale.x, scale.y, scale.z };
            if (ImGui::DragFloat3("Scale", scaleFloat, 0.1f))
            {
                mesh->SetScale(scaleFloat[0], scaleFloat[1], scaleFloat[2]);
            }

            // Material Section under Geometry
            std::string matName = mesh->GetMaterialName();
            if (!matName.empty())
            {
                Material* mat = MaterialManager::GetInstance().GetMaterialByName(matName);
                if (mat)
                {
                    if (ImGui::TreeNode(("Material: " + matName).c_str()))
                    {
                        MaterialConstants constData = mat->GetConstantData();
                        bool isModified = false;

                        float albedo[4] = { constData.Albedo.x, constData.Albedo.y, constData.Albedo.z, constData.Albedo.w };
                        if (ImGui::ColorEdit4("Albedo", albedo))
                        {
                            constData.Albedo = { albedo[0], albedo[1], albedo[2], albedo[3] };
                            isModified = true;
                        }

                        float roughness = constData.Roughness;
                        if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f))
                        {
                            constData.Roughness = roughness;
                            isModified = true;
                        }

                        float metallic = constData.Metallic;
                        if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f))
                        {
                            constData.Metallic = metallic;
                            isModified = true;
                        }

                        if (isModified)
                        {
                            mat->SetConstantData(constData);
                        }

                        ImGui::TreePop();
                    }
                }
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}
