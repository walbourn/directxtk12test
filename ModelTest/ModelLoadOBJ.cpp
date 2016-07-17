//--------------------------------------------------------------------------------------
// File: ModelTestOBJ.cpp
//
// Code for loading a Model from a WaveFront OBJ file
//
// http://en.wikipedia.org/wiki/Wavefront_.obj_file
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"

#include <map>

#if 0
#include <algorithm>
#include <fstream>
#include <vector>
#include <unordered_map>

#include "PlatformHelpers.h"
#endif

#include "WaveFrontReader.h"

using namespace DirectX;

static_assert(sizeof(VertexPositionNormalTexture) == sizeof(WaveFrontReader<uint16_t>::Vertex), "vertex size mismatch");

namespace
{
    //--------------------------------------------------------------------------------------
    // Shared VB input element description
    INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;
    std::shared_ptr<std::vector<D3D12_INPUT_ELEMENT_DESC>> g_vbdecl;

    BOOL CALLBACK InitializeDecl(PINIT_ONCE initOnce, PVOID Parameter, PVOID *lpContext)
    {
        UNREFERENCED_PARAMETER(initOnce);
        UNREFERENCED_PARAMETER(Parameter);
        UNREFERENCED_PARAMETER(lpContext);
        g_vbdecl = std::make_shared<std::vector<D3D12_INPUT_ELEMENT_DESC>>(
            VertexPositionNormalTexture::InputLayout.pInputElementDescs,
            VertexPositionNormalTexture::InputLayout.pInputElementDescs + VertexPositionNormalTexture::InputLayout.NumElements);
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
std::unique_ptr<Model> CreateModelFromOBJ(_In_z_ const wchar_t* szFileName)
{
    if (!InitOnceExecuteOnce(&g_InitOnce, InitializeDecl, nullptr, nullptr))
        throw std::exception("One-time initialization failed");

    auto obj = std::make_unique<WaveFrontReader<uint16_t>>();

    if ( FAILED( obj->Load( szFileName ) ) )
    {
        throw std::exception("Failed loading WaveFront file");
    }

    if ( obj->vertices.empty() || obj->indices.empty() || obj->attributes.empty() || obj->materials.empty() )
    {
        throw std::exception("Missing data in WaveFront file");
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

        for (auto it = faces.cbegin(); it != faces.cend(); ++it)
        {
            obj->attributes.push_back(it->attribute);
            obj->indices.push_back(it->a);
            obj->indices.push_back(it->b);
            obj->indices.push_back(it->c);
        }
    }

    // Create mesh
    auto mesh = std::make_shared<ModelMesh>();
    mesh->name = szFileName;

    BoundingSphere::CreateFromPoints(mesh->boundingSphere, obj->vertices.size(), &obj->vertices[0].position, sizeof(VertexPositionNormalTexture));
    BoundingBox::CreateFromPoints(mesh->boundingBox, obj->vertices.size(), &obj->vertices[0].position, sizeof(VertexPositionNormalTexture));

    // Create vertex & index buffer
    size_t vertSize = sizeof(VertexPositionNormalTexture) * obj->vertices.size();
    SharedGraphicsResource vb = GraphicsMemory::Get().Allocate(vertSize);
    memcpy(vb.Memory(), obj->vertices.data(), vertSize);

    size_t indexSize = sizeof(uint16_t) * obj->indices.size();
    SharedGraphicsResource ib = GraphicsMemory::Get().Allocate(indexSize);
    memcpy(ib.Memory(), obj->indices.data(), indexSize);

    // Create a subset for each attribute/material
    std::vector<Model::ModelMaterialInfo> materials;
    
    std::map<std::wstring, int> textureDictionary;

    uint32_t curmaterial = static_cast<uint32_t>(-1);

    size_t index = 0;
    size_t sindex = 0;
    size_t nindices = 0;
    uint32_t matIndex = 0;
    bool isAlpha = false;
    for (auto it = obj->attributes.cbegin(); it != obj->attributes.cend(); ++it)
    {
        if (*it != curmaterial)
        {
            auto mat = obj->materials[*it];

            isAlpha = (mat.fAlpha < 1.f) ? true : false;

            Model::ModelMaterialInfo info;
            info.name = mat.strName;
            info.alphaValue = mat.fAlpha;
            info.ambientColor = mat.vAmbient;
            info.diffuseColor = mat.vDiffuse;

            if (mat.bSpecular)
            {
                info.specularPower = static_cast<float>(mat.nShininess);
                info.specularColor = mat.vSpecular;
            }

            info.diffuseTextureIndex = GetUniqueTextureIndex(mat.strTexture, textureDictionary);
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
            auto part = new ModelMeshPart;

            part->indexCount = static_cast<uint32_t>(nindices);
            part->startIndex = static_cast<uint32_t>(sindex);
            part->vertexStride = static_cast<uint32_t>(sizeof(VertexPositionNormalTexture));

            part->indexBuffer = ib;
            part->vertexBuffer = vb;
            part->materialIndex = matIndex;
            part->vbDecl = g_vbdecl;

            if (isAlpha)
            {
                mesh->alphaMeshParts.emplace_back(part);
            }
            else
            {
                mesh->opaqueMeshParts.emplace_back(part);
            }

            nindices = 0;
            sindex = index + 3;
        }

        index += 3;
    }

    // Create model
    std::unique_ptr<Model> model(new Model());
    model->name = szFileName;
    model->meshes.emplace_back(mesh);
    model->materials = std::move(materials);
    model->textureNames.resize(textureDictionary.size());
    for (auto texture = std::cbegin(textureDictionary); texture != std::cend(textureDictionary); ++texture)
    {
        model->textureNames[texture->second] = texture->first;
    }

    return model;
}
