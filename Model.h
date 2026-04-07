#pragma once

#include "Geometry.h"
#include <string>
#include <vector>

struct aiNode;
struct aiScene;
struct aiMesh;

struct SubMesh
{
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseVertexLocation = 0;
    std::string MaterialName;
};

class Model : public MeshBase
{
public:
    Model() { Name = L"Model"; }

    bool LoadFromFile(const std::string& filepath,
                      ID3D12Device* Device,
                      ID3D12GraphicsCommandList* CommandList);

    void InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;

    const std::vector<SubMesh>& GetSubMeshes() const { return SubMeshes; }

private:
    void ProcessNode(aiNode* node, const aiScene* scene,
                     ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList);
    void ProcessMesh(aiMesh* mesh, const aiScene* scene,
                     ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList);

    std::vector<SubMesh> SubMeshes;
    std::string Directory;

    std::vector<std::uint32_t> IndiceList32;
};
