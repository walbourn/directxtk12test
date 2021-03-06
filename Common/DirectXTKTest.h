//--------------------------------------------------------------------------------------
// File: DirectXTKTest.h
//
// Helper header for DirectX Tool Kit for DX12 test suite
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkID=615561
//-------------------------------------------------------------------------------------

#pragma once

#ifdef _GAMING_XBOX
#define XBOX
#define GAMEINPUT
#include "DeviceResourcesGXDK.h"

#elif defined(_XBOX_ONE) && defined(_TITLE)
#define XBOX
#define COREWINDOW
#include "DeviceResourcesXDK.h"

#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#define UWP
#define COREWINDOW
#define LOSTDEVICE
#define WGI
#include "DeviceResourcesUWP.h"

#else
#define PC
#define LOSTDEVICE
#include "DeviceResourcesPC.h"

#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
#define WGI
#endif

#endif
