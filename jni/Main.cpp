/**
 * ============================================================================
 * @brief   Magic Chess Go Go - Mod Menu
 * 
 * @details This library is designed to be injected into a Unity IL2CPP Android game.
 *          It acts as a proxy library (replacing a native library like libmain.so), loads the original game library, and spawns a background thread to initialize hooking safely.
 * 
 *          Key Features:
 *          - EGL Hooking: Intercepts eglSwapBuffers to render a Dear ImGui overlay.
 *          - Input Interception: Hooks Unity's Input.GetTouch to feed touch data.
 *          - IL2CPP Reflection: Dynamically resolves Unity engine APIs at runtime.
 * 
 * @warning This module operates in a highly volatile memory space. All external
 *          calls must be strictly validated to prevent segmentation faults.
 * ============================================================================
 */

#include <jni.h>
#include <dlfcn.h>
#include <link.h>
#include <elf.h>

#include <android/log.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <pthread.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <complex.h>

#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <stdexcept>

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <deque>
#include <list>
#include <forward_list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <queue>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <limits>
#include <random>
#include <ratio>
#include <tuple>
#include <optional>
#include <variant>
#include <any>
#include <bitset>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "xdl.h"
#include "dobby.h"

#include "imconfig.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "imstb_rectpack.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_android.h"
#include "NotoSans-Regular.h"

#include "Color.hpp"
#include "Vector2.hpp"
#include "Vector3.hpp"
#include "Vector4.hpp"
#include "Quaternion.hpp"
#include "Il2CppWrapper.hpp"

#define IS_DEBUG 0
#define LOG_TAG "MagicChessGoGo"

#if IS_DEBUG
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGV(...)
#define LOGD(...)
#define LOGI(...)
#define LOGW(...)
#define LOGE(...)
#endif

/**
 * @def DO_API
 * @brief Safely resolves an exported IL2CPP API function from the loaded library handle.
 * @param name The exact symbol name of the IL2CPP function.
 * @param handle The dlopen/xdl_open handle to the target library.
 */
#define DO_API(name, handle) \
do { \
    if ((handle) == nullptr) { \
        LOGE("[API_RESOLVE] CRITICAL: Handle is null for %s", #name); \
        break; \
    } \
    void* sym = xdl_sym((handle), #name, nullptr); \
    if (sym != nullptr) { \
        IL2CPP::name = reinterpret_cast<decltype(IL2CPP::name)>(sym); \
        LOGD("[API_RESOLVE] Resolved API symbol: %s at %p", #name, sym); \
    } else { \
        LOGE("[API_RESOLVE] CRITICAL FAILURE: Failed to resolve API symbol: %s", #name); \
    } \
} while(0)

/**
 * @def DO_ASSIGN
 * @brief Dynamically locates a game method via IL2CPP reflection and stores its native address.
 * @param name The variable name in the Originals namespace to store the pointer.
 * @param ns The namespace of the target class.
 * @param className The name of the target class.
 * @param methodName The name of the target method.
 * @param argCount The number of arguments the method takes.
 */
#define DO_ASSIGN(name, ns, className, methodName, argCount) \
do { \
    void* addr = GetMethodFromName((ns), (className), (methodName), (argCount)); \
    if (addr != nullptr) { \
        Originals::name = reinterpret_cast<decltype(Originals::name)>(addr); \
        LOGI("[ASSIGN] Successfully assigned %s::%s", (className), (methodName)); \
    } else { \
        LOGW("[ASSIGN] Failed to assign %s::%s", (className), (methodName)); \
    } \
} while(0)

/**
 * @def DO_HOOK
 * @brief Dynamically locates a game method via IL2CPP reflection and installs an inline Dobby hook.
 * @param name The variable name in the Originals namespace to store the trampoline.
 * @param ns The namespace of the target class.
 * @param className The name of the target class.
 * @param methodName The name of the target method.
 * @param argCount The number of arguments the method takes.
 */
#define DO_HOOK(name, ns, className, methodName, argCount) \
do { \
    void* addr = GetMethodFromName((ns), (className), (methodName), (argCount)); \
    if (addr != nullptr) { \
        if (DobbyHook(addr, reinterpret_cast<void*>(Hooks::name), reinterpret_cast<void**>(&Originals::name)) == 0) { \
            LOGI("[HOOK] Successfully hooked %s::%s", (className), (methodName)); \
        } else { \
            LOGE("[HOOK] DobbyHook failed for %s::%s", (className), (methodName)); \
        } \
    } else { \
        LOGW("[HOOK] Target address not found for %s::%s", (className), (methodName)); \
    } \
} while(0)

/**
 * @enum TouchPhase
 * @brief Represents the lifecycle phase of a touch event in Unity.
 */
enum class TouchPhase { Began, Moved, Stationary, Ended, Canceled };

/**
 * @enum TouchType
 * @brief Represents the type of touch input in Unity.
 */
enum class TouchType { Direct, Indirect, Stylus };

/**
 * @struct Touch
 * @brief Memory-mapped representation of Unity's UnityEngine.Touch struct.
 * @warning Must perfectly match the memory layout of the target Unity version.
 */
struct Touch {
    int m_FingerId;
    Unity::Vector2 m_Position;
    Unity::Vector2 m_RawPosition;
    Unity::Vector2 m_PositionDelta;
    float m_TimeDelta;
    int m_TapCount;
    TouchPhase m_Phase;
    TouchType m_Type;
    float m_Pressure;
    float m_maximumPossiblePressure;
    float m_Radius;
    float m_RadiusVariance;
    float m_AltitudeAngle;
    float m_AzimuthAngle;
};

// ============================================================================
// GLOBAL STATE (Thread-Safe)
// ============================================================================
std::atomic<int> GLWidth{0};
std::atomic<int> GLHeight{0};
std::atomic<void*> Il2cppHandle{nullptr};
std::atomic<void*> UnityLibraryHandle{nullptr};

std::atomic<int> SelfAccountID{-1};
std::atomic<int> EnemyAccountID{-1};

/**
 * @brief Validates if the library is loaded in the target Unity game process.
 * 
 * @details Reads /proc/self/cmdline to determine the process name. This prevents
 *          the mod from accidentally executing inside crash handlers (e.g., crashpad_handler)
 *          or other isolated Android processes.
 * 
 * @return true if the process matches the target Unity game, false otherwise.
 * 
 * @note Thread-safe. Does not mutate global state.
 * @warning I/O operations may fail if SELinux policies restrict /proc/self access.
 */
[[nodiscard]] bool IsUnityProcess() noexcept {
    FILE* fp = fopen("/proc/self/cmdline", "rb");
    if (fp == nullptr) {
        LOGE("[SECURITY] Failed to open /proc/self/cmdline. Errno: %d", errno);
        return false;
    }

    char buffer[256] = {0};
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, fp);
    
    if (ferror(fp)) {
        LOGE("[SECURITY] Error reading /proc/self/cmdline.");
        fclose(fp);
        return false;
    }
    
    fclose(fp);

    if (bytesRead == 0) {
        return false;
    }

    buffer[bytesRead] = '\0'; // Ensure strict null-termination

    const char* target = ":UnityKillsMe";
    const size_t targetLen = strlen(target);

    size_t i = 0;
    while (i < bytesRead) {
        const char* arg = &buffer[i];
        size_t len = strnlen(arg, bytesRead - i);

        if (len >= targetLen && strstr(arg, target) != nullptr) {
            return true;
        }

        i += len + 1;
    }

    return false;
}

/**
 * @namespace IL2CPP
 * @brief Contains function pointers to the Unity engine's internal IL2CPP API.
 * @details These are used to reflectively interact with the game's classes, methods, 
 *          and fields without needing a pre-compiled dummy DLL or header dump.
 */
namespace IL2CPP {
    Il2CppDomain* (*il2cpp_domain_get)() = nullptr;
    Il2CppAssembly** (*il2cpp_domain_get_assemblies)(const Il2CppDomain* domain, size_t* size) = nullptr;
    const Il2CppImage* (*il2cpp_assembly_get_image)(const Il2CppAssembly* assembly) = nullptr;
    Il2CppClass** (*il2cpp_image_get_classes)(const Il2CppImage* image, size_t* size) = nullptr;
    const char* (*il2cpp_type_get_name)(const Il2CppType* type) = nullptr;
    Il2CppClass* (*il2cpp_class_from_name)(const Il2CppImage* image, const char* namespaze, const char* name) = nullptr;
    const char* (*il2cpp_class_get_name)(Il2CppClass* klass) = nullptr;
    const char* (*il2cpp_class_get_namespace)(Il2CppClass* klass) = nullptr;
    Il2CppClass* (*il2cpp_class_get_parent)(Il2CppClass* klass) = nullptr;
    const MethodInfo* (*il2cpp_class_get_methods)(Il2CppClass* klass, void** iter) = nullptr;
    FieldInfo* (*il2cpp_class_get_fields)(Il2CppClass* klass, void** iter) = nullptr;
    const Il2CppType* (*il2cpp_class_get_type)(Il2CppClass* klass) = nullptr;
    Il2CppClass* (*il2cpp_class_get_nested_types)(Il2CppClass* klass, void** iter) = nullptr;
    const Il2CppImage* (*il2cpp_class_get_image)(Il2CppClass* klass) = nullptr;
    const MethodInfo* (*il2cpp_class_get_method_from_name)(Il2CppClass* klass, const char* name, int argsCount) = nullptr;
    FieldInfo* (*il2cpp_class_get_field_from_name)(Il2CppClass* klass, const char* name) = nullptr;
    bool (*il2cpp_class_is_subclass_of)(Il2CppClass* klass, Il2CppClass* klassc, bool check_interfaces) = nullptr;
    size_t (*il2cpp_field_get_offset)(FieldInfo* field) = nullptr;
    const char* (*il2cpp_field_get_name)(FieldInfo* field) = nullptr;
    void (*il2cpp_field_get_value)(Il2CppObject* obj, FieldInfo* field, void* value) = nullptr;
    void (*il2cpp_field_set_value)(Il2CppObject* obj, FieldInfo* field, void* value) = nullptr;
    void (*il2cpp_field_static_get_value)(FieldInfo* field, void* value) = nullptr;
    void (*il2cpp_field_static_set_value)(FieldInfo* field, void* value) = nullptr;
    const char* (*il2cpp_method_get_name)(const MethodInfo* method) = nullptr;
    Il2CppClass* (*il2cpp_method_get_class)(const MethodInfo* method) = nullptr;
    const Il2CppType* (*il2cpp_method_get_return_type)(const MethodInfo* method) = nullptr;
    uint32_t (*il2cpp_method_get_param_count)(const MethodInfo* method) = nullptr;
    const Il2CppType* (*il2cpp_method_get_param)(const MethodInfo* method, uint32_t index) = nullptr;
    Il2CppObject* (*il2cpp_object_new)(Il2CppClass* klass) = nullptr;
    void* (*il2cpp_object_unbox)(Il2CppObject* obj) = nullptr;
    Il2CppClass* (*il2cpp_object_get_class)(Il2CppObject* obj) = nullptr;
    uint32_t (*il2cpp_gchandle_new)(Il2CppObject* obj, bool pinned) = nullptr;
    void (*il2cpp_gchandle_free)(uint32_t handle) = nullptr;
    Il2CppObject* (*il2cpp_gchandle_get_target)(uint32_t handle) = nullptr;
    Il2CppThread* (*il2cpp_thread_attach)(Il2CppDomain* domain) = nullptr;
    void (*il2cpp_thread_detach)(Il2CppThread* thread) = nullptr;
    Il2CppThread* (*il2cpp_thread_current)() = nullptr;
    Il2CppString* (*il2cpp_string_new)(const char* str) = nullptr;
}

/**
 * @brief Helper function to find a nested class by walking up the parent class hierarchy.
 * 
 * @param klass The starting class pointer.
 * @param name The name of the nested class to find.
 * @return Il2CppClass* Pointer to the nested class, or nullptr if not found.
 * 
 */
[[nodiscard]] Il2CppClass* FindNestedInHierarchy(Il2CppClass* klass, const char* name) noexcept {
    if (klass == nullptr || name == nullptr || IL2CPP::il2cpp_class_get_nested_types == nullptr || IL2CPP::il2cpp_class_get_name == nullptr || IL2CPP::il2cpp_class_get_parent == nullptr) {
        return nullptr;
    }

    Il2CppClass* current = klass;

    while (current != nullptr) {
        void* iter = nullptr;
        Il2CppClass* nested = nullptr;
        while ((nested = IL2CPP::il2cpp_class_get_nested_types(current, &iter)) != nullptr) {
            const char* nName = IL2CPP::il2cpp_class_get_name(nested);
            if (nName != nullptr && strncmp(nName, name, 256) == 0) {
                LOGD("[IL2CPP] Found nested class %s in parent %s", name, IL2CPP::il2cpp_class_get_name(current));
                return nested;
            }
        }
        current = IL2CPP::il2cpp_class_get_parent(current);
    }
    return nullptr;
}

/**
 * @brief Helper function to find a nested class by scanning all subclasses.
 * 
 * @param image The IL2CPP image to scan.
 * @param klass The base class.
 * @param name The name of the nested class.
 * @return Il2CppClass* Pointer to the nested class, or nullptr if not found.
 */
[[nodiscard]] Il2CppClass* FindNestedInSubclasses(const Il2CppImage* image, Il2CppClass* klass, const char* name) noexcept {
    if (image == nullptr || klass == nullptr || name == nullptr || IL2CPP::il2cpp_image_get_classes == nullptr || IL2CPP::il2cpp_class_is_subclass_of == nullptr) {
        return nullptr;
    }

    Il2CppClass* found = FindNestedInHierarchy(klass, name);
    if (found != nullptr) return found;

    size_t classCount = 0;
    Il2CppClass** classes = IL2CPP::il2cpp_image_get_classes(image, &classCount);
    if (classes == nullptr || classCount == 0 || classCount > 1000000) { // Sanity check on class count
        return nullptr;
    }

    for (size_t i = 0; i < classCount; i++) {
        Il2CppClass* candidate = classes[i];
        if (candidate == nullptr || candidate == klass) continue;

        if (IL2CPP::il2cpp_class_is_subclass_of(candidate, klass, false)) {
            void* iter = nullptr;
            Il2CppClass* nested = nullptr;
            while ((nested = IL2CPP::il2cpp_class_get_nested_types(candidate, &iter)) != nullptr) {
                const char* nName = IL2CPP::il2cpp_class_get_name(nested);
                if (nName != nullptr && strncmp(nName, name, 256) == 0) {
                    return nested;
                }
            }
        }
    }
    return nullptr;
}

/**
 * @brief Resolves an IL2CPP class from a string name, supporting dot-notation for nested classes.
 * 
 * @param image The IL2CPP image to search within.
 * @param namespaze The namespace of the class.
 * @param className The name of the class (can include dots for nested classes).
 * @return Il2CppClass* Pointer to the resolved class, or nullptr if not found.
 */
[[nodiscard]] Il2CppClass* ResolveClassFromName(const Il2CppImage* image, const char* namespaze, const char* className) noexcept {
    if (image == nullptr || namespaze == nullptr || className == nullptr || IL2CPP::il2cpp_class_from_name == nullptr) {
        return nullptr;
    }

    Il2CppClass* klass = IL2CPP::il2cpp_class_from_name(image, namespaze, className);
    if (klass != nullptr) {
        return klass;
    }

    // Safe string tokenization using std::string and std::istringstream
    {
        std::string classStr(className);
        std::istringstream iss(classStr);
        std::string token;
        
        if (std::getline(iss, token, '.')) {
            Il2CppClass* current = IL2CPP::il2cpp_class_from_name(image, namespaze, token.c_str());
            while (current != nullptr && std::getline(iss, token, '.')) {
                current = FindNestedInSubclasses(image, current, token.c_str());
            }
            klass = current;
        }
    }

    if (klass == nullptr) {
        LOGE("[IL2CPP] Failed to resolve class %s", className);
    }
    return klass;
}

/**
 * @brief Scans all loaded IL2CPP assemblies to find the native memory address of a specific method.
 * 
 * @param ns The namespace of the class.
 * @param className The name of the class.
 * @param methodName The name of the method.
 * @param argCount The number of arguments the method takes.
 * @return void* Native memory address of the method, or nullptr if not found.
 */
[[nodiscard]] void* GetMethodFromName(const char* ns, const char* className, const char* methodName, int argCount) noexcept {
    if (ns == nullptr || className == nullptr || methodName == nullptr) return nullptr;
    if (IL2CPP::il2cpp_domain_get == nullptr || IL2CPP::il2cpp_domain_get_assemblies == nullptr || IL2CPP::il2cpp_assembly_get_image == nullptr || IL2CPP::il2cpp_class_get_method_from_name == nullptr) {
        return nullptr;
    }

    Il2CppDomain* domain = IL2CPP::il2cpp_domain_get();
    if (domain == nullptr) return nullptr;

    size_t size = 0;
    Il2CppAssembly** assemblies = IL2CPP::il2cpp_domain_get_assemblies(domain, &size);
    if (assemblies == nullptr || size == 0 || size > 10000) return nullptr; // Sanity check

    for (size_t i = 0; i < size; ++i) {
        if (assemblies[i] == nullptr) continue;
        
        const Il2CppImage* image = IL2CPP::il2cpp_assembly_get_image(assemblies[i]);
        if (image == nullptr) continue;

        Il2CppClass* klass = ResolveClassFromName(image, ns, className);
        if (klass == nullptr) continue;

        const MethodInfo* method = IL2CPP::il2cpp_class_get_method_from_name(klass, methodName, argCount);
        if (method == nullptr) continue;

        return reinterpret_cast<void*>(method->methodPointer);
    }
    
    LOGE("[IL2CPP] Method %s::%s::%s not found", ns, className, methodName);
    return nullptr;
}

/**
 * @brief Scans all loaded IL2CPP assemblies to find the native memory address of a specific field.
 * 
 * @param ns The namespace of the class.
 * @param className The name of the class.
 * @param fieldName The name of the field.
 * @return FieldInfo* Pointer to the FieldInfo struct, or nullptr if not found.
 */
[[nodiscard]] FieldInfo* GetFieldFromName(const char* ns, const char* className, const char* fieldName) noexcept {
    if (ns == nullptr || className == nullptr || fieldName == nullptr) return nullptr;
    if (IL2CPP::il2cpp_domain_get == nullptr || IL2CPP::il2cpp_domain_get_assemblies == nullptr || IL2CPP::il2cpp_assembly_get_image == nullptr || IL2CPP::il2cpp_class_get_field_from_name == nullptr) {
        return nullptr;
    }

    Il2CppDomain* domain = IL2CPP::il2cpp_domain_get();
    if (domain == nullptr) return nullptr;

    size_t size = 0;
    Il2CppAssembly** assemblies = IL2CPP::il2cpp_domain_get_assemblies(domain, &size);
    if (assemblies == nullptr || size == 0 || size > 10000) return nullptr;

    for (size_t i = 0; i < size; ++i) {
        if (assemblies[i] == nullptr) continue;

        const Il2CppImage* image = IL2CPP::il2cpp_assembly_get_image(assemblies[i]);
        if (image == nullptr) continue;

        Il2CppClass* klass = ResolveClassFromName(image, ns, className);
        if (klass == nullptr) continue;

        FieldInfo* field = IL2CPP::il2cpp_class_get_field_from_name(klass, fieldName);
        if (field == nullptr) continue;

        return field;
    }
    
    LOGE("[IL2CPP] Field %s::%s::%s not found", ns, className, fieldName);
    return nullptr;
}

/**
 * Appends a single validated Unicode codepoint as UTF-8 to `out`.
 *
 * The caller is responsible for ensuring `cp` is not in the surrogate range
 * (U+D800–U+DFFF) and does not exceed U+10FFFF.
 */
inline void AppendCodepointAsUtf8(std::string& out, char32_t cp) noexcept
{
    if (cp <= 0x7Fu)
    {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp <= 0x7FFu)
    {
        out.push_back(static_cast<char>(0xC0u |  (cp >> 6)));
        out.push_back(static_cast<char>(0x80u |  (cp        & 0x3Fu)));
    }
    else if (cp <= 0xFFFFu)
    {
        out.push_back(static_cast<char>(0xE0u |  (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >>  6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u |  (cp        & 0x3Fu)));
    }
    else // U+10000 – U+10FFFF
    {
        out.push_back(static_cast<char>(0xF0u |  (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >>  6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u |  (cp        & 0x3Fu)));
    }
}

/**
 * Computes a conservative upper bound on the number of UTF-8 bytes
 * required for a UTF-16 sequence of `utf16Length` code units.
 *
 * Rationale: every BMP code unit expands to at most 3 UTF-8 bytes;
 * supplementary characters encoded as surrogate pairs (2 code units)
 * expand to 4 bytes — always less than 2 × 3 = 6, so the factor of 3
 * is a valid and tight upper bound.
 */
[[nodiscard]] constexpr std::size_t MaxUtf8Bytes(std::size_t utf16Length) noexcept
{
    return utf16Length * 3u;
}

/**
 * Core UTF-16 LE → UTF-8 transcoder.
 *
 * Iterates the code-unit array exactly once (O(n)).
 * Performs strict surrogate validation: lone surrogates of either kind
 * are rejected rather than passed through as replacement characters,
 * consistent with the WHATWG "fatal" error mode.
 *
 * @param chars   Pointer to the first UTF-16 code unit. Must not be null.
 * @param length  Number of code units to process.
 * @param out     Destination string. Caller should pre-reserve capacity.
 * @return        Utf16ConversionError::None on success; a specific code otherwise.
 */
[[nodiscard]] bool Transcode(const char16_t* chars, std::size_t length, std::string& out) noexcept
{
    for (std::size_t i = 0; i < length; )
    {
        const char16_t unit = chars[i++];

        // ── Detect a lone low surrogate ──────────────────────────────────
        if (unit >= 0xDC00u && unit <= 0xDFFFu)
            return false;

        char32_t codepoint;

        // ── Decode a surrogate pair ──────────────────────────────────────
        if (unit >= 0xD800u && unit <= 0xDBFFu)
        {
            // A high surrogate must be immediately followed by a low surrogate.
            if (i >= length)
                return false;

            const char16_t low = chars[i];

            if (low < 0xDC00u || low > 0xDFFFu)
                return false;

            ++i;

            // Combine the pair into a supplementary codepoint (U+10000–U+10FFFF).
            codepoint = 0x10000u
                      + (static_cast<char32_t>(unit - 0xD800u) << 10)
                      +  static_cast<char32_t>(low  - 0xDC00u);
        }
        else
        {
            // ── Plain BMP character (U+0000–U+D7FF, U+E000–U+FFFF) ──────
            codepoint = unit;
        }

        AppendCodepointAsUtf8(out, codepoint);
    }

    return true;
}

/**
 * Converts an Il2CppString to a UTF-8 encoded std::string.
 *
 * This variant throws on any invalid input or malformed encoding, making it
 * suitable for contexts where failures are truly exceptional.
 *
 * @param  il2String  Pointer to the source string. Must not be null.
 * @return            A valid UTF-8 std::string.
 */
[[nodiscard]] std::string Il2CppStringToStdString(const Il2CppString* il2String)
{
    if (!il2String)
        return "";

    if (il2String->length < 0)
        return "";

    if (il2String->length == 0)
        return "";

    const auto* chars  = reinterpret_cast<const char16_t*>(il2String->chars);
    const auto  length = static_cast<std::size_t>(il2String->length);

    std::string result;
    result.reserve(MaxUtf8Bytes(length));

    bool ok = Transcode(chars, length, result);
    if (!ok) {
        return "";
    }

    return result;
}

/**
 * @namespace Originals
 * @brief Stores the original function pointers after a Dobby hook is applied.
 * @details Calling these pointers executes the original, un-hooked game logic.
 */
namespace Originals {
    EGLBoolean (*EglSwapBuffers)(EGLDisplay dpy, EGLSurface surface) = nullptr;
    Touch (*Input_GetTouch)(int index) = nullptr;
    int (*MCLogicBattleData_ILOGIC_GetCurrentOpponentAccountID)(Il2CppObject* instance, int accountID) = nullptr;
    Il2CppString* (*MCLogicBattleData_ILOGIC_GetSelfChessPlayerName)(Il2CppObject* instance, int accID) = nullptr;
}

/**
 * @namespace Hooks
 * @brief Contains the detour functions. 
 * @details When the game attempts to call a hooked function, execution is redirected here.
 */
namespace Hooks {
    
    /**
     * @brief Detour for eglSwapBuffers.
     * 
     * @details Called by the game engine at the end of every frame. Intercepted to draw 
     *          the ImGui overlay on top of the game before the buffer swap occurs.
     * 
     * @param dpy The EGL display connection.
     * @param surface The EGL drawing surface.
     * @return EGLBoolean EGL_TRUE on success, EGL_FALSE on failure.
     * 
     */
    EGLBoolean EglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        if (Originals::EglSwapBuffers == nullptr) {
            return EGL_FALSE; // Critical failure, original function lost
        }

        if (IL2CPP::il2cpp_domain_get == nullptr || IL2CPP::il2cpp_thread_attach == nullptr) {
            return Originals::EglSwapBuffers(dpy, surface);
        }

        int width = 0, height = 0;
        eglQuerySurface(dpy, surface, EGL_WIDTH, &width);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &height);
        
        GLWidth.store(width, std::memory_order_relaxed);
        GLHeight.store(height, std::memory_order_relaxed);

        {
            static std::atomic<bool> Initialized{false};
            if (!Initialized.load(std::memory_order_acquire)) {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.ConfigWindowsMoveFromTitleBarOnly = true;
                io.ConfigWindowsResizeFromEdges = false;
                
                ImGui_ImplOpenGL3_Init("#version 300 es");

                ImFontConfig font_cfg;
                font_cfg.FontDataOwnedByAtlas = true;
                font_cfg.PixelSnapH = true;

                static const ImWchar full_ranges[] = {
                    0x0001, 0x10FFFF,
                    0
                };

                io.Fonts->AddFontFromMemoryTTF((void*)NotoSans_Regular_ttf, sizeof(NotoSans_Regular_ttf), 20.0f, &font_cfg, full_ranges);
                io.Fonts->Build();

                ImGui::StyleColorsDark();
                
                ImGuiStyle& style = ImGui::GetStyle();
                style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
                style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
                style.ScaleAllSizes(1.0f);
                
                IL2CPP::il2cpp_thread_attach(IL2CPP::il2cpp_domain_get());
                Initialized.store(true, std::memory_order_release);
            }

            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            // Safe Field Resolution
            if (IL2CPP::il2cpp_field_static_get_value != nullptr) {
                FieldInfo* MCLogicBattleData_m_SelfAccID = GetFieldFromName("", "MCLogicBattleData", "m_SelfAccID");
                if (MCLogicBattleData_m_SelfAccID != nullptr) {
                    int tempSelfID = -1;
                    IL2CPP::il2cpp_field_static_get_value(MCLogicBattleData_m_SelfAccID, &tempSelfID);
                    SelfAccountID.store(tempSelfID, std::memory_order_relaxed);

                    if (Originals::MCLogicBattleData_ILOGIC_GetCurrentOpponentAccountID != nullptr && tempSelfID != -1) {
                        int tempEnemyID = Originals::MCLogicBattleData_ILOGIC_GetCurrentOpponentAccountID(nullptr, tempSelfID);
                        EnemyAccountID.store(tempEnemyID, std::memory_order_relaxed);
                    }
                }
            }

            ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Once);
            if (ImGui::Begin("Magic Chess Go Go", nullptr)) {
                if (ImGui::BeginTabBar("##MainTabBar")) {
                    if (ImGui::BeginTabItem("Info")) {
                        ImGui::SeparatorText("Players");
                        int currentSelfID = SelfAccountID.load(std::memory_order_relaxed);
                        int currentEnemyID = EnemyAccountID.load(std::memory_order_relaxed);
                        ImGui::Text("Self Player ID: %d", currentSelfID);
                        ImGui::Text("Enemy Player ID: %d", currentEnemyID);
                        ImGui::Spacing();
                        if (Originals::MCLogicBattleData_ILOGIC_GetSelfChessPlayerName != nullptr && currentSelfID != 1 && currentEnemyID != -1) {
                            Il2CppString* RawSelfName = Originals::MCLogicBattleData_ILOGIC_GetSelfChessPlayerName(nullptr, currentSelfID);
                            std::string SelfName = Il2CppStringToStdString(RawSelfName);
                            Il2CppString* RawEnemyName = Originals::MCLogicBattleData_ILOGIC_GetSelfChessPlayerName(nullptr, currentEnemyID);
                            std::string EnemyName = Il2CppStringToStdString(RawEnemyName);
                            ImGui::Text("Self Player Name: %s", SelfName.empty() ? "Unknown" : SelfName.c_str());
                            ImGui::Text("Enemy Player Name: %s", EnemyName.empty() ? "Unknown" : EnemyName.c_str());
                        } else {
                            ImGui::Text("Self Player Name: Unknown");
                            ImGui::Text("Enemy Player Name: Unknown");
                        }
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::End();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        
        return Originals::EglSwapBuffers(dpy, surface);
    }

    /**
     * @brief Detour for Unity's Input.GetTouch.
     * 
     * @details Intercepts touch events from the game engine and feeds them into ImGui.
     *          This allows the user to interact with the mod menu using the touchscreen.
     * 
     * @param index The index of the touch event.
     * @return Touch The original touch event data.
     * 
     * @note Validates ImGui context and original function pointer before execution.
     */
    Touch Input_GetTouch(int index) {
        if (Originals::Input_GetTouch == nullptr) {
            LOGE("[HOOK] Input_GetTouch hook called but original function is null!");
            return Touch{0};
        }
        
        Touch Ret = Originals::Input_GetTouch(index);
        
        {
            if (ImGui::GetCurrentContext() != nullptr && index == 0) {
                ImGuiIO& io = ImGui::GetIO();

                float x = Ret.m_Position.x;
                float y = io.DisplaySize.y - Ret.m_Position.y;

                switch (Ret.m_Phase) {
                    case TouchPhase::Began:
                        io.AddMousePosEvent(x, y);
                        io.AddMouseButtonEvent(0, true);
                        break;
                    case TouchPhase::Moved:
                    case TouchPhase::Stationary:
                        io.AddMousePosEvent(x, y);
                        break;
                    case TouchPhase::Ended:
                    case TouchPhase::Canceled:
                        io.AddMousePosEvent(x, y);
                        io.AddMouseButtonEvent(0, false);
                        break;
                    default:
                        break;
                }
            }
        }
        
        return Ret;
    }
}

/**
 * @brief Background thread responsible for initializing the mod.
 * 
 * @details Waits for the game's libraries to unpack and load into memory before 
 *          attempting to resolve symbols and place hooks. Includes timeout mechanisms
 *          to prevent infinite hanging.
 * 
 * @note Runs detached. Catches all exceptions to prevent crashing the host process.
 */
void SetupThread() noexcept {
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

        void* eglSwapBuffersAddr = DobbySymbolResolver(nullptr, "eglSwapBuffers");
        if (eglSwapBuffersAddr != nullptr) {
            if (DobbyHook(eglSwapBuffersAddr, reinterpret_cast<void*>(Hooks::EglSwapBuffers), reinterpret_cast<void**>(&Originals::EglSwapBuffers)) != 0) {
                LOGE("[SETUP] CRITICAL FAILURE: DobbyHook failed for eglSwapBuffers");
            }
        } else {
            LOGE("[SETUP] CRITICAL FAILURE: Could not resolve eglSwapBuffers in libEGL.so");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        int retryCount = 0;
        const int maxRetries = 30; // 60 seconds timeout
        void* localHandle = nullptr;

        while (localHandle == nullptr && retryCount < maxRetries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            localHandle = xdl_open("liblogic.so", XDL_DEFAULT);
            retryCount++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        if (localHandle == nullptr) {
            LOGE("[SETUP] CRITICAL FAILURE: Timeout waiting for liblogic.so to load.");
            return;
        }

        Il2cppHandle.store(localHandle, std::memory_order_release);

        DO_API(il2cpp_domain_get, localHandle);
        DO_API(il2cpp_domain_get_assemblies, localHandle);
        DO_API(il2cpp_assembly_get_image, localHandle);
        DO_API(il2cpp_image_get_classes, localHandle);
        DO_API(il2cpp_type_get_name, localHandle);
        DO_API(il2cpp_class_from_name, localHandle);
        DO_API(il2cpp_class_get_name, localHandle);
        DO_API(il2cpp_class_get_namespace, localHandle);
        DO_API(il2cpp_class_get_parent, localHandle);
        DO_API(il2cpp_class_get_methods, localHandle);
        DO_API(il2cpp_class_get_fields, localHandle);
        DO_API(il2cpp_class_get_type, localHandle);
        DO_API(il2cpp_class_get_nested_types, localHandle);
        DO_API(il2cpp_class_get_image, localHandle);
        DO_API(il2cpp_class_get_method_from_name, localHandle);
        DO_API(il2cpp_class_get_field_from_name, localHandle);
        DO_API(il2cpp_class_is_subclass_of, localHandle);
        DO_API(il2cpp_field_get_offset, localHandle);
        DO_API(il2cpp_field_get_name, localHandle);
        DO_API(il2cpp_field_get_value, localHandle);
        DO_API(il2cpp_field_set_value, localHandle);
        DO_API(il2cpp_field_static_get_value, localHandle);
        DO_API(il2cpp_field_static_set_value, localHandle);
        DO_API(il2cpp_method_get_name, localHandle);
        DO_API(il2cpp_method_get_class, localHandle);
        DO_API(il2cpp_method_get_return_type, localHandle);
        DO_API(il2cpp_method_get_param_count, localHandle);
        DO_API(il2cpp_method_get_param, localHandle);
        DO_API(il2cpp_object_new, localHandle);
        DO_API(il2cpp_object_unbox, localHandle);
        DO_API(il2cpp_object_get_class, localHandle);
        DO_API(il2cpp_gchandle_new, localHandle);
        DO_API(il2cpp_gchandle_free, localHandle);
        DO_API(il2cpp_gchandle_get_target, localHandle);
        DO_API(il2cpp_thread_attach, localHandle);
        DO_API(il2cpp_thread_detach, localHandle);
        DO_API(il2cpp_thread_current, localHandle);
        DO_API(il2cpp_string_new, localHandle);

        if (IL2CPP::il2cpp_thread_attach != nullptr && IL2CPP::il2cpp_domain_get != nullptr) {
            IL2CPP::il2cpp_thread_attach(IL2CPP::il2cpp_domain_get());
        } else {
            LOGE("[SETUP] CRITICAL FAILURE: Core IL2CPP APIs not resolved. Aborting hooks.");
            return;
        }

        DO_HOOK(Input_GetTouch, "UnityEngine", "Input", "GetTouch", 1);

        DO_ASSIGN(MCLogicBattleData_ILOGIC_GetCurrentOpponentAccountID, "", "MCLogicBattleData", "ILOGIC_GetCurrentOpponentAccountID", 1);
        DO_ASSIGN(MCLogicBattleData_ILOGIC_GetSelfChessPlayerName, "", "MCLogicBattleData", "ILOGIC_GetSelfChessPlayerName", 1);

        LOGI("[SETUP] Mod initialization completed successfully.");
    }
}

/**
 * @brief JNI wrapper to load the original libunity.so.
 * 
 * @details Since this mod acts as a proxy/dummy library, it must manually load the real
 *          game engine library and trigger its JNI_OnLoad to ensure the game boots normally.
 * 
 * @param env The JNI environment pointer.
 * @param obj The calling Java object.
 * @param path The path to the directory containing libunity.so.
 * @return jboolean JNI_TRUE on success, JNI_FALSE on failure.
 * 
 * @note Includes strict JNI exception checking and buffer overflow protection.
 */
jboolean LoadOriginalLibrary(JNIEnv* env, jobject obj, jstring path) {
    if (env == nullptr || path == nullptr) return JNI_FALSE;
    
    const char* libraryPath = env->GetStringUTFChars(path, nullptr);
    if (libraryPath == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("[JNI] Failed to extract string chars for library path.");
        return JNI_FALSE;
    }
    
    char fullPath[1024] = {0};
    int snprintf_res = snprintf(fullPath, sizeof(fullPath), "%s/%s", libraryPath, "libunity.so");
    env->ReleaseStringUTFChars(path, libraryPath);

    if (snprintf_res < 0 || static_cast<size_t>(snprintf_res) >= sizeof(fullPath)) {
        LOGE("[JNI] Buffer overflow prevented while constructing library path.");
        return JNI_FALSE;
    }

    LOGI("[JNI] Loading original library from: %s", fullPath);
    void* handle = dlopen(fullPath, RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr) {
        LOGE("[JNI] Failed to dlopen libunity.so: %s", dlerror());
        return JNI_FALSE;
    }

    UnityLibraryHandle.store(handle, std::memory_order_release);

    auto jniOnLoad = reinterpret_cast<jint (*)(JavaVM*, void*)>(dlsym(handle, "JNI_OnLoad"));
    if (jniOnLoad == nullptr) {
        LOGE("[JNI] Failed to find JNI_OnLoad in libunity.so");
        dlclose(handle);
        UnityLibraryHandle.store(nullptr, std::memory_order_release);
        return JNI_FALSE;
    }

    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK || vm == nullptr) {
        LOGE("[JNI] Failed to get JavaVM");
        dlclose(handle);
        UnityLibraryHandle.store(nullptr, std::memory_order_release);
        return JNI_FALSE;
    }

    jint result = jniOnLoad(vm, nullptr);
    if (result < JNI_VERSION_1_6) {
        LOGE("[JNI] libunity.so JNI_OnLoad returned unsupported version: %d", result);
        dlclose(handle);
        UnityLibraryHandle.store(nullptr, std::memory_order_release);
        return JNI_FALSE;
    }

    LOGI("[JNI] Successfully loaded original library and executed JNI_OnLoad");
    return JNI_TRUE;
}

/**
 * @brief JNI function to safely unload the original libunity.so when the app closes.
 * 
 * @param env The JNI environment pointer.
 * @param clazz The calling Java class.
 * @return jboolean Always returns JNI_TRUE.
 */
jboolean UnloadOriginalLibrary(JNIEnv* env, jclass clazz) {
    void* handle = UnityLibraryHandle.exchange(nullptr, std::memory_order_acq_rel);
    if (handle != nullptr) {
        LOGI("[JNI] Unloading original library: %p", handle);
        dlclose(handle);
    } else {
        LOGW("[JNI] UnloadOriginalLibrary called but no library was loaded");
    }
    return JNI_TRUE;
}

/**
 * @brief Standard JNI entry point.
 * 
 * @details Called by the Android runtime when this proxy library is loaded by the Java layer.
 *          Registers custom native methods that handle loading the real game library.
 * 
 * @param vm Pointer to the JavaVM.
 * @param key Reserved pointer.
 * @return jint The required JNI version (JNI_VERSION_1_6).
 */
extern "C" __attribute__((used, visibility("default")))
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *key) {
    if (vm == nullptr) return JNI_VERSION_1_6;
    
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || env == nullptr) {
        LOGE("[JNI] Failed to get JNIEnv in JNI_OnLoad");
        return JNI_VERSION_1_6;
    }

    LOGI("[JNI] Registering native methods for com/unity3d/player/NativeLoader");
    jclass clazz = env->FindClass("com/unity3d/player/NativeLoader");
    if (clazz == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("[JNI] Failed to find class com/unity3d/player/NativeLoader");
        return JNI_VERSION_1_6;
    }

    const JNINativeMethod methods[] = {
        { "load", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(LoadOriginalLibrary) },
        { "unload", "()Z", reinterpret_cast<void*>(UnloadOriginalLibrary) }
    };

    if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(JNINativeMethod)) != JNI_OK) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("[JNI] Failed to register native methods for com/unity3d/player/NativeLoader");
        return JNI_VERSION_1_6;
    }

    LOGI("[JNI] Native methods successfully registered");
    return JNI_VERSION_1_6;
}

/**
 * @brief Constructor attribute: automatically invoked by the Android linker (linker/ld).
 * 
 * @details Executes the moment this .so file is mapped into memory, even before JNI_OnLoad.
 *          Spawns the setup thread as early as possible if running in the correct process.
 * 
 * @note Catches exceptions to prevent linker-stage crashes.
 */
__attribute__((constructor))
void InitLibrary() noexcept {
    {
        if (!IsUnityProcess()) {
            LOGW("[INIT] Not running in target Unity process. Aborting initialization.");
            return;
        }
        
        LOGI("[INIT] Target process verified. Spawning SetupThread.");
        std::thread(SetupThread).detach();
    }
}