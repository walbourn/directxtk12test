# GitHub Copilot Instructions — DirectXTK12 Test Suite

These instructions define how GitHub Copilot should assist with the DirectX Tool Kit for DirectX 12 **test suite**. They supplement the [parent project instructions](../../.github/copilot-instructions.md) with test-specific conventions.

## Context

- **Project**: Test suite for DirectX Tool Kit for DirectX 12
- **Repository**: <https://github.com/walbourn/directxtk12test>
- **Language**: C++
- **Test Framework**: Custom lightweight framework (no third-party dependency)
- **Build System**: CMake with CTest integration
- **Documentation**: <https://github.com/walbourn/directxtk12test/wiki>

## Test Categories

| Category | Test Directories | CTest Labels |
| --- | --- | --- |
| Header compilation | `HeaderTest` | — |
| API unit tests | `ApiTest` | `API` |
| Audio I/O | `WavTest` | `Audio`, `wav` |
| Audio playback | `Audio3DTest`, `SimpleAudioTest` | `Audio` |
| Graphics rendering | `D3D12Test`, `MSAATest`, `EffectsTest`, `PrimitivesTest`, `SpriteBatchTest`, `SpriteFontTest`, `LoadTest`, `ModelTest`, `ShaderTest`, `AnimTest`, `HDRTest`, `PBRTest`, `PBRModelTest`, `PostProcessTest`, `mGPUTest` | `Graphics`, and optionally `HLSL`, `Sprites`, `Models`, `Shapes`, `PostProcess` |
| Image formats | `DdsWicTest` | `ImageFormats` |
| Font file I/O | `FontFileTest` | `ImageFormats`, `Sprites` |
| Input devices | `GamePadTest`, `KeyboardTest`, `MouseTest` | `Input` |
| Xbox-specific | `XboxLoadTest` | — |
| Fuzz testing | `fuzzloaders` | — |

## Test Architecture

### Two Test Patterns

**1. Console tests** (`ApiTest`, `HeaderTest`, `WavTest`, `DdsWicTest`, `FontFileTest`)

These are headless executables that run to completion without creating a window or D3D12 device (or create a device briefly). They use a **test registry** pattern:

```cpp
typedef bool (*TestFN)(_In_ ID3D12Device*);

struct TestInfo { const char *name; TestFN func; };

const TestInfo g_Tests[] = {
    { "FeatureName", Test01 },
    { "OtherFeature", Test02 },
};
```

Each test function returns `true` (pass) or `false` (fail). The `wmain()` entry point iterates the registry, prints results, and exits with `0` on success or `-1` on failure.

**2. Graphics tests** (`D3D12Test`, `EffectsTest`, `ModelTest`, etc.)

These are visual tests that create a window, initialize D3D12, and run a game loop. They use the **Game class** pattern:

```cpp
class Game final : public DX::IDeviceNotify
{
public:
    Game() noexcept(false);
    void Initialize(HWND window, int width, int height, DXGI_MODE_ROTATION rotation);
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

private:
    void Update(DX::StepTimer const& timer);
    void Render();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    std::unique_ptr<DX::DeviceResources> m_deviceResources;
    DX::StepTimer m_timer;
    uint64_t m_frame;
};
```

Each graphics test defines a `c_testTimeout` constant (milliseconds) that controls when the test automatically exits. Tests cycle through visual scenarios using the frame counter.

### Common Infrastructure (`Common/`)

| File | Purpose |
| --- | --- |
| `DirectXTKTest.h` | Platform detection macros (`PC`, `XBOX`, `UWP`, `COMBO_GDK`, `LOSTDEVICE`, `COREWINDOW`) |
| `DeviceResourcesPC.h/.cpp` | D3D12 device lifecycle for Windows desktop |
| `DeviceResourcesUWP.h/.cpp` | D3D12 device lifecycle for UWP |
| `DeviceResourcesGDK.h/.cpp` | D3D12 device lifecycle for GDK |
| `MainPC.cpp` | Win32 entry point and message pump |
| `MainUWP.cpp` | UWP entry point |
| `MainGDK.cpp` | GDK entry point |
| `StepTimer.h` | High-resolution timer for game loop |
| `FindMedia.h` | Media file search utility |
| `TextConsole.h/.cpp` | On-screen text overlay for visual tests |
| `d3dx12.h` | D3D12 helper structures |

### Precompiled Headers

Every test directory has its own `pch.h` / `pch.cpp`. The precompiled header includes DirectXTK12 public headers needed by that test. All `.cpp` files in a test must include `pch.h` as their first include.

## Patterns to Follow

### Naming Conventions

| Element | Convention | Example |
| --- | --- | --- |
| Test directory | PascalCase + `Test` suffix | `EffectsTest`, `ModelTest` |
| Test executable | lowercase (CMake target) | `effectstest`, `modeltest` |
| Test function | `TestNN` (zero-padded number) | `Test01`, `Test02`, `Test21` |
| Test entry point | `wmain()` or Game class | Unicode-aware entry point |
| Game class files | `Game.h` / `Game.cpp` | Per-test implementation |

### Console Test Function Signature

```cpp
_Success_(return)
bool Test01(_In_ ID3D12Device* device)
{
    // ... test logic ...
    return true;  // PASS
}
```

- Always annotate with `_Success_(return)` SAL annotation.
- Accept `ID3D12Device*` parameter annotated with `_In_`.
- Return `bool` — `true` for pass, `false` for fail.
- Use `printf()` for output (not `std::cout` or logging frameworks).

### Error Reporting

```cpp
// HRESULT failures
if (FAILED(hr))
{
    printf("ERROR: Description (%08X)\n", static_cast<unsigned int>(hr));
    return false;
}

// Exception handling
try
{
    auto obj = std::make_unique<SomeClass>(device);
}
catch (const std::exception& e)
{
    printf("ERROR: Failed creating object (except: %s)\n", e.what());
    return false;
}
```

- Print `"ERROR: "` prefix for failures with HRESULT as `%08X`.
- Wrap resource creation in `try`/`catch` blocks.
- Never use `assert()` for test validation — return `false` instead.

### Test Registration and Main

```cpp
struct TestInfo
{
    const char *name;
    TestFN func;
};

const TestInfo g_Tests[] =
{
    { "FeatureName", Test01 },
    { "OtherFeature", Test02 },
};

int wmain()
{
    // Initialize COM if needed
    // Create D3D12 device if needed
    // Run test loop
    size_t nPass = 0;
    size_t nFail = 0;

    for (size_t i = 0; i < std::size(g_Tests); ++i)
    {
        printf("%s: ", g_Tests[i].name);
        if (g_Tests[i].func(device))
        {
            ++nPass;
            printf("PASS\n");
        }
        else
        {
            ++nFail;
            printf("FAIL\n");
        }
    }

    printf("Ran %zu tests, %zu pass, %zu fail\n", nPass + nFail, nPass, nFail);
    return (nFail == 0) ? 0 : -1;
}
```

### Graphics Test Timeout

```cpp
constexpr uint32_t c_testTimeout = 10000;  // milliseconds
```

Graphics tests must define a timeout constant and exit automatically when reached. Use `ExitGame()` to terminate. The `Escape` key and gamepad `View` button should also trigger exit for interactive use.

### Media File Discovery

```cpp
wchar_t path[MAX_PATH] = {};
DX::FindMediaFile(path, MAX_PATH, L"texture.dds");
```

Use `FindMedia.h` to locate test assets. Place assets in the test's own directory or a `Media/` subdirectory. Register custom search folders when needed:

```cpp
static const wchar_t* s_searchFolders[] =
{
    L"MyTest",
    L"Tests\\MyTest",
    nullptr
};
```

### Platform Conditionals

Use the macros defined in `DirectXTKTest.h`:

```cpp
#ifdef PC
    // Windows desktop code
#endif

#ifdef XBOX
    // Xbox-specific code
#endif

#ifdef UWP
    // UWP-specific code
#endif

#ifdef LOSTDEVICE
    // Device lost/restored handling
#endif
```

Do not invent new platform macros. Use the established `#ifdef` guards from the parent project instructions.

## CMake Integration

### Adding a New Console Test

```cmake
add_executable(mynewtest
    main.cpp
    test01.cpp
    pch.h
    pch.cpp
)

target_link_libraries(mynewtest PRIVATE DirectXTK12)
target_precompile_headers(mynewtest PRIVATE pch.h)

add_test(NAME "mynewtest" COMMAND mynewtest -ctest)
set_tests_properties("mynewtest" PROPERTIES
    LABELS "API"
    TIMEOUT 30
)
```

### Adding a New Graphics Test

```cmake
add_executable(mynewgfxtest WIN32
    Game.cpp
    Game.h
    pch.h
    pch.cpp
    ${D3D_COMMON_FILES}
)

target_link_libraries(mynewgfxtest PRIVATE DirectXTK12 d3d12.lib dxgi.lib dxguid.lib)
target_precompile_headers(mynewgfxtest PRIVATE pch.h)

add_test(NAME "mygfxtest" COMMAND mynewgfxtest -ctest)
set_tests_properties("mygfxtest" PROPERTIES
    LABELS "Graphics"
    TIMEOUT 60
)
```

### CTest Labels

Always assign at least one label. Use multiple labels for cross-cutting concerns:

- `API` — headless API tests
- `Audio` — audio engine tests
- `Graphics` — visual rendering tests
- `HLSL` — shader compilation tests
- `Sprites` — SpriteBatch/SpriteFont tests
- `Models` — model loading/rendering tests
- `Shapes` — geometric primitive tests
- `PostProcess` — post-processing tests
- `ImageFormats` — texture loader tests
- `Input` — input device tests
- `Math` — math/utility tests (run in both Debug and Release CI)

### CTest Timeouts

| Test type | Typical timeout |
| --- | --- |
| Header compilation | 30s |
| API unit tests | 30s |
| Audio file tests | 60s |
| Graphics rendering | 60s |
| Image format loading | 180s |
| Input device tests | 60s |

### Running Tests Locally

```bash
cmake --preset=x64-Debug -DBUILD_TESTING=ON
cmake --build out\build\x64-Debug
ctest --preset=x64-Debug                  # All tests
ctest --preset=x64-Debug -L Math          # By label
ctest --preset=x64-Debug -R apitest       # By name
```

## Fuzz Testing (`fuzzloaders/`)

Fuzz tests use the LLVM fuzzer entry point and target file format parsers:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Parse data as target format
    // Return 0
}
```

Compile with `/fsanitize=fuzzer` (MSVC 19.32+). Target formats: DDS, WAV, XWB, WIC, CMO, SDKMESH, VBO.

## Code Coverage

Enable with `ENABLE_CODE_COVERAGE=ON` in CMake. Coverage configs are in `codecov/` with separate configurations for graphics, audio, input, and BVT tests.

## Patterns to Avoid

- Don't use third-party test frameworks (Google Test, Catch2, etc.) — use the existing custom pattern.
- Don't use `std::cout` or `std::cerr` — use `printf()` for test output.
- Don't use `assert()` for test validation — return `false` from test functions.
- Don't create standalone test CMake projects — tests must build from the parent `CMakeLists.txt` with `BUILD_TESTING=ON`.
- Don't hardcode absolute paths to media files — use `FindMediaFile()`.
- Don't skip the precompiled header — every `.cpp` file must include `pch.h` first.
- Don't add external dependencies beyond what the parent project already uses.

## File Header Convention

Test files use the same header block as the main project, referencing the test repository URL:

```cpp
//--------------------------------------------------------------------------------------
// File: Game.cpp
//
// Developer unit test for DirectX Tool Kit - <Feature Name>
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
```

## References

- [DirectXTK12 Test Suite Repository](https://github.com/walbourn/directxtk12test)
- [Test Suite Wiki](https://github.com/walbourn/directxtk12test/wiki)
- [Parent Project Copilot Instructions](../../.github/copilot-instructions.md)
- [DirectXTK12 Implementation Guide](https://github.com/microsoft/DirectXTK12/wiki/Implementation)
