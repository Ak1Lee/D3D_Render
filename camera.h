#pragma once

#include "DirectXMath.h"
#include "MathHelper.h"


class Camera
{

	public:
	Camera() = default;
	Camera(float InWidth, float InHeight) : Width(InWidth), Height(InHeight) {};
	~Camera() = default;



	float GetWidth() const { return Width; }
	float GetHeight() const { return Height; }
	float GetAspectRatio() const { return Width / Height; }
	void Init(float InWidth, float InHeight);
	DirectX::XMMATRIX CalViewProjMatrix();


private:

	DirectX::XMFLOAT3 Pos = DirectX::XMFLOAT3(4.f, 4.f, -8.0f);
	DirectX::XMFLOAT3 Target = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT3 Up = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);


//	DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(Pos, Target, Up);
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();

	float Width = 800.0f;
	float Height = 600.0f;

};