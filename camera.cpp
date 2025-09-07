#include "camera.h"
#include <algorithm>

void Camera::Init(float InWidth, float InHeight)
{
    Width = InWidth;
	Height = InHeight;
}

DirectX::XMMATRIX Camera::CalViewProjMatrix()
{

    DirectX::XMVECTOR PosVec = DirectX::XMVectorSet(Pos.x, Pos.y, Pos.z,  1.0f);
    DirectX::XMVECTOR TargetVec = DirectX::XMVectorSet(Target.x, Target.y, Target.z, 0.0);
    DirectX::XMVECTOR UpVec = DirectX::XMVectorSet(Up.x, Up.y, Up.z, 0.0f);

    DirectX::XMMATRIX TempView = DirectX::XMMatrixLookAtLH(PosVec, TargetVec, UpVec);
	DirectX::XMStoreFloat4x4(&View, TempView);

    DirectX::XMMATRIX TempProj = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, GetAspectRatio(), 0.5f, 1000.0f);
    DirectX::XMStoreFloat4x4(&Proj, TempProj);

    DirectX::XMMATRIX ViewProj = TempView * TempProj;

    return ViewProj;
}
