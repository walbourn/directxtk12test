//--------------------------------------------------------------------------------------
// File: bufferhelpers.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "BufferHelpers.h"

#include "DirectXHelpers.h"
#include "ResourceUploadBatch.h"
#include "VertexTypes.h"

#include <cstdio>
#include <iterator>
#include <vector>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    const VertexPositionColor s_vertexData[3] =
    {
        { XMFLOAT3{ 0.0f,   0.5f,  0.5f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },  // Top / Red
        { XMFLOAT3{ 0.5f,  -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },  // Right / Green
        { XMFLOAT3{ -0.5f, -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }   // Left / Blue
    };

    const uint32_t s_pixels[16] = {
        0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffff, 0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff,
        0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffff, 0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff,
    };
}

_Success_(return)
bool Test01(_In_ ID3D12Device* device)
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

    bool success = true;

    ComPtr<ID3D12Resource> test1;
    ComPtr<ID3D12Resource> test2;
    ComPtr<ID3D12Resource> test3;
    ComPtr<ID3D12Resource> test4;
    ComPtr<ID3D12Resource> test5;
    ComPtr<ID3D12Resource> test6;
    ComPtr<ID3D12Resource> test7;
    ComPtr<ID3D12Resource> test8;
    ComPtr<ID3D12Resource> test9;
    ComPtr<ID3D12Resource> test10;
    ComPtr<ID3D12Resource> test11;
    ComPtr<ID3D12Resource> test12;
    ComPtr<ID3D12Resource> test13;

    // CreateUploadBuffer
    {
        hr = CreateUploadBuffer(device,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            test1.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateUploadBuffer(1) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateUploadBuffer(device,
            s_vertexData, std::size(s_vertexData),
            test2.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateUploadBuffer(2) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        std::vector<VertexPositionColor> verts(s_vertexData, s_vertexData + std::size(s_vertexData));

        hr = CreateUploadBuffer(device,
            verts,
            test3.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateUploadBuffer(3) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // DSR
        hr = CreateUploadBuffer(device,
            verts,
            test4.ReleaseAndGetAddressOf(),
            D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateUploadBuffer(DSR) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // No data
        hr = CreateUploadBuffer(device,
            nullptr, 14, sizeof(VertexPositionDualTexture),
            test5.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateUploadBuffer(no data) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }
    }

    // CreateUAVBuffer
    hr = CreateUAVBuffer(device,
        sizeof(VertexPositionNormalColorTexture),
        test6.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        printf("ERROR: Failed CreateUAVBuffer test (%08X)\n", static_cast<unsigned int>(hr));
        success = false;
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    // CreateStaticBuffer
    {
        hr = CreateStaticBuffer(device,
            resourceUpload,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test7.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateStaticBuffer(1) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateStaticBuffer(device,
            resourceUpload,
            s_vertexData, std::size(s_vertexData),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test8.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateStaticBuffer(2) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        std::vector<VertexPositionColor> verts(s_vertexData, s_vertexData + std::size(s_vertexData));

        hr = CreateStaticBuffer(device,
            resourceUpload,
            verts,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test9.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateStaticBuffer(3) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // UAV
        hr = CreateStaticBuffer(device,
            resourceUpload,
            verts,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test10.ReleaseAndGetAddressOf(),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateStaticBuffer(UAV) test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }
    }

    // CreateTextureFromMemory
    {
        D3D12_SUBRESOURCE_DATA initData = { s_pixels, 0, 0 };
        hr = CreateTextureFromMemory(device, resourceUpload, 4u, DXGI_FORMAT_B8G8R8A8_UNORM, initData, test11.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateTextureFromMemory 1D test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        initData = { s_pixels, sizeof(uint32_t) * 8, 0 };
        hr = CreateTextureFromMemory(device, resourceUpload, 8u, 2u, DXGI_FORMAT_B8G8R8A8_UNORM, initData, test12.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateTextureFromMemory 2D test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        initData = { s_pixels, sizeof(uint32_t) * 2, sizeof(uint32_t) * 4 };
        hr = CreateTextureFromMemory(device, resourceUpload, 8u, 2u, 4u, DXGI_FORMAT_B8G8R8A8_UNORM, initData, test13.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            printf("ERROR: Failed CreateTextureFromMemory 3D test (%08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }
    }

    // invalid args
    #pragma warning(push)
    #pragma warning(disable:6385 6387)
    {
        ComPtr<ID3D12Resource> res;
        ID3D12Device* nullDevice = nullptr;

        // CreateUploadBuffer
        hr = CreateUploadBuffer(device, nullptr, 0, 0, nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateUploadBuffer - expected failure for null buffer (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateUploadBuffer(nullDevice, nullptr, 0, 0, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateUploadBuffer - expected failure for null device (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateUploadBuffer(device, s_vertexData, 0, 0, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateUploadBuffer - expected failure for zero length (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateUploadBuffer(device, s_vertexData, UINT32_MAX, INT_MAX, res.ReleaseAndGetAddressOf());
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))
        {
            printf("ERROR: CreateUploadBuffer - expected failure for too large bytes (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // CreateUAVBuffer
        hr = CreateUAVBuffer(device, sizeof(VertexPositionNormalColorTexture), nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateUAVBuffer - expected failure for null buffer (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateUAVBuffer(nullDevice, sizeof(VertexPositionNormalColorTexture), res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateUAVBuffer - expected failure for null device (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateUAVBuffer(device, UINT32_MAX, res.ReleaseAndGetAddressOf());
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))
        {
            printf("ERROR: CreateUAVBuffer - expected failure for too large bytes (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // CreateStaticBuffer
        hr = CreateStaticBuffer(device, resourceUpload, nullptr, 0, 0, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateStaticBuffer - expected failure for null buffer (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateStaticBuffer(nullDevice, resourceUpload, nullptr, 0, 0, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateStaticBuffer - expected failure for null device (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateStaticBuffer(device, resourceUpload, s_vertexData, 0, 0, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateStaticBuffer - expected failure for zero length (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateStaticBuffer(device, resourceUpload, s_vertexData, UINT32_MAX, INT32_MAX, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, res.ReleaseAndGetAddressOf());
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))
        {
            printf("ERROR: CreateUploadBuffer - expected failure for too large bytes (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // CreateTextureFromMemory 1D
        D3D12_SUBRESOURCE_DATA initData = { s_pixels, 0, 0 };
        hr = CreateTextureFromMemory(device, resourceUpload, 0, DXGI_FORMAT_UNKNOWN, initData, nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 1D - expected failure for null buffer (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(nullDevice, resourceUpload, 0, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 1D - expected failure for null device (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(device, resourceUpload, 0, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 1D - expected failure for zero dimensions (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(device, resourceUpload, UINT32_MAX, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))
        {
            printf("ERROR: CreateTextureFromMemory 1D - expected failure for too large bytes (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // CreateTextureFromMemory 2D
        initData = { s_pixels, sizeof(uint32_t) * 8, 0 };
        hr = CreateTextureFromMemory(device, resourceUpload, 0, 0, DXGI_FORMAT_UNKNOWN, initData, nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 2D - expected failure for null buffer (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(nullDevice, resourceUpload, 0, 0, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 2D - expected failure for null device (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(device, resourceUpload, 0, 0, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 2D - expected failure for zero dimensions (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(device, resourceUpload, UINT32_MAX, UINT32_MAX, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))
        {
            printf("ERROR: CreateTextureFromMemory 2D - expected failure for too large bytes (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        // CreateTextureFromMemory 2D
        initData = { s_pixels, sizeof(uint32_t) * 2, sizeof(uint32_t) * 4 };
        hr = CreateTextureFromMemory(device, resourceUpload, 0, 0, 0, DXGI_FORMAT_UNKNOWN, initData, nullptr);
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 3D - expected failure for null buffer (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(nullDevice, resourceUpload, 0, 0, 0, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 3D - expected failure for null device (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(device, resourceUpload, 0, 0, 0, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != E_INVALIDARG)
        {
            printf("ERROR: CreateTextureFromMemory 3D - expected failure for zero dimensions (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }

        hr = CreateTextureFromMemory(device, resourceUpload, UINT32_MAX, UINT32_MAX, UINT32_MAX, DXGI_FORMAT_UNKNOWN, initData, res.ReleaseAndGetAddressOf());
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))
        {
            printf("ERROR: CreateTextureFromMemory 3D - expected failure for too large bytes (HRESULT: %08X)\n", static_cast<unsigned int>(hr));
            success = false;
        }
    }
    #pragma warning(pop)

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
