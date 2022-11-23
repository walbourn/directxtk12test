# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.20)

project (DirectXTK12Test
  DESCRIPTION "DirectX Tool Kit for DX12 Test Suite"
  HOMEPAGE_URL "https://github.com/walbourn/directxtk12test/wiki"
  LANGUAGES CXX)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  message(FATAL_ERROR "DirectX Tool Kit Test Suite should be built by the main CMakeLists")
endif()

set(TEST_EXES headertest)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/HeaderTest)

# TODO: fuzzloaders

# d3d12test
list(APPEND TEST_EXES d3d12test)
add_executable(d3d12test WIN32
    D3D12Test/Game.cpp
    D3D12Test/Game.h
    D3D12Test/pch.h
    Common/MainPC.cpp
    Common/DeviceResourcesPC.cpp
    Common/DeviceResourcesPC.h
    Common/StepTimer.h
    Common/d3dx12.h
    Common/DirectXTKTest.h
    )
target_compile_definitions(d3d12test PRIVATE USE_LEGACY_PIX_EVENTS)
target_include_directories(d3d12test PRIVATE ./D3D12Test)
target_precompile_headers(d3d12test PRIVATE D3D12Test/pch.h)
add_test(NAME "d3d12" COMMAND d3d12test -ctest WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set_tests_properties(d3d12 PROPERTIES TIMEOUT 30)

# TODO: AnimTest
# TODO: EffectsTest
# TODO: HDRTest
# TODO: LoadTest
# TODO: ModelTest
# TODO: PBRModelTest
# TODO: PBRTest
# TODO: PostProcessTest
# TODO: PrimitivesTest
# TODO: ShaderTest
# TODO: SpriteBatchTest
# TODO: SpriteFontTest
# TOOD: mGPUTest

# TODO: Audio3DTest
# TODO: SimpleAudioTest

# TODO: GamePadTest
# TODO: KeyboardTest
# TOOD: MouseTest

message("INFO: Enabled tests: ${TEST_EXES}")
foreach(t IN LISTS TEST_EXES)
  target_include_directories(d3d12test PRIVATE ./Common)
  target_link_libraries(${t} PRIVATE DirectXTK12 dxgi.lib d3d12.lib)
endforeach()

if (MINGW OR (NOT WIN32) OR VCPKG_TOOLCHAIN)
    foreach(t IN LISTS TEST_EXES)
      target_link_libraries(${t} PRIVATE Microsoft::DirectXMath)
    endforeach()
endif()

if (MSVC)
  foreach(t IN LISTS TEST_EXES)
    target_compile_options(${t} PRIVATE "$<IF:$<STREQUAL:${t},headertest>,/Wall,/W4>" /GR- /fp:fast "$<$<NOT:$<CONFIG:DEBUG>>:/guard:cf>")
    target_link_options(${t} PRIVATE /DYNAMICBASE /NXCOMPAT)
  endforeach()

  if((${CMAKE_SIZEOF_VOID_P} EQUAL 4) AND (NOT (${DIRECTX_ARCH} MATCHES "^arm")))
    foreach(t IN LISTS TEST_EXES)
      target_link_options(${t} PRIVATE /SAFESEH)
    endforeach()
  endif()

  if((MSVC_VERSION GREATER_EQUAL 1928) AND (CMAKE_SIZEOF_VOID_P EQUAL 8)
    AND NOT ENABLE_OPENEXR_SUPPORT
    AND ((NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang")) OR (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13.0)))
      foreach(t IN LISTS TEST_EXES)
        target_compile_options(${t} PRIVATE "$<$<NOT:$<CONFIG:DEBUG>>:/guard:ehcont>")
        target_link_options(${t} PRIVATE "$<$<NOT:$<CONFIG:DEBUG>>:/guard:ehcont>")
      endforeach()
    endif()
endif()

if(NOT (${DIRECTX_ARCH} MATCHES "^arm"))
    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        set(ARCH_SSE2 $<$<CXX_COMPILER_ID:MSVC>:/arch:SSE2> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-msse2>)
    else()
        set(ARCH_SSE2 $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-msse2>)
    endif()

    foreach(t IN LISTS TEST_EXES)
      target_compile_options(${t} PRIVATE ${ARCH_SSE2})
    endforeach()
endif()

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    target_compile_options(headertest PRIVATE -Wall -Wpedantic -Wextra)
    foreach(t IN LISTS TEST_EXE)
      target_compile_options(${t} PRIVATE "-Wno-c++98-compat" "-Wno-c++98-compat-pedantic" "-Wno-language-extension-token")
    endforeach()
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    foreach(t IN LISTS TEST_EXE)
      target_compile_options(${t} PRIVATE /sdl /permissive- /JMC- /Zc:__cplusplus)
    endforeach()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.24)
      foreach(t IN LISTS TEST_EXES)
        target_compile_options(${t} PRIVATE /ZH:SHA_256)
      endforeach()
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
      foreach(t IN LISTS TEST_EXES)
        target_compile_options(${t} PRIVATE /Zc:preprocessor /wd5105)
      endforeach()
    endif()

    if ((CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.27) AND (NOT ((${DIRECTX_ARCH} MATCHES "^arm"))))
      foreach(t IN LISTS TEST_EXES)
        target_link_options(${t} PRIVATE /CETCOMPAT)
      endforeach()
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.34)
      foreach(t IN LISTS TEST_EXES)
        target_compile_options(${t} PRIVATE /wd5262 /wd5264)
      endforeach()
    endif()
endif()

if(WIN32)
  foreach(t IN LISTS TEST_EXES)
    target_compile_definitions(${t} PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0A00)
  endforeach()
endif()
