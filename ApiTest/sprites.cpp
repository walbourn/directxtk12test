//--------------------------------------------------------------------------------------
// File: sprites.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#include "SpriteBatch.h"
#include "SpriteFont.h"

#include "CommonStates.h"
#include "DescriptorHeap.h"
#include "EffectPipelineStateDescription.h"
#include "ResourceUploadBatch.h"

#include <cstdio>
#include <iterator>
#include <type_traits>
#include <vector>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_move_constructible<ResourceUploadBatch>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ResourceUploadBatch>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SpriteBatch>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SpriteBatch>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SpriteBatchPipelineStateDescription>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SpriteBatchPipelineStateDescription>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SpriteFont>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SpriteFont>::value, "Move Assign.");

// SpriteBatch
bool Test07(ID3D12Device *device)
{
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

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    std::unique_ptr<SpriteBatch> batch;
    try
    {
        const SpriteBatchPipelineStateDescription pd(
            rtState,
            &CommonStates::NonPremultiplied);

        batch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object 1 (except: %s)\n", e.what());
        return false;
    }

    std::unique_ptr<SpriteBatch> batch2;
    try
    {
        const auto sampler = states->PointClamp();

        const SpriteBatchPipelineStateDescription pd(
            rtState,
            &CommonStates::NonPremultiplied,
            nullptr, nullptr, &sampler);

        batch2 = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object 2 (except: %s)\n", e.what());
        return false;
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

    return true;
}

// SpriteFont
bool Test08(ID3D12Device *device)
{
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

    static const wchar_t* s_fonts[] =
    {
        L"SpriteFontTest\\comic.spritefont",
        L"SpriteFontTest\\italic.spritefont",
        L"SpriteFontTest\\script.spritefont",
        L"SpriteFontTest\\nonproportional.spritefont",
        L"SpriteFontTest\\multicolored.spritefont",
        L"SpriteFontTest\\japanese.spritefont",
        L"SpriteFontTest\\xboxController.spritefont",
        L"SpriteFontTest\\xboxOneController.spritefont",
        L"SpriteFontTest\\consolas.spritefont",
    };

    std::unique_ptr<DescriptorHeap> resourceDescriptors;
    try
    {
        resourceDescriptors = std::make_unique<DescriptorHeap>(device, std::size(s_fonts));
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating descriptor object (except: %s)\n", e.what());
        return false;
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    std::vector<std::unique_ptr<SpriteFont>> fonts;

    for(size_t j = 0; j < std::size(s_fonts); j++)
    {
        try
        {
            auto font = std::make_unique<SpriteFont>(device, resourceUpload,
                s_fonts[j],
                resourceDescriptors->GetCpuHandle(j), resourceDescriptors->GetGpuHandle(j));
            fonts.emplace_back(std::move(font));
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating %ls object (except: %s)\n", s_fonts[j], e.what());
            return false;
        }
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

    return true;
}
