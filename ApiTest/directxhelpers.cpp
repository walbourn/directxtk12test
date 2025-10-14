//--------------------------------------------------------------------------------------
// File: directxhelpers.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "DirectXHelpers.h"

#include "BufferHelpers.h"
#include "DescriptorHeap.h"
#include "ResourceUploadBatch.h"
#include "VertexTypes.h"

#include <cmath>
#include <cstdio>
#include <iterator>
#include <random>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    inline bool CheckIsPowerOf2(size_t x) noexcept
    {
        if (!x)
            return false;

        return (ceil(log2(x)) == float(log2(x)));
    }

    inline uint64_t CheckAlignUp(uint64_t size, size_t alignment) noexcept
    {
        return ((size + alignment - 1) / alignment) * alignment;
    }

    inline uint64_t CheckAlignDown(uint64_t size, size_t alignment) noexcept
    {
        return (size / alignment) * alignment;
    }
}

_Success_(return)
bool Test03(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    std::unique_ptr<DescriptorHeap> resourceDescriptors;
    try
    {
        resourceDescriptors = std::make_unique<DescriptorHeap>(device, 3);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating descriptor object (except: %s)\n", e.what());
        return false;
    }

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

    bool success = true;

    std::random_device rd;
    std::default_random_engine generator(rd());

    // IsPowerOf2
    {
        for (size_t j = 0; j < 0x20000; ++j)
        {
            if (IsPowerOf2(j) != CheckIsPowerOf2(j))
            {
                printf("ERROR: Failed IsPowerOf2 tests\n");
                success = false;
            }
        }
    }

    // AlignUp/Down - uint32_t
    if (AlignUp(2, 0) != 2 || AlignDown(2, 0) != 2)
    {
        printf("ERROR: Failed Align(x,0) tests\n");
        success = false;
    }

    {
        std::uniform_int_distribution<uint32_t> dist(1, UINT16_MAX);
        for (size_t j = 1; j < 0x20000; j <<= 1)
        {
            if (!IsPowerOf2(j))
            {
                printf("ERROR: Failed IsPowerOf2 Align(32)\n");
                success = false;
            }

            for (size_t k = 0; k < 20000; k++)
            {
                uint32_t value = dist(generator);
                uint32_t up = AlignUp(value, j);
                uint32_t down = AlignDown(value, j);
                auto upCheck = static_cast<uint32_t>(CheckAlignUp(value, j));
                auto downCheck = static_cast<uint32_t>(CheckAlignDown(value, j));

                if (!up)
                {
                    printf("ERROR: Failed AlignUp(32) tests\n");
                    success = false;
                }
                else if (!down && value > j)
                {
                    printf("ERROR: Failed AlignDown(32) tests\n");
                    success = false;
                }
                else if (up < down)
                {
                    printf("ERROR: Failed Align(32) tests\n");
                    success = false;
                }
                else if (value > up)
                {
                    printf("ERROR: Failed AlignUp(32) tests\n");
                    success = false;
                }
                else if (value < down)
                {
                    printf("ERROR: Failed AlignDown(32) tests\n");
                    success = false;
                }
                else if (up != upCheck)
                {
                    printf("ERROR: Failed AlignUp(32) tests\n");
                    success = false;
                }
                else if (down != downCheck)
                {
                    printf("ERROR: Failed AlignDown(32) tests\n");
                    success = false;
                }
            }
        }
    }

    // AlignUp/Down - uint64_t
    {
        std::uniform_int_distribution<uint64_t> dist(1, UINT32_MAX);
        for (size_t j = 1; j < 0x20000; j <<= 1)
        {
            if (!IsPowerOf2(j))
            {
                printf("ERROR: Failed IsPowerOf2 Align(64)\n");
                success = false;
            }

            for (size_t k = 0; k < 20000; k++)
            {
                uint64_t value = dist(generator);
                uint64_t up = AlignUp(value, j);
                uint64_t down = AlignDown(value, j);
                uint64_t upCheck = CheckAlignUp(value, j);
                uint64_t downCheck = CheckAlignDown(value, j);

                if (!up)
                {
                    printf("ERROR: Failed AlignUp(64) tests\n");
                    success = false;
                }
                else if (!down && value > j)
                {
                    printf("ERROR: Failed AlignDown(64) tests\n");
                    success = false;
                }
                else if (up < down)
                {
                    printf("ERROR: Failed Align(64) tests\n");
                    success = false;
                }
                else if (value > up)
                {
                    printf("ERROR: Failed AlignUp(64) tests\n");
                    success = false;
                }
                else if (value < down)
                {
                    printf("ERROR: Failed AlignDown(64) tests\n");
                    success = false;
                }
                else if (up != upCheck)
                {
                    printf("ERROR: Failed AlignUp(64) tests\n");
                    success = false;
                }
                else if (down != downCheck)
                {
                    printf("ERROR: Failed AlignDown(64) tests\n");
                    success = false;
                }
            }
        }
    }

    ComPtr<ID3D12Resource> test1;
    ComPtr<ID3D12Resource> test2;
    ComPtr<ID3D12Resource> test3;

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    // CreateBufferShaderResourceView
    {
        static const VertexPositionColor s_vertexData[3] =
        {
            { XMFLOAT3{ 0.0f,   0.5f,  0.5f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },  // Top / Red
            { XMFLOAT3{ 0.5f,  -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },  // Right / Green
            { XMFLOAT3{ -0.5f, -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }   // Left / Blue
        };

        hr = CreateUploadBuffer(device,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            test1.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateUploadBuffer(1) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        try
        {
            CreateBufferShaderResourceView(device, test1.Get(),
                resourceDescriptors->GetFirstCpuHandle(),
                sizeof(float));
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating srv object 1 (except: %s)\n", e.what());
            success = false;
        }
    }

    static const VertexPositionColor s_vertexData[3] =
    {
        { XMFLOAT3{ 0.0f,   0.5f,  0.5f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },  // Top / Red
        { XMFLOAT3{ 0.5f,  -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },  // Right / Green
        { XMFLOAT3{ -0.5f, -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }   // Left / Blue
    };

    {
        hr = CreateStaticBuffer(device,
            resourceUpload,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test2.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateStaticBuffer(1) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        try
        {
            CreateBufferShaderResourceView(device, test2.Get(),
                resourceDescriptors->GetCpuHandle(1),
                sizeof(float));
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating srv object 2 (except: %s)\n", e.what());
            success = false;
        }
    }

    // CreateBufferUnorderedAccessView
    {
        // UAV
        hr = CreateStaticBuffer(device, resourceUpload,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test3.ReleaseAndGetAddressOf(),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateStaticBuffer(UAV) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        try
        {
            CreateBufferUnorderedAccessView(device, test3.Get(),
                resourceDescriptors->GetCpuHandle(2),
                sizeof(VertexPositionColor));
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed creating uav object (except: %s)\n", e.what());
            success = false;
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

    // invalid args
    {
        ID3D12Device* nullDevice = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor = {};

        // CreateShaderResourceView
        try
        {
            CreateShaderResourceView(nullDevice, nullptr, cpuDescriptor);

            printf("ERROR: CreateShaderResourceView - Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            CreateShaderResourceView(device, nullptr, cpuDescriptor);

            printf("ERROR: CreateShaderResourceView - Failed to throw for null resource\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        // CreateUnorderedAccessView
        try
        {
            CreateUnorderedAccessView(nullDevice, nullptr, cpuDescriptor);

            printf("ERROR: CreateUnorderedAccessView - Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            CreateUnorderedAccessView(device, nullptr, cpuDescriptor);

            printf("ERROR: CreateUnorderedAccessView - Failed to throw for null resource\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        // CreateRenderTargetView
        try
        {
            CreateRenderTargetView(nullDevice, nullptr, cpuDescriptor);

            printf("ERROR: CreateRenderTargetView - Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            CreateRenderTargetView(device, nullptr, cpuDescriptor);

            printf("ERROR: CreateRenderTargetView - Failed to throw for null resource\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        // CreateBufferShaderResourceView
        try
        {
            CreateBufferShaderResourceView(nullDevice, nullptr, cpuDescriptor, sizeof(float));

            printf("ERROR: CreateBufferShaderResourceView - Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            CreateBufferShaderResourceView(device, nullptr, cpuDescriptor, sizeof(float));

            printf("ERROR: CreateBufferShaderResourceView - Failed to throw for null resource\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        // CreateBufferUnorderedAccessView
        try
        {
            CreateBufferUnorderedAccessView(nullDevice, nullptr, cpuDescriptor, sizeof(float));

            printf("ERROR: CreateBufferUnorderedAccessView - Failed to throw for null device\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        try
        {
            CreateBufferUnorderedAccessView(device, nullptr, cpuDescriptor, sizeof(float));

            printf("ERROR: CreateBufferUnorderedAccessView - Failed to throw for null resource\n");
            success = false;
        }
        catch(const std::exception&)
        {
        }

        // CreateRootSignature
        hr = CreateRootSignature(nullDevice, nullptr, nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateRootSignature - Expected failure for null device (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateRootSignature(device, nullptr, nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateRootSignature - Expected failure for null root signature desc (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // GetTextureSize
        auto size = GetTextureSize(nullptr);
        if (size.x != 0 || size.y != 0)
        {
            printf("ERROR: GetTextureSize - Failed for null resource\n");
            success = false;
        }
    }

    return success;
}
