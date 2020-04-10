//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK DDSTextureLoader & WICTextureLoader
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include "ReadData.h"

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#include <Windows.ApplicationModel.h>
#include <Windows.Storage.h>
#endif

#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

namespace
{
    float dist = 10.f;
};

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

// Constructor.
Game::Game() noexcept(false) :
    m_frame(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    const DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

#if defined(_XBOX_ONE) && defined(_TITLE)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD
        );
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
    m_deviceResources->RegisterDeviceNotify(this);
#endif
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
    HWND window,
#else
    IUnknown* window,
#endif
    int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();

#if defined(_XBOX_ONE) && defined(_TITLE)
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_deviceResources->SetWindow(window, width, height, rotation);
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#else
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
#endif

    m_deviceResources->CreateDeviceResources();
    {
        auto device = m_deviceResources->GetD3DDevice();
        D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
        DX::ThrowIfFailed(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS,
            &options,
            sizeof(options)));

        if (!options.TypedUAVLoadAdditionalFormats)
        {
            OutputDebugStringA("NOTE: This device does not support TypedUAVLoadAdditionalFormats so autogen mips is not supported except for R32\n");
        }

#if !defined(_XBOX_ONE) || !defined(_TITLE)
        if (!options.StandardSwizzle64KBSupported)
        {
            OutputDebugStringA("NOTE: This device does not support StandardSwizzle64KBSupported so autogen mips for BGR is not supported\n");
        }
#endif
    }
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    ++m_frame;
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    elapsedTime;

    auto pad = m_gamePad->GetState(0);
    auto kb = m_keyboard->GetState();
    if (kb.Escape || (pad.IsConnected() && pad.IsViewPressed()))
    {
        ExitGame();
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    auto t = static_cast<float>(m_timer.GetTotalSeconds());

    // Cube 1
    XMMATRIX world = XMMatrixRotationY(t) * XMMatrixTranslation(1.5f, -2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Earth), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 2
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(1.5f, 0, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Windows95_sRGB), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 3
    world = XMMatrixRotationY(t) * XMMatrixTranslation(1.5f, 2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Windows95), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 4
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(-1.5f, -2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::Earth_Imm), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 5
    world = XMMatrixRotationY(t) * XMMatrixTranslation(-1.5f, 0, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo_BC1), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    // Cube 6
    world = XMMatrixRotationY(-t) * XMMatrixTranslation(-1.5f, 2.1f, (dist / 2.f) + dist * sin(t));
    m_effect->SetWorld(world);
    m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::DirectXLogo), m_states->LinearClamp());
    m_effect->Apply(commandList);
    m_cube->Draw(commandList);

    PIXEndEvent(commandList);

    if (m_frame == 10)
    {
        m_screenshot = m_deviceResources->GetRenderTarget();
    }

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    // Sample stats to update peak values
    (void)m_graphicsMemory->GetStatistics();

    PIXEndEvent(m_deviceResources->GetCommandQueue());

    if (m_screenshot)
    {
        // We take the shot here to cope with lost device

        OutputDebugStringA("******** SCREENSHOT TEST BEGIN *************\n");

        bool success = true;

#if defined(_XBOX_ONE) && defined(_TITLE)
        const wchar_t sspath[MAX_PATH] = L"T:\\";
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
        wchar_t sspath[MAX_PATH] = {};

        using namespace Microsoft::WRL;
        using namespace Microsoft::WRL::Wrappers;
        using namespace ABI::Windows::Foundation;

        ComPtr<ABI::Windows::Storage::IApplicationDataStatics> appStatics;
        DX::ThrowIfFailed(GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_ApplicationData).Get(), appStatics.GetAddressOf()));

        ComPtr<ABI::Windows::Storage::IApplicationData> appData;
        DX::ThrowIfFailed(appStatics->get_Current(appData.GetAddressOf()));

        // Temporary folder
        {
            ComPtr<ABI::Windows::Storage::IStorageFolder> folder;
            DX::ThrowIfFailed(appData->get_TemporaryFolder(folder.GetAddressOf()));

            ComPtr<ABI::Windows::Storage::IStorageItem> item;
            DX::ThrowIfFailed(folder.As(&item));

            HString folderName;
            DX::ThrowIfFailed(item->get_Path(folderName.GetAddressOf()));

            unsigned int len;
            PCWSTR szPath = folderName.GetRawBuffer(&len);
            if (wcscpy_s(sspath, MAX_PATH, szPath) > 0
                || wcscat_s(sspath, L"\\") > 0)
            {
                throw std::exception("TemporaryFolder");
            }
        }
#else
        const wchar_t sspath[MAX_PATH] = L".";
#endif

        OutputDebugStringA("Output path: ");
        OutputDebugStringW(sspath);
        OutputDebugStringA("\n");

        wchar_t sspng[MAX_PATH] = {};
        wchar_t ssjpg[MAX_PATH] = {};
        wchar_t ssbmp[MAX_PATH] = {};
        wchar_t sstif[MAX_PATH] = {};
        wchar_t ssdds[MAX_PATH] = {};

        swprintf_s(sspng, L"%ls\\SCREENSHOT.PNG", sspath);
        swprintf_s(ssjpg, L"%ls\\SCREENSHOT.JPG", sspath);
        swprintf_s(ssbmp, L"%ls\\SCREENSHOT.BMP", sspath);
        swprintf_s(sstif, L"%ls\\SCREENSHOT.TIF", sspath);
        swprintf_s(ssdds, L"%ls\\SCREENSHOT.DDS", sspath);

        DeleteFileW(sspng);
        DeleteFileW(ssjpg);
        DeleteFileW(ssbmp);
        DeleteFileW(sstif);
        DeleteFileW(ssdds);

        HRESULT hr = SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatPng, sspng,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, &GUID_WICPixelFormat32bppBGRA, nullptr, true);

        if (FAILED(hr))
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: SaveWICTextureToFile (PNG) failed %08X\n", static_cast<unsigned int>(hr));
            OutputDebugStringA(buff);
            success = false;
        }
        else if (GetFileAttributesW(sspng) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.PNG\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.PNG!\n");
            success = false;
        }

        hr = SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatJpeg, ssjpg,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, nullptr, nullptr, true);

        if (FAILED(hr))
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: SaveWICTextureToFile (JPG) failed %08X\n", static_cast<unsigned int>(hr));
            OutputDebugStringA(buff);
            success = false;
        }
        else if (GetFileAttributesW(ssjpg) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.JPG\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.JPG!\n");
            success = false;
        }

        hr = SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatBmp, ssbmp,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
            &GUID_WICPixelFormat16bppBGR565);

        if (FAILED(hr))
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: SaveWICTextureToFile (BMP) failed %08X\n", static_cast<unsigned int>(hr));
            OutputDebugStringA(buff);
            success = false;
        }
        else if (GetFileAttributesW(ssbmp) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.BMP\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.BMP!\n");
            success = false;
        }

        hr = SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatTiff, sstif,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
            nullptr,
            [&](IPropertyBag2* props)
        {
            PROPBAG2 options[2] = { 0, 0 };
            options[0].pstrName = const_cast<wchar_t*>(L"CompressionQuality");
            options[1].pstrName = const_cast<wchar_t*>(L"TiffCompressionMethod");

            VARIANT varValues[2];
            varValues[0].vt = VT_R4;
            varValues[0].fltVal = 0.75f;

            varValues[1].vt = VT_UI1;
            varValues[1].bVal = WICTiffCompressionNone;

            (void)props->Write(2, options, varValues);
        }, true);

        if (FAILED(hr))
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: SaveWICTextureToFile (TIFF) failed %08X\n", static_cast<unsigned int>(hr));
            OutputDebugStringA(buff);
            success = false;
        }
        else if (GetFileAttributesW(sstif) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.TIF\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.TIF!\n");
            success = false;
        }

        hr = SaveDDSTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(), ssdds,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT);

        if (FAILED(hr))
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: SaveDDSTextureToFile failed %08X\n", static_cast<unsigned int>(hr));
            OutputDebugStringA(buff);
            success = false;
        }
        else if (GetFileAttributesW(ssdds) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.DDS\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.DDS!\n");
            success = false;
        }

        // TODO - pass in D3D12_HEAP_TYPE_READBACK resource

        // ScreenGrab of an MSAA resource is tested elsewhere

        OutputDebugStringA(success ? "Passed\n" : "Failed\n");
        OutputDebugStringA("********* SCREENSHOT TEST END **************\n");
        m_screenshot.Reset();
    }
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    XMVECTORF32 color;
#ifdef GAMMA_CORRECT_RENDERING
    color.v = XMColorSRGBToRGB(Colors::CornflowerBlue);
#else
    color.v = Colors::CornflowerBlue;
#endif
    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, color, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
#if defined(_XBOX_ONE) && defined(_TITLE)
    auto queue = m_deviceResources->GetCommandQueue();
    queue->SuspendX(0);
#endif
}

void Game::OnResuming()
{
#if defined(_XBOX_ONE) && defined(_TITLE)
    auto queue = m_deviceResources->GetCommandQueue();
    queue->ResumeX();
#endif

    m_timer.ResetElapsedTime();
}

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP) 
void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}
#endif

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;
#else
    UNREFERENCED_PARAMETER(rotation);
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;
#endif

    CreateWindowSizeDependentResources();
}
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
void Game::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}
#endif

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    m_states = std::make_unique<CommonStates>(device);

#ifdef LH_COORDS
    m_cube = GeometricPrimitive::CreateCube(2.f, false);
#else
    m_cube = GeometricPrimitive::CreateCube(2.f, true);
#endif

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullCounterClockwise,
            rtState);

        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Texture, pd);
    }

    bool success = true;

    {
        ResourceUploadBatch resourceUpload(device);

        resourceUpload.Begin();

        // View textures
        OutputDebugStringA("*********** UINT TESTS BEGIN ***************\n");

        m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            Descriptors::Count);

        // Earth
        // m_earth / m_earth2 is loaded in the copy queue test

        // DirectX Logo
        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen_bgra.dds", m_dxlogo.ReleaseAndGetAddressOf(), true));

        {
            auto desc = m_dxlogo->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 9)
            {
                if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_B8G8R8A8_UNORM)
                    && desc.MipLevels == 1)
                {
                    OutputDebugStringA("NOTE: dx5_logo_autogen_bgra.dds - device doesn't support autogen mips for 8:8:8:8 and/or lacks standard swizzle\n");
                }
                else
                {
                    OutputDebugStringA("FAILED: dx5_logo_autogen_bgra.dds (autogen) desc unexpected\n");
                    success = false;
                }
            }
        }

        CreateShaderResourceView(device, m_dxlogo.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo));

        DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"dx5_logo.dds", 0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_FORCE_SRGB, m_dxlogo2.ReleaseAndGetAddressOf()));

        {
            auto desc = m_dxlogo2->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_BC1_UNORM_SRGB
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 9)
            {
                OutputDebugStringA("FAILED: dx5_logo.dds (BC1 sRGB) desc unexpected\n");
                success = false;
            }
        }

        CreateShaderResourceView(device, m_dxlogo2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::DirectXLogo_BC1));

        // Windows 95 logo
        DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"win95.bmp", m_win95.ReleaseAndGetAddressOf(), true));

        {
            auto desc = m_win95->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 9)
            {
                if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R8G8B8A8_UNORM)
                    && desc.MipLevels == 1)
                {
                    OutputDebugStringA("NOTE: win95.bmp - device doesn't support autogen mips for 8:8:8:8\n");
                }
                else
                {
                    OutputDebugStringA("FAILED: win95.bmp (autogen) desc unexpected\n");
                    success = false;
                }
            }
        }

        CreateShaderResourceView(device, m_win95.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Windows95));

        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"win95.bmp", 0, D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FORCE_SRGB | WIC_LOADER_MIP_AUTOGEN, m_win95_2.ReleaseAndGetAddressOf()));

        {
            auto desc = m_win95_2->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 9)
            {
                if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                    && desc.MipLevels == 1)
                {
                    OutputDebugStringA("NOTE: win95.bmp - device doesn't support autogen mips for 8:8:8:8\n");
                }
                else
                {
                    OutputDebugStringA("FAILED: win95.bmp (autogen, sRGB) desc unexpected\n");
                    success = false;
                }
            }
        }

        CreateShaderResourceView(device, m_win95_2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Windows95_sRGB));

        UnitTests(resourceUpload, success);

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();
    }

    // Copy Queue test
    {
        ResourceUploadBatch resourceUpload(device);

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

        resourceUpload.Begin(queueDesc.Type);

        DX::ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_GRAPHICS_PPV_ARGS(m_copyQueue.ReleaseAndGetAddressOf())));

        m_copyQueue->SetName(L"CopyTest");

        DDS_ALPHA_MODE alphaMode = DDS_ALPHA_MODE_UNKNOWN;
        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"earth_A2B10G10R10.dds", m_earth.ReleaseAndGetAddressOf(), false, 0, &alphaMode));

        {
            if (alphaMode != DDS_ALPHA_MODE_UNKNOWN)
            {
                OutputDebugStringA("FAILED: earth_A2B10G10R10.dds alpha mode value unexpected\n");
                success = false;
            }

            auto desc = m_earth->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM
                || desc.Width != 512
                || desc.Height != 256
                || desc.MipLevels != 10)
            {
                OutputDebugStringA("FAILED: earth_A2B10G10R10.dds desc unexpected\n");
                success = false;
            }
        }

        CreateShaderResourceView(device, m_earth.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth));

        DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"win95.bmp", m_copyTest.ReleaseAndGetAddressOf(), false));

        {
            auto desc = m_copyTest->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 1)
            {
                OutputDebugStringA("FAILED: win95.bmp copy command list desc unexpected\n");
                success = false;
            }
        }

        auto uploadResourcesFinished = resourceUpload.End(m_copyQueue.Get());
        uploadResourcesFinished.wait();
    }

    // Compute Queue test
    {
        ResourceUploadBatch resourceUpload(device);

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

        resourceUpload.Begin(queueDesc.Type);

        DX::ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_GRAPHICS_PPV_ARGS(m_computeQueue.ReleaseAndGetAddressOf())));

        m_computeQueue->SetName(L"ComputeTest");

        DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"earth_A2B10G10R10_autogen.dds", 0,
            D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_MIP_AUTOGEN, m_earth2.ReleaseAndGetAddressOf()));

        {
            auto desc = m_earth2->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM
                || desc.Width != 512
                || desc.Height != 256
                || desc.MipLevels != 10)
            {
                if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R10G10B10A2_UNORM)
                    && desc.MipLevels == 1)
                {
                    OutputDebugStringA("NOTE: earth_A2B10G10R10_autogen.dds - device doesn't support autogen mips for 10:10:10:2\n");
                }
                else
                {
                    OutputDebugStringA("FAILED: earth_A2B10G10R10_autogen.dds compute queue desc unexpected\n");
                    success = false;
                }
            }
        }

        CreateShaderResourceView(device, m_earth2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth_Imm));

        DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"win95.bmp", m_computeTest.ReleaseAndGetAddressOf(), true));

        {
            auto desc = m_computeTest->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 9)
            {
                if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R8G8B8A8_UNORM)
                    && desc.MipLevels == 1)
                {
                    // Message was already output above
                }
                else
                {
                    OutputDebugStringA("FAILED: win95.bmp (autogen) compute queu desc unexpected\n");
                    success = false;
                }
            }
        }

        auto uploadResourcesFinished = resourceUpload.End(m_computeQueue.Get());
        uploadResourcesFinished.wait();
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 eyePosition = { 0.0f, 3.0f, -6.0f, 0.0f };
    static const XMVECTORF32 At = { 0.0f, 1.0f, 0.0f, 0.0f };
    static const XMVECTORF32 Up = { 0.0f, 1.0f, 0.0f, 0.0f };

    auto size = m_deviceResources->GetOutputSize();
    float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(eyePosition, At, Up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.01f, 100.0f);
#else
    XMMATRIX view = XMMatrixLookAtRH(eyePosition, At, Up);
    XMMATRIX projection = XMMatrixPerspectiveFovRH(XM_PIDIV4, aspect, 0.01f, 100.0f);
#endif

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        projection *= orient;
    }
#endif

    m_effect->SetView(view);
    m_effect->SetProjection(projection);
}

#if !defined(_XBOX_ONE) || !defined(_TITLE)
void Game::OnDeviceLost()
{
    m_earth.Reset();
    m_earth2.Reset();
    m_dxlogo.Reset();
    m_dxlogo2.Reset();
    m_win95.Reset();
    m_win95_2.Reset();

    m_test1.Reset();
    m_test2.Reset();
    m_test3.Reset();
    m_test4.Reset();
    m_test5.Reset();
    m_test6.Reset();
    m_test7.Reset();
    m_test8.Reset();
    m_test9.Reset();
    m_test10.Reset();
    m_test11.Reset();
    m_test12.Reset();
    m_test13.Reset();
    m_test14.Reset();
    m_test15.Reset();
    m_test16.Reset();
    m_test17.Reset();
    m_test18.Reset();
    m_test19.Reset();
    m_test20.Reset();
    m_test21.Reset();
    m_test22.Reset();
    m_test23.Reset();
    m_test24.Reset();
    m_test25.Reset();
    m_test26.Reset();

    m_copyTest.Reset();
    m_computeTest.Reset();

    m_screenshot.Reset();

    m_cube.reset();
    m_effect.reset();
    m_resourceDescriptors.reset();
    m_states.reset();
    m_graphicsMemory.reset();

    m_copyQueue.Reset();
    m_computeQueue.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion

void Game::UnitTests(ResourceUploadBatch& resourceUpload, bool success)
{
    auto device = m_deviceResources->GetD3DDevice();

    // DirectX Logo (verify DDS for autogen has no mipmaps)
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen_bgra.dds", m_test1.ReleaseAndGetAddressOf(), false));
    {
        auto desc = m_test1->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: dx5_logo_autogen_bgra.dds (no autogen) desc unexpected\n");
            success = false;
        }
    }

    // DirectX Logo (verify DDS is BC1 without sRGB)
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo.dds", m_test2.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test2->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_BC1_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: dx5_logo.dds (BC1) desc unexpected\n");
            success = false;
        }
    }

    // Windows 95 logo
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"win95.bmp", m_test3.ReleaseAndGetAddressOf(), false));

    {
        auto desc = m_test3->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: win95.bmp desc unexpected\n");
            success = false;
        }
    }

    // Alpha mode test
    DDS_ALPHA_MODE alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"tree02S_pmalpha.dds", m_test4.ReleaseAndGetAddressOf(), false, 0, &alphaMode));
    
    {
        if (alphaMode != DDS_ALPHA_MODE_PREMULTIPLIED)
        {
            OutputDebugStringA("FAILED: tree02S_pmalpha.dds alpha mode unexpected\n");
            success = false;
        }

        auto desc = m_test4->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 304
            || desc.Height != 268
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: tree02S_pmalpha.dds desc unexpected\n");
            success = false;
        }
    }

    // 1D texture
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1D_MipOff.dds", m_test5.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test5->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1D_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // 1D texture array
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1DArray_MipOff.dds", m_test6.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test6->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.MipLevels != 1
            || desc.DepthOrArraySize != 6)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1DArray_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // 2D texture array
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE2DArray_MipOff.dds", m_test7.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test7->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.Height != 128
            || desc.MipLevels != 1
            || desc.DepthOrArraySize != 6)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE2DArray_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // 3D texture
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE3D_MipOff.dds", m_test8.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test8->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 32
            || desc.Height != 128
            || desc.MipLevels != 1
            || desc.DepthOrArraySize != 32)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE3D_MipOff.dds desc unexpected\n");
            success = false;
        }
    }

    // DX12 version doesn't support auto-gen mips for 1D, 1D arrays, 2D arrays, or 3D. They throw an exception instead of being ignored like on DX11

    // sRGB test
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"cup_small.jpg", m_test9.ReleaseAndGetAddressOf(), false));

    {
        auto desc = m_test9->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 512
            || desc.Height != 683
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"cup_small.jpg", m_test10.ReleaseAndGetAddressOf(), true));

    {
        auto desc = m_test10->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 512
            || desc.Height != 683
            || desc.MipLevels != 10)
        {
            if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                && desc.MipLevels == 1)
            {
                OutputDebugStringA("NOTE: cup_small.jpg - device doesn't support autogen mips for 8:8:8:8\n");
            }
            else
            {
                OutputDebugStringA("FAILED: cup_small.jpg (autogen) desc unexpected\n");
                success = false;
            }
        }
    }

    DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"cup_small.jpg", 0,
        D3D12_RESOURCE_FLAG_NONE, WIC_LOADER_IGNORE_SRGB, m_test11.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test11->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 512
            || desc.Height != 683
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg (ignore srgb) desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"cup_small.jpg", 0,
        D3D12_RESOURCE_FLAG_NONE, WIC_LOADER_IGNORE_SRGB | WIC_LOADER_MIP_AUTOGEN, m_test12.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test12->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 512
            || desc.Height != 683
            || desc.MipLevels != 10)
        {
            if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R8G8B8A8_UNORM)
                && desc.MipLevels == 1)
            {
                OutputDebugStringA("NOTE: cup_small.jpg - device doesn't support autogen mips for 8:8:8:8\n");
            }
            else
            {
                OutputDebugStringA("FAILED: cup_small.jpg (autogen ignore srgb) desc unexpected\n");
                success = false;
            }
        }
    }

    // Video textures
#if !defined(_XBOX_ONE) || !defined(_TITLE)
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"lenaNV12.dds", m_test13.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test13->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_NV12
            || desc.Width != 200
            || desc.Height != 200
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: lenaNV12.dds desc unexpected\n");
            success = false;
        }
    }
#endif

    // Autogen
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen.dds", m_test14.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test14->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: dx5_logo_autogen.dds desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen.dds", m_test15.ReleaseAndGetAddressOf(), true));

    {
        auto desc = m_test15->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R8G8B8A8_UNORM)
                && desc.MipLevels == 1)
            {
                OutputDebugStringA("NOTE: dx5_logo_autogen.dds - device doesn't support autogen mips for 8:8:8:8\n");
            }
            else
            {
                OutputDebugStringA("FAILED: dx5_logo_autogen.dds (autogen) desc unexpected\n");
                success = false;
            }
        }
    }

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen_srgb.dds", m_test16.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test16->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: dx5_logo_autogen_srgb.dds desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen_srgb.dds", m_test17.ReleaseAndGetAddressOf(), true));

    {
        auto desc = m_test17->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            if (!resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                && desc.MipLevels == 1)
            {
                OutputDebugStringA("NOTE: dx5_logo_autogen_srgb.dd - device doesn't support autogen mips for 8:8:8:8\n");
            }
            else
            {
                OutputDebugStringA("FAILED: dx5_logo_autogen_srgb.dds (autogen) desc unexpected\n");
                success = false;
            }
        }
    }

    // WIC load without format conversion or resize
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"testpattern.png", m_test18.ReleaseAndGetAddressOf()));

    {
        auto desc = m_test18->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
            || desc.Width != 1280
            || desc.Height != 1024
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: testpattern.png desc unexpected\n");
            success = false;
        }
    }

    // WIC load with resize
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"testpattern.png", m_test19.ReleaseAndGetAddressOf(), false, 1024));

    {
        auto desc = m_test19->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
            || desc.Width != 1024
            || desc.Height != 819
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: testpattern.png resize desc unexpected\n");
            success = false;
        }
    }

    // WIC load with resize and format conversion
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"cup_small.jpg", m_test20.ReleaseAndGetAddressOf(), false, 256));

    {
        auto desc = m_test20->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 191
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg resize desc unexpected\n");
            success = false;
        }
    }

    // DDS load with auto-gen request ignore (BC1 is not supported for GenerateMips)
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_nomips.dds", m_test21.ReleaseAndGetAddressOf(), true));
    {
        auto desc = m_test21->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_BC1_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: dx5_logo_nomips.dds (ignore autogen) desc unexpected\n");
            success = false;
        }
    }

    // WIC load with auto-gen request ignore (16bpp is not usually supported for GenerateMips)
    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"grad4d_a1r5g5b5.bmp", m_test22.ReleaseAndGetAddressOf(), true));
    {
        auto desc = m_test22->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B5G5R5A1_UNORM
            || desc.Width != 32
            || desc.Height != 32)
        {
            OutputDebugStringA("FAILED: grad4d_a1r5g5b5.bmp (ignore autogen) desc unexpected\n");
            success = false;
        }
        else if ((desc.MipLevels == 6) && resourceUpload.IsSupportedForGenerateMips(DXGI_FORMAT_B5G5R5A1_UNORM))
        {
            OutputDebugStringA("NOTE: grad4d_a1r5g5b5.bmp - this device supports 16bpp autogen mips\n");
        }
        else if (desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: grad4d_a1r5g5b5.bmp (ignore autogen) desc unexpected\n");
            success = false;
        }
    }

    // DDS load (force sRGB on 10:10:10:2)
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"earth_A2B10G10R10.dds",
        0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_FORCE_SRGB, m_test23.ReleaseAndGetAddressOf()));
    {
        // forceSRGB has no effect for 10:10:10:2

        auto desc = m_test23->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM
            || desc.Width != 512
            || desc.Height != 256
            || desc.MipLevels != 10)
        {
            OutputDebugStringA("FAILED: earth_A2B10G10R10.dds (force sRGB) desc unexpected\n");
            success = false;
        }
    }

    // DDS load of R32F (autogen)
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"windowslogo_r32f.dds",
        0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_MIP_AUTOGEN, m_test24.ReleaseAndGetAddressOf()));
    {
        auto desc = m_test24->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R32_FLOAT
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: windowslogo_r32f.dds (autogen) desc unexpected\n");
            success = false;
        }
    }

    // From memory
    {
        auto blob = DX::ReadData(L"dx5_logo.dds");

        DX::ThrowIfFailed(CreateDDSTextureFromMemory(device, resourceUpload, blob.data(), blob.size(), m_test25.ReleaseAndGetAddressOf()));

        {
            auto desc = m_test25->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_BC1_UNORM
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 9)
            {
                OutputDebugStringA("FAILED: dx5_logo.dds (mem) desc unexpected\n");
                success = false;
            }
        }
    }

    {
        auto blob = DX::ReadData(L"win95.bmp");

        DX::ThrowIfFailed(CreateWICTextureFromMemory(device, resourceUpload, blob.data(), blob.size(), m_test26.ReleaseAndGetAddressOf(), false));

        {
            auto desc = m_test26->GetDesc();
            if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
                || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
                || desc.Width != 256
                || desc.Height != 256
                || desc.MipLevels != 1)
            {
                OutputDebugStringA("FAILED: win95.bmp (mem) desc unexpected\n");
                success = false;
            }
        }
    }

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");
}
