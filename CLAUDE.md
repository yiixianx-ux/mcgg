# CLAUDE.md

This file provides context and guidance to **Claude Code** (`claude.ai/code`) when working within this repository. It describes the build system, architecture, key components, and development conventions.

---

## Project Summary

This is an **Android native modding library** for Magic Chess: Go Go, written in C++20 and built with the Android NDK. The compiled output is a shared library (`libmain.so`) intended for injection into the game process at runtime.

---

## Build System

### Toolchain

| Tool | Role |
|---|---|
| `ndk-build` | Primary build tool (wraps `make`) |
| `Android.mk` | Defines modules, source files, and linker flags |
| `Application.mk` | Sets ABI targets, platform, C++ standard, and STL |

### Common Commands

```bash
# Full build
ndk-build -C jni

# Clean all artifacts
ndk-build -C jni clean

# Stream device logs filtered to this library
adb logcat -s MagicChessGoGo
```

### Build Configuration

| Setting | Value |
|---|---|
| C++ Standard | C++20 |
| Minimum SDK | API 21 |
| Target ABI(s) | `arm64-v8a` |
| Output | `libs/<abi>/libmain.so` |

---

## Architecture

### Directory Layout

```
jni/
├── Main.cpp          # Library entry point; all initialization lives here
├── Android.mk        # Module definitions, source list, link libraries
├── Application.mk    # Platform, ABI, C++ standard configuration
├── DOBBY/            # Dobby hooking framework (prebuilts + headers)
├── XDL/              # XDL dynamic linker utility (source + headers)
├── IMGUI/            # Dear ImGui UI library (source + headers)
├── CURL/             # libcurl (headers + static libs)
└── OPENSSL/          # OpenSSL (headers + static libs)

dump/                 # IL2CPP dump for class/methods/fields/enum reference
libs/                 # Final .so output per ABI
obj/                  # Intermediate .o and build state files
```

### Initialization Flow

The library entry point is the **`InitLibrary` constructor** in `jni/Main.cpp`. It is invoked automatically when the `.so` is loaded into the process via `__attribute__((constructor))`. 

Before proceeding, the library validates that it is running within the intended game process by checking `/proc/self/cmdline` for the `:UnityKillsMe` identifier.

The typical flow is:

```
Library loaded into process
        │
        ▼
InitLibrary() constructor runs
        │
        ├──▶ Process validated via /proc/self/cmdline
        │
        └──▶ Detached SetupThread spawned
                │
                ├──▶ Hook eglSwapBuffers for UI overlay
                │
                ├──▶ Wait for liblogic.so to load
                │
                ├──▶ Resolve IL2CPP API (using DO_API macro)
                │
                ├──▶ Attach thread to IL2CPP domain
                │
                └──▶ Resolve game methods & install hooks (using DO_HOOK macro)

Parallel JNI Flow:
JNI_OnLoad (registered via NativeLoader)
        │
        └──▶ LoadOriginalLibrary() ──▶ dlopen("libunity.so") ──▶ execute original JNI_OnLoad
```

---

## Key Components

### Dobby — Runtime Hooking

- **Location:** `jni/DOBBY/`
- **Purpose:** Intercepts calls to target functions at runtime by patching their prologues with trampolines.
- **Usage pattern:**

```cpp
DobbyHook(
    (void*)target_function_address,
    (void*)my_hook_function,
    (void**)&original_function
);
```

- Always store the original function pointer — Dobby provides it via the third argument.
- Hooks must be installed after the target library is loaded; use XDL to confirm the library is present first.

### XDL — Symbol Resolution

- **Location:** `jni/XDL/`
- **Purpose:** Resolves function and data symbols from loaded shared libraries without the restrictions of standard `dlopen`/`dlsym`.
- **Usage pattern:**

```cpp
void* handle = xdl_open("libtarget.so", XDL_DEFAULT);
void* sym    = xdl_sym(handle, "TargetFunctionName", nullptr);
```

- Prefer XDL over `dlsym` for game library symbols, as many are stripped from the standard export table.
- Always null-check returned addresses before passing to Dobby.

### External Libraries

- **Dobby**: Core hooking engine.
- **XDL**: Advanced symbol resolution and library loading.
- **Dear ImGui**: Mod menu UI framework. Rendered via `eglSwapBuffers` hook and interactive via `Input.GetTouch` hook.
- **CURL & OpenSSL**: Linked for future networking capabilities (not currently active in `Main.cpp`).


### Android Logging

- Uses `__android_log_print` from `<android/log.h>`.
- Log levels: `LOGV` (Verbose), `LOGD` (Debug), `LOGI` (Info), `LOGW` (Warn), `LOGE` (Error).
- All log calls are wrapped behind the `IS_DEBUG` macro to allow zero-overhead production builds:

```cpp
#define IS_DEBUG 0

#if IS_DEBUG
  #define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "MagicChessGoGo", __VA_ARGS__)
  #define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "MagicChessGoGo", __VA_ARGS__)
  #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MagicChessGoGo", __VA_ARGS__)
  #define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "MagicChessGoGo", __VA_ARGS__)
  #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MagicChessGoGo", __VA_ARGS__)
#else
  #define LOGV(...) ((void)0)
  #define LOGD(...) ((void)0)
  #define LOGI(...) ((void)0)
  #define LOGW(...) ((void)0)
  #define LOGE(...) ((void)0)
#endif
```

---

## Development Conventions

- **Hook safety:** All Dobby hooks must guard against null symbol resolution and invalid state. Never hook a null address, and always validate critical values (e.g., account IDs != -1) before passing them to original game functions.
- **Input validation:** All IL2CPP helper functions (e.g., `ResolveClassFromName`, `GetMethodFromName`) must strictly validate all input pointers and returned symbols to prevent null pointer dereferences.
- **Thread safety:** ImGui rendering and hook callbacks may run on different threads. Use mutexes or atomic flags to share state.
- **Macro hygiene:** Use `DO_API` for symbol resolution and `DO_HOOK` for installing hooks to reduce boilerplate. Keep `IS_DEBUG` set to `0` in committed code unless actively debugging. CI builds should always use `IS_DEBUG 0`.
- **Memory:** Prefer stack allocation in hook callbacks. Avoid heap allocations in hot-path hooks to minimize latency impact.
- **Symbol names:** Game symbols may be mangled. Use `xdl_sym` with the demangled name where possible; fall back to offset-based lookup when symbols are fully stripped.
- **IL2CPP Resolution:** When resolving classes, the library supports nested class notation (e.g., `Namespace.Class+Nested`) and scans both parent hierarchies and subclasses for robustness.
- **Large File Handling:** Only use `rg` or `grep` to search `dump/dump.cs` as it contains over 1 million lines and should never be read in full.
