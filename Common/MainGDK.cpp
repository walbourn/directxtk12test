//--------------------------------------------------------------------------------------
// Main.cpp
//
// Entry point for Microsoft Game Development Kit (GDK)
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include <appnotify.h>
#include <XGameRuntimeInit.h>
#include <XGameErr.h>
#include <XSystem.h>

using namespace DirectX;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

namespace
{
    std::unique_ptr<Game> g_game;
    bool g_testTimer = false;

#ifdef _GAMING_XBOX
    HANDLE g_plmSuspendComplete = nullptr;
    HANDLE g_plmSignalResume = nullptr;
#endif
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ExitGame() noexcept;

namespace
{
    void ParseCommandLine(_In_ LPWSTR lpCmdLine)
    {
        if (wcsstr(lpCmdLine, L"-ctest") != nullptr)
        {
            g_testTimer = true;
        }
    }
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    if (!XMVerifyCPUSupport())
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: This hardware does not support the required instruction set.\n");
#if defined(_GAMING_XBOX) && defined(__AVX2__)
        OutputDebugStringA("This may indicate a Gaming.Xbox.Scarlett.x64 binary is being run on an Xbox One.\n");
#endif
#endif
        return 1;
    }

    // Initialize COM for WIC usage
    if (FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)))
        return 1;

    HRESULT hr = XGameRuntimeInitialize();
    if (FAILED(hr))
    {
        if (hr == E_GAMERUNTIME_DLL_NOT_FOUND || hr == E_GAMERUNTIME_VERSION_MISMATCH)
        {
#ifdef _GAMING_DESKTOP
            std::ignore = MessageBoxW(nullptr, L"Game Runtime is not installed on this system or needs updating.", L"D3D12Test", MB_ICONERROR | MB_OK);
#endif
        }
        return 1;
    }

#ifdef _GAMING_XBOX
    // Default main thread to CPU 0
    SetThreadAffinityMask(GetCurrentThread(), 0x1);
#endif

    ParseCommandLine(lpCmdLine);

    g_game = std::make_unique<Game>();

    // Register class and create window
#ifdef _GAMING_XBOX
    PAPPSTATE_REGISTRATION hPLM = {};
#endif
    {
        // Register class
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszClassName = L"D3D12TestWindowClass";
        if (!RegisterClassExW(&wcex))
            return 1;

        // Create window
#ifdef _GAMING_XBOX
        RECT rc = { 0, 0, 1920, 1080 };
        float uiScale = 1.f;
        switch (XSystemGetDeviceType())
        {
        case XSystemDeviceType::XboxOne:
        case XSystemDeviceType::XboxOneS:
#ifdef _DEBUG
            OutputDebugStringA("INFO: Swapchain using 1080p (1920 x 1080)\n");
#endif
            break;

        case XSystemDeviceType::XboxScarlettLockhart /* Xbox Series S */:
            rc = { 0, 0, 2560, 1440 };
            uiScale = 1.333333f;
#ifdef _DEBUG
            OutputDebugStringA("INFO: Swapchain using 1440p (2560 x 1440)\n");
#endif
            break;

        case XSystemDeviceType::XboxScarlettAnaconda /* Xbox Series X */:
        case XSystemDeviceType::XboxOneXDevkit:
        case XSystemDeviceType::XboxScarlettDevkit:
        default:
            rc = { 0, 0, 3840, 2160 };
            uiScale = 2.f;
#ifdef _DEBUG
            OutputDebugStringA("INFO: Swapchain using 4k (3840 x 2160)\n");
#endif
            break;
        }
#else
        int w, h;
        g_game->GetDefaultSize(w, h);

        RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
#endif

        HWND hwnd = CreateWindowExW(0, L"D3D12TestWindowClass", g_game->GetAppName(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
            nullptr);
        if (!hwnd)
            return 1;

        ShowWindow(hwnd, nCmdShow);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_game.get()));

        GetClientRect(hwnd, &rc);

        g_game->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top, DXGI_MODE_ROTATION_IDENTITY);

        if (g_testTimer)
        {
            SetTimer(hwnd, 1, c_testTimeout, nullptr);
        }

#ifdef _GAMING_XBOX
        Mouse::SetResolution(uiScale);

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
#endif
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

#ifdef _GAMING_XBOX
    UnregisterAppStateChangeNotification(hPLM);

    CloseHandle(g_plmSuspendComplete);
    CloseHandle(g_plmSignalResume);
#endif

    XGameRuntimeUninitialize();

    CoUninitialize();

    return static_cast<int>(msg.wParam);
}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef _GAMING_DESKTOP
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;
    static bool s_fullscreen = false;
#endif

    auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_ACTIVATEAPP:
        if (game)
        {
            Keyboard::ProcessMessage(message, wParam, lParam);
            Mouse::ProcessMessage(message, wParam, lParam);

            if (wParam)
            {
                game->OnActivated();
            }
            else
            {
                game->OnDeactivated();
            }
        }
        break;

    case WM_ACTIVATE:
        Keyboard::ProcessMessage(message, wParam, lParam);
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

#ifdef _GAMING_XBOX
    case WM_USER:
        if (game)
        {
            game->OnSuspending();

            // Complete deferral
            SetEvent(g_plmSuspendComplete);

            std::ignore = WaitForSingleObject(g_plmSignalResume, INFINITE);

            game->OnResuming();
        }
        break;
#else
    case WM_PAINT:
        if (s_in_sizemove && game)
        {
            game->Tick();
        }
        else
        {
            PAINTSTRUCT ps;
            std::ignore = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_MOVE:
        if (game)
        {
            game->OnWindowMoved();
        }
        break;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            if (!s_minimized)
            {
                s_minimized = true;
                if (!s_in_suspend && game)
                    game->OnSuspending();
                s_in_suspend = true;
            }
        }
        else if (s_minimized)
        {
            s_minimized = false;
            if (s_in_suspend && game)
                game->OnResuming();
            s_in_suspend = false;
        }
        else if (!s_in_sizemove && game)
        {
            game->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam), DXGI_MODE_ROTATION_IDENTITY);
        }
        break;

    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;

    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        if (game)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);

            game->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top, DXGI_MODE_ROTATION_IDENTITY);
        }
        break;

    case WM_GETMINMAXINFO:
        if (lParam)
        {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320;
            info->ptMinTrackSize.y = 200;
        }
        break;

    case WM_POWERBROADCAST:
        switch (wParam)
        {
        case PBT_APMQUERYSUSPEND:
            if (!s_in_suspend && game)
                game->OnSuspending();
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if (!s_minimized)
            {
                if (s_in_suspend && game)
                    game->OnResuming();
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

#ifndef USING_GAMEINPUT
    case WM_INPUT:
    case WM_MOUSEHOVER:
        Mouse::ProcessMessage(message, wParam, lParam);
        break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Keyboard::ProcessMessage(message, wParam, lParam);
        break;
#endif

    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
        {
            // Implements the classic ALT+ENTER fullscreen toggle
            if (s_fullscreen)
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

                int width = 800;
                int height = 600;
                if (game)
                    game->GetDefaultSize(width, height);

                ShowWindow(hWnd, SW_SHOWNORMAL);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
            else
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, 0);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                ShowWindow(hWnd, SW_SHOWMAXIMIZED);
            }

            s_fullscreen = !s_fullscreen;
        }
#ifndef USING_GAMEINPUT
        Keyboard::ProcessMessage(message, wParam, lParam);
#endif
        break;

    case WM_MOUSEACTIVATE:
        // When you click activate the window, we want Mouse to ignore that event.
        return MA_ACTIVATEANDEAT;

    case WM_MENUCHAR:
        // A menu is active and the user presses a key that does not correspond
        // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
        return MAKELRESULT(0, MNC_CLOSE);
#endif

    case WM_TIMER:
        if (g_testTimer)
        {
            ExitGame();
        }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
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
