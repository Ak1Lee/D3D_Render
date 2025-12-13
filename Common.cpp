#include "Common.h"

// 修正 StandardVertex 构造函数参数类型，TexCoord 应为 XMFLOAT2，Color 应为 XMFLOAT4
StandardVertex::StandardVertex(
    const DirectX::XMFLOAT3& InPosition,
    const DirectX::XMFLOAT3& InNormal,
    const DirectX::XMFLOAT2& InTexCoord,
    const DirectX::XMFLOAT3& InTangent,
    const DirectX::XMFLOAT4& InColor)
    : Position(InPosition),
      Normal(InNormal),
      TexCoord(InTexCoord),
      Tangent(InTangent),
      Color(InColor)
{
}

StandardVertex::StandardVertex(const DirectX::XMFLOAT3& InPosition, const DirectX::XMFLOAT3& InNormal)
{
    Position = InPosition;
    Normal = InNormal;
    TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
    Tangent = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
}

StandardVertex::StandardVertex(const DirectX::XMFLOAT3& InPosition, const DirectX::XMFLOAT4& InColor)
    : Position(InPosition),
	Color(InColor)
{
    Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
    Tangent = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

}

StandardVertex::StandardVertex(const DirectX::XMFLOAT3& InPosition) : Position(InPosition)
{
    Normal = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
    Tangent = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
}

StandardVertex::StandardVertex(const DirectX::XMFLOAT3& InPosition, const DirectX::XMFLOAT3& InNormal, const DirectX::XMFLOAT4& InColor)
    : Position(InPosition),
      Normal(InNormal),
	  Color(InColor)
{
    TexCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
    Tangent = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

StandardVertex::StandardVertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v, float tx, float ty, float tz, float r, float g, float b, float a) 
    : Position(px, py, pz),
      Normal(nx, ny, nz),
      TexCoord(u, v),
      Tangent(tx, ty, tz),
      Color(r, g, b, a)
{
}
