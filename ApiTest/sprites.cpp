//--------------------------------------------------------------------------------------
// File: sprites.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "SpriteBatch.h"
#include "SpriteFont.h"

#include "CommonStates.h"
#include "DescriptorHeap.h"
#include "EffectPipelineStateDescription.h"
#include "RenderTargetState.h"
#include "ResourceUploadBatch.h"

#include <cstdio>
#include <iterator>
#include <type_traits>
#include <vector>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(!std::is_default_constructible<SpriteBatch>::value, "Default Ctor.");
static_assert(!std::is_copy_assignable<SpriteBatch>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<SpriteBatch>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SpriteBatch>::value, "Move Assign.");

static_assert(!std::is_default_constructible<SpriteBatchPipelineStateDescription>::value, "Default Ctor.");
static_assert(std::is_nothrow_copy_constructible<SpriteBatchPipelineStateDescription>::value, "Copy Ctor.");
static_assert(std::is_copy_assignable<SpriteBatchPipelineStateDescription>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<SpriteBatchPipelineStateDescription>::value, "Move Ctor.");
static_assert(std::is_move_assignable<SpriteBatchPipelineStateDescription>::value, "Move Assign.");

static_assert(!std::is_default_constructible<SpriteFont>::value, "Default Ctor.");
static_assert(!std::is_copy_assignable<SpriteFont>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<SpriteFont>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SpriteFont>::value, "Move Assign.");

// SpriteBatch
_Success_(return)
bool Test08(_In_ ID3D12Device *device)
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

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    const RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    bool success = true;

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
        success = false;
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
        success = false;
    }

    // invalid args
    try
    {
        ID3D12Device* nullDevice = nullptr;
        const SpriteBatchPipelineStateDescription pd(
            rtState,
            &CommonStates::NonPremultiplied);

        auto invalid = std::make_unique<SpriteBatch>(nullDevice, resourceUpload, pd);

        printf("ERROR: Failed to throw on null device\n");
        success = false;
    }
    catch(const std::exception&)
    {
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

// SpriteFont
_Success_(return)
bool Test09(_In_ ID3D12Device *device)
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
    fonts.resize(std::size(s_fonts));

    bool success = true;

    for(size_t j = 0; j < std::size(s_fonts); j++)
    {
        try
        {
            auto font = std::make_unique<SpriteFont>(device, resourceUpload,
                s_fonts[j],
                resourceDescriptors->GetCpuHandle(j), resourceDescriptors->GetGpuHandle(j));
            fonts[j] = std::move(font);
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating %ls object (except: %s)\n", s_fonts[j], e.what());
            success = false;
        }
    }

    // invalid args
    try
    {
        ID3D12Device* nullDevice = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor = {};

        auto invalid = std::make_unique<SpriteFont>(nullDevice, resourceUpload, nullptr, cpuDescriptor, gpuDescriptor);

        printf("ERROR: Failed to throw on null device\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor = {};

        auto invalid = std::make_unique<SpriteFont>(device, resourceUpload, nullptr, cpuDescriptor, gpuDescriptor);

        printf("ERROR: Failed to throw on null filename\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        ID3D12Device* nullDevice = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor = {};
        auto invalid = std::make_unique<SpriteFont>(nullDevice, resourceUpload, nullptr, 0, cpuDescriptor, gpuDescriptor);

        printf("ERROR: Failed to throw on null device ctor 2\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor = {};
        auto invalid = std::make_unique<SpriteFont>(device, resourceUpload, nullptr, 0, cpuDescriptor, gpuDescriptor);

        printf("ERROR: Failed to throw on null memory ctor 2\n");
        success = false;
    }
    catch(const std::exception&)
    {
    }

    try
    {
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor = {};
        XMUINT2 size = {};
        auto invalid = std::make_unique<SpriteFont>(gpuDescriptor, size, nullptr, 0, 0.f);

        printf("ERROR: Failed to throw on null ctor 3\n");
        success = false;
    }
    catch(const std::exception&)
    {
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

    for(size_t j = 0; j < std::size(s_fonts); ++j)
    {
        if (!fonts[j])
            continue;

    #ifndef NO_WCHAR_T
        if (fonts[j]->GetDefaultCharacter() != 0)
        {
            printf("FAILED: GetDefaultCharacter for %ls\n", s_fonts[j]);
            success = false;
        }
    #endif

        if (fonts[j]->GetLineSpacing() == 0)
        {
            printf("FAILED: GetLineSpacing for %ls\n", s_fonts[j]);
            success = false;
        }

        if (fonts[j]->ContainsCharacter(637))
        {
            printf("FAILED: ContainsCharacter for %ls\n", s_fonts[j]);
            success = false;
        }

        const auto spriteSheet = fonts[j]->GetSpriteSheet();

        if (!spriteSheet.ptr)
        {
            printf("FAILED: GetSpriteSheet for %ls\n", s_fonts[j]);
            success = false;
        }

        const auto spriteSheetSize = fonts[j]->GetSpriteSheetSize();
        if (!spriteSheetSize.x || !spriteSheetSize.y)
        {
            printf("FAILED: GetSpriteSheetSize for %ls\n", s_fonts[j]);
            success = false;
        }
    }

    return success;
}
