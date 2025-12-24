#include "DXShader.h"
#include "dxUtils.h"

DXShader::DXShader(const std::wstring& shaderName, const std::wstring& filePath, const std::string& entryPoint, const std::string& target)
{
    FilePath = filePath;
    EntryPoint = entryPoint;
	Target = target;
	ShaderName = shaderName;
}

ID3DBlob* DXShader::GetBytecode()
{
    return ShaderByteCode.Get();
}

DXShader* DXShaderManager::CreateOrFindShader(const std::wstring& ShaderName, const std::wstring& FilePath, const std::string& EntryPoint, const std::string& Target)
{
	auto it = ShaderCache.find(ShaderName);
	if(it != ShaderCache.end())
	{
		return it->second.get();
	}
	else
	{
		std::shared_ptr<DXShader> ShaderPtr(new DXShader(ShaderName, FilePath, EntryPoint, Target));
		// auto ShaderPtr = std::make_shared<DXShader>(ShaderName, FilePath, EntryPoint, Target);
#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif


		Microsoft::WRL::ComPtr<ID3DBlob> errors = nullptr;
		HRESULT hr = D3DCompileFromFile(
			FilePath.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			EntryPoint.c_str(),
			Target.c_str(),
			compileFlags,
			0,
			(ShaderPtr.get()->GetBytecodeAddress()),    // 输出到这个临时变量
			&errors       // 错误信息输出到这里
		);

		if (FAILED(hr))
		{
			if (errors)
			{
				std::string errorMsg(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
				OutputDebugStringA(errorMsg.c_str());
			}
			ThrowIfFailed(hr);
		}

		//ThrowIfFailed(D3DCompileFromFile(FilePath.c_str(), nullptr, nullptr, EntryPoint.c_str(), Target.c_str(), compileFlags, 0, (ShaderPtr.get()->GetBytecodeAddress()), nullptr));
		ShaderCache[ShaderName] = ShaderPtr;
		return ShaderPtr.get();
	}
}
