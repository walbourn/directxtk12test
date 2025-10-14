//--------------------------------------------------------------------------------------
// File: ApiTest.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include <crtdbg.h>

#include <cstdio>
#include <exception>
#include <iterator>

#include "DirectXMath.h"

#ifdef USING_DIRECTX_HEADERS
#include <directx/d3d12.h>
#else
#include <d3d12.h>
#endif

#include <dxgi1_6.h>

#include "GraphicsMemory.h"

#include <wrl/client.h>

//#define D3D_DEBUG

using Microsoft::WRL::ComPtr;

namespace
{
    IDXGIFactory4* g_pdxgiFactory = nullptr;

    //-------------------------------------------------------------------------------------
    HRESULT CreateDevice(_Outptr_ ID3D12Device** pDev)
    {
        if (!pDev)
            return E_INVALIDARG;

        *pDev = nullptr;

        HRESULT hr = E_FAIL;

    #ifdef D3D_DEBUG
        // Enable the debug layer (only available if the Graphics Tools feature-on-demand is enabled).
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
            {
                debugController->EnableDebugLayer();

            #ifdef ENABLE_GPU_VALIDATION
                ComPtr<ID3D12Debug1> debugController1;
                if (SUCCEEDED(debugController.As(&debugController1)))
                {
                    debugController1->SetEnableGPUBasedValidation(true);
                }
            #endif
            }
            else
            {
                OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
            }
        }

        if (!g_pdxgiFactory)
        {
            hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&g_pdxgiFactory));
            if (FAILED(hr))
                return hr;
        }
    #else
        if (!g_pdxgiFactory)
        {
            hr = CreateDXGIFactory1(IID_PPV_ARGS(&g_pdxgiFactory));
            if (FAILED(hr))
                return hr;
        }
    #endif

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != g_pdxgiFactory->EnumAdapters1(adapterIndex, adapter.ReleaseAndGetAddressOf()); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(adapter->GetDesc1(&desc)))
            {
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_ID3D12Device, nullptr)))
            {
                break;
            }
        }

        if (!adapter)
        {
            // Try WARP12 instead
            if (FAILED(g_pdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))))
            {
                return E_FAIL;
            }
        }

        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(pDev));
        if (FAILED(hr))
            return hr;

    #ifndef NDEBUG
        // Configure debug device (if active).
        ComPtr<ID3D12InfoQueue> d3dInfoQueue;
        if (SUCCEEDED((*pDev)->QueryInterface(IID_PPV_ARGS(&d3dInfoQueue))))
        {
    #ifdef _DEBUG
            d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    #endif
            D3D12_MESSAGE_ID hide[] =
            {
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
            };
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    #endif

        return S_OK;
    }
}

//-------------------------------------------------------------------------------------
// Types and globals

typedef _Success_(return) bool (*TestFN)(_In_ ID3D12Device *device);

struct TestInfo
{
    const char *name;
    TestFN func;
};

extern _Success_(return) bool Test00(_In_ ID3D12Device *device);
extern _Success_(return) bool Test01(_In_ ID3D12Device *device);
extern _Success_(return) bool Test02(_In_ ID3D12Device *device);
extern _Success_(return) bool Test03(_In_ ID3D12Device *device);
extern _Success_(return) bool Test04(_In_ ID3D12Device *device);
extern _Success_(return) bool Test05(_In_ ID3D12Device *device);
extern _Success_(return) bool Test06(_In_ ID3D12Device *device);
extern _Success_(return) bool Test07(_In_ ID3D12Device *device);
extern _Success_(return) bool Test08(_In_ ID3D12Device *device);
extern _Success_(return) bool Test09(_In_ ID3D12Device *device);
extern _Success_(return) bool Test10(_In_ ID3D12Device *device);
extern _Success_(return) bool Test11(_In_ ID3D12Device *device);
extern _Success_(return) bool Test12(_In_ ID3D12Device *device);
extern _Success_(return) bool Test13(_In_ ID3D12Device *device);
extern _Success_(return) bool Test14(_In_ ID3D12Device *device);
extern _Success_(return) bool Test15(_In_ ID3D12Device *device);
extern _Success_(return) bool Test16(_In_ ID3D12Device *device);
extern _Success_(return) bool Test17(_In_ ID3D12Device *device);
extern _Success_(return) bool Test18(_In_ ID3D12Device *device);
extern _Success_(return) bool Test19(_In_ ID3D12Device *device);
extern _Success_(return) bool Test20(_In_ ID3D12Device *device);
extern _Success_(return) bool Test21(_In_ ID3D12Device *device);

#ifdef TEST_AUDIO
extern _Success_(return) bool TestA01(_In_ ID3D12Device *device);
extern _Success_(return) bool TestA02(_In_ ID3D12Device *device);
#endif

const TestInfo g_Tests[] =
{
    // Note that Test00 is GraphicsMemory, which must be run first in wmain.
    { "BasicEffects", Test05 },
    { "BufferHelpers", Test01 },
    { "CommonStates", Test02 },
    { "DescriptorHeap", Test18 },
    { "DescriptorPile", Test19 },
    { "DirectXHelpers", Test03 },
    { "ResourceUploadBatch", Test21 },
    { "GeometricPrimitive", Test04 },
    { "PostProcess", Test06 },
    { "PrimitiveBatch", Test07 },
    { "SpriteBatch", Test08 },
    { "SpriteFont", Test09 },
    { "VertexTypes", Test10 },
    { "NormalMapEffect", Test11 },
    { "PBREffect", Test12 },
    { "Model", Test13 },
    { "GamePad", Test14 },
    { "Keyboard", Test15 },
    { "Mouse", Test16 },
#ifndef DIRECTX_TOOLKIT_IMPORT
    { "BinaryReader (internal)", Test20 },
#endif
    { "LoaderHelpers (internal)", Test17 },

#ifdef TEST_AUDIO
    { "Audio", TestA01 },
    { "Audio (internal)", TestA02 },
#endif
};

//-------------------------------------------------------------------------------------
_Success_(return)
bool RunTests(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    size_t nPass = 1; // Test00 in wmain
    size_t nFail = 0;

    for(size_t i=0; i < std::size(g_Tests); ++i)
    {
        printf("%s: ", g_Tests[i].name );

        bool passed = false;

        try
        {
            passed = g_Tests[i].func(device);
        }
        catch(const std::exception& e)
        {
            printf("ERROR: Failed with a standard C++ exception: %s\n", e.what());
        }
        catch(...)
        {
            printf("ERROR: Failed with an unknown C++ exception\n");
        }

        if (passed)
        {
            ++nPass;
            printf("PASS\n");
        }
        else
        {
            ++nFail;
            printf("FAIL\n");
        }
    }

    printf("Ran %zu tests, %zu pass, %zu fail\n", nPass+nFail, nPass, nFail);

    return (nFail == 0);
}


//-------------------------------------------------------------------------------------
int __cdecl wmain()
{
    printf("**************************************************************\n");
    printf("*** DirectX tool Kit for DX 12 API Test\n" );
    printf("**************************************************************\n");

    if ( !DirectX::XMVerifyCPUSupport() )
    {
        printf("ERROR: XMVerifyCPUSupport fails on this system, not a supported platform\n");
        return -1;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
    if (FAILED(hr))
    {
        printf("ERROR: Unable to initialize COM (%08X).\n", static_cast<unsigned int>(hr));
        return -1;
    }

    ComPtr<ID3D12Device> device;
    hr = CreateDevice(device.GetAddressOf());
    if (FAILED(hr))
    {
        printf("ERROR: Unable to create a Direct3D 12 device (%08X).\n", static_cast<unsigned int>(hr));
        return -1;
    }

#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // Explicitly do Graphicsmemory tests first.
    printf("GraphicsMemory: ");

    if (Test00(device.Get()))
    {
        printf("PASS\n");
    }
    else
    {
        printf("FAIL\n");
        return -1;
    }

    // The DirectX 12 version of the tool kit requires this singleton.
    std::unique_ptr<DirectX::GraphicsMemory> graphicsMemory;
    try
    {
        graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device.Get());
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating graphics memory object (except: %s)\n", e.what());
        return -1;
    }

    if ( !RunTests(device.Get()) )
        return -1;

    return 0;
}
