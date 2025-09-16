#include "camera.h"
#include <algorithm>
#include <iostream>


using namespace DirectX;

void Camera::Init(float InWidth, float InHeight)
{
    Width = InWidth;
	Height = InHeight;

    auto f = (XMLoadFloat3(&Target) - XMLoadFloat3(&Position));

	f = XMVector3Normalize(f);

	XMStoreFloat3(&Forward, f);

}

void Camera::SetPosition(float x, float y, float z)
{
    Position = DirectX::XMFLOAT3(x, y, z);
}

void Camera::UpdateForwardVector()
{
    XMVECTOR f = XMVectorSet(
        cosf(Pitch) * cosf(Yaw),
        sinf(Pitch),
        cosf(Pitch) * sinf(Yaw),
        0.0f
    );
	f = XMVector3Normalize(f);
    XMStoreFloat3(&Forward, f);

	// std::cout << "Camera Forward: (" << Forward.x << ", " << Forward.y << ", " << Forward.z << ")\n";
 
    // right
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR r = XMVector3Cross(up, f);
    r = XMVector3Normalize(r);
    XMStoreFloat3(&Right, r);
	//// up
    XMVECTOR u = XMVector3Cross(f, r);
    XMStoreFloat3(&Up, u);

}

void Camera::ProcessKeyboard(char key, float dt)
{
    XMVECTOR pos = XMLoadFloat3(&Position);
    XMVECTOR f = XMLoadFloat3(&Forward);
    XMVECTOR r = XMLoadFloat3(&Right);
	XMVECTOR u = XMLoadFloat3(&Up);

    if (key == 'W')
    {
        pos += f * MoveSpeed * dt;
    }
    else if (key == 'S')
    {
        pos -= f * MoveSpeed * dt;
    }
    else if (key == 'A')
    {
        pos -= r * MoveSpeed * dt;
    }
    else if (key == 'D')
    {
        pos += r * MoveSpeed * dt;
    }
    else if (key == 'Q')
    {
        pos += u * MoveSpeed * dt;
    }
    else if (key == 'E')
    {
        pos -= u * MoveSpeed * dt;
	}
    //pos += r * MoveSpeed * dt;

    XMStoreFloat3(&Position, pos);
    MarkDirty();

	//std::cout << "Camera Position: (" << Position.x << ", " << Position.y << ", " << Position.z << ")\n";
}

void Camera::ProcessMouse(float dx, float dy)
{
    float sensitivity = 0.002f; // 鼠标灵敏度
    Yaw += dx * sensitivity;
    Pitch += dy * sensitivity;
	std::cout << "Camera Yaw: " << Yaw << ", Pitch: " << Pitch << std::endl;

    // 限制 pitch [-89,89] 避免翻转
    if (Pitch > XM_PIDIV2 - 0.1f) Pitch = XM_PIDIV2 - 0.1f;
    if (Pitch < -XM_PIDIV2 + 0.1f) Pitch = -XM_PIDIV2 + 0.1f;

    UpdateForwardVector();
    MarkDirty();
}

void Camera::Tick()
{
    if (!IsDirty())
    {
		return;
    }
	CalViewProjMatrix();
	ClearDirtyFlag();
}

DirectX::XMMATRIX Camera::CalViewProjMatrix()
{

    DirectX::XMVECTOR PosVec = DirectX::XMVectorSet(Position.x, Position.y, Position.z,  1.0f);
    // DirectX::XMVECTOR TargetVec = DirectX::XMVectorSet(Target.x, Target.y, Target.z, 0.0);
	DirectX::XMVECTOR ForwardVec = DirectX::XMVectorSet(Forward.x, Forward.y, Forward.z, 0.0f);
    DirectX::XMVECTOR UpVec = DirectX::XMVectorSet(Up.x, Up.y, Up.z, 0.0f);

    // DirectX::XMMATRIX TempView = DirectX::XMMatrixLookAtLH(PosVec, TargetVec, UpVec);
    DirectX::XMMATRIX TempView = DirectX::XMMatrixLookAtLH(PosVec, PosVec + ForwardVec, UpVec);
	DirectX::XMStoreFloat4x4(&View, TempView);

    DirectX::XMMATRIX TempProj = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, GetAspectRatio(), 0.5f, 1000.0f);
    DirectX::XMStoreFloat4x4(&Proj, TempProj);

    DirectX::XMMATRIX ViewProj = TempView * TempProj;

    return ViewProj;
}

LRESULT Camera::CameraWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    switch (msg)
    {

    case WM_MOUSEMOVE:
    {
        bool leftDown = (wParam & MK_LBUTTON);
        if (leftDown)
        {
            static POINT LastMousePos = { 0,0 };
            POINT CurrentMousePos;
            GetCursorPos(&CurrentMousePos);
            ScreenToClient(hwnd, &CurrentMousePos);

            float dx = CurrentMousePos.x - LastMousePos.x;
            float dy = CurrentMousePos.y - LastMousePos.y;
			dx = std::clamp(dx, -10.0f, 10.0f);
			dy = std::clamp(dy, -10.0f, 10.0f);
            
            const float sensitivity = 0.005f;
            ProcessMouse(dx, dy);

            LastMousePos = CurrentMousePos;
			std::cout << "Mouse Delta: (" << dx << ", " << dy << ")\n";

			std::cout << "Mouse Pos: (" << LastMousePos.x << ", " << LastMousePos.y << ")\n";
        }

    }
    case WM_KEYDOWN:
    {
        char key = static_cast<char>(wParam);
        float dt = 0.016f; // 假设每帧16ms
        ProcessKeyboard(key, dt);
    }
	break;
    default:
        break;
    }

    return LRESULT();
}
