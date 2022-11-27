// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifdef __MINGW32__
#include <unknwn.h>
#endif

// Suppress additional off-by-default warnings from <future>
#pragma warning(push)
#pragma warning(disable : 4355 4626 4625 5026 5027 5204)
#include <future>
#pragma warning(pop)

#include "ResourceUploadBatch.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif


void resourceuploadbatchtest()
{
}
