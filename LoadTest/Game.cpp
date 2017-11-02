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

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#include <Windows.ApplicationModel.h>
#include <Windows.Storage.h>
#endif

#pragma warning( disable : 4238 )

//#define GAMMA_CORRECT_RENDERING

// Build for LH vs. RH coords
//#define LH_COORDS

namespace
{
    float dist = 10.f;
};

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

// Constructor.
Game::Game() :
    m_frame(0)
{
#ifdef GAMMA_CORRECT_RENDERING
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>();
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
#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
    m_deviceResources->SetWindow(window, width, height, rotation);
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#else
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
#endif

    m_deviceResources->CreateDeviceResources();
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

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatPng, sspng,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT));

        if (GetFileAttributesW(sspng) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.PNG\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.PNG!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatJpeg, ssjpg,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT));

        if (GetFileAttributesW(ssjpg) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.JPG\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.JPG!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatBmp, ssbmp,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
            &GUID_WICPixelFormat16bppBGR565));

        if (GetFileAttributesW(ssbmp) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.BMP\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.BMP!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveWICTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(),
            GUID_ContainerFormatTiff, sstif,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
            nullptr,
            [&](IPropertyBag2* props)
        {
            PROPBAG2 options[2] = { 0, 0 };
            options[0].pstrName = L"CompressionQuality";
            options[1].pstrName = L"TiffCompressionMethod";

            VARIANT varValues[2];
            varValues[0].vt = VT_R4;
            varValues[0].fltVal = 0.75f;

            varValues[1].vt = VT_UI1;
            varValues[1].bVal = WICTiffCompressionNone;

            (void)props->Write(2, options, varValues);
        }));

        if (GetFileAttributesW(sstif) != INVALID_FILE_ATTRIBUTES)
        {
            OutputDebugStringA("Wrote SCREENSHOT.TIF\n");
        }
        else
        {
            OutputDebugStringA("ERROR: Missing SCREENSHOT.TIF!\n");
            success = false;
        }

        DX::ThrowIfFailed(SaveDDSTextureToFile(m_deviceResources->GetCommandQueue(), m_screenshot.Get(), ssdds,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT));

        if (GetFileAttributesW(ssdds) != INVALID_FILE_ATTRIBUTES)
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
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();
}

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

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    // View textures
    bool success = true;
    OutputDebugStringA("*********** UINT TESTS BEGIN ***************\n");

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    // Earth
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

    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"earth_A2B10G10R10.dds", 0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_FORCE_SRGB, m_earth2.ReleaseAndGetAddressOf()));

    {
        // forceSRGB has no effect for 10:10:10:2

        auto desc = m_earth2->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM
            || desc.Width != 512
            || desc.Height != 256
            || desc.MipLevels != 10)
        {
            OutputDebugStringA("FAILED: earth_A2B10G10R10.dds (2) desc unexpected\n");
            success = false;
        }
    }

    CreateShaderResourceView(device, m_earth2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth_Imm));

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
            OutputDebugStringA("FAILED: dx5_logo_autogen_bgra.dds (autogen) desc unexpected\n");
            success = false;
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
            OutputDebugStringA("FAILED: win95.bmp (autogen) desc unexpected\n");
            success = false;
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
            OutputDebugStringA("FAILED: win95.bmp (sRGB) desc unexpected\n");
            success = false;
        }
    }

    CreateShaderResourceView(device, m_win95_2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Windows95_sRGB));

    UnitTests(resourceUpload, success);

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();
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
    XMMATRIX orient = XMLoadFloat4x4(&m_deviceResources->GetOrientationTransform3D());
    projection *= orient;
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

    m_screenshot.Reset();

    m_cube.reset();
    m_effect.reset();
    m_resourceDescriptors.reset();
    m_states.reset();
    m_graphicsMemory.reset();
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
            OutputDebugStringA("FAILED: cup_small.jpg (autogen) desc unexpected\n");
            success = false;
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
            OutputDebugStringA("FAILED: cup_small.jpg (autogen ignore srgb) desc unexpected\n");
            success = false;
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
            OutputDebugStringA("FAILED: dx5_logo_autogen.dds (autogen) desc unexpected\n");
            success = false;
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
            OutputDebugStringA("FAILED: dx5_logo_autogen_srgb.dds (autogen) desc unexpected\n");
            success = false;
        }
    }

    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");
}
