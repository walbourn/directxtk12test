//--------------------------------------------------------------------------------------
// File: bufferhelpers.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

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

bool Test01(ID3D12Device* device)
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

    ComPtr<ID3D12CommandQueue> copyQueue;
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_GRAPHICS_PPV_ARGS(copyQueue.GetAddressOf()));
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

    // CreateUploadBuffer
    {
        static const VertexPositionColor s_vertexData[3] =
        {
            { XMFLOAT3{ 0.0f,   0.5f,  0.5f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },  // Top / Red
            { XMFLOAT3{ 0.5f,  -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },  // Right / Green
            { XMFLOAT3{ -0.5f, -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }   // Left / Blue
        };

        if (FAILED(CreateUploadBuffer(device,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            test1.ReleaseAndGetAddressOf())))
        {
            printf("ERROR: Failed CreateUploadBuffer(1) test\n");
            success = false;
        }

        if (FAILED(CreateUploadBuffer(device,
            s_vertexData, std::size(s_vertexData),
            test2.ReleaseAndGetAddressOf())))
        {
            printf("ERROR: Failed CreateUploadBuffer(2) test\n");
            success = false;
        }

        std::vector<VertexPositionColor> verts(s_vertexData, s_vertexData + std::size(s_vertexData));

        if (FAILED(CreateUploadBuffer(device,
            verts,
            test3.ReleaseAndGetAddressOf())))
        {
            printf("ERROR: Failed CreateUploadBuffer(3) test\n");
            success = false;
        }

        // DSR
        if (FAILED(CreateUploadBuffer(device,
            verts,
            test4.ReleaseAndGetAddressOf(),
            D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)))
        {
            printf("ERROR: Failed CreateUploadBuffer(DSR) test\n");
            success = false;
        }

        // No data
        if (FAILED(CreateUploadBuffer(device,
            nullptr, 14, sizeof(VertexPositionDualTexture),
            test5.ReleaseAndGetAddressOf())))
        {
            printf("ERROR: Failed CreateUploadBuffer(no data) test\n");
            success = false;
        }
    }

    // CreateUAVBuffer
    if (FAILED(CreateUAVBuffer(device,
        sizeof(VertexPositionNormalColorTexture),
        test6.ReleaseAndGetAddressOf())))
    {
        printf("ERROR: Failed CreateUAVBuffer test\n");
        success = false;
    }

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin(queueDesc.Type);

    // CreateStaticBuffer
    {
        static const VertexPositionColor s_vertexData[3] =
        {
            { XMFLOAT3{ 0.0f,   0.5f,  0.5f }, XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f } },  // Top / Red
            { XMFLOAT3{ 0.5f,  -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f } },  // Right / Green
            { XMFLOAT3{ -0.5f, -0.5f,  0.5f }, XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f } }   // Left / Blue
        };

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            s_vertexData, std::size(s_vertexData), sizeof(VertexPositionColor),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test7.ReleaseAndGetAddressOf())))
        {
            printf("ERROR: Failed CreateStaticBuffer(1) test\n");
            success = false;
        }

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            s_vertexData, std::size(s_vertexData),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test8.ReleaseAndGetAddressOf())))
        {
            printf("ERROR: Failed CreateStaticBuffer(2) test\n");
            success = false;
        }

        std::vector<VertexPositionColor> verts(s_vertexData, s_vertexData + std::size(s_vertexData));

        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            verts,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test9.ReleaseAndGetAddressOf())))
        {
            printf("ERROR: Failed CreateStaticBuffer(3) test\n");
            success = false;
        }

        // UAV
        if (FAILED(CreateStaticBuffer(device, resourceUpload,
            verts,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            test10.ReleaseAndGetAddressOf(),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)))
        {
            printf("ERROR: Failed CreateStaticBuffer(UAV) test\n");
            success = false;
        }
    }

    auto uploadResourcesFinished = resourceUpload.End(copyQueue.Get());
    uploadResourcesFinished.wait();

    return success;
}
