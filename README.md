# Modmenu for Magic Chess: Go Go

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Android-green.svg)](https://developer.android.com)
[![NDK](https://img.shields.io/badge/NDK-arm64--v8a%20%7C%20armeabi--v7a-blue.svg)](https://developer.android.com/ndk)
[![C++](https://img.shields.io/badge/C++-20-00599C.svg?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
<a href="https://play.google.com/store/apps/details?id=com.mobilechess.gp">
  <img src="https://img.shields.io/badge/Game-Magic_Chess_Go_Go-important?logo=youtubegaming" alt="Game - Magic Chess Go Go">
</a>

A high-performance, open-source native modding framework for **Magic Chess: Go Go**, built on C++20 and Android NDK. Enables runtime game modification via process injection, binary instrumentation, and a real-time ImGui overlay.

</div>

---

## Table of Contents

- [Overview](#overview)
- [Technical Stack](#technical-stack)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Building](#building)
- [CI/CD & Releases](#cicd--releases)
- [Logging & Debugging](#logging--debugging)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

This project compiles to a native Android shared library (`.so`) that is injected into the Magic Chess: Go Go game process at runtime. It hooks into internal game functions, resolves symbols from loaded libraries, and renders a floating mod menu using Dear ImGui.

**Key capabilities:**

- Runtime function interception via Dobby hooks with aggressive safety validation
- Advanced IL2CPP symbol resolution (supporting nested classes and hierarchy scanning) via XDL
- In-game overlay UI rendered through ImGui (hooked into `eglSwapBuffers`)
- Interactive UI input via Unity `Input.GetTouch` interception
- Structured logging via Android `logcat`

---

## Technical Stack

| Component | Technology | Purpose |
|---|---|---|
| Language | C++20 | Core mod logic |
| Build System | Android NDK (`ndk-build`) | Native compilation |
| Hooking | [Dobby](https://github.com/dobbygj/dobby) | Runtime function interception |
| Symbol Resolution | [XDL](https://github.com/hexhacking/xDL) | Dynamic library introspection |
| UI | [Dear ImGui](https://github.com/ocornut/imgui) | In-game mod menu overlay |
| Target OS | Android API 21+ | Minimum supported platform |
| Architecture | `arm64-v8a`, `armeabi-v7a` | Target ABI |

---

## Project Structure

```text
.
├── jni/                        # Native source and NDK build config
│   ├── Main.cpp                # Library entry point and core mod logic
│   ├── Android.mk              # NDK module and linker configuration
│   ├── Application.mk          # ABI, platform, and STL settings
│   ├── DOBBY/                  # Dobby prebuilts and headers
│   ├── XDL/                    # XDL source and headers
│   ├── IMGUI/                  # Dear ImGui source and headers
│   ├── CURL/                   # libcurl prebuilts
│   └── OPENSSL/                # OpenSSL prebuilts
├── dump/                       # IL2CPP metadata dump
├── libs/                       # Output: compiled .so files
│   ├── arm64-v8a/
│   │   └── libmain.so
│   └── armeabi-v7a/
│       └── libmain.so
├── obj/                        # Intermediate build objects
└── .github/
    └── workflows/
        └── release.yml         # CI/CD pipeline
```

---

## Getting Started

### Prerequisites

| Requirement | Notes |
|---|---|
| [Android NDK](https://developer.android.com/ndk/downloads) | Must be on your system `PATH` |
| `ndk-build` | Included with NDK |
| `adb` (optional) | For log inspection on a connected device |

Verify your NDK installation:

```bash
ndk-build --version
```

---

## Building

Clone the repository and build from the project root:

```bash
git clone https://github.com/Yan-0001/MagicChessGoGo.git
cd MagicChessGoGo

# Build the native library
ndk-build -C jni

# Clean all build artifacts
ndk-build -C jni clean
```

The compiled library will be placed at:

```
libs/arm64-v8a/libmain.so
libs/armeabi-v7a/libmain.so
```

> **Note:** Ensure `APP_ABI` in `Application.mk` matches your target device architecture.

---

## CI/CD & Releases

This project uses **GitHub Actions** for automated builds and tagged releases.

### Triggering a Release

1. Create and push a version tag (must start with `v`):

```bash
git tag v1.0.0
git push origin v1.0.0
```

2. The CI pipeline will automatically:
   - Build `libmain.so` for all configured architectures
   - Create a GitHub Release
   - Attach the compiled `.so` files as release assets

### Workflow Location

```
.github/workflows/release.yml
```

---

## Logging & Debugging

The project uses Android's native logging system (`__android_log_print`).

### Viewing Logs

```bash
adb logcat -s MagicChessGoGo
```

### Debug Mode

Verbose logging is controlled by the `IS_DEBUG` macro in `jni/Main.cpp`:

```cpp
#define IS_DEBUG 1   // Enable verbose logging
#define IS_DEBUG 0   // Disable (production builds)
```

> Set `IS_DEBUG` to `0` before building release artifacts to reduce overhead.

---

## Contributing

Contributions are welcome and encouraged. To get started:

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Commit your changes: `git commit -m 'Add some feature'`
4. Push to the branch: `git push origin feature/your-feature`
5. Open a Pull Request

Please open an issue first for significant changes or new feature proposals.

---

## License

Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for full details.
