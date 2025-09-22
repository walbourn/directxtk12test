//--------------------------------------------------------------------------------------
// File: graphicsmemory.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "GraphicsMemory.h"

#include <cstdio>
#include <type_traits>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_move_constructible<GraphicsResource>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GraphicsResource>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<SharedGraphicsResource>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<SharedGraphicsResource>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<GraphicsMemory>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GraphicsMemory>::value, "Move Assign.");

_Success_(return)
bool Test00(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    std::unique_ptr<DirectX::GraphicsMemory> graphicsMemory;
    bool caught = false;
    try
    {
        graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(nullptr);
    }
    catch(const std::exception&)
    {
        caught = true;
    }
    if (!caught)
    {
        printf("ERROR: Failed to catch null device pointer\n");
        return false;
    }

    try
    {
        graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating graphics memory object (except: %s)\n", e.what());
        return false;
    }

    if (!graphicsMemory || graphicsMemory->GetDevice() != device)
    {
        printf("ERROR: Failed to create valid graphics memory object\n");
        return false;
    }

    bool success = true;

    GraphicsResource res = {};
    try
    {
        res = graphicsMemory->Allocate(1024);

        if (!res
            || res.Size() < 1024
            || res.Memory() == nullptr
            || res.Resource() == nullptr
            || res.GpuAddress() == 0)
        {
            printf("ERROR: Failed to allocate graphics memory\n");
            success = false;
        }

        GraphicsResource res2 = std::move(res);
        if (res
            || !res2
            || res2.Size() < 1024
            || res2.Memory() == nullptr
            || res2.Resource() == nullptr
            || res2.GpuAddress() == 0)
        {
            printf("ERROR: Move ctor of GraphicsResource failed\n");
            success = false;
        }

        GraphicsResource res3;
        res3 = std::move(res2);
        if (res2
            || !res3
            || res3.Size() < 1024
            || res3.Memory() == nullptr
            || res3.Resource() == nullptr
            || res3.GpuAddress() == 0)
        {
            printf("ERROR: Move assign of GraphicsResource failed\n");
            success = false;
        }

        res3.Reset();
        if (res3)
        {
            printf("ERROR: Reset of GraphicsResource failed\n");
            success = false;
        }
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed to allocate graphics memory (except: %s)\n", e.what());
        success = false;
    }

    SharedGraphicsResource sharedRes;
    try
    {
        sharedRes = graphicsMemory->Allocate(1024);

        if (!sharedRes
            || sharedRes.Size() < 1024
            || sharedRes.Memory() == nullptr
            || sharedRes.Resource() == nullptr
            || sharedRes.GpuAddress() == 0)
        {
            printf("ERROR: Failed to allocate [shared] graphics memory\n");
            success = false;
        }

        SharedGraphicsResource sharedRes2 = std::move(sharedRes);
        if (sharedRes
            || !sharedRes2
            || sharedRes2.Size() < 1024
            || sharedRes2.Memory() == nullptr
            || sharedRes2.Resource() == nullptr
            || sharedRes2.GpuAddress() == 0)
        {
            printf("ERROR: Move ctor of SharedGraphicsResource failed\n");
            success = false;
        }

        SharedGraphicsResource sharedRes3 = sharedRes2;
        if (!sharedRes3
            || sharedRes3.Size() < 1024
            || sharedRes3.Memory() == nullptr
            || sharedRes3.Resource() == nullptr
            || sharedRes3.GpuAddress() == 0
            || !sharedRes2
            || sharedRes2.Size() != sharedRes3.Size()
            || sharedRes2.Memory() != sharedRes3.Memory()
            || sharedRes2.Resource() != sharedRes3.Resource()
            || sharedRes2.GpuAddress() != sharedRes3.GpuAddress())
        {
            printf("ERROR: Copy ctor of SharedGraphicsResource failed\n");
            success = false;
        }

        sharedRes2.Reset();
        if (sharedRes2)
        {
            printf("ERROR: Reset of SharedGraphicsResource failed\n");
            success = false;
        }

        SharedGraphicsResource sharedRes4 = std::move(sharedRes3);
        if(!sharedRes4
            || sharedRes3
            || sharedRes4.Size() < 1024
            || sharedRes4.Memory() == nullptr
            || sharedRes4.Resource() == nullptr
            || sharedRes4.GpuAddress() == 0)
        {
            printf("ERROR: Move ctor of SharedGraphicsResource failed\n");
            success = false;
        }

        GraphicsResource res3 = graphicsMemory->Allocate(1024 * 1024 * 1024);
        if (!res3
            || res3.Size() < 1024 * 1024 * 1024
            || res3.Memory() == nullptr
            || res3.Resource() == nullptr
            || res3.GpuAddress() == 0)
        {
            printf("ERROR: Failed to allocate large graphics memory\n");
            success = false;
        }

        SharedGraphicsResource sharedRes5;
        sharedRes5.Reset(std::move(res3));
        if (!sharedRes5
            || res3
            || sharedRes5.Size() < 1024 * 1024 * 1024
            || sharedRes5.Memory() == nullptr
            || sharedRes5.Resource() == nullptr
            || sharedRes5.GpuAddress() == 0)
        {
            printf("ERROR: Failed to reset to large graphics memory unique to shared\n");
            success = false;
        }

        SharedGraphicsResource sharedRes6;
        sharedRes6.Reset(sharedRes5);
        if (!sharedRes6
            || !sharedRes5
            || sharedRes6.Size() < 1024 * 1024 * 1024
            || sharedRes6.Memory() == nullptr
            || sharedRes6.Resource() == nullptr
            || sharedRes6.GpuAddress() == 0)
        {
            printf("ERROR: Failed to reset to large graphics memory shared to shared\n");
            success = false;
        }

        SharedGraphicsResource sharedRes7;
        sharedRes7 = std::move(sharedRes6);
        if (!sharedRes7
            || sharedRes6
            || sharedRes7.Size() < 1024 * 1024 * 1024
            || sharedRes7.Memory() == nullptr
            || sharedRes7.Resource() == nullptr
            || sharedRes7.GpuAddress() == 0)
        {
            printf("ERROR: Move ctor of SharedGraphicsResource failed\n");
            success = false;
        }

        SharedGraphicsResource sharedRes8;
        sharedRes8.Reset(std::move(sharedRes7));
        if (sharedRes7
            || !sharedRes8
            || sharedRes8.Size() < 1024 * 1024 * 1024
            || sharedRes8.Memory() == nullptr
            || sharedRes8.Resource() == nullptr
            || sharedRes8.GpuAddress() == 0)
        {
            printf("ERROR: Move shared reset of SharedGraphicsResource failed\n");
            success = false;
        }
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed to allocate [shared] graphics memory (except: %s)\n", e.what());
        success = false;
    }

    try
    {
        auto stats = graphicsMemory->GetStatistics();
        if (stats.totalMemory == 0 || stats.totalPages == 0)
        {
            printf("ERROR: GetStatistics of GraphicsMemory failed\n");
            success = false;
        }

        graphicsMemory->ResetStatistics();

        graphicsMemory->GarbageCollect();
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Method tests for GraphicsMemory failed (except: %s)\n", e.what());
        success = false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

        ComPtr<ID3D12CommandQueue> commandQueue;
        HRESULT hr = device->CreateCommandQueue(&queueDesc,
            IID_PPV_ARGS(commandQueue.GetAddressOf()));
        if (FAILED(hr))
        {
            printf("ERROR: Failed to create command queue (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }
        else
        {
            try
            {
                graphicsMemory->Commit(commandQueue.Get());
            }
            catch (const std::exception& e)
            {
                printf("ERROR: Commit of GraphicsMemory failed (except: %s)\n", e.what());
                success = false;
            }
        }
    }

    try
    {
        auto graphicsMemory2 = std::move(*graphicsMemory);
        if (graphicsMemory->GetDevice() != nullptr || graphicsMemory2.GetDevice() == nullptr)
        {
            printf("ERROR: Move ctor of GraphicsMemory failed\n");
            success = false;
        }
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Move ctor/assign of GraphicsMemory failed (except: %s)\n", e.what());
        success = false;
    }

    return success;
}
