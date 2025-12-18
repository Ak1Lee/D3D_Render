#pragma once

#include "MathHelper.h"
#include <d3d12.h> 
// 常量缓冲区结构
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

struct MaterialConstants
{
    DirectX::XMFLOAT4 Albedo = {0.8f, 0.3f,0.3f,0.0f};    // 16 bytes
    float Roughness = 0.5;    // 4 bytes
    float Metallic = 0.1;     // 4 bytes
    float AO = 0;           // 4 bytes
    float Padding;      // 4 bytes (凑齐 16 bytes 对齐)
};

// 光照常量缓冲区结构
struct LightConstants
{
    DirectX::XMFLOAT3 LightDirection = { 1.f, 0.f, 0.f };
    float LightIntensity = 1.0f;

    DirectX::XMFLOAT3 LightColor = { 1.0f, 1.0f, 0.0f };
    float _Padding1;

    DirectX::XMFLOAT3 AmbientColor = { 0.1f, 0.1f, 0.1f };
    float _Padding2;

    DirectX::XMFLOAT3 CameraPosition = { 0.0f, 0.0f, 0.0f };
    float _Padding3;
};

// 顶点结构
struct SimpleVertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

struct StandardVertex
{
	
    // 基础属性
    DirectX::XMFLOAT3 Position;     // 偏移: 0,  大小: 12字节
	DirectX::XMFLOAT3 Normal;       // 偏移: 12, 大小: 12字节
	DirectX::XMFLOAT2 TexCoord;     // 偏移：24, 大小: 8字节
	DirectX::XMFLOAT3 Tangent;      // 偏移：32, 大小: 12字节
	DirectX::XMFLOAT4 Color;        // 偏移：44, 大小: 16字节

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
