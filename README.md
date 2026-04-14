# Modmenu for Magic Chess: Go Go

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Android-green.svg)](https://developer.android.com)
[![NDK](https://img.shields.io/badge/NDK-arm64--v8a-blue.svg)](https://developer.android.com/ndk)
[![C++](https://img.shields.io/badge/C++-20-00599C.svg?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Build](https://img.shields.io/github/actions/workflow/status/Yan-0001/MagicChessGoGo/release.yml?label=CI)](https://github.com/Yan-0001/MagicChessGoGo/actions)
<a href="https://play.google.com/store/apps/details?id=com.mobilechess.gp">
  <img src="https://img.shields.io/badge/Game-Magic_Chess_Go_Go-important?logo=youtubegaming" alt="Game - Magic Chess Go Go">
</a>

An open-source native mod menu for **Magic Chess: Go Go**, built with C++20 and the Android NDK. It injects a floating in-game overlay that exposes gameplay utilities through a Dear ImGui interface, with hooks powered by Dobby and IL2CPP symbol resolution via XDL.

</div>

---

## Table of Contents

- [Overview](#overview)
- [Technical Stack](#technical-stack)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Building](#building)
- [CI/CD & Releases](#cicd--releases)
- [Available Features](#available-features)
- [Logging & Debugging](#logging--debugging)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

This project compiles to a native Android shared library (`.so`) that is injected into the Magic Chess: Go Go game process at runtime. Once loaded, it intercepts internal game functions, resolves symbols from in-memory libraries, and renders a floating mod menu overlay using Dear ImGui — all without modifying the game's APK.

**How it works:**

The library entry point (`Main.cpp`) is executed as a background thread upon injection. It uses **Dobby** to hook into key game functions at runtime — including `eglSwapBuffers` for rendering the ImGui overlay and Unity's `Input.GetTouch` for capturing touch input within the mod menu. IL2CPP symbols are resolved dynamically using **XDL**, with support for nested class hierarchies, enabling reliable access to game internals across updates.

---

## Technical Stack

| Component | Technology | Purpose |
|---|---|---|
| Language | C++20 | Core mod logic and hook implementation |
| Build System | Android NDK (`ndk-build`) | Native compilation for ARM targets |
| Hooking | [Dobby](https://github.com/dobbygj/dobby) | Runtime function interception |
| Symbol Resolution | [XDL](https://github.com/hexhacking/xDL) | Dynamic library introspection |
| UI Rendering | [Dear ImGui](https://github.com/ocornut/imgui) | In-game floating menu overlay |
| Networking | libcurl + OpenSSL | Optional HTTPS communication |
| Target OS | Android API 21+ | Minimum supported platform |
| Architecture | `arm64-v8a` | Primary target ABI |

---

## Project Structure

```text
.
├── jni/                        # Native source and NDK build configuration
│   ├── Main.cpp                # Library entry point and core mod logic
│   ├── Android.mk              # NDK module definitions and linker flags
│   ├── Application.mk          # ABI, platform, and STL settings
│   ├── DOBBY/                  # Dobby hook framework (prebuilts + headers)
│   ├── XDL/                    # XDL symbol resolver (source + headers)
│   ├── IMGUI/                  # Dear ImGui (source + headers)
│   ├── CURL/                   # libcurl prebuilts
│   └── OPENSSL/                # OpenSSL prebuilts
├── dump/                       # IL2CPP metadata dump (used for symbol offsets)
├── libs/                       # Output directory for compiled .so files
│   └── arm64-v8a/
│       └── libmain.so
├── obj/                        # Intermediate build objects (generated)
└── .github/
    └── workflows/
        └── release.yml         # Automated build and release pipeline
```

---

## Getting Started

### Prerequisites

Before building, ensure the following tools are available on your system:

| Requirement | Notes |
|---|---|
| [Android NDK r25+](https://developer.android.com/ndk/downloads) | Must be on your system `PATH` |
| `ndk-build` | Included with the NDK installation |
| `adb` *(optional)* | Required only for log inspection on a connected device |

Verify your NDK installation before proceeding:

```bash
ndk-build --version
```

---

## Building

Clone the repository and run `ndk-build` from the project root:

```bash
git clone https://github.com/Yan-0001/MagicChessGoGo.git
cd MagicChessGoGo

ndk-build -C jni
```

On a successful build, the compiled shared library will be placed at:

```
libs/arm64-v8a/libmain.so
```

---

## CI/CD & Releases

Automated builds are handled by **GitHub Actions**. Every push of a version tag triggers the pipeline, which compiles the library and publishes a new GitHub Release with the `.so` file attached as a downloadable asset.

### Creating a Release

Create and push a version tag prefixed with `v`:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The pipeline will then build `libmain.so` for all configured architectures, create a GitHub Release entry, and attach the compiled artifacts. The workflow definition is located at `.github/workflows/release.yml`.

---

## Available Features

The following mod features are currently implemented:

| Feature | Description |
|---|---|
| **Next Enemy Info** | Displays information about the next incoming enemy formation before it arrives |

Additional features are planned. Community contributions for new features are welcomed — please review the [Contributing](#contributing) section before submitting.

---

## Logging & Debugging

The project uses Android's native logging subsystem (`__android_log_print`). All log output is tagged `MagicChessGoGo` and can be filtered with `adb logcat`.

### Viewing Logs

```bash
adb logcat -s MagicChessGoGo
```

### Debug vs. Production Builds

Verbose logging is controlled by the `IS_DEBUG` macro in `jni/Main.cpp`:

```cpp
#define IS_DEBUG 1   // Enable verbose logging (development)
#define IS_DEBUG 0   // Disable logging (production)
```

Set `IS_DEBUG` to `0` before building release artifacts to eliminate logging overhead.

---

## Contributing

Contributions are welcome. For minor fixes or improvements, feel free to open a pull request directly. For significant changes or new feature proposals, please open an issue first to allow for discussion before implementation begins.

### Workflow

```bash
# 1. Fork the repository and clone your fork
git clone https://github.com/YOUR_USERNAME/MagicChessGoGo.git

# 2. Create a feature branch
git checkout -b feature/your-feature-name

# 3. Commit your changes with a descriptive message
git commit -m "feat: add your feature description"

# 4. Push to your fork and open a Pull Request
git push origin feature/your-feature-name
```

Please follow existing code style and include relevant comments where the logic is non-obvious.

---

## Disclaimer

This project is intended for **educational and research purposes only**. Use of this mod in online or competitive game modes may violate the Magic Chess: Go Go [Terms of Service](https://www.moonton.com/terms) and could result in account penalties. The authors assume no responsibility for misuse.

---

## License

Distributed under the **MIT License**. See [`LICENSE`](LICENSE) for full details.
