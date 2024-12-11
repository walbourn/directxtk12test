//
// Main.cpp
//

#include "pch.h"
#include "Game.h"

#include <wrl/wrappers/corewrappers.h>
#include <shellapi.h>

using namespace DirectX;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

//#define TRACE_WINDOWS_MESSAGES

namespace
{
    std::unique_ptr<Game> g_game;
    bool g_testTimer = false;

#ifdef WM_DEVICECHANGE
    HDEVNOTIFY g_hNewAudio;
#endif

#ifdef TRACE_WINDOWS_MESSAGES
#define TRACE_ID(iD) case iD: return L## #iD;

    const WCHAR* WINAPI DXUTTraceWindowsMessage(_In_ UINT uMsg)
    {
        switch (uMsg)
        {
            TRACE_ID(WM_NULL);
            TRACE_ID(WM_CREATE);
            TRACE_ID(WM_DESTROY);
            TRACE_ID(WM_MOVE);
            TRACE_ID(WM_SIZE);
            TRACE_ID(WM_ACTIVATE);
            TRACE_ID(WM_SETFOCUS);
            TRACE_ID(WM_KILLFOCUS);
            TRACE_ID(WM_ENABLE);
            TRACE_ID(WM_SETREDRAW);
            TRACE_ID(WM_SETTEXT);
            TRACE_ID(WM_GETTEXT);
            TRACE_ID(WM_GETTEXTLENGTH);
            TRACE_ID(WM_PAINT);
            TRACE_ID(WM_CLOSE);
            TRACE_ID(WM_QUERYENDSESSION);
            TRACE_ID(WM_QUERYOPEN);
            TRACE_ID(WM_ENDSESSION);
            TRACE_ID(WM_QUIT);
            TRACE_ID(WM_ERASEBKGND);
            TRACE_ID(WM_SYSCOLORCHANGE);
            TRACE_ID(WM_SHOWWINDOW);
            TRACE_ID(WM_WININICHANGE);
            TRACE_ID(WM_DEVMODECHANGE);
            TRACE_ID(WM_ACTIVATEAPP);
            TRACE_ID(WM_FONTCHANGE);
            TRACE_ID(WM_TIMECHANGE);
            TRACE_ID(WM_CANCELMODE);
            TRACE_ID(WM_SETCURSOR);
            TRACE_ID(WM_MOUSEACTIVATE);
            TRACE_ID(WM_CHILDACTIVATE);
            TRACE_ID(WM_QUEUESYNC);
            TRACE_ID(WM_GETMINMAXINFO);
            TRACE_ID(WM_PAINTICON);
            TRACE_ID(WM_ICONERASEBKGND);
            TRACE_ID(WM_NEXTDLGCTL);
            TRACE_ID(WM_SPOOLERSTATUS);
            TRACE_ID(WM_DRAWITEM);
            TRACE_ID(WM_MEASUREITEM);
            TRACE_ID(WM_DELETEITEM);
            TRACE_ID(WM_VKEYTOITEM);
            TRACE_ID(WM_CHARTOITEM);
            TRACE_ID(WM_SETFONT);
            TRACE_ID(WM_GETFONT);
            TRACE_ID(WM_SETHOTKEY);
            TRACE_ID(WM_GETHOTKEY);
            TRACE_ID(WM_QUERYDRAGICON);
            TRACE_ID(WM_COMPAREITEM);
            TRACE_ID(WM_GETOBJECT);
            TRACE_ID(WM_COMPACTING);
            TRACE_ID(WM_COMMNOTIFY);
            TRACE_ID(WM_WINDOWPOSCHANGING);
            TRACE_ID(WM_WINDOWPOSCHANGED);
            TRACE_ID(WM_POWER);
            TRACE_ID(WM_COPYDATA);
            TRACE_ID(WM_CANCELJOURNAL);
            TRACE_ID(WM_NOTIFY);
            TRACE_ID(WM_INPUTLANGCHANGEREQUEST);
            TRACE_ID(WM_INPUTLANGCHANGE);
            TRACE_ID(WM_TCARD);
            TRACE_ID(WM_HELP);
            TRACE_ID(WM_USERCHANGED);
            TRACE_ID(WM_NOTIFYFORMAT);
            TRACE_ID(WM_CONTEXTMENU);
            TRACE_ID(WM_STYLECHANGING);
            TRACE_ID(WM_STYLECHANGED);
            TRACE_ID(WM_DISPLAYCHANGE);
            TRACE_ID(WM_GETICON);
            TRACE_ID(WM_SETICON);
            TRACE_ID(WM_NCCREATE);
            TRACE_ID(WM_NCDESTROY);
            TRACE_ID(WM_NCCALCSIZE);
            TRACE_ID(WM_NCHITTEST);
            TRACE_ID(WM_NCPAINT);
            TRACE_ID(WM_NCACTIVATE);
            TRACE_ID(WM_GETDLGCODE);
            TRACE_ID(WM_SYNCPAINT);
            TRACE_ID(WM_NCMOUSEMOVE);
            TRACE_ID(WM_NCLBUTTONDOWN);
            TRACE_ID(WM_NCLBUTTONUP);
            TRACE_ID(WM_NCLBUTTONDBLCLK);
            TRACE_ID(WM_NCRBUTTONDOWN);
            TRACE_ID(WM_NCRBUTTONUP);
            TRACE_ID(WM_NCRBUTTONDBLCLK);
            TRACE_ID(WM_NCMBUTTONDOWN);
            TRACE_ID(WM_NCMBUTTONUP);
            TRACE_ID(WM_NCMBUTTONDBLCLK);
            TRACE_ID(WM_NCXBUTTONDOWN);
            TRACE_ID(WM_NCXBUTTONUP);
            TRACE_ID(WM_NCXBUTTONDBLCLK);
            TRACE_ID(WM_INPUT_DEVICE_CHANGE);
            TRACE_ID(WM_INPUT);
            TRACE_ID(WM_KEYDOWN);
            TRACE_ID(WM_KEYUP);
            TRACE_ID(WM_CHAR);
            TRACE_ID(WM_DEADCHAR);
            TRACE_ID(WM_SYSKEYDOWN);
            TRACE_ID(WM_SYSKEYUP);
            TRACE_ID(WM_SYSCHAR);
            TRACE_ID(WM_SYSDEADCHAR);
            TRACE_ID(WM_UNICHAR);
            TRACE_ID(WM_IME_STARTCOMPOSITION);
            TRACE_ID(WM_IME_ENDCOMPOSITION);
            TRACE_ID(WM_IME_COMPOSITION);
            TRACE_ID(WM_INITDIALOG);
            TRACE_ID(WM_COMMAND);
            TRACE_ID(WM_SYSCOMMAND);
            TRACE_ID(WM_TIMER);
            TRACE_ID(WM_HSCROLL);
            TRACE_ID(WM_VSCROLL);
            TRACE_ID(WM_INITMENU);
            TRACE_ID(WM_INITMENUPOPUP);
            TRACE_ID(WM_GESTURE);
            TRACE_ID(WM_GESTURENOTIFY);
            TRACE_ID(WM_MENUSELECT);
            TRACE_ID(WM_MENUCHAR);
            TRACE_ID(WM_ENTERIDLE);
            TRACE_ID(WM_MENURBUTTONUP);
            TRACE_ID(WM_MENUDRAG);
            TRACE_ID(WM_MENUGETOBJECT);
            TRACE_ID(WM_UNINITMENUPOPUP);
            TRACE_ID(WM_MENUCOMMAND);
            TRACE_ID(WM_CHANGEUISTATE);
            TRACE_ID(WM_UPDATEUISTATE);
            TRACE_ID(WM_QUERYUISTATE);
            TRACE_ID(WM_CTLCOLORMSGBOX);
            TRACE_ID(WM_CTLCOLOREDIT);
            TRACE_ID(WM_CTLCOLORLISTBOX);
            TRACE_ID(WM_CTLCOLORBTN);
            TRACE_ID(WM_CTLCOLORDLG);
            TRACE_ID(WM_CTLCOLORSCROLLBAR);
            TRACE_ID(WM_CTLCOLORSTATIC);
            TRACE_ID(MN_GETHMENU);
            TRACE_ID(WM_MOUSEMOVE);
            TRACE_ID(WM_LBUTTONDOWN);
            TRACE_ID(WM_LBUTTONUP);
            TRACE_ID(WM_LBUTTONDBLCLK);
            TRACE_ID(WM_RBUTTONDOWN);
            TRACE_ID(WM_RBUTTONUP);
            TRACE_ID(WM_RBUTTONDBLCLK);
            TRACE_ID(WM_MBUTTONDOWN);
            TRACE_ID(WM_MBUTTONUP);
            TRACE_ID(WM_MBUTTONDBLCLK);
            TRACE_ID(WM_MOUSEWHEEL);
            TRACE_ID(WM_XBUTTONDOWN);
            TRACE_ID(WM_XBUTTONUP);
            TRACE_ID(WM_XBUTTONDBLCLK);
            TRACE_ID(WM_MOUSEHWHEEL);
            TRACE_ID(WM_PARENTNOTIFY);
            TRACE_ID(WM_ENTERMENULOOP);
            TRACE_ID(WM_EXITMENULOOP);
            TRACE_ID(WM_NEXTMENU);
            TRACE_ID(WM_SIZING);
            TRACE_ID(WM_CAPTURECHANGED);
            TRACE_ID(WM_MOVING);
            TRACE_ID(WM_POWERBROADCAST);
            TRACE_ID(WM_DEVICECHANGE);
            TRACE_ID(WM_MDICREATE);
            TRACE_ID(WM_MDIDESTROY);
            TRACE_ID(WM_MDIACTIVATE);
            TRACE_ID(WM_MDIRESTORE);
            TRACE_ID(WM_MDINEXT);
            TRACE_ID(WM_MDIMAXIMIZE);
            TRACE_ID(WM_MDITILE);
            TRACE_ID(WM_MDICASCADE);
            TRACE_ID(WM_MDIICONARRANGE);
            TRACE_ID(WM_MDIGETACTIVE);
            TRACE_ID(WM_MDISETMENU);
            TRACE_ID(WM_ENTERSIZEMOVE);
            TRACE_ID(WM_EXITSIZEMOVE);
            TRACE_ID(WM_DROPFILES);
            TRACE_ID(WM_MDIREFRESHMENU);
            TRACE_ID(WM_POINTERDEVICECHANGE);
            TRACE_ID(WM_POINTERDEVICEINRANGE);
            TRACE_ID(WM_POINTERDEVICEOUTOFRANGE);
            TRACE_ID(WM_TOUCH);
            TRACE_ID(WM_NCPOINTERUPDATE);
            TRACE_ID(WM_NCPOINTERDOWN);
            TRACE_ID(WM_NCPOINTERUP);
            TRACE_ID(WM_POINTERUPDATE);
            TRACE_ID(WM_POINTERDOWN);
            TRACE_ID(WM_POINTERUP);
            TRACE_ID(WM_POINTERENTER);
            TRACE_ID(WM_POINTERLEAVE);
            TRACE_ID(WM_POINTERACTIVATE);
            TRACE_ID(WM_POINTERCAPTURECHANGED);
            TRACE_ID(WM_TOUCHHITTESTING);
            TRACE_ID(WM_POINTERWHEEL);
            TRACE_ID(WM_POINTERHWHEEL);
            TRACE_ID(DM_POINTERHITTEST);
            TRACE_ID(WM_POINTERROUTEDTO);
            TRACE_ID(WM_POINTERROUTEDAWAY);
            TRACE_ID(WM_POINTERROUTEDRELEASED);
            TRACE_ID(WM_IME_SETCONTEXT);
            TRACE_ID(WM_IME_NOTIFY);
            TRACE_ID(WM_IME_CONTROL);
            TRACE_ID(WM_IME_COMPOSITIONFULL);
            TRACE_ID(WM_IME_SELECT);
            TRACE_ID(WM_IME_CHAR);
            TRACE_ID(WM_IME_REQUEST);
            TRACE_ID(WM_IME_KEYDOWN);
            TRACE_ID(WM_IME_KEYUP);
            TRACE_ID(WM_MOUSEHOVER);
            TRACE_ID(WM_MOUSELEAVE);
            TRACE_ID(WM_NCMOUSEHOVER);
            TRACE_ID(WM_NCMOUSELEAVE);
            TRACE_ID(WM_WTSSESSION_CHANGE);
            TRACE_ID(WM_DPICHANGED);
            TRACE_ID(WM_CUT);
            TRACE_ID(WM_COPY);
            TRACE_ID(WM_PASTE);
            TRACE_ID(WM_CLEAR);
            TRACE_ID(WM_UNDO);
            TRACE_ID(WM_RENDERFORMAT);
            TRACE_ID(WM_RENDERALLFORMATS);
            TRACE_ID(WM_DESTROYCLIPBOARD);
            TRACE_ID(WM_DRAWCLIPBOARD);
            TRACE_ID(WM_PAINTCLIPBOARD);
            TRACE_ID(WM_VSCROLLCLIPBOARD);
            TRACE_ID(WM_SIZECLIPBOARD);
            TRACE_ID(WM_ASKCBFORMATNAME);
            TRACE_ID(WM_CHANGECBCHAIN);
            TRACE_ID(WM_HSCROLLCLIPBOARD);
            TRACE_ID(WM_QUERYNEWPALETTE);
            TRACE_ID(WM_PALETTEISCHANGING);
            TRACE_ID(WM_PALETTECHANGED);
            TRACE_ID(WM_HOTKEY);
            TRACE_ID(WM_PRINT);
            TRACE_ID(WM_PRINTCLIENT);
            TRACE_ID(WM_APPCOMMAND);
            TRACE_ID(WM_THEMECHANGED);
            TRACE_ID(WM_CLIPBOARDUPDATE);
            TRACE_ID(WM_DWMCOMPOSITIONCHANGED);
            TRACE_ID(WM_DWMNCRENDERINGCHANGED);
            TRACE_ID(WM_DWMCOLORIZATIONCOLORCHANGED);
            TRACE_ID(WM_DWMWINDOWMAXIMIZEDCHANGE);
            TRACE_ID(WM_DWMSENDICONICTHUMBNAIL);
            TRACE_ID(WM_DWMSENDICONICLIVEPREVIEWBITMAP);
            TRACE_ID(WM_GETTITLEBARINFOEX);
            TRACE_ID(WM_APP);
            TRACE_ID(WM_USER);
        default:
            return L"Unknown";
        }
    }

#undef TRACE_ID
#endif // TRACE_WINDOWS_MESSAGES
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ExitGame() noexcept;

namespace
{
    void ParseCommandLine(_In_ LPWSTR lpCmdLine)
    {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(lpCmdLine, &argc);

        for (int iArg = 0; iArg < argc; iArg++)
        {
            wchar_t* pArg = argv[iArg];

            if (('-' == pArg[0]) || ('/' == pArg[0]))
            {
                pArg++;
                wchar_t* pValue;

                for (pValue = pArg; *pValue && (':' != *pValue); pValue++);

                if (*pValue)
                    *pValue++ = 0;

                if (_wcsicmp(pArg, L"ctest") == 0)
                {
                    g_testTimer = true;
                }
                else if (_wcsicmp(pArg, L"forcewarp") == 0)
                {
                    DX::DeviceResources::DebugForceWarp(true);
                }
                else if (_wcsicmp(pArg, L"minpower") == 0)
                {
                    DX::DeviceResources::DebugPreferMinimumPower(true);
                }
                else if (_wcsicmp(pArg, L"adapter") == 0)
                {
                    if (pValue && *pValue != 0)
                    {
                        DX::DeviceResources::DebugSetAdapter(_wtoi(pValue));
                    }
                }

            }
        }

        LocalFree(argv);
    }
}

extern "C"
{
    // Indicates to hybrid graphics systems to prefer the discrete part by default
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#ifdef USING_D3D12_AGILITY_SDK
    // Used to enable the "Agility SDK" components
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\";
#endif
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    if (!XMVerifyCPUSupport())
        return 1;

#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

#if defined(USING_D3D12_AGILITY_SDK) && defined(_DEBUG)
    OutputDebugStringA("INFO: Using the DirectX Agility SDK\n");
#endif

#ifdef USING_WINDOWS_GAMING_INPUT
    // RoUnitialize has been hanging in the tests.
    if (FAILED(RoInitialize(RO_INIT_MULTITHREADED)))
        return 1;
#else
    if (FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)))
        return 1;
#endif

    ParseCommandLine(lpCmdLine);

    g_game = std::make_unique<Game>();

    // Register class and create window
    {
        // Register class
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszClassName = L"D3D12TestWindowClass";
        wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");
        if (!RegisterClassExW(&wcex))
            return 1;

        // Create window
        int w, h;
        g_game->GetDefaultSize(w, h);

        RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };

        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        HWND hwnd = CreateWindowExW(0, L"D3D12TestWindowClass", g_game->GetAppName(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance,
            g_game.get());

        if (!hwnd)
            return 1;

        ShowWindow(hwnd, nCmdShow);

        GetClientRect(hwnd, &rc);

        g_game->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top, DXGI_MODE_ROTATION_IDENTITY);

        if (g_testTimer)
        {
            SetTimer(hwnd, 1, c_testTimeout, nullptr);
        }

#ifdef _DBT_H
        DEV_BROADCAST_DEVICEINTERFACE filter = {};
        filter.dbcc_size = sizeof(filter);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        filter.dbcc_classguid = KSCATEGORY_AUDIO;

        g_hNewAudio = RegisterDeviceNotification(hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
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

#ifdef WM_DEVICECHANGE
    UnregisterDeviceNotification(g_hNewAudio);
#endif

#ifndef USING_WINDOWS_GAMING_INPUT
    CoUninitialize();
#endif

    return static_cast<int>(msg.wParam);
}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;
    static bool s_fullscreen = false;

#ifdef TRACE_WINDOWS_MESSAGES
    OutputDebugStringW(DXUTTraceWindowsMessage(message));
    OutputDebugStringA("\n");
#endif

    auto game = reinterpret_cast<Game*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
        if (lParam)
        {
            auto params = reinterpret_cast<LPCREATESTRUCTW>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(params->lpCreateParams));
        }
        break;

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
        return 0;

    case WM_DISPLAYCHANGE:
        if (game)
        {
            game->OnDisplayChange();
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

    case WM_ACTIVATEAPP:
        Keyboard::ProcessMessage(message, wParam, lParam);
        Mouse::ProcessMessage(message, wParam, lParam);

        if (game)
        {
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

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Keyboard::ProcessMessage(message, wParam, lParam);
        break;

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
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                ShowWindow(hWnd, SW_SHOWMAXIMIZED);
            }

            s_fullscreen = !s_fullscreen;
        }
        Keyboard::ProcessMessage(message, wParam, lParam);
        break;

    case WM_INPUT:
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
    case WM_MOUSEHOVER:
        Mouse::ProcessMessage(message, wParam, lParam);
        break;

    case WM_MOUSEACTIVATE:
        // When you click activate the window, we want Mouse to ignore it.
        return MA_ACTIVATEANDEAT;

#ifdef _DBT_H
    case WM_DEVICECHANGE:
        if (wParam == DBT_DEVICEARRIVAL)
        {
            auto pDev = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);
            if (pDev)
            {
                if (pDev->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                {
                    auto pInter = reinterpret_cast<const PDEV_BROADCAST_DEVICEINTERFACE>(pDev);
                    if (pInter->dbcc_classguid == KSCATEGORY_AUDIO)
                    {
#ifdef _DEBUG
                        OutputDebugStringA("INFO: New audio device detected: ");
                        OutputDebugString(pInter->dbcc_name);
                        OutputDebugStringA("\n");
#endif
                        PostMessage(hWnd, WM_USER, 0, 0);
                    }
                }
            }
        }
        return 0;

    case WM_USER:
        if (g_game)
        {
            g_game->OnAudioDeviceChange();
        }
        break;
#endif

    case WM_MENUCHAR:
        // A menu is active and the user presses a key that does not correspond
        // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
        return MAKELRESULT(0, MNC_CLOSE);

    case WM_TIMER:
        if (g_testTimer)
        {
            ExitGame();
            OutputDebugStringA("WM_TIMER\n");
        }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Exit helper
void ExitGame() noexcept
{
#ifndef TEST_MGPU
    auto& graphicsMemory = GraphicsMemory::Get();

    auto stats = graphicsMemory.GetStatistics();

    char buff[1024] = {};
    sprintf_s(buff, "GraphicsMemory: committed %zu KB, total %zu KB (%zu pages)\n"
                    "                peak commited %zu KB, peak total %zu KB (%zu pages)\n",
        stats.committedMemory / 1024,
        stats.totalMemory / 1024,
        stats.totalPages,
        stats.peakCommitedMemory / 1024,
        stats.peakTotalMemory / 1024,
        stats.peakTotalPages);
    OutputDebugStringA(buff);
#endif

    PostQuitMessage(0);
}
