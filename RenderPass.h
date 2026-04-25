#pragma once
#include <string>
#include <d3d12.h>
#include <wrl.h>
#include <functional>
#include "Texture.h"

struct PassDependency
{
    Texture* Tex;
    D3D12_RESOURCE_STATES NeededState;

};


struct RenderPass
{
    std::string Name;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSig;

    D3D12_VIEWPORT Viewport;
    D3D12_RECT ScissorRect;

	std::vector<PassDependency> Dependencies;
    RenderPass& AddDependency(Texture* tex, D3D12_RESOURCE_STATES neededState)
    {
        if (tex)
        {
            Dependencies.push_back({ tex, neededState });
            
        }
		return *this;

	}

    std::function<void(ID3D12GraphicsCommandList*)> Execute;
};

class PassManager
{
public:
    RenderPass& AddPass(const std::string& name)
    {
        Passes.emplace_back();
        Passes.back().Name = name;
        return Passes.back();
	}

    void ExecuteAllPasses(ID3D12GraphicsCommandList* CmdList)
    {
        for (auto& pass : Passes)
        {
            for (auto& dependence : pass.Dependencies)
            {
				dependence.Tex->TransitionTo(CmdList, dependence.NeededState);
            }

            if (pass.RootSig)
            {
				CmdList->SetGraphicsRootSignature(pass.RootSig.Get());
            }
            if (pass.PSO)
            {
				CmdList->SetPipelineState(pass.PSO.Get());
            }
            if (pass.Execute)
            {
                pass.Execute(CmdList);
            }
        }
    }

private:
	std::vector<RenderPass> Passes;
};