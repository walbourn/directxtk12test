//--------------------------------------------------------------------------------------
// Main.cpp
//
// Entry point for Microsoft GDK with Xbox extensions
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include <appnotify.h>
#include <XDisplay.h>
#include <XGameRuntimeInit.h>

using namespace DirectX;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

namespace
{
    std::unique_ptr<Game> g_game;
    HANDLE g_plmSuspendComplete = nullptr;
    HANDLE g_plmSignalResume = nullptr;
}

bool g_HDRMode = false;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void SetDisplayMode() noexcept;
void ExitGame() noexcept;

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!XMVerifyCPUSupport())
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: This hardware does not support the required instruction set.\n");
#ifdef __AVX2__
        OutputDebugStringA("This may indicate a Gaming.Xbox.Scarlett.x64 binary is being run on an Xbox One.\n");
#endif
#endif
        return 1;
    }

    // Initialize COM for WIC usage
    if (FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)))
        return 1;

    if (FAILED(XGameRuntimeInitialize()))
        return 1;

    // Default main thread to CPU 0
    SetThreadAffinityMask(GetCurrentThread(), 0x1);

    // Microsoft GDKX supports UTF-8 everywhere
    assert(GetACP() == CP_UTF8);

    g_game = std::make_unique<Game>();

    // Register class and create window
    PAPPSTATE_REGISTRATION hPLM = {};
    PAPPCONSTRAIN_REGISTRATION hPLM2 = {};
    {
        // Register class
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.lpszClassName = L"D3D12TestWindowClass";
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClassExW(&wcex))
            return 1;

        // Create window
        HWND hwnd = CreateWindowExW(0, L"D3D12TestWindowClass", g_game->GetAppName(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1080, nullptr, nullptr, hInstance,
            nullptr);
        if (!hwnd)
            return 1;

        ShowWindow(hwnd, nCmdShow);

        SetDisplayMode();

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_game.get()));

        g_game->Initialize(hwnd, 0, 0, DXGI_MODE_ROTATION_IDENTITY);

        g_plmSuspendComplete = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        g_plmSignalResume = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        if (!g_plmSuspendComplete || !g_plmSignalResume)
            return 1;

        if (RegisterAppStateChangeNotification([](BOOLEAN quiesced, PVOID context)
        {
            if (quiesced)
            {
                ResetEvent(g_plmSuspendComplete);
                ResetEvent(g_plmSignalResume);

                // To ensure we use the main UI thread to process the notification, we self-post a message
                PostMessage(reinterpret_cast<HWND>(context), WM_USER, 0, 0);

                // To defer suspend, you must wait to exit this callback
                std::ignore = WaitForSingleObject(g_plmSuspendComplete, INFINITE);
            }
            else
            {
                SetEvent(g_plmSignalResume);
            }
        }, hwnd, &hPLM))
            return 1;

        if (RegisterAppConstrainedChangeNotification([](BOOLEAN constrained, PVOID context)
        {
            // To ensure we use the main UI thread to process the notification, we self-post a message
            SendMessage(reinterpret_cast<HWND>(context), WM_USER + 1, (constrained) ? 1u : 0u, 0);
        }, hwnd, &hPLM2))
            return 1;
    }

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_game->Tick();
        }
    }

    g_game.reset();

    UnregisterAppStateChangeNotification(hPLM);
    UnregisterAppConstrainedChangeNotification(hPLM2);

    CloseHandle(g_plmSuspendComplete);
    CloseHandle(g_plmSignalResume);

    XGameRuntimeUninitialize();

    CoUninitialize();

    return static_cast<int>(msg.wParam);
}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        Mouse::ProcessMessage(message, wParam, lParam);
        break;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        Mouse::ProcessMessage(message, wParam, lParam);
        break;

    case WM_USER:
        if (game)
        {
            game->OnSuspending();

            // Complete deferral
            SetEvent(g_plmSuspendComplete);

            std::ignore = WaitForSingleObject(g_plmSignalResume, INFINITE);

            SetDisplayMode();

            game->OnResuming();
        }
        break;

    case WM_USER + 1:
        if (game && !wParam)
        {
            // OnUnConstrained
            SetDisplayMode();
        }
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// HDR helper
void SetDisplayMode() noexcept
{
    if (g_game && g_game->RequestHDRMode())
    {
        // Request HDR mode.
        auto result = XDisplayTryEnableHdrMode(XDisplayHdrModePreference::PreferHdr, nullptr);

        g_HDRMode = (result == XDisplayHdrModeResult::Enabled);

#ifdef _DEBUG
        OutputDebugStringA((g_HDRMode) ? "INFO: Display in HDR Mode\n" : "INFO: Display in SDR Mode\n");
#endif
    }
}

// Exit helper
void ExitGame() noexcept
{
    auto& graphicsMemory = GraphicsMemory::Get();

    auto stats = graphicsMemory.GetStatistics();

    char buff[1024] = {};
    sprintf_s(buff, "GraphicsMemory: committed %zu KB, total %zu KB (%zu pages)\n                peak commited %zu KB, peak total %zu KB (%zu pages)\n",
        stats.committedMemory / 1024,
        stats.totalMemory / 1024,
        stats.totalPages,
        stats.peakCommitedMemory / 1024,
        stats.peakTotalMemory / 1024,
        stats.peakTotalPages);
    OutputDebugStringA(buff);

    PostQuitMessage(0);
}
