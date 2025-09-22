//--------------------------------------------------------------------------------------
// File: descriptorheap.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "DescriptorHeap.h"

#include <cstdio>
#include <iterator>
#include <vector>

#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_move_constructible<DescriptorHeap>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DescriptorHeap>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<DescriptorPile>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DescriptorPile>::value, "Move Assign.");

_Success_(return)
bool Test18(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    std::unique_ptr<DescriptorHeap> resourceDescriptors;
    try
    {
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            DescriptorHeap::DefaultDesc(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, &desc);
            desc.NumDescriptors = 3;

            ComPtr<ID3D12DescriptorHeap> existingHeap;
            HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&existingHeap));
            if (FAILED(hr))
            {
                printf("ERROR: Failed to create test descriptor heap (hr: 0x%08X)\n", static_cast<unsigned long>(hr));
                return false;
            }

            resourceDescriptors = std::make_unique<DescriptorHeap>(existingHeap.Get());
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            DescriptorHeap::DefaultDesc(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, &desc);
            desc.NumDescriptors = 3;

            resourceDescriptors = std::make_unique<DescriptorHeap>(device, &desc);
        }

        resourceDescriptors = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 3);

        resourceDescriptors = std::make_unique<DescriptorHeap>(device, 3);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating descriptor object (except: %s)\n", e.what());
        return false;
    }

    bool success = true;

    auto handle = resourceDescriptors->GetCpuHandle(0);
    if (!handle.ptr)
    {
        printf("ERROR: Failed to get CPU handle\n");
        success = false;
    }

    auto handle1st = resourceDescriptors->GetFirstCpuHandle();
    if (!handle1st.ptr || handle1st.ptr != handle.ptr)
    {
        printf("ERROR: Failed to get first CPU handle\n");
        success = false;
    }

    auto gpuHandle = resourceDescriptors->GetGpuHandle(0);
    if (!gpuHandle.ptr)
    {
        printf("ERROR: Failed to get GPU handle\n");
        success = false;
    }

    auto gpuHandle1st = resourceDescriptors->GetFirstGpuHandle();
    if (!gpuHandle1st.ptr || gpuHandle1st.ptr != gpuHandle.ptr)
    {
        printf("ERROR: Failed to get first GPU handle\n");
        success = false;
    }

    bool caught = false;
    try
    {
        handle = resourceDescriptors->GetCpuHandle(UINT16_MAX);
    }
    catch(const std::exception&)
    {
        caught = true;
    }
    if (!caught)
    {
        printf("ERROR: Failed to catch out of range CPU handle\n");
        success = false;
    }

    caught = false;
    try
    {
        gpuHandle = resourceDescriptors->GetGpuHandle(UINT16_MAX);
    }
    catch(const std::exception&)
    {
        caught = true;
    }
    if (!caught)
    {
        printf("ERROR: Failed to catch out of range GPU handle\n");
        success = false;
    }

    // TODO - WriteDescriptors test?



    caught = false;
    ID3D12Device* nullDevice = nullptr;
    try
    {
        resourceDescriptors = std::make_unique<DescriptorHeap>(nullDevice, 3);
    }
    catch(const std::exception&)
    {
        caught = true;
    }
    if (!caught)
    {
        printf("ERROR: Failed to catch null device creation\n");
        success = false;
    }

    caught = false;
    ID3D12DescriptorHeap* nullHeap = nullptr;
    try
    {
        resourceDescriptors = std::make_unique<DescriptorHeap>(nullHeap);
    }
    catch(const std::exception&)
    {
        caught = true;
    }
    if (!caught)
    {
        printf("ERROR: Failed to catch null existing heap creation\n");
        success = false;
    }

    return success;
}

_Success_(return)
bool Test19(_In_ ID3D12Device* device)
{
    if (!device)
        return false;

    std::unique_ptr<DescriptorPile> resourceDescriptors;
    try
    {
        // TODO - Try existing heap ctor
        // TODO - Try device with desc ctor
        // TODO - Try device with type ctor

        resourceDescriptors = std::make_unique<DescriptorPile>(device, 3);
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating descriptor object (except: %s)\n", e.what());
        return false;
    }

    bool success = true;

    return success;
}
