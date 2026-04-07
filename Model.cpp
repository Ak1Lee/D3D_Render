#include "Model.h"
#include "DXMaterial.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

bool Model::LoadFromFile(const std::string& filepath,
                         ID3D12Device* Device,
                         ID3D12GraphicsCommandList* CommandList)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        OutputDebugStringA("Assimp Error: ");
        OutputDebugStringA(importer.GetErrorString());
        OutputDebugStringA("\n");
        return false;
    }

    Directory = filepath.substr(0, filepath.find_last_of("\\/") + 1);

    VertexList.clear();
    IndiceList32.clear();
    SubMeshes.clear();

    ProcessNode(scene->mRootNode, scene, Device, CommandList);

    InitVertexBufferAndIndexBuffer(Device, CommandList);

    return true;
}

void Model::ProcessNode(aiNode* node, const aiScene* scene,
                        ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, Device, CommandList);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNode(node->mChildren[i], scene, Device, CommandList);
    }
}

void Model::ProcessMesh(aiMesh* mesh, const aiScene* scene,
                        ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
    SubMesh sub;
    sub.StartIndexLocation = static_cast<UINT>(IndiceList32.size());
    sub.BaseVertexLocation = static_cast<INT>(VertexList.size());

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        StandardVertex v;

        v.Position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        };

        if (mesh->HasNormals())
        {
            v.Normal = {
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            };
        }
        else
        {
            v.Normal = { 0.0f, 1.0f, 0.0f };
        }

        if (mesh->mTextureCoords[0])
        {
            v.TexCoord = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            };
        }
        else
        {
            v.TexCoord = { 0.0f, 0.0f };
        }

        if (mesh->HasTangentsAndBitangents())
        {
            v.Tangent = {
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z
            };
        }
        else
        {
            v.Tangent = { 1.0f, 0.0f, 0.0f };
        }

        v.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

        VertexList.push_back(v);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        {
            IndiceList32.push_back(face.mIndices[j] + sub.BaseVertexLocation);
        }
    }

    sub.IndexCount = static_cast<UINT>(IndiceList32.size()) - sub.StartIndexLocation;

    if (mesh->mMaterialIndex < scene->mNumMaterials)
    {
        aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
        aiString name;
        aiMat->Get(AI_MATKEY_NAME, name);
        sub.MaterialName = name.C_Str();

        if (!MaterialManager::GetInstance().GetMaterialByName(sub.MaterialName))
        {
            auto mat = std::make_shared<Material>(sub.MaterialName);

            aiString texPath;
            // Albedo
            if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                std::string fullPath = Directory + texPath.C_Str();
                auto tex = std::make_shared<Texture>(sub.MaterialName + "_Albedo");
                tex->LoadFromFile(Device, CommandList, fullPath, true);
                if (tex->GetResource())
                    mat->SetAlbedoTexture(tex);
            }
            // Normal
            aiString normalPath;
            if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &normalPath) == AI_SUCCESS ||
                aiMat->GetTexture(aiTextureType_HEIGHT, 0, &normalPath) == AI_SUCCESS)
            {
                std::string fullPath = Directory + normalPath.C_Str();
                auto tex = std::make_shared<Texture>(sub.MaterialName + "_Normal");
                tex->LoadFromFile(Device, CommandList, fullPath);
                if (tex->GetResource())
                    mat->SetNormalTexture(tex);
            }
            // Metallic / Specular
            if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS ||
                aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS)
            {
                std::string fullPath = Directory + texPath.C_Str();
                auto tex = std::make_shared<Texture>(sub.MaterialName + "_Metallic");
                tex->LoadFromFile(Device, CommandList, fullPath);
                if (tex->GetResource())
                    mat->SetMetallicTexture(tex);
            }

            MaterialManager::GetInstance().AddMaterial(mat);
        }
    }

    SubMeshes.push_back(sub);
}

void Model::InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
    VertexCount = static_cast<UINT>(VertexList.size());
    IndexCount = static_cast<UINT>(IndiceList32.size());

    UINT VBSize = VertexCount * sizeof(StandardVertex);
    UINT IBSize = IndexCount * sizeof(std::uint32_t);

    // Vertex Buffer
    ThrowIfFailed(D3DCreateBlob(VBSize, &VertexBufferCPU));
    memcpy(VertexBufferCPU->GetBufferPointer(), VertexList.data(), VBSize);

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(VBSize);

    Device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&VertexDefaultBufferGPU));
    Device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&VertexUploadBuffer));

    UINT8* vbData = nullptr;
    VertexUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vbData));
    memcpy(vbData, VertexList.data(), VBSize);
    VertexUploadBuffer->Unmap(0, nullptr);

    auto b1 = CD3DX12_RESOURCE_BARRIER::Transition(VertexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    CommandList->ResourceBarrier(1, &b1);
    CommandList->CopyBufferRegion(VertexDefaultBufferGPU.Get(), 0, VertexUploadBuffer.Get(), 0, VBSize);
    auto b2 = CD3DX12_RESOURCE_BARRIER::Transition(VertexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    CommandList->ResourceBarrier(1, &b2);

    VertexBufferView.BufferLocation = VertexDefaultBufferGPU->GetGPUVirtualAddress();
    VertexBufferView.StrideInBytes = sizeof(StandardVertex);
    VertexBufferView.SizeInBytes = VBSize;

    // Index Buffer (32-bit)
    CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(IBSize);

    Device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&IndexDefaultBufferGPU));
    Device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&IndexUploadBuffer));

    UINT8* ibData = nullptr;
    IndexUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&ibData));
    memcpy(ibData, IndiceList32.data(), IBSize);
    IndexUploadBuffer->Unmap(0, nullptr);

    auto b3 = CD3DX12_RESOURCE_BARRIER::Transition(IndexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    CommandList->ResourceBarrier(1, &b3);
    CommandList->CopyBufferRegion(IndexDefaultBufferGPU.Get(), 0, IndexUploadBuffer.Get(), 0, IBSize);
    auto b4 = CD3DX12_RESOURCE_BARRIER::Transition(IndexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    CommandList->ResourceBarrier(1, &b4);

    IndexBufferView.BufferLocation = IndexDefaultBufferGPU->GetGPUVirtualAddress();
    IndexBufferView.SizeInBytes = IBSize;
    IndexBufferView.Format = DXGI_FORMAT_R32_UINT;

    VertexDefaultBufferGPU->SetName(L"Model Vertex Buffer");
    IndexDefaultBufferGPU->SetName(L"Model Index Buffer");
}
