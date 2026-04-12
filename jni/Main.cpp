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

// Debugging configuration
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

// Macros for simplifying IL2CPP API resolution and hooking
#define DO_API(name, handle) \
{ \
    void* sym = xdl_sym(handle, #name, nullptr); \
    if (sym) { \
        LOGD("Resolved API symbol: %s at %p", #name, sym); \
    } else { \
        LOGE("Failed to resolve API symbol: %s", #name); \
    } \
    IL2CPP::name = (decltype(IL2CPP::name))(sym); \
}

#define DO_ASSIGN(name, ns, className, methodName, argCount) \
{ \
    void* addr = GetMethodFromName(ns, className, methodName, argCount); \
    if (addr) { \
        Originals::name = (decltype(Originals::name))(addr); \
    } \
}

#define DO_HOOK(name, ns, className, methodName, argCount) \
{ \
    void* addr = GetMethodFromName(ns, className, methodName, argCount); \
    if (addr) { \
        DobbyHook((void*)addr, (void*)(Hooks::name), (void**)&(Originals::name)); \
    } \
}

// Unity basic types
struct Vector2 {
    float x, y;
};

struct Vector3 {
    float x, y, z;
};

// Unity Input types
enum class TouchPhase { Began, Moved, Stationary, Ended, Canceled };
enum class TouchType { Direct, Indirect, Stylus };
struct Touch {
    int m_FingerId;
    Vector2 m_Position;
    Vector2 m_RawPosition;
    Vector2 m_PositionDelta;
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

// Global state
int GLWidth = 0;
int GLHeight = 0;
void* Il2cppHandle = nullptr;

// Validates if the library is loaded in the target Unity game process
bool IsUnityProcess() {
    FILE* fp = fopen("/proc/self/cmdline", "rb");
    if (!fp) {
        return false;
    }

    char buffer[256];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);

    if (bytesRead == 0) {
        return false;
    }

    buffer[bytesRead] = '\0';

    const char* target = ":UnityKillsMe";
    size_t targetLen = strlen(target);

    size_t i = 0;
    while (i < bytesRead) {
        const char* arg = &buffer[i];
        size_t len = strlen(arg);

        if (len >= targetLen && strstr(arg, target) != nullptr) {
            return true;
        }

        i += len + 1;
    }

    return false;
}

// IL2CPP API function pointers
namespace IL2CPP {
    void* (*il2cpp_domain_get)();
    void* (*il2cpp_domain_get_assemblies)(void* domain, size_t* size);
    void* (*il2cpp_assembly_get_image)(void* assembly);
    void** (*il2cpp_image_get_classes)(void* image, size_t*);
    const char* (*il2cpp_type_get_name)(void* type);
    void* (*il2cpp_class_from_name)(void* image, const char* namespaze, const char* name);
    const char* (*il2cpp_class_get_name)(void* klass);
    const char* (*il2cpp_class_get_namespace)(void* klass);
    void* (*il2cpp_class_get_parent)(void* klass);
    const void* (*il2cpp_class_get_methods)(void* klass, void** iter);
    const void* (*il2cpp_class_get_fields)(void* klass, void** iter);
    const void* (*il2cpp_class_get_type)(void* klass);
    void* (*il2cpp_class_get_nested_types)(void* klass, void** iter);
    void* (*il2cpp_class_get_image)(void* klass);
    void* (*il2cpp_class_get_method_from_name)(void* klass, const char* name, int argsCount);
    void* (*il2cpp_class_get_field_from_name)(void* klass, const char* name);
    bool (*il2cpp_class_is_subclass_of)(void* klass, void* klassc, bool check_interfaces);
    size_t (*il2cpp_field_get_offset)(void* field);
    const char* (*il2cpp_field_get_name)(void* field);
    void (*il2cpp_field_get_value)(void* obj, void* field, void* value);
    void (*il2cpp_field_set_value)(void* obj, void* field, void* value);
    void (*il2cpp_field_static_get_value)(void* field, void* value);
    void (*il2cpp_field_static_set_value)(void* field, void* value);
    const char* (*il2cpp_method_get_name)(const void* method);
    void* (*il2cpp_method_get_class)(const void* method);
    const void* (*il2cpp_method_get_return_type)(const void* method);
    int (*il2cpp_method_get_param_count)(const void* method);
    const void* (*il2cpp_method_get_param)(const void* method, uint32_t index);
    void* (*il2cpp_object_new)(const void* klass);
    void* (*il2cpp_object_unbox)(void* obj);
    void* (*il2cpp_object_get_class)(void* obj);
    uint32_t (*il2cpp_gchandle_new)(void* obj, bool pinned);
    void (*il2cpp_gchandle_free)(uint32_t handle);
    void* (*il2cpp_gchandle_get_target)(uint32_t handle);
    void* (*il2cpp_thread_attach)(void* domain);
    void (*il2cpp_thread_detach)(void* thread);
    void* (*il2cpp_thread_current)();
}

// Searches for a nested class by name within a given class, walking up its parent chain
void* FindNestedInHierarchy(void* klass, const char* name) {
    void* current = klass;
    while (current) {
        void* iter = nullptr;
        void* nested = nullptr;
        while ((nested = IL2CPP::il2cpp_class_get_nested_types(current, &iter))) {
            const char* nName = IL2CPP::il2cpp_class_get_name(nested);
            if (nName && strcmp(nName, name) == 0) {
                LOGD("Found nested class %s in parent %s", name, IL2CPP::il2cpp_class_get_name(current));
                return nested;
            }
        }
        current = IL2CPP::il2cpp_class_get_parent(current);
    }
    return nullptr;
}

// Searches for a nested class by name within a class AND all its subclasses in the image.
// Subclass lookup requires a full image scan since IL2CPP has no reverse-parent API.
void* FindNestedInSubclasses(void* image, void* klass, const char* name) {
    void* found = FindNestedInHierarchy(klass, name);
    if (found) return found;

    LOGD("Scanning image for subclasses of %s to find nested %s...", IL2CPP::il2cpp_class_get_name(klass), name);
    size_t classCount = 0;
    void** classes = IL2CPP::il2cpp_image_get_classes(image, &classCount);
    if (!classes) return nullptr;

    for (size_t i = 0; i < classCount; i++) {
        void* candidate = classes[i];
        if (!candidate || candidate == klass) continue;

        if (IL2CPP::il2cpp_class_is_subclass_of(candidate, klass, false)) {
            LOGD("Checking subclass %s for nested %s", IL2CPP::il2cpp_class_get_name(candidate), name);
            void* iter = nullptr;
            void* nested = nullptr;
            while ((nested = IL2CPP::il2cpp_class_get_nested_types(candidate, &iter))) {
                const char* nName = IL2CPP::il2cpp_class_get_name(nested);
                if (nName && strcmp(nName, name) == 0) {
                    LOGD("Found nested class %s in subclass %s", name, IL2CPP::il2cpp_class_get_name(candidate));
                    return nested;
                }
            }
        }
    }
    return nullptr;
}

// Resolves an IL2CPP class from name, including nested classes (e.g., "ClassName.NestedClass").
// For each nesting token, searches the class hierarchy (parents) and subclasses.
void* ResolveClassFromName(void* image, const char* namespaze, const char* className) {
    LOGD("Resolving class %s::%s", namespaze, className);
    void* klass = IL2CPP::il2cpp_class_from_name(image, namespaze, className);
    if (klass) {
        LOGD("Found class %s directly", className);
        return klass;
    }

    LOGD("Class %s not found directly, searching for nested classes...", className);
    char nameCopy[512]{0};
    strncpy(nameCopy, className, sizeof(nameCopy) - 1);
    char* ctx = nullptr;
    char* token = strtok_r(nameCopy, ".", &ctx);

    if (token) {
        void* current = IL2CPP::il2cpp_class_from_name(image, namespaze, token);
        while (current && (token = strtok_r(nullptr, ".", &ctx))) {
            LOGD("Searching for nested class %s within %s (+ parents/subclasses)",
                 token, IL2CPP::il2cpp_class_get_name(current));
            current = FindNestedInSubclasses(image, current, token);
        }
        klass = current;
    }

    if (klass) {
        LOGD("Successfully resolved class %s", className);
    } else {
        LOGE("Failed to resolve class %s", className);
    }
    return klass;
}

// Finds a method address by iterating through all loaded assemblies
void* GetMethodFromName(const char* ns, const char* className, const char* methodName, int argCount) {
    LOGD("Searching for method %s::%s::%s (args: %d)", ns, className, methodName, argCount);
    size_t size = 0;
    void* domain = IL2CPP::il2cpp_domain_get();
    if (!domain) {
        LOGE("GetMethodFromName failed: Domain not found");
        return nullptr;
    }

    void** assemblies = (void**)IL2CPP::il2cpp_domain_get_assemblies(domain, &size);
    if (!assemblies || size == 0) {
        LOGE("GetMethodFromName failed: No assemblies found");
        return nullptr;
    }

    for (size_t i = 0; i < size; ++i) {
        void* image = IL2CPP::il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;

        void* klass = ResolveClassFromName(image, ns, className);
        if (!klass) continue;

        void* method = IL2CPP::il2cpp_class_get_method_from_name(klass, methodName, argCount);
        if (!method) continue;

        void* addr = *(void**)method;
        LOGD("Found method %s::%s::%s at %p", ns, className, methodName, addr);
        return addr;
    }
    LOGE("Method %s::%s::%s not found in any assembly", ns, className, methodName);
    return nullptr;
}

// Finds a field address by iterating through all loaded assemblies
void* GetFieldFromName(const char* ns, const char* className, const char* fieldName) {
    LOGD("Searching for field %s::%s::%s", ns, className, fieldName);
    size_t size = 0;
    void* domain = IL2CPP::il2cpp_domain_get();
    if (!domain) {
        LOGE("GetFieldFromName failed: Domain not found");
        return nullptr;
    }

    void** assemblies = (void**)IL2CPP::il2cpp_domain_get_assemblies(domain, &size);
    if (!assemblies || size == 0) {
        LOGE("GetFieldFromName failed: No assemblies found");
        return nullptr;
    }

    for (size_t i = 0; i < size; ++i) {
        void* image = IL2CPP::il2cpp_assembly_get_image(assemblies[i]);
        if (!image) continue;

        void* klass = ResolveClassFromName(image, ns, className);
        if (!klass) continue;

        void* field = IL2CPP::il2cpp_class_get_field_from_name(klass, fieldName);
        if (!field) continue;

        LOGD("Found field %s::%s::%s at %p", ns, className, fieldName, field);
        return field;
    }
    LOGE("Field %s::%s::%s not found in any assembly", ns, className, fieldName);
    return nullptr;
}

// Original function pointers for hooks
namespace Originals {
    EGLBoolean (*EglSwapBuffers)(EGLDisplay dpy, EGLSurface surface) = nullptr;

    Touch (*Input_GetTouch)(int index) = nullptr;

    int (*MCLogicBattleData_ILOGIC_GetCurrentOpponentAccountID)(int accountID) = nullptr;
}

// Hook implementations
namespace Hooks {
    // Hooks eglSwapBuffers to render the ImGui overlay
    EGLBoolean EglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        eglQuerySurface(dpy, surface, EGL_WIDTH, &GLWidth);
        eglQuerySurface(dpy, surface, EGL_HEIGHT, &GLHeight);

        {
            static bool Initialized = false;
            if (!Initialized) {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.ConfigWindowsMoveFromTitleBarOnly = true;
                io.ConfigWindowsResizeFromEdges = false;
                ImGui_ImplOpenGL3_Init("#version 300 es");
                ImGui::StyleColorsDark();
                ImGuiStyle& style = ImGui::GetStyle();
                style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
                style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
                style.ScaleAllSizes(1.0f);
                IL2CPP::il2cpp_thread_attach(IL2CPP::il2cpp_domain_get());
                Initialized = true;
            }
        }

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)GLWidth, (float)GLHeight);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Magic Chess Go Go", nullptr)) {
            if (ImGui::BeginTabBar("##MainTabBar")) {
                if (ImGui::BeginTabItem("Info")) {

                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return Originals::EglSwapBuffers(dpy, surface);
    }

    // Hooks Unity's GetTouch to pass input events to ImGui
    Touch Input_GetTouch(int index) {
        auto Ret = Originals::Input_GetTouch(index);
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
        return Ret;
    }
}

// Core setup thread that initializes symbols and installs hooks
void SetupThread() {
    DobbyHook((void*)DobbySymbolResolver("libEGL.so", "eglSwapBuffers"), (void*)Hooks::EglSwapBuffers, (void**)&Originals::EglSwapBuffers);

    while (!Il2cppHandle) {
        Il2cppHandle = xdl_open("liblogic.so", XDL_DEFAULT);
        if (Il2cppHandle) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    DO_API(il2cpp_domain_get, Il2cppHandle)
    DO_API(il2cpp_domain_get_assemblies, Il2cppHandle)
    DO_API(il2cpp_assembly_get_image, Il2cppHandle)
    DO_API(il2cpp_image_get_classes, Il2cppHandle)
    DO_API(il2cpp_type_get_name, Il2cppHandle)
    DO_API(il2cpp_class_from_name, Il2cppHandle)
    DO_API(il2cpp_class_get_name, Il2cppHandle)
    DO_API(il2cpp_class_get_namespace, Il2cppHandle)
    DO_API(il2cpp_class_get_parent, Il2cppHandle)
    DO_API(il2cpp_class_get_methods, Il2cppHandle)
    DO_API(il2cpp_class_get_fields, Il2cppHandle)
    DO_API(il2cpp_class_get_type, Il2cppHandle)
    DO_API(il2cpp_class_get_nested_types, Il2cppHandle)
    DO_API(il2cpp_class_get_image, Il2cppHandle)
    DO_API(il2cpp_class_get_method_from_name, Il2cppHandle)
    DO_API(il2cpp_class_get_field_from_name, Il2cppHandle)
    DO_API(il2cpp_class_is_subclass_of, Il2cppHandle)
    DO_API(il2cpp_field_get_offset, Il2cppHandle)
    DO_API(il2cpp_field_get_name, Il2cppHandle)
    DO_API(il2cpp_field_get_value, Il2cppHandle)
    DO_API(il2cpp_field_set_value, Il2cppHandle)
    DO_API(il2cpp_field_static_get_value, Il2cppHandle)
    DO_API(il2cpp_field_static_set_value, Il2cppHandle)
    DO_API(il2cpp_method_get_name, Il2cppHandle)
    DO_API(il2cpp_method_get_class, Il2cppHandle)
    DO_API(il2cpp_method_get_return_type, Il2cppHandle)
    DO_API(il2cpp_method_get_param_count, Il2cppHandle)
    DO_API(il2cpp_method_get_param, Il2cppHandle)
    DO_API(il2cpp_object_new, Il2cppHandle)
    DO_API(il2cpp_object_unbox, Il2cppHandle)
    DO_API(il2cpp_object_get_class, Il2cppHandle)
    DO_API(il2cpp_gchandle_new, Il2cppHandle)
    DO_API(il2cpp_gchandle_free, Il2cppHandle)
    DO_API(il2cpp_gchandle_get_target, Il2cppHandle)
    DO_API(il2cpp_thread_attach, Il2cppHandle)
    DO_API(il2cpp_thread_detach, Il2cppHandle)
    DO_API(il2cpp_thread_current, Il2cppHandle)

    IL2CPP::il2cpp_thread_attach(IL2CPP::il2cpp_domain_get());

    DO_HOOK(Input_GetTouch, "UnityEngine", "Input", "GetTouch", 1);

    DO_ASSIGN(MCLogicBattleData_ILOGIC_GetCurrentOpponentAccountID, "", "MCLogicBattleData", "ILOGIC_GetCurrentOpponentAccountID", 1);
}

// // Handle for the Unity engine library
// void* UnityHandle = nullptr;

// // JNI function to manually load the original Unity library
// jboolean LoadOriginalLibrary(JNIEnv* env, jobject /*thiz*/, jstring path) {
    // if (env == nullptr || path == nullptr) {
        // return JNI_FALSE;
    // }

    // const char* basePath = env->GetStringUTFChars(path, nullptr);
    // if (basePath == nullptr) {
        // return JNI_FALSE;
    // }

    // const char* kLibName = "libunity.so";

    // char fullPath[1024];
    // std::snprintf(fullPath, sizeof(fullPath), "%s/%s", basePath, kLibName);
    // env->ReleaseStringUTFChars(path, basePath);

    // void* handle = xdl_open(fullPath, XDL_DEFAULT);
    // if (!handle) {
        // return JNI_FALSE;
    // }

    // typedef jint (*JniOnLoadFn)(JavaVM*, void*);
    // JniOnLoadFn jniOnLoad = (JniOnLoadFn)xdl_sym(handle, "JNI_OnLoad", nullptr);

    // if (!jniOnLoad) {
        // xdl_close(handle);
        // return JNI_FALSE;
    // }

    // JavaVM* vm = nullptr;
    // if (env->GetJavaVM(&vm) != JNI_OK || vm == nullptr) {
        // xdl_close(handle);
        // return JNI_FALSE;
    // }

    // jint version = jniOnLoad(vm, nullptr);
    // if (version < JNI_VERSION_1_6) {
        // xdl_close(handle);
        // return JNI_FALSE;
    // }

    // UnityHandle = handle;
    // return JNI_TRUE;
// }

// // JNI function to unload the Unity library
// jboolean UnloadOriginalLibrary(JNIEnv* /*env*/, jclass /*clazz*/) {
    // if (UnityHandle != nullptr) {
        // xdl_close(UnityHandle);
        // UnityHandle = nullptr;
    // }
    // return JNI_TRUE;
// }

// // Entry point for the shared library
// JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // if (vm == nullptr) {
        // return JNI_VERSION_1_6;
    // }

    // JNIEnv* env = nullptr;
    // if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || env == nullptr) {
        // return JNI_VERSION_1_6;
    // }

    // if (reserved == (void*)1337) {
        // return JNI_VERSION_1_6;
    // }

    // jclass clazz = env->FindClass("com/unity3d/player/NativeLoader");
    // if (clazz == nullptr) {
        // return JNI_VERSION_1_6;
    // }

    // const JNINativeMethod methods[] = {
        // {"load", "(Ljava/lang/String;)Z", (void*)LoadOriginalLibrary},
        // {"unload", "()Z", (void*)UnloadOriginalLibrary}
    // };

    // if (env->RegisterNatives(clazz, methods,
                             // sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
        // return JNI_VERSION_1_6;
    // }

    // return JNI_VERSION_1_6;
// }

// Constructor attribute: runs automatically when the .so is loaded into the process
__attribute__((constructor))
void InitLibrary() {
    if (!IsUnityProcess()) return;
    std::thread(SetupThread).detach();
}
