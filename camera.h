#pragma once

#include "DirectXMath.h"
#include "MathHelper.h"
#include "Common.h"

struct CamPosition
{
	float x;
	float y;
	float z;
};

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

	void SetPosition(float x, float y, float z);
	CamPosition GetPosition() const { return CamPosition{ Position.x, Position.y,  Position.z }; }

	void UpdateForwardVector();
	void ProcessKeyboard(char key, float dt);
	void ProcessMouse(float dx, float dy);

	void Tick();
	DirectX::XMMATRIX CalViewProjMatrix();

	LRESULT CALLBACK CameraWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void MarkDirty() { bMarkedAsDirty = true; }
	bool IsDirty() const { return bMarkedAsDirty; }
	void ClearDirtyFlag() { bMarkedAsDirty = false; }


private:
	DirectX::XMFLOAT3 Forward = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
	//DirectX::XMFLOAT3 Position		= DirectX::XMFLOAT3(4.f, 4.f, -8.0f);
	DirectX::XMFLOAT3 Position =  DirectX::XMFLOAT3(0.f, 0.f, -8.0f);
	DirectX::XMFLOAT3 Target	= DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT3 Up		= DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
	DirectX::XMFLOAT3 Right		= DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);


	float Yaw = 1.57f;     // ×óÓÒ½Ç
	float Pitch = 0.0f;   // ÉÏÏÂ½Ç

	float MoveSpeed = 10.0f;


	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();

	float Width = 800.0f;
	float Height = 600.0f;

	bool bMarkedAsDirty = false;

};