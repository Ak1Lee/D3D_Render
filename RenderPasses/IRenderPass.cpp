#include "IRenderPass.h"
#include "../Texture.h"


void PassManager::ExecuteAllPasses(ID3D12GraphicsCommandList* CmdList)
{
	for (auto& pass : m_Passes)
	{
		for (auto& dependence : pass->m_Dependencies)
		{
			dependence.Tex->TransitionTo(CmdList, dependence.NeededState);
		}
		if (pass->m_RootSig)
		{
			if (pass->m_IsCompute)
				CmdList->SetComputeRootSignature(pass->m_RootSig.Get());
			else
				CmdList->SetGraphicsRootSignature(pass->m_RootSig.Get());
		}
		if (pass->m_PSO)
		{
			CmdList->SetPipelineState(pass->m_PSO.Get());
		}
		if (pass->Execute)
		{
			pass->Execute(CmdList);
		}

	}
}
