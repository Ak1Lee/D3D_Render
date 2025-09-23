#include "DXPSOManager.h"

Microsoft::WRL::ComPtr<ID3D12PipelineState> DXPSOManager::GetOrCreatePSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
    return Microsoft::WRL::ComPtr<ID3D12PipelineState>();
}
