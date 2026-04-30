#pragma once

#include "MathHelper.h"
#include <d3d12.h> 

struct DescriptorAllocation
{
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle;
    UINT Index = -1;
};
// 甯搁噺缂撳啿鍖虹粨鏋?
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct MaterialConstants
{
    DirectX::XMFLOAT4 Albedo = {0.8f, 0.8f,0.8f,0.0f};    // 16 bytes
    float Roughness = 0.5f;    // 4 bytes
    float Metallic = 0.1f;     // 4 bytes
    float AO = 0;           // 4 bytes
    float Padding;      // 4 bytes (鍑戦綈 16 bytes 瀵归綈)
};

// 鍏夌収甯搁噺缂撳啿鍖虹粨鏋?
struct LightConstants
{
    DirectX::XMFLOAT3 LightDirection = { 1.f, 1.f, 0.f };
    float LightIntensity = 1.0f;

    DirectX::XMFLOAT3 LightColor = { 1.0f, 1.0f, 1.0f };
    float LightSize = 1.0f; // light 鐗╃悊灏哄

    DirectX::XMFLOAT3 AmbientColor = { 0.1f, 0.1f, 0.1f };
    float _Padding2;

    DirectX::XMFLOAT3 CameraPosition = { 0.0f, 0.0f, 0.0f };
    float _Padding3;

	// Light View-Projection 鐭╅樀锛岀敤浜庨槾褰辨槧灏?
    DirectX::XMFLOAT4X4 LightViewProj = MathHelper::Identity4x4();

	// camera inverse view matrix
	DirectX::XMFLOAT4X4 CameraInvViewProj = MathHelper::Identity4x4();
};

// 椤剁偣缁撴瀯
struct SimpleVertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

struct StandardVertex
{
	
    // 鍩虹灞炴€?
    DirectX::XMFLOAT3 Position;     // 鍋忕Щ: 0,  澶у皬: 12瀛楄妭
	DirectX::XMFLOAT3 Normal;       // 鍋忕Щ: 12, 澶у皬: 12瀛楄妭
	DirectX::XMFLOAT2 TexCoord;     // 鍋忕Щ锛?4, 澶у皬: 8瀛楄妭
	DirectX::XMFLOAT3 Tangent;      // 鍋忕Щ锛?2, 澶у皬: 12瀛楄妭
	DirectX::XMFLOAT4 Color;        // 鍋忕Щ锛?4, 澶у皬: 16瀛楄妭

    StandardVertex() = default;

    StandardVertex(const DirectX::XMFLOAT3& InPosition,
        const DirectX::XMFLOAT3& InNormal,
        const DirectX::XMFLOAT2& InTexCoord,
        const DirectX::XMFLOAT3& InTangent,
        const DirectX::XMFLOAT4& InColor);

    StandardVertex(const DirectX::XMFLOAT3& InPosition,
        const DirectX::XMFLOAT3& InNormal);

    StandardVertex(const DirectX::XMFLOAT3& InPosition, const DirectX::XMFLOAT4& InColor);

    StandardVertex(const DirectX::XMFLOAT3& InPosition);

    StandardVertex(const DirectX::XMFLOAT3& InPosition,
        const DirectX::XMFLOAT3& InNormal,
        const DirectX::XMFLOAT4& InColor);

    StandardVertex(float px, float py, float pz,
        float nx, float ny, float nz,
        float u, float v,
        float tx = 1.0f, float ty = 1.0, float tz = 1.0,
        float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);



};

static const D3D12_INPUT_ELEMENT_DESC StandardVertexInputLayout[]=
{

  //{SemanticName,SemanticIndex,Format,InputSlot,AlignedByteOffset, InputSlotClass(Vertex/Instance),   InstanceDataStepRate}
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};
