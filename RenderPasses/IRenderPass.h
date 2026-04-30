#pragma once 
#include <string>
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <functional>

class Texture;


struct PassDependency
{
	Texture* Tex;
	D3D12_RESOURCE_STATES NeededState;

};
class PassManager;

class IRenderPass
{
public:
	virtual ~IRenderPass() = default;

	IRenderPass()
	{

	}

	IRenderPass(std::string InName)
	{
		m_Name = InName;
	}


	virtual void Init(ID3D12Device* device) = 0;
	void SetRootSignature(ID3D12RootSignature* rootSig) { m_RootSig = rootSig; }
	void SetPipelineState(ID3D12PipelineState* pso) { m_PSO = pso; }
	IRenderPass& AddDependency(Texture* tex, D3D12_RESOURCE_STATES neededState)
	{
		if (tex)
		{
			m_Dependencies.push_back({ tex, neededState });

		}
		return *this;
	}
	std::function<void(ID3D12GraphicsCommandList*)> Execute;

protected:
	//D3D12_VIEWPORT m_Viewport;
	//D3D12_RECT m_ScissorRect;


	std::string m_Name;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSig;

	std::vector<PassDependency> m_Dependencies;

	
	friend class PassManager;
};

class PassManager
{
	public:

	template<typename T, typename... Args>
	T* AddPass(Args&&... args) {
		auto pass = std::make_unique<T>(std::forward<Args>(args)...);
		T* passPtr = pass.get();
		m_Passes.push_back(std::move(pass));
		return passPtr;
	}
	void InitAllPasses(ID3D12Device* device)
	{
		for (auto& pass : m_Passes)
		{
			pass->Init(device);
		}
	}
	void ExecuteAllPasses(ID3D12GraphicsCommandList* CmdList);

private:
	std::vector<std::unique_ptr<IRenderPass>> m_Passes;
};

