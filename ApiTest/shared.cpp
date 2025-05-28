//--------------------------------------------------------------------------------------
// File: shared.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#include "GamePad.h"
#include "Keyboard.h"
#include "Mouse.h"

#include <cstdio>

#ifdef USING_DIRECTX_HEADERS
#include <directx/d3d12.h>
#else
#include <d3d12.h>
#endif

using namespace DirectX;

_Success_(return)
bool Test14(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    std::unique_ptr<GamePad> pad;
    try
    {
        pad = std::make_unique<GamePad>();
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        return false;
    }

    return true;
}

_Success_(return)
bool Test15(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    std::unique_ptr<Keyboard> kb;
    try
    {
        kb = std::make_unique<Keyboard>();
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        return false;
    }

    return true;
}

_Success_(return)
bool Test16(_In_ ID3D12Device *device)
{
    if (!device)
        return false;

    std::unique_ptr<Mouse> mouse;
    try
    {
        mouse = std::make_unique<Mouse>();
    }
    catch(const std::exception& e)
    {
        printf("ERROR: Failed creating object (except: %s)\n", e.what());
        return false;
    }

    return true;
}
