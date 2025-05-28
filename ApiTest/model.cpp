//--------------------------------------------------------------------------------------
// File: model.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "Model.h"

#include "CommonStates.h"
#include "DescriptorHeap.h"
#include "EffectPipelineStateDescription.h"
#include "RenderTargetState.h"
#include "ResourceUploadBatch.h"

#include <cstdio>
#include <type_traits>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_move_constructible<Model>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<Model>::value, "Move Assign.");

// VS 2017 and the XDK isn't noexcept correct here
static_assert(std::is_move_constructible<ModelMesh>::value, "Move Ctor.");
static_assert(std::is_move_assignable<ModelMesh>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ModelMeshPart>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ModelMeshPart>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ModelBone>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ModelBone>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<ModelBone::TransformArray>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ModelBone::TransformArray>::value, "Move Assign.");

_Success_(return)
bool Test13(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

    ComPtr<ID3D12CommandQueue> copyQueue;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(copyQueue.GetAddressOf()));
    if (FAILED(hr))
    {
        printf("ERROR: Failed creating copy command queue (%08X)\n", static_cast<unsigned int>(hr));
        return false;
    }

    std::unique_ptr<CommonStates> states;
    try
    {
        states = std::make_unique<CommonStates>(device);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        return false;
    }

    std::unique_ptr<DescriptorPile> resourceDescriptors;
    try
    {
        resourceDescriptors = std::make_unique<DescriptorPile>(device, 128);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating descriptor object (except: %s)\n", e.what());
        return false;
    }

    // Create model
    bool success = true;

    std::unique_ptr<Model> vbo;
    try
    {
        vbo = Model::CreateFromVBO(device, L"ModelTest\\player_ship_a.vbo");
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating model from VBO (except: %s)\n", e.what());
        success = false;
    }

    std::unique_ptr<Model> cmo;
    try
    {
        cmo = Model::CreateFromCMO(device, L"ModelTest\\teapot.cmo");
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating model from CMO (except: %s)\n", e.what());
        success = false;
    }

    std::unique_ptr<Model> sdkmesh;
    try
    {
        sdkmesh = Model::CreateFromSDKMESH(device, L"ModelTest\\cup.sdkmesh");
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating model from SDKMESH (except: %s)\n", e.what());
        success = false;
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    // LoadStaticBuffers
    try
    {
        if (vbo)
            vbo->LoadStaticBuffers(device, resourceUpload);
        if (cmo)
            cmo->LoadStaticBuffers(device, resourceUpload);
        if (sdkmesh)
            sdkmesh->LoadStaticBuffers(device, resourceUpload);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed loading static buffers for model (except: %s)\n", e.what());
        success = false;
    }

    // CreateTextures/Effects
    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    const EffectPipelineStateDescription pd(
        nullptr,
        CommonStates::Opaque,
        CommonStates::DepthDefault,
        CommonStates::CullClockwise,
        rtState);

    std::unique_ptr<EffectFactory> fxFactory;
    std::unique_ptr<EffectTextureFactory> modelResources;
    DirectX::Model::EffectCollection fxCMO;
    DirectX::Model::EffectCollection fxSDKMESH;
    try
    {
        modelResources = std::make_unique<EffectTextureFactory>(device, resourceUpload, resourceDescriptors->Heap());
        modelResources->SetDirectory(L"ModelTest");

        fxFactory = std::make_unique<EffectFactory>(resourceDescriptors->Heap(), states->Heap());

        int txtOffset = 0;

        if (cmo)
        {
            fxCMO = cmo->CreateEffects(*fxFactory, pd, pd, txtOffset);
        }

        if (sdkmesh)
        {
            size_t start, end;
            resourceDescriptors->AllocateRange(sdkmesh->textureNames.size(), start, end);
            txtOffset = static_cast<int>(start);
            sdkmesh->LoadTextures(*modelResources, txtOffset);

            fxSDKMESH = sdkmesh->CreateEffects(*fxFactory, pd, pd, txtOffset);
        }
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating effects for model (except: %s)\n", e.what());
        success = false;
    }

    try
    {
        auto uploadResourcesFinished = resourceUpload.End(copyQueue.Get());
        uploadResourcesFinished.wait();
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed doing resource upload via copy command queue (except: %s)\n", e.what());
        return false;
    }

    return success;
}
