#pragma once

#include "d3dx12.h"
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <windows.h>
#include <wrl.h>
#include <unordered_map>

struct PSO
{
	Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState;
};
class DXPSOManager
{
public:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreatePSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
private:
	std::unordered_map<size_t, PSO> PSOCache;
};