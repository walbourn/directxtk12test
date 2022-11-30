// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifdef __MINGW32__
#include <unknwn.h>
#endif

#ifdef USING_DIRECTX_HEADERS
#include <directx/d3d12.h>
#else
#include <d3d12.h>
#endif

#include "SimpleMath.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif

void simplemathtest()
{
}
