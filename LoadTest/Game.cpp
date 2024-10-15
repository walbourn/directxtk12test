//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK DDSTextureLoader & WICTextureLoader
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include "ReadData.h"

#ifdef UWP
#include <Windows.ApplicationModel.h>
#include <Windows.Storage.h>
#endif

#define GAMMA_CORRECT_RENDERING
#define USE_COPY_QUEUE
#define USE_COMPUTE_QUEUE

// Build for LH vs. RH coords
//#define LH_COORDS

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float dist = 10.f;

#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
};


//--------------------------------------------------------------------------------------

static_assert(std::is_nothrow_move_constructible<DescriptorHeap>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DescriptorHeap>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<DescriptorPile>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<DescriptorPile>::value, "Move Assign.");

//--------------------------------------------------------------------------------------

// Constructor.
Game::Game() noexcept(false) :
    m_frame(0),
    m_firstFrame(false)
{
#ifdef GAMMA_CORRECT_RENDERING
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

#ifdef XBOX
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2,
        DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat);
#endif

#ifdef _GAMING_XBOX
    m_deviceResources->SetClearColor(c_clearColor);
#endif

#ifdef LOSTDEVICE
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
#ifdef COREWINDOW
    IUnknown* window,
#else
    HWND window,
#endif
    int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();

#ifdef XBOX
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
#ifdef COREWINDOW
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));
#endif
#elif defined(UWP)
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

#ifndef XBOX
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
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

#ifdef _GAMING_XBOX
    m_deviceResources->WaitForOrigin();
#endif

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    ++m_frame;
}

// Updates the world.
void Game::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

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

    if (m_firstFrame)
    {
        //
        // This is not strictly needed on Windows due to common state promotion, but this behavior is optional on Xbox
        //

        // Copy queue resources are left in D3D12_RESOURCE_STATE_COPY_DEST state
        #ifdef USE_COPY_QUEUE
        TransitionResource(commandList, m_earth.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        #endif

        // Compute queue resources are left in the D3D12_RESOURCE_STATE_COPY_DEST state
        #ifdef USE_COMPUTE_QUEUE
        TransitionResource(commandList, m_earth2.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        #endif

        m_firstFrame = false;
    }

    // Set the descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

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
    std::ignore = m_graphicsMemory->GetStatistics();

    PIXEndEvent(m_deviceResources->GetCommandQueue());

    if (m_screenshot)
    {
        // We take the shot here to cope with lost device

        OutputDebugStringA("******** SCREENSHOT TEST BEGIN *************\n");

        bool success = true;

#ifdef XBOX
        const wchar_t sspath[MAX_PATH] = L"T:\\";
#elif defined(UWP)
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
                throw std::runtime_error("TemporaryFolder");
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

        std::ignore = swprintf_s(sspng, L"%ls\\SCREENSHOT.PNG", sspath);
        std::ignore = swprintf_s(ssjpg, L"%ls\\SCREENSHOT.JPG", sspath);
        std::ignore = swprintf_s(ssbmp, L"%ls\\SCREENSHOT.BMP", sspath);
        std::ignore = swprintf_s(sstif, L"%ls\\SCREENSHOT.TIF", sspath);
        std::ignore = swprintf_s(ssdds, L"%ls\\SCREENSHOT.DDS", sspath);

        std::ignore = DeleteFileW(sspng);
        std::ignore = DeleteFileW(ssjpg);
        std::ignore = DeleteFileW(ssbmp);
        std::ignore = DeleteFileW(sstif);
        std::ignore = DeleteFileW(ssdds);

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
            PROPBAG2 options[2] = {};
            options[0].pstrName = const_cast<wchar_t*>(L"CompressionQuality");
            options[1].pstrName = const_cast<wchar_t*>(L"TiffCompressionMethod");

            VARIANT varValues[2];
            varValues[0].vt = VT_R4;
            varValues[0].fltVal = 0.75f;

            varValues[1].vt = VT_UI1;
            varValues[1].bVal = WICTiffCompressionNone;

            std::ignore = props->Write(2, options, varValues);
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
    auto const rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto const dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, c_clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto const viewport = m_deviceResources->GetScreenViewport();
    auto const scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnSuspending()
{
    m_deviceResources->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_timer.ResetElapsedTime();
}

#ifdef PC
void Game::OnWindowMoved()
{
    auto const r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}
#endif

#if defined(PC) || defined(UWP)
void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}
#endif

#ifndef XBOX
void Game::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
#ifdef UWP
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

#ifdef UWP
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

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

    {
        const EffectPipelineStateDescription pd(
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

        // DescriptorHeap.h
        // TODO - ctor from existing
        // TODO - ctor from DESC
        // TODO - DefaultDesc
        // TODO - WriteDescriptors x3

        m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
            Descriptors::Count);

        m_renderDescriptors = std::make_unique<DescriptorHeap>(device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            RTDescriptors::RTCount);

        // Earth
#ifndef USE_COPY_QUEUE
        DDS_ALPHA_MODE alphaMode = DDS_ALPHA_MODE_UNKNOWN;
        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"earth_A2B10G10R10.dds", m_earth.ReleaseAndGetAddressOf(), false, 0, &alphaMode));

        {
            if (alphaMode != DDS_ALPHA_MODE_UNKNOWN)
            {
                OutputDebugStringA("FAILED: earth_A2B10G10R10.dds alpha mode value unexpected\n");
                success = false;
            }

        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_earth->GetDesc(&desc);
        #else
            auto const desc = m_earth->GetDesc();
        #endif
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
#endif // !USE_COPY_QUEUE

#ifndef USE_COMPUTE_QUEUE
        DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"earth_A2B10G10R10_autogen.dds", 0,
            D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_MIP_AUTOGEN, m_earth2.ReleaseAndGetAddressOf()));

        {
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_earth2->GetDesc(&desc);
        #else
            auto const desc = m_earth2->GetDesc();
        #endif
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
                    OutputDebugStringA("FAILED: earth_A2B10G10R10_autogen.dds desc unexpected\n");
                    success = false;
                }
            }
        }

        CreateShaderResourceView(device, m_earth2.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::Earth_Imm));
#endif // !USE_COMPUTE_QUEUE

        // DirectX Logo
        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen_bgra.dds", m_dxlogo.ReleaseAndGetAddressOf(), true));

        {
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_dxlogo->GetDesc(&desc);
        #else
            auto const desc = m_dxlogo->GetDesc();
        #endif
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_dxlogo2->GetDesc(&desc);
        #else
            auto const desc = m_dxlogo2->GetDesc();
        #endif
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_win95->GetDesc(&desc);
        #else
            auto const desc = m_win95->GetDesc();
        #endif
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_win95_2->GetDesc(&desc);
        #else
            auto const desc = m_win95_2->GetDesc();
        #endif
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
#ifdef USE_COPY_QUEUE
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

        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_earth->GetDesc(&desc);
        #else
            auto const desc = m_earth->GetDesc();
        #endif
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_copyTest->GetDesc(&desc);
        #else
            auto const desc = m_copyTest->GetDesc();
        #endif
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

        m_firstFrame = true;
    }
#endif // USE_COPY_QUEUE

    // Compute Queue test
#ifdef USE_COMPUTE_QUEUE
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_earth2->GetDesc(&desc);
        #else
            auto const desc = m_earth2->GetDesc();
        #endif
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_computeTest->GetDesc(&desc);
        #else
            auto const desc = m_computeTest->GetDesc();
        #endif
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

        m_firstFrame = true;
    }
#endif // USE_COMPUTE_QUEUE

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    static const XMVECTORF32 eyePosition = { { { 0.0f, 3.0f, -6.0f, 0.0f } } };
    static const XMVECTORF32 At = { { { 0.0f, 1.0f, 0.0f, 0.0f } } };
    static const XMVECTORF32 Up = { { { 0.0f, 1.0f, 0.0f, 0.0f } } };

    auto const size = m_deviceResources->GetOutputSize();
    const float aspect = (float)size.right / (float)size.bottom;

#ifdef LH_COORDS
    XMMATRIX view = XMMatrixLookAtLH(eyePosition, At, Up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.01f, 100.0f);
#else
    XMMATRIX view = XMMatrixLookAtRH(eyePosition, At, Up);
    XMMATRIX projection = XMMatrixPerspectiveFovRH(XM_PIDIV4, aspect, 0.01f, 100.0f);
#endif

#ifdef UWP
    {
        auto orient3d = m_deviceResources->GetOrientationTransform3D();
        XMMATRIX orient = XMLoadFloat4x4(&orient3d);
        projection *= orient;
    }
#endif

    m_effect->SetView(view);
    m_effect->SetProjection(projection);
}

#ifdef LOSTDEVICE
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
    m_test27.Reset();
    m_test28.Reset();
    m_test29.Reset();
    m_test30.Reset();
    m_test31.Reset();
    m_test32.Reset();
    m_test33.Reset();
    m_test34.Reset();
    m_test35.Reset();
    m_test36.Reset();
    m_test37.Reset();
    m_test38.Reset();
    m_test39.Reset();
    m_test40.Reset();
    m_test41.Reset();
    m_test42.Reset();

    m_testA.Reset();
    m_testB.Reset();
    m_testC.Reset();
    m_testD.Reset();
    m_testE.Reset();
    m_testF.Reset();

    m_copyTest.Reset();
    m_computeTest.Reset();

    m_screenshot.Reset();

    m_cube.reset();
    m_effect.reset();
    m_resourceDescriptors.reset();
    m_renderDescriptors.reset();
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

    //----------------------------------------------------------------------------------
    // CreateTextureFromMemory 1D
    {
        static const uint32_t s_pixels[4] = { 0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff };

        D3D12_SUBRESOURCE_DATA initData = { s_pixels, 0, 0 };

        DX::ThrowIfFailed(CreateTextureFromMemory(device, resourceUpload, 4u, DXGI_FORMAT_B8G8R8A8_UNORM, initData,
            m_testA.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_testA->GetDesc(&desc);
    #else
        auto const desc = m_testA->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 4
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: CreateTextureFromMemory 1D res desc unexpected\n");
            success = false;
        }
    }

    // CreateTextureFromMemory 2D
    {
        static const uint32_t s_pixels[16] = {
            0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffff, 0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff,
            0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffff, 0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff,
        };

        D3D12_SUBRESOURCE_DATA initData = { s_pixels, sizeof(uint32_t) * 8, 0 };

        DX::ThrowIfFailed(CreateTextureFromMemory(device, resourceUpload, 8u, 2u, DXGI_FORMAT_B8G8R8A8_UNORM, initData,
            m_testB.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_testB->GetDesc(&desc);
    #else
        auto desc = m_testB->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 8
            || desc.Height != 2
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: CreateTextureFromMemory 2D res desc unexpected\n");
            success = false;
        }

        initData = { s_pixels, sizeof(uint32_t) * 4, 0 };

        DX::ThrowIfFailed(CreateTextureFromMemory(device, resourceUpload, 4u, 4u, DXGI_FORMAT_R8G8B8A8_UNORM, initData,
            m_testC.ReleaseAndGetAddressOf()));

    #ifdef __MINGW32__
        std::ignore = m_testC->GetDesc(&desc);
    #else
        desc = m_testC->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 4
            || desc.Height != 4
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: CreateTextureFromMemory 2Db res desc unexpected\n");
            success = false;
        }
    }

    // CreateTextureFromMemory 2D (autogen)
    {
        auto pixels = std::make_unique<uint32_t[]>(256 * 256 * sizeof(uint32_t));
        memset(pixels.get(), 0xff, 256 * 256 * sizeof(uint32_t));

        D3D12_SUBRESOURCE_DATA initData = { pixels.get(), sizeof(uint32_t) * 256, 0 };

        DX::ThrowIfFailed(CreateTextureFromMemory(device, resourceUpload, 256u, 256u, DXGI_FORMAT_R8G8B8A8_UNORM, initData,
            m_testD.GetAddressOf(), true));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_testD->GetDesc(&desc);
    #else
        auto const desc = m_testD->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 9)
        {
            OutputDebugStringA("FAILED: CreateTextureFromMemory 2D autogen res desc unexpected\n");
            success = false;
        }
    }

    // CreateTextureFromMemory 3D
    {
        static const uint32_t s_pixels[16] = {
            0xff0000ff, 0xff0000ff,
            0xff0000ff, 0xff0000ff,
            0xff00ff00, 0xff00ff00,
            0xff00ff00, 0xff00ff00,
            0xffff0000, 0xffff0000,
            0xffff0000, 0xffff0000,
            0xffffffff, 0xffffffff,
            0xffffffff, 0xffffffff,
        };

        D3D12_SUBRESOURCE_DATA initData = { s_pixels, sizeof(uint32_t) * 2, sizeof(uint32_t) * 4 };

        DX::ThrowIfFailed(CreateTextureFromMemory(device, resourceUpload, 2u, 2u, 4, DXGI_FORMAT_B8G8R8A8_UNORM, initData,
            m_testE.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_testE->GetDesc(&desc);
    #else
        auto desc = m_testE->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D
            || desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM
            || desc.Width != 2
            || desc.Height != 2
            || desc.DepthOrArraySize != 4
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: CreateTextureFromMemory 3D res desc unexpected\n");
            success = false;
        }

        initData = { s_pixels, sizeof(uint32_t) * 4, sizeof(uint32_t) * 8 };

        DX::ThrowIfFailed(CreateTextureFromMemory(device, resourceUpload, 4u, 2u, 2u, DXGI_FORMAT_R8G8B8A8_UNORM, initData,
            m_testF.ReleaseAndGetAddressOf()));

    #ifdef __MINGW32__
        std::ignore = m_testF->GetDesc(&desc);
    #else
        desc = m_testF->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 4
            || desc.Height != 2
            || desc.DepthOrArraySize != 2
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: CreateTextureFromMemory 3Db res desc unexpected\n");
            success = false;
        }
    }

    // DirectX Logo (verify DDS for autogen has no mipmaps)
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"dx5_logo_autogen_bgra.dds", m_test1.ReleaseAndGetAddressOf(), false));
    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test1->GetDesc(&desc);
    #else
        auto const desc = m_test1->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test2->GetDesc(&desc);
    #else
        auto const desc = m_test2->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test3->GetDesc(&desc);
    #else
        auto const desc = m_test3->GetDesc();
    #endif
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

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test4->GetDesc(&desc);
    #else
        auto const desc = m_test4->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test5->GetDesc(&desc);
    #else
        auto const desc = m_test5->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test6->GetDesc(&desc);
    #else
        auto const desc = m_test6->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test7->GetDesc(&desc);
    #else
        auto const desc = m_test7->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test8->GetDesc(&desc);
    #else
        auto const desc = m_test8->GetDesc();
    #endif
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

    // SkipMips
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"world8192.dds", m_test40.ReleaseAndGetAddressOf()));

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test40->GetDesc(&desc);
    #else
        auto const desc = m_test40->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_BC1_UNORM
            || desc.Width != 8192
            || desc.Height != 4096
            || desc.MipLevels != 14
            || desc.DepthOrArraySize != 1)
        {
            OutputDebugStringA("FAILED: world8192.dds desc unexpected\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"world8192.dds", m_test41.ReleaseAndGetAddressOf(), false, 2048));

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test41->GetDesc(&desc);
    #else
        auto const desc = m_test41->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_BC1_UNORM
            || desc.Width != 2048
            || desc.Height != 1024
            || desc.MipLevels != 12
            || desc.DepthOrArraySize != 1)
        {
            OutputDebugStringA("FAILED: world8192.dds skipmips desc unexpected\n");
            success = false;
        }
    }

    // Ignore mips
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(
        device, resourceUpload, L"world8192.dds",
        0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_IGNORE_MIPS,
        m_test42.ReleaseAndGetAddressOf())
        );

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test42->GetDesc(&desc);
    #else
        auto const desc = m_test42->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_BC1_UNORM
            || desc.Width != 8192
            || desc.Height != 4096
            || desc.MipLevels != 1
            || desc.DepthOrArraySize != 1)
        {
            OutputDebugStringA("FAILED: world8192.dds desc unexpected\n");
            success = false;
        }
    }

    // DX12 version doesn't support auto-gen mips for 1D, 1D arrays, 2D arrays, or 3D. They throw an exception instead of being ignored like on DX11

    // sRGB test
    DX::ThrowIfFailed(CreateDDSTextureFromFileEx(device, resourceUpload, L"io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1D_MipOff.dds",
        0, D3D12_RESOURCE_FLAG_NONE, DDS_LOADER_IGNORE_SRGB, m_test36.ReleaseAndGetAddressOf()));

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test36->GetDesc(&desc);
    #else
        auto const desc = m_test36->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 32
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: io_R8G8B8A8_UNORM_SRGB_SRV_DIMENSION_TEXTURE1D_MipOff.dds desc unexpected for IGNORE_SRGB\n");
            success = false;
        }
    }

    DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"cup_small.jpg", m_test9.ReleaseAndGetAddressOf(), false));

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test9->GetDesc(&desc);
    #else
        auto const desc = m_test9->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test10->GetDesc(&desc);
    #else
        auto const desc = m_test10->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test11->GetDesc(&desc);
    #else
        auto const desc = m_test11->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test12->GetDesc(&desc);
    #else
        auto const desc = m_test12->GetDesc();
    #endif
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
#ifndef XBOX
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, resourceUpload, L"lenaNV12.dds", m_test13.ReleaseAndGetAddressOf()));

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test13->GetDesc(&desc);
    #else
        auto const desc = m_test13->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test14->GetDesc(&desc);
    #else
        auto const desc = m_test14->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test15->GetDesc(&desc);
    #else
        auto const desc = m_test15->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test16->GetDesc(&desc);
    #else
        auto const desc = m_test16->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test17->GetDesc(&desc);
    #else
        auto const desc = m_test17->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test18->GetDesc(&desc);
    #else
        auto const desc = m_test18->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test19->GetDesc(&desc);
    #else
        auto const desc = m_test19->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test20->GetDesc(&desc);
    #else
        auto const desc = m_test20->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test21->GetDesc(&desc);
    #else
        auto const desc = m_test21->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test22->GetDesc(&desc);
    #else
        auto const desc = m_test22->GetDesc();
    #endif
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

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test23->GetDesc(&desc);
    #else
        auto const desc = m_test23->GetDesc();
    #endif
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
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test24->GetDesc(&desc);
    #else
        auto const desc = m_test24->GetDesc();
    #endif
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_test25->GetDesc(&desc);
        #else
            auto const desc = m_test25->GetDesc();
        #endif
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
        #ifdef __MINGW32__
            D3D12_RESOURCE_DESC desc;
            std::ignore = m_test26->GetDesc(&desc);
        #else
            auto const desc = m_test26->GetDesc();
        #endif
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

    // WIC force RGBA32
    {
        DX::ThrowIfFailed(CreateWICTextureFromFile(device, resourceUpload, L"pentagon.tiff", m_test27.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test27->GetDesc(&desc);
    #else
        auto desc = m_test27->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8_UNORM
            || desc.Width != 1024
            || desc.Height != 1024
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: pentagon.tiff res desc unexpected\n");
            success = false;
        }

        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"pentagon.tiff",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FORCE_RGBA32,
            m_test28.GetAddressOf()));

    #ifdef __MINGW32__
        std::ignore = m_test28->GetDesc(&desc);
    #else
        desc = m_test28->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 1024
            || desc.Height != 1024
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: pentagon.tiff res desc rgba32 unexpected\n");
            success = false;
        }
    }

    // WIC SQUARE flags
    {
        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"cup_small.jpg",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_MAKE_SQUARE,
            m_test29.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test29->GetDesc(&desc);
    #else
        auto const desc = m_test29->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 683
            || desc.Height != 683
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg square res desc unexpected\n");
            success = false;
        }
    }

    // WIC POW2 flags
    {
        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"cup_small.jpg",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FIT_POW2,
            m_test30.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test30->GetDesc(&desc);
    #else
        auto const desc = m_test30->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 256
            || desc.Height != 512
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg pow2 res desc unexpected\n");
            success = false;
        }
    }

    // WIC POW2 + SQUARE flags
    {
        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"cup_small.jpg",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FIT_POW2 | WIC_LOADER_MAKE_SQUARE,
            m_test31.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test31->GetDesc(&desc);
    #else
        auto const desc = m_test31->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 512
            || desc.Height != 512
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: cup_small.jpg pow2+square res desc unexpected\n");
            success = false;
        }
    }

    // WIC SRGB_DEFAULT + WIC RGBA32
    {
        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"pentagon.tiff",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FORCE_RGBA32 | WIC_LOADER_SRGB_DEFAULT,
            m_test32.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test32->GetDesc(&desc);
    #else
        auto const desc = m_test32->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            || desc.Width != 1024
            || desc.Height != 1024
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: pentagon.tiff res desc default srgb / rgba32 unexpected\n");
            success = false;
        }
    }

    // WIC RGBA32 + POW2 + SQUARE
    {
        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"text.tif",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_DEFAULT,
            m_test33.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test33->GetDesc(&desc);
    #else
        auto desc = m_test33->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8_UNORM
            || desc.Width != 1512
            || desc.Height != 359
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: text.tif res desc unexpected\n");
            success = false;
        }

        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"text.tif",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FORCE_RGBA32 | WIC_LOADER_FIT_POW2,
            m_test34.GetAddressOf()));

    #ifdef __MINGW32__
        std::ignore = m_test34->GetDesc(&desc);
    #else
        desc = m_test34->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 1024
            || desc.Height != 256
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: text.tif res rgba32+pow2 unexpected\n");
            success = false;
        }

        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"text.tif",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FORCE_RGBA32 | WIC_LOADER_FIT_POW2 | WIC_LOADER_MAKE_SQUARE,
            m_test35.GetAddressOf()));

    #ifdef __MINGW32__
        std::ignore = m_test35->GetDesc(&desc);
    #else
        desc = m_test35->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 1024
            || desc.Height != 1024
            || desc.MipLevels != 1)
        {
            OutputDebugStringA("FAILED: text.tif res rgba32+pow2+square unexpected\n");
            success = false;
        }
    }

    // UAV
    DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"win95.bmp", 0,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        WIC_LOADER_DEFAULT, m_test37.ReleaseAndGetAddressOf()));

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test37->GetDesc(&desc);
    #else
        auto const desc = m_test37->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1
            || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
        {
            OutputDebugStringA("FAILED: win95.bmp (UAV) desc unexpected\n");
            success = false;
        }
    }

    CreateUnorderedAccessView(device, m_test37.Get(), m_resourceDescriptors->GetCpuHandle(Win95_UAV));

    // RTV
    DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"win95.bmp", 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        WIC_LOADER_DEFAULT, m_test38.ReleaseAndGetAddressOf()));

    {
    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test38->GetDesc(&desc);
    #else
        auto const desc = m_test38->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 256
            || desc.Height != 256
            || desc.MipLevels != 1
            || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == 0)
        {
            OutputDebugStringA("FAILED: win95.bmp (RTV) desc unexpected\n");
            success = false;
        }
    }

    CreateRenderTargetView(device, m_test38.Get(), m_renderDescriptors->GetCpuHandle(Win95_RTV));

    // WIC Force RGAB32+AUTOGEN
    {
        DX::ThrowIfFailed(CreateWICTextureFromFileEx(device, resourceUpload, L"pentagon.tiff",
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_MIP_AUTOGEN | WIC_LOADER_FORCE_RGBA32,
            m_test39.GetAddressOf()));

    #ifdef __MINGW32__
        D3D12_RESOURCE_DESC desc;
        std::ignore = m_test39->GetDesc(&desc);
    #else
        auto const desc = m_test39->GetDesc();
    #endif
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.Width != 1024
            || desc.Height != 1024
            || desc.MipLevels == 1)
        {
            OutputDebugStringA("FAILED: pentagon.tiff res desc rgba32+autogen unexpected\n");
            success = false;
        }
    }


    OutputDebugStringA(success ? "Passed\n" : "Failed\n");
    OutputDebugStringA("***********  UNIT TESTS END  ***************\n");

    if (!success)
    {
        throw std::runtime_error("Unit Tests Failed");
    }
}
