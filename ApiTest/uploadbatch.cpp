//--------------------------------------------------------------------------------------
// File: uploadbatch.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "ResourceUploadBatch.h"
#include "GraphicsMemory.h"

#include <cstdio>
#include <type_traits>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(!std::is_copy_constructible<ResourceUploadBatch>::value, "Copy Ctor.");
static_assert(!std::is_copy_assignable<ResourceUploadBatch>::value, "Copy Assign.");
static_assert(std::is_nothrow_move_constructible<ResourceUploadBatch>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<ResourceUploadBatch>::value, "Move Assign.");

_Success_(return)
bool Test21(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> copyQueue;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(copyQueue.GetAddressOf()));
    if (FAILED(hr))
    {
        printf("ERROR: Failed creating copy command queue (%08X)\n", static_cast<unsigned int>(hr));
        return false;
    }

    bool success = true;

    std::unique_ptr<ResourceUploadBatch> uploadBatch;
    try
    {
        uploadBatch = std::make_unique<ResourceUploadBatch>(device);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Ctor failed (except: %s)\n", e.what());
        success = false;
    }

    try
    {
        uploadBatch->Begin(queueDesc.Type);

        uploadBatch->End(copyQueue.Get());
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Begin/End failed (except: %s)\n", e.what());
        success = false;
    }

    // invalid args
    #pragma warning(push)
    #pragma warning(disable:6385 6387)
    {
        try
        {
            uploadBatch->Begin(D3D12_COMMAND_LIST_TYPE_NONE);

            printf("ERROR: Failed to throw for unknown command list type\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            uploadBatch->Upload(nullptr, 0, nullptr, 0);

            printf("ERROR: Failed to throw for not in Begin/End pair\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            const SharedGraphicsResource buffer;
            uploadBatch->Upload(nullptr, buffer);

            printf("ERROR: Failed to throw for not in Begin/End pair (2)\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            uploadBatch->GenerateMips(nullptr);

            printf("ERROR: Failed to throw for not in Begin/End pair (3)\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            uploadBatch->Transition(nullptr, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);

            printf("ERROR: Failed to throw for not in Begin/End pair (4)\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            uploadBatch->Begin(queueDesc.Type);

            try
            {
                uploadBatch->Upload(nullptr, 0, nullptr, 0);

                printf("ERROR: Failed to throw for null resource\n");
                success = false;
            }
            catch(const std::exception&)
            {
            }

            try
            {
                const SharedGraphicsResource buffer;
                uploadBatch->Upload(nullptr, buffer);

                printf("ERROR: Failed to throw for null resource (2)\n");
                success = false;
            }
            catch(const std::exception&)
            {
            }

            try
            {
                uploadBatch->GenerateMips(nullptr);

                printf("ERROR: Failed to throw for null resource (3)\n");
                success = false;
            }
            catch(const std::exception&)
            {
            }

            try
            {
                uploadBatch->Transition(nullptr, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);

                printf("ERROR: Failed to throw for null resource (4)\n");
                success = false;
            }
            catch(const std::exception&)
            {
            }

            uploadBatch->End(copyQueue.Get());
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Begin/End failed (except: %s)\n", e.what());
            success = false;
        }

        try
        {
            uploadBatch->Begin(queueDesc.Type);
            uploadBatch->Begin(queueDesc.Type);

            printf("ERROR: Failed to throw for double Begin\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        ID3D12Device* nullDevice = nullptr;
        try
        {
            auto invalid = std::make_unique<ResourceUploadBatch>(nullDevice);

            printf("ERROR: Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }
    }
    #pragma warning(pop)

    return success;
}
