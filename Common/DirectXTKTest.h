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

#ifdef COMBO_GDK
#include "DeviceResourcesGDK.h"
#ifdef _GAMING_DESKTOP
#define PC
#define LOSTDEVICE
#else
#define XBOX
#endif
#elif defined(_GAMING_XBOX)
#define XBOX
#include "DeviceResourcesGXDK.h"
#elif defined(_XBOX_ONE) && defined(_TITLE)
#define XBOX
#define COREWINDOW
#include "DeviceResourcesXDK.h"

#elif defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#define UWP
#define COREWINDOW
#define LOSTDEVICE
#include "DeviceResourcesUWP.h"

#else
#define PC
#define LOSTDEVICE
#include "DeviceResourcesPC.h"

#endif
