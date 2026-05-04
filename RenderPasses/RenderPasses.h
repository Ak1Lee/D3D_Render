#pragma once 
#include "IRenderPass.h"


class DeferredLightPass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};


class ZPrePass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class ShadowPass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class BasePass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class ShadowMaskPass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class SkyboxPass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class GI_VoxelBuildPass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class GI_Cascade0Pass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class GI_CascadePass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};

class GI_ViewPass : public IRenderPass
{
	void Init(ID3D12Device* device) override;
};