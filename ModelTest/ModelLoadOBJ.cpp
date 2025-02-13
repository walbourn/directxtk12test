//--------------------------------------------------------------------------------------
// File: ModelTestOBJ.cpp
//
// Code for loading a Model from a WaveFront OBJ file
//
// http://en.wikipedia.org/wiki/Wavefront_.obj_file
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"

#include <map>

#include "WaveFrontReader.h"

using namespace DirectX;

static_assert(sizeof(VertexPositionNormalTexture) == sizeof(DX::WaveFrontReader<uint16_t>::Vertex), "vertex size mismatch");

namespace
{
    inline XMFLOAT3 GetMaterialColor(float r, float g, float b, bool srgb)
    {
        if (srgb)
        {
            XMVECTOR v = XMVectorSet(r, g, b, 1.f);
            v = XMColorSRGBToRGB(v);

            XMFLOAT3 result;
            XMStoreFloat3(&result, v);
            return result;
        }
        else
        {
            return XMFLOAT3(r, g, b);
        }
    }

    //--------------------------------------------------------------------------------------
    // Shared VB input element description
    INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;
    std::shared_ptr<ModelMeshPart::InputLayoutCollection> g_vbdecl;
    std::shared_ptr<ModelMeshPart::InputLayoutCollection> g_vbdeclInst;

    static const D3D12_INPUT_ELEMENT_DESC s_instElements[] =
    {
        // XMFLOAT3X4
        { "InstMatrix",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "InstMatrix",  2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    BOOL CALLBACK InitializeDecl(PINIT_ONCE initOnce, PVOID Parameter, PVOID *lpContext)
    {
        UNREFERENCED_PARAMETER(initOnce);
        UNREFERENCED_PARAMETER(Parameter);
        UNREFERENCED_PARAMETER(lpContext);
        g_vbdecl = std::make_shared<ModelMeshPart::InputLayoutCollection>(
            VertexPositionNormalTexture::InputLayout.pInputElementDescs,
            VertexPositionNormalTexture::InputLayout.pInputElementDescs + VertexPositionNormalTexture::InputLayout.NumElements);

        g_vbdeclInst = std::make_shared<ModelMeshPart::InputLayoutCollection>(
            VertexPositionNormalTexture::InputLayout.pInputElementDescs,
            VertexPositionNormalTexture::InputLayout.pInputElementDescs + VertexPositionNormalTexture::InputLayout.NumElements);
        g_vbdeclInst->push_back(s_instElements[0]);
        g_vbdeclInst->push_back(s_instElements[1]);
        g_vbdeclInst->push_back(s_instElements[2]);

        return TRUE;
    }

    int GetUniqueTextureIndex(const wchar_t* textureName, std::map<std::wstring, int>& textureDictionary)
    {
        if (textureName == nullptr || !textureName[0])
            return -1;

        auto i = textureDictionary.find(textureName);
        if (i == std::cend(textureDictionary))
        {
            int index = static_cast<int>(textureDictionary.size());
            textureDictionary[textureName] = index;
            return index;
        }
        else
        {
            return i->second;
        }
    }
}


//--------------------------------------------------------------------------------------
std::unique_ptr<Model> CreateModelFromOBJ(
    _In_opt_ ID3D12Device* device,
    _In_z_ const wchar_t* szFileName,
    bool enableInstacing,
    ModelLoaderFlags flags)
{
    if (!InitOnceExecuteOnce(&g_InitOnce, InitializeDecl, nullptr, nullptr))
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "InitOnceExecuteOnce");

    auto obj = std::make_unique<DX::WaveFrontReader<uint16_t>>();

    if ( FAILED( obj->Load( szFileName ) ) )
    {
        throw std::runtime_error("Failed loading WaveFront file");
    }

    if ( obj->vertices.empty() || obj->indices.empty() || obj->attributes.empty() || obj->materials.empty() )
    {
        throw std::runtime_error("Missing data in WaveFront file");
    }

    // Sort by attributes
    {
        struct Face
        {
            uint32_t attribute;
            uint16_t a;
            uint16_t b;
            uint16_t c;
        };

        std::vector<Face> faces;
        faces.reserve(obj->attributes.size());

        assert(obj->attributes.size() * 3 == obj->indices.size());

        for (size_t i = 0; i < obj->attributes.size(); ++i)
        {
            Face f;
            f.attribute = obj->attributes[i];
            f.a = obj->indices[i * 3];
            f.b = obj->indices[i * 3 + 1];
            f.c = obj->indices[i * 3 + 2];

            faces.push_back(f);
        }

        std::stable_sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) -> bool
        {
            return (a.attribute < b.attribute);
        });

        obj->attributes.clear();
        obj->indices.clear();

        for (const auto& it : faces)
        {
            obj->attributes.push_back(it.attribute);
            obj->indices.push_back(it.a);
            obj->indices.push_back(it.b);
            obj->indices.push_back(it.c);
        }
    }

    // Create mesh
    auto mesh = std::make_shared<ModelMesh>();
    mesh->name = szFileName;

    BoundingSphere::CreateFromPoints(mesh->boundingSphere, obj->vertices.size(), &obj->vertices[0].position, sizeof(VertexPositionNormalTexture));
    BoundingBox::CreateFromPoints(mesh->boundingBox, obj->vertices.size(), &obj->vertices[0].position, sizeof(VertexPositionNormalTexture));

    // Create vertex & index buffer
    size_t vertSize = sizeof(VertexPositionNormalTexture) * obj->vertices.size();
    SharedGraphicsResource vb = GraphicsMemory::Get(device).Allocate(vertSize);
    memcpy(vb.Memory(), obj->vertices.data(), vertSize);

    size_t indexSize = sizeof(uint16_t) * obj->indices.size();
    SharedGraphicsResource ib = GraphicsMemory::Get(device).Allocate(indexSize);
    memcpy(ib.Memory(), obj->indices.data(), indexSize);

    // Create a subset for each attribute/material
    std::vector<Model::ModelMaterialInfo> materials;
    
    std::map<std::wstring, int> textureDictionary;

    uint32_t curmaterial = static_cast<uint32_t>(-1);

    size_t index = 0;
    size_t sindex = 0;
    size_t nindices = 0;
    uint32_t matIndex = 0;
    uint32_t partIndex = 0;
    bool isAlpha = false;
    for (auto it = obj->attributes.cbegin(); it != obj->attributes.cend(); ++it)
    {
        if (*it != curmaterial)
        {
            auto& mat = obj->materials[*it];

            isAlpha = (mat.fAlpha < 1.f) ? true : false;

            Model::ModelMaterialInfo info;
            info.name = mat.strName;
            info.alphaValue = mat.fAlpha;
            info.ambientColor = GetMaterialColor(mat.vAmbient.x, mat.vAmbient.y, mat.vAmbient.z, (flags & ModelLoader_MaterialColorsSRGB) != 0);
            info.diffuseColor = GetMaterialColor(mat.vDiffuse.x, mat.vDiffuse.y, mat.vDiffuse.z, (flags & ModelLoader_MaterialColorsSRGB) != 0);

            info.diffuseTextureIndex = GetUniqueTextureIndex(mat.strTexture, textureDictionary);

            if (enableInstacing)
            {
                // Hack to make sure we use NormalMapEffect in order to test instancing.
                info.enableNormalMaps = true;

                if (info.diffuseTextureIndex == -1)
                {
                    info.diffuseTextureIndex = GetUniqueTextureIndex(L"default.dds", textureDictionary);
                    info.normalTextureIndex = GetUniqueTextureIndex(L"smoothMap.dds", textureDictionary);
                }
                else
                {
                    info.normalTextureIndex = GetUniqueTextureIndex(L"normalMap.dds", textureDictionary);
                }
            }

            if (mat.bSpecular)
            {
                info.specularPower = static_cast<float>(mat.nShininess);
                info.specularColor = mat.vSpecular;
            }

            if (info.diffuseTextureIndex != -1)
            {
                info.samplerIndex = static_cast<int>(CommonStates::SamplerIndex::AnisotropicWrap);
            }

            matIndex = static_cast<uint32_t>(materials.size());
            materials.push_back(info);

            curmaterial = *it;
        }

        nindices += 3;

        auto nit = it + 1;
        if (nit == obj->attributes.cend() || *nit != curmaterial)
        {
            auto part = std::make_unique<ModelMeshPart>(partIndex++);

            part->indexCount = static_cast<uint32_t>(nindices);
            part->startIndex = static_cast<uint32_t>(sindex);
            part->vertexStride = static_cast<uint32_t>(sizeof(VertexPositionNormalTexture));

            part->indexBufferSize = static_cast<uint32_t>(indexSize);
            part->indexBuffer = ib;
            part->vertexBufferSize = static_cast<uint32_t>(vertSize);
            part->vertexBuffer = vb;
            part->materialIndex = matIndex;
            part->vbDecl = (enableInstacing) ? g_vbdeclInst : g_vbdecl;

            if (isAlpha)
            {
                mesh->alphaMeshParts.emplace_back(std::move(part));
            }
            else
            {
                mesh->opaqueMeshParts.emplace_back(std::move(part));
            }

            nindices = 0;
            sindex = index + 3;
        }

        index += 3;
    }

    // Create model
    auto model = std::make_unique<Model>();
    model->name = szFileName;
    model->meshes.emplace_back(mesh);
    model->materials = std::move(materials);
    model->textureNames.resize(textureDictionary.size());
    for (auto texture = std::cbegin(textureDictionary); texture != std::cend(textureDictionary); ++texture)
    {
        model->textureNames[size_t(texture->second)] = texture->first;
    }

    return model;
}
