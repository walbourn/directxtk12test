//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectXTK GamePad
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Game.h"

#include "FindMedia.h"

#define GAMMA_CORRECT_RENDERING

#ifdef USING_GAMEINPUT
#include <GameInput.h>
#ifndef GAMEINPUT_API_VERSION
#define GAMEINPUT_API_VERSION 0
#endif
#if GAMEINPUT_API_VERSION == 1
using namespace GameInput::v1;
#elif GAMEINPUT_API_VERSION == 2
using namespace GameInput::v2;
#endif
#elif defined(USING_WINDOWS_GAMING_INPUT)
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4619 6553)
#endif
#include <Windows.UI.Core.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#elif !defined(_XBOX_ONE)
#include <xinput.h>
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

static_assert(std::is_nothrow_move_constructible<GamePad>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GamePad>::value, "Move Assign.");

static_assert(std::is_nothrow_move_constructible<GamePad::ButtonStateTracker>::value, "Move Ctor.");
static_assert(std::is_nothrow_move_assignable<GamePad::ButtonStateTracker>::value, "Move Assign.");

namespace
{
#ifdef GAMMA_CORRECT_RENDERING
    const XMVECTORF32 c_clearColor = { { { 0.127437726f, 0.300543845f, 0.846873462f, 1.f } } };
#else
    const XMVECTORF32 c_clearColor = Colors::CornflowerBlue;
#endif
}

// Constructor.
Game::Game() noexcept(false) :
    m_state{},
    m_frame(0),
    m_lastStr(nullptr)
{
#ifdef GAMMA_CORRECT_RENDERING
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#else
    constexpr DXGI_FORMAT c_RenderFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#endif

    // 2D only rendering
#ifdef COMBO_GDK
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat, DXGI_FORMAT_UNKNOWN);
#elif defined(XBOX)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_UNKNOWN, 2,
        DX::DeviceResources::c_Enable4K_UHD
#ifdef _GAMING_XBOX
        | DX::DeviceResources::c_EnableQHD
#endif
        );
#elif defined(UWP)
    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_RenderFormat, DXGI_FORMAT_UNKNOWN, 2, D3D_FEATURE_LEVEL_11_0,
        DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox
        );
#else
    m_deviceResources = std::make_unique<DX::DeviceResources>(c_RenderFormat, DXGI_FORMAT_UNKNOWN);
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
#ifdef COMBO_GDK
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
#elif defined(XBOX)
    UNREFERENCED_PARAMETER(rotation);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
    m_deviceResources->SetWindow(window);
#elif defined(UWP)
    m_deviceResources->SetWindow(window, width, height, rotation);
#else
    UNREFERENCED_PARAMETER(rotation);
    m_deviceResources->SetWindow(window, width, height);
#endif

    m_gamePad = std::make_unique<GamePad>();

#if 0 // BUGBUG - #ifdef PC
    // Singleton test
    {
        bool thrown = false;

        try
        {
            auto gamePad2 = std::make_unique<GamePad>();
        }
        catch (...)
        {
            thrown = true;
        }

        if (!thrown)
        {
            MessageBoxW(window, L"GamePad not acting like a singleton", L"GamePadTest", MB_ICONERROR);
            throw std::runtime_error("GamePad not acting like a singleton");
        }

        auto state = GamePad::Get().GetState(0);
        state;
    }
#endif

#ifdef USING_GAMEINPUT

    char buff[64] = {};
    sprintf_s(buff, "INFO: Using GameInput (API v%d)\n", GAMEINPUT_API_VERSION);
    OutputDebugStringA(buff);

    m_ctrlChanged.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    if (!m_ctrlChanged.IsValid())
    {
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEvent");
    }

    m_gamePad->RegisterEvents(m_ctrlChanged.Get());

#elif defined(USING_WINDOWS_GAMING_INPUT) || defined(_XBOX_ONE)

#ifdef _XBOX_ONE
    OutputDebugStringA("INFO: Using Windows::Xbox::Input\n");
#else
    OutputDebugStringA("INFO: Using Windows.Gaming.Input\n");
#endif

    m_ctrlChanged.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    m_userChanged.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    if (!m_ctrlChanged.IsValid() || !m_userChanged.IsValid())
    {
        throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "CreateEvent");
    }

    m_gamePad->RegisterEvents(m_ctrlChanged.Get(), m_userChanged.Get());

#elif !defined(__MINGW32__)

    OutputDebugStringA("INFO: Using XInput (" XINPUT_DLL_A ")\n");

#endif

    m_found.reset(new bool[GamePad::MAX_PLAYER_COUNT]);
    memset(m_found.get(), 0, sizeof(bool) * GamePad::MAX_PLAYER_COUNT);

    m_deviceResources->CreateDeviceResources();
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

    m_state.connected = false;

#ifdef USING_GAMEINPUT

    if (WaitForSingleObject(m_ctrlChanged.Get(), 0) == WAIT_OBJECT_0)
    {
        OutputDebugStringA("EVENT: Controller changed\n");
    }

#elif defined(USING_WINDOWS_GAMING_INPUT) || defined(_XBOX_ONE)

    HANDLE events[2] = { m_ctrlChanged.Get(), m_userChanged.Get() };
    switch (WaitForMultipleObjects(static_cast<DWORD>(std::size(events)), events, FALSE, 0))
    {
    case WAIT_OBJECT_0:
        OutputDebugStringA("EVENT: Controller changed\n");
        break;
    case WAIT_OBJECT_0 + 1:
        OutputDebugStringA("EVENT: User changed\n");
        break;
    }

#endif

    for (int j = 0; j < GamePad::MAX_PLAYER_COUNT; ++j)
    {
        auto state2 = m_gamePad->GetState(j);
        auto caps = m_gamePad->GetCapabilities(j);

        assert(state2.IsConnected() == caps.IsConnected());

        if (state2.IsConnected())
        {
            if (!m_found[size_t(j)])
            {
                m_found[size_t(j)] = true;

                if (caps.IsConnected())
                {
#ifdef USING_GAMEINPUT
                    char idstr[128] = {};
                    for (size_t l = 0; l < APP_LOCAL_DEVICE_ID_SIZE; ++l)
                    {
                        sprintf_s(idstr + l * 2, 128 - l * 2, "%02x", caps.id.value[l]);
                    }
                    char buff[128] = {};
                    sprintf_s(buff, "Player %d -> connected (type %u, %04X/%04X, id %s)\n", j, caps.gamepadType, caps.vid, caps.pid, idstr);
                    OutputDebugStringA(buff);

                    {
                        ComPtr<GamePad::GameInputDevice_t> idevice;
                        m_gamePad->GetDevice(j, idevice.GetAddressOf());

                        if (!idevice)
                        {
                            OutputDebugStringA("             **ERROR** GetDevice failed unexpectedly\n");
                        }
                        else
                        {
                            auto status = idevice->GetDeviceStatus();
                            switch (status)
                            {
                                case GameInputDeviceNoStatus:   OutputDebugStringA("             No device status\n"); break;
                                case GameInputDeviceConnected:  OutputDebugStringA("             Device connected\n"); break;
                                default:                        OutputDebugStringA("             Failed getting device status\n"); break;
                            }

                        #if GAMEINPUT_API_VERSION == 0
                            GameInputBatteryState battery;
                            idevice->GetBatteryState(&battery);
                            switch (battery.status)
                            {
                            default:
                            case GameInputBatteryUnknown:       break;
                            case GameInputBatteryNotPresent:    OutputDebugStringA("             Battery not present\n"); break;
                            case GameInputBatteryDischarging:   OutputDebugStringA("             Battery discharging\n"); break;
                            case GameInputBatteryIdle:          OutputDebugStringA("             Battery idle\n"); break;
                            case GameInputBatteryCharging:      OutputDebugStringA("             Battery charging\n"); break;
                            }
                        #endif
                        }
                    }
#elif defined(USING_WINDOWS_GAMING_INPUT)
                    if (!caps.id.empty())
                    {
                        using namespace Microsoft::WRL;
                        using namespace Microsoft::WRL::Wrappers;
                        using namespace ABI::Windows::Foundation;
                        using namespace ABI::Windows::System;

                        ComPtr<IUserStatics> statics;
                        DX::ThrowIfFailed(GetActivationFactory(HStringReference(RuntimeClass_Windows_System_User).Get(), statics.GetAddressOf()));

                        ComPtr<IUser> user;
                        HString str;
                        str.Set(caps.id.c_str(), static_cast<unsigned int>(caps.id.length()));
                        HRESULT hr = statics->GetFromId(str.Get(), user.GetAddressOf());
                        if (SUCCEEDED(hr))
                        {
                            UserType userType = UserType_RemoteUser;
                            DX::ThrowIfFailed(user->get_Type(&userType));

                            char buff[1024] = {};
                            sprintf_s(buff, "Player %d -> connected (type %u, %04X/%04X, id \"%ls\" (user found))\n", j, caps.gamepadType, caps.vid, caps.pid, caps.id.c_str());
                            OutputDebugStringA(buff);
                        }
                        else
                        {
                            char buff[1024] = {};
                            sprintf_s(buff, "Player %d -> connected (type %u, %04X/%04X, id \"%ls\" (user fail %08X))\n", j, caps.gamepadType, caps.vid, caps.pid, caps.id.c_str(), static_cast<unsigned int>(hr));
                            OutputDebugStringA(buff);
                        }
                    }
                    else
                    {
                        char buff[64] = {};
                        sprintf_s(buff, "Player %d -> connected (type %u, %04X/%04X, id is empty!)\n", j, caps.gamepadType, caps.vid, caps.pid);
                        OutputDebugStringA(buff);
                    }
#else
                    char buff[64] = {};
                    sprintf_s(buff, "Player %d -> connected (type %u, %04X/%04X, id %llu)\n", j, caps.gamepadType, caps.vid, caps.pid, caps.id);
                    OutputDebugStringA(buff);
#endif
                }
            }
        }
        else
        {
            if (m_found[size_t(j)])
            {
                m_found[size_t(j)] = false;

                char buff[32];
                sprintf_s(buff, "Player %d <- disconnected\n", j);
                OutputDebugStringA(buff);
            }
        }
    }

    m_state = m_gamePad->GetState(GamePad::c_MostRecent);

    if (m_state.IsConnected())
    {
        m_tracker.Update(m_state);

        using ButtonState = GamePad::ButtonStateTracker::ButtonState;

        if (m_tracker.a == ButtonState::PRESSED)
            m_lastStr = L"Button A was pressed\n";
        else if (m_tracker.a == ButtonState::RELEASED)
            m_lastStr = L"Button A was released\n";
        else if (m_tracker.b == ButtonState::PRESSED)
            m_lastStr = L"Button B was pressed\n";
        else if (m_tracker.b == ButtonState::RELEASED)
            m_lastStr = L"Button B was released\n";
        else if (m_tracker.x == ButtonState::PRESSED)
            m_lastStr = L"Button X was pressed\n";
        else if (m_tracker.x == ButtonState::RELEASED)
            m_lastStr = L"Button X was released\n";
        else if (m_tracker.y == ButtonState::PRESSED)
            m_lastStr = L"Button Y was pressed\n";
        else if (m_tracker.y == ButtonState::RELEASED)
            m_lastStr = L"Button Y was released\n";
        else if (m_tracker.leftStick == ButtonState::PRESSED)
            m_lastStr = L"Button LeftStick was pressed\n";
        else if (m_tracker.leftStick == ButtonState::RELEASED)
            m_lastStr = L"Button LeftStick was released\n";
        else if (m_tracker.rightStick == ButtonState::PRESSED)
            m_lastStr = L"Button RightStick was pressed\n";
        else if (m_tracker.rightStick == ButtonState::RELEASED)
            m_lastStr = L"Button RightStick was released\n";
        else if (m_tracker.leftShoulder == ButtonState::PRESSED)
            m_lastStr = L"Button LeftShoulder was pressed\n";
        else if (m_tracker.leftShoulder == ButtonState::RELEASED)
            m_lastStr = L"Button LeftShoulder was released\n";
        else if (m_tracker.rightShoulder == ButtonState::PRESSED)
            m_lastStr = L"Button RightShoulder was pressed\n";
        else if (m_tracker.rightShoulder == ButtonState::RELEASED)
            m_lastStr = L"Button RightShoulder was released\n";
        else if (m_tracker.view == ButtonState::PRESSED)
            m_lastStr = L"Button BACK/VIEW was pressed\n";
        else if (m_tracker.view == ButtonState::RELEASED)
            m_lastStr = L"Button BACK/VIEW was released\n";
        else if (m_tracker.menu == ButtonState::PRESSED)
            m_lastStr = L"Button START/MENU was pressed\n";
        else if (m_tracker.menu == ButtonState::RELEASED)
            m_lastStr = L"Button START/MENU was released\n";
        else if (m_tracker.dpadUp == ButtonState::PRESSED)
            m_lastStr = L"Button DPAD UP was pressed\n";
        else if (m_tracker.dpadUp == ButtonState::RELEASED)
            m_lastStr = L"Button DPAD UP was released\n";
        else if (m_tracker.dpadDown == ButtonState::PRESSED)
            m_lastStr = L"Button DPAD DOWN was pressed\n";
        else if (m_tracker.dpadDown == ButtonState::RELEASED)
            m_lastStr = L"Button DPAD DOWN was released\n";
        else if (m_tracker.dpadLeft == ButtonState::PRESSED)
            m_lastStr = L"Button DPAD LEFT was pressed\n";
        else if (m_tracker.dpadLeft == ButtonState::RELEASED)
            m_lastStr = L"Button DPAD LEFT was released\n";
        else if (m_tracker.dpadRight == ButtonState::PRESSED)
            m_lastStr = L"Button DPAD RIGHT was pressed\n";
        else if (m_tracker.dpadRight == ButtonState::RELEASED)
            m_lastStr = L"Button DPAD RIGHT was released\n";
        else if (m_tracker.leftStickUp == ButtonState::PRESSED)
            m_lastStr = L"Button LEFT STICK was pressed UP\n";
        else if (m_tracker.leftStickUp == ButtonState::RELEASED)
            m_lastStr = L"Button LEFT STICK was released from UP\n";
        else if (m_tracker.leftStickDown == ButtonState::PRESSED)
            m_lastStr = L"Button LEFT STICK was pressed DOWN\n";
        else if (m_tracker.leftStickDown == ButtonState::RELEASED)
            m_lastStr = L"Button LEFT STICK was released from DOWN\n";
        else if (m_tracker.leftStickLeft == ButtonState::PRESSED)
            m_lastStr = L"Button LEFT STICK was pressed LEFT\n";
        else if (m_tracker.leftStickLeft == ButtonState::RELEASED)
            m_lastStr = L"Button LEFT STICK was released from LEFT\n";
        else if (m_tracker.leftStickRight == ButtonState::PRESSED)
            m_lastStr = L"Button LEFT STICK was pressed RIGHT\n";
        else if (m_tracker.leftStickRight == ButtonState::RELEASED)
            m_lastStr = L"Button LEFT STICK was released from RIGHT\n";
        else if (m_tracker.rightStickUp == ButtonState::PRESSED)
            m_lastStr = L"Button RIGHT STICK was pressed UP\n";
        else if (m_tracker.rightStickUp == ButtonState::RELEASED)
            m_lastStr = L"Button RIGHT STICK was released from UP\n";
        else if (m_tracker.rightStickDown == ButtonState::PRESSED)
            m_lastStr = L"Button RIGHT STICK was pressed DOWN\n";
        else if (m_tracker.rightStickDown == ButtonState::RELEASED)
            m_lastStr = L"Button RIGHT STICK was released from DOWN\n";
        else if (m_tracker.rightStickLeft == ButtonState::PRESSED)
            m_lastStr = L"Button RIGHT STICK was pressed LEFT\n";
        else if (m_tracker.rightStickLeft == ButtonState::RELEASED)
            m_lastStr = L"Button RIGHT STICK was released from LEFT\n";
        else if (m_tracker.rightStickRight == ButtonState::PRESSED)
            m_lastStr = L"Button RIGHT STICK was pressed RIGHT\n";
        else if (m_tracker.rightStickRight == ButtonState::RELEASED)
            m_lastStr = L"Button RIGHT STICK was released from RIGHT\n";
        else if (m_tracker.leftTrigger == ButtonState::PRESSED)
            m_lastStr = L"Button LEFT TRIGGER was pressed\n";
        else if (m_tracker.leftTrigger == ButtonState::RELEASED)
            m_lastStr = L"Button LEFT TRIGGER was released\n";
        else if (m_tracker.rightTrigger == ButtonState::PRESSED)
            m_lastStr = L"Button RIGHT TRIGGER was pressed\n";
        else if (m_tracker.rightTrigger == ButtonState::RELEASED)
            m_lastStr = L"Button RIGHT TRIGGER was released\n";

        assert(m_tracker.back == m_tracker.view);
        assert(m_tracker.start == m_tracker.menu);

        m_gamePad->SetVibration(GamePad::c_MostRecent, m_state.triggers.left, m_state.triggers.right);
    }
    else
    {
        memset(&m_state, 0, sizeof(GamePad::State));

        m_lastStr = nullptr;
        m_tracker.Reset();
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

    XMVECTORF32 yellow;
#ifdef GAMMA_CORRECT_RENDERING
    yellow.v = XMColorSRGBToRGB(Colors::Yellow);
#else
    yellow.v = Colors::Yellow;
#endif

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    const auto heap = m_resourceDescriptors->Heap();
    commandList->SetDescriptorHeaps(1, &heap);

    m_spriteBatch->Begin(commandList);

    for (int j = 0; j < std::min(GamePad::MAX_PLAYER_COUNT, 4); ++j)
    {
        const XMVECTOR color = m_found[size_t(j)] ? Colors::White : Colors::DimGray;
        m_ctrlFont->DrawString(m_spriteBatch.get(), L"$", XMFLOAT2(800.f, float(50 + j * 150)), color);
    }

    if (m_lastStr)
    {
        m_comicFont->DrawString(m_spriteBatch.get(), m_lastStr, XMFLOAT2(25.f, 650.f), yellow);
    }

    // X Y A B
    m_ctrlFont->DrawString(m_spriteBatch.get(), L"&", XMFLOAT2(325, 150), m_state.IsXPressed() ? Colors::White : Colors::DimGray);
    m_ctrlFont->DrawString(m_spriteBatch.get(), L"(", XMFLOAT2(400, 110), m_state.IsYPressed() ? Colors::White : Colors::DimGray);
    m_ctrlFont->DrawString(m_spriteBatch.get(), L"'", XMFLOAT2(400, 200), m_state.IsAPressed() ? Colors::White : Colors::DimGray);
    m_ctrlFont->DrawString(m_spriteBatch.get(), L")", XMFLOAT2(475, 150), m_state.IsBPressed() ? Colors::White : Colors::DimGray);

    // Left/Right sticks
    auto loc = XMFLOAT2(10, 110);
    loc.x -= m_state.IsLeftThumbStickLeft() ? 20.f : 0.f;
    loc.x += m_state.IsLeftThumbStickRight() ? 20.f : 0.f;
    loc.y -= m_state.IsLeftThumbStickUp() ? 20.f : 0.f;
    loc.y += m_state.IsLeftThumbStickDown() ? 20.f : 0.f;

    m_ctrlFont->DrawString(m_spriteBatch.get(), L" ", loc, m_state.IsLeftStickPressed() ? Colors::White : Colors::DimGray, 0, XMFLOAT2(0, 0));

    loc = XMFLOAT2(450, 300);
    loc.x -= m_state.IsRightThumbStickLeft() ? 20.f : 0.f;
    loc.x += m_state.IsRightThumbStickRight() ? 20.f : 0.f;
    loc.y -= m_state.IsRightThumbStickUp() ? 20.f : 0.f;
    loc.y += m_state.IsRightThumbStickDown() ? 20.f : 0.f;

    m_ctrlFont->DrawString(m_spriteBatch.get(), L"\"", loc, m_state.IsRightStickPressed() ? Colors::White : Colors::DimGray, 0, XMFLOAT2(0, 0));

    // DPad
    XMVECTOR color = Colors::DimGray;
    if (m_state.dpad.up || m_state.dpad.down || m_state.dpad.right || m_state.dpad.left)
        color = Colors::White;

    loc = XMFLOAT2(175, 300);
    loc.x -= m_state.IsDPadLeftPressed() ? 20.f : 0.f;
    loc.x += m_state.IsDPadRightPressed() ? 20.f : 0.f;
    loc.y -= m_state.IsDPadUpPressed() ? 20.f : 0.f;
    loc.y += m_state.IsDPadDownPressed() ? 20.f : 0.f;

    m_ctrlFont->DrawString(m_spriteBatch.get(), L"!", loc, color);

    // Back/Start (aka View/Menu)
    m_ctrlFont->DrawString(m_spriteBatch.get(), L"#", XMFLOAT2(175, 75), m_state.IsViewPressed() ? Colors::White : Colors::DimGray);
    assert(m_state.IsViewPressed() == m_state.IsBackPressed());
    assert(m_state.buttons.back == m_state.buttons.view);

    m_ctrlFont->DrawString(m_spriteBatch.get(), L"%", XMFLOAT2(300, 75), m_state.IsMenuPressed() ? Colors::White : Colors::DimGray);
    assert(m_state.IsMenuPressed() == m_state.IsStartPressed());
    assert(m_state.buttons.start == m_state.buttons.menu);

    // Triggers/Shoulders
    m_ctrlFont->DrawString(m_spriteBatch.get(), L"*", XMFLOAT2(500, 10), m_state.IsRightShoulderPressed() ? Colors::White : Colors::DimGray, 0, XMFLOAT2(0, 0), 0.5f);

    loc = XMFLOAT2(450, 10);
    loc.x += m_state.IsRightTriggerPressed() ? 5.f : 0.f;
    color = XMVectorLerp(Colors::DimGray, Colors::White, m_state.triggers.right);
    m_ctrlFont->DrawString(m_spriteBatch.get(), L"+", loc, color, 0, XMFLOAT2(0, 0), 0.5f);

    loc = XMFLOAT2(130, 10);
    loc.x -= m_state.IsLeftTriggerPressed() ? 5.f : 0.f;
    color = XMVectorLerp(Colors::DimGray, Colors::White, m_state.triggers.left);
    m_ctrlFont->DrawString(m_spriteBatch.get(), L",", loc, color, 0, XMFLOAT2(0, 0), 0.5f);

    m_ctrlFont->DrawString(m_spriteBatch.get(), L"-", XMFLOAT2(10, 10), m_state.IsLeftShoulderPressed() ? Colors::White : Colors::DimGray, 0, XMFLOAT2(0, 0), 0.5f);

    // Sticks
    const RECT src = { 0, 0, 1, 1 };

    RECT rc;
    rc.top = 500;
    rc.left = 10;
    rc.bottom = 525;
    rc.right = rc.left + int(((m_state.thumbSticks.leftX + 1.f) / 2.f) * 275);

    const auto texSize = XMUINT2(1, 1);
    m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex), texSize, rc, &src);

    rc.top = 550;
    rc.bottom = 575;

    rc.right = rc.left + int(((m_state.thumbSticks.leftY + 1.f) / 2.f) * 275);

    m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex), texSize, rc, &src);

    rc.top = 500;
    rc.left = 325;
    rc.bottom = 525;
    rc.right = rc.left + int(((m_state.thumbSticks.rightX + 1.f) / 2.f) * 275);

    m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex), texSize, rc, &src);

    rc.top = 550;
    rc.bottom = 575;

    rc.right = rc.left + int(((m_state.thumbSticks.rightY + 1.f) / 2.f) * 275);

    m_spriteBatch->Draw(m_resourceDescriptors->GetGpuHandle(Descriptors::DefaultTex), texSize, rc, &src);

    m_spriteBatch->End();

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    // Sample stats to update peak values
    std::ignore = m_graphicsMemory->GetStatistics();

    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    const auto rtvDescriptor = m_deviceResources->GetRenderTargetView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);
    commandList->ClearRenderTargetView(rtvDescriptor, c_clearColor, 0, nullptr);

    // Set the viewport and scissor rect.
    const auto viewport = m_deviceResources->GetScreenViewport();
    const auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    m_tracker.Reset();
}

void Game::OnSuspending()
{
    m_deviceResources->Suspend();
}

void Game::OnResuming()
{
    m_deviceResources->Resume();

    m_tracker.Reset();
    m_timer.ResetElapsedTime();
}

#ifdef PC
void Game::OnWindowMoved()
{
    const auto r = m_deviceResources->GetOutputSize();
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

    const RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetDepthBufferFormat());

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    {
        const SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"comic.spritefont");
    m_comicFont = std::make_unique<SpriteFont>(device, resourceUpload,
        strFilePath,
        m_resourceDescriptors->GetCpuHandle(Descriptors::ComicFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::ComicFont));

    DX::FindMediaFile(strFilePath, MAX_PATH, L"xboxController.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload,
        strFilePath,
        m_resourceDescriptors->GetCpuHandle(Descriptors::ControllerFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::ControllerFont));

    {
        const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);

        const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &defaultHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_GRAPHICS_PPV_ARGS(m_defaultTex.ReleaseAndGetAddressOf())));

        static const uint32_t s_pixel = 0xffffffff;

        const D3D12_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };

        resourceUpload.Upload(m_defaultTex.Get(), 0,&initData, 1);

        resourceUpload.Transition(
            m_defaultTex.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = desc.Format;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels = 1;

        device->CreateShaderResourceView(m_defaultTex.Get(), &SRVDesc, m_resourceDescriptors->GetCpuHandle(Descriptors::DefaultTex));
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    const auto viewPort = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewPort);

#ifdef XBOX
    // Scale sprite batch rendering when running 1440p/4k
    static const D3D12_VIEWPORT s_vp1080 = { 0.f, 0.f, 1920.f, 1080.f, D3D12_DEFAULT_VIEWPORT_MIN_DEPTH, D3D12_DEFAULT_VIEWPORT_MAX_DEPTH };
    m_spriteBatch->SetViewport(s_vp1080);
#elif defined(UWP)
    if (m_deviceResources->GetDeviceOptions() & (DX::DeviceResources::c_Enable4K_Xbox | DX::DeviceResources::c_EnableQHD_Xbox))
    {
        // Scale sprite batch rendering when running 4k
        static const D3D12_VIEWPORT s_vp1080 = { 0.f, 0.f, 1920.f, 1080.f, D3D12_DEFAULT_VIEWPORT_MIN_DEPTH, D3D12_DEFAULT_VIEWPORT_MAX_DEPTH };
        m_spriteBatch->SetViewport(s_vp1080);
    }

    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());
#endif
}

#ifdef LOSTDEVICE
void Game::OnDeviceLost()
{
    m_spriteBatch.reset();
    m_comicFont.reset();
    m_ctrlFont.reset();

    m_defaultTex.Reset();

    m_resourceDescriptors.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#endif
#pragma endregion
