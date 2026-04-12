LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := dobby

LOCAL_SRC_FILES := $(LOCAL_PATH)/DOBBY/lib/$(TARGET_ARCH_ABI)/libdobby.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := curl

LOCAL_SRC_FILES := $(LOCAL_PATH)/CURL/lib/$(TARGET_ARCH_ABI)/libcurl.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := openssl

LOCAL_SRC_FILES := $(LOCAL_PATH)/OPENSSL/lib/$(TARGET_ARCH_ABI)/libssl.a \

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := crypto

LOCAL_SRC_FILES := $(LOCAL_PATH)/OPENSSL/lib/$(TARGET_ARCH_ABI)/libcrypto.a \

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := main

LOCAL_CFLAGS := \
    -O0 \
    -w \
    -Wno-error \
    -DNDEBUG \
    -g0 \

LOCAL_CPPFLAGS := \
    $(LOCAL_CFLAGS) \
    -std=c++20 \

LOCAL_LDFLAGS := \

LOCAL_LDLIBS := \
    -llog \
    -landroid \
    -lEGL \
    -lGLESv3 \

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/XDL/include \
    $(LOCAL_PATH)/DOBBY/include \
    $(LOCAL_PATH)/IMGUI/include \
    $(LOCAL_PATH)/IMGUI/include/backends \
    $(LOCAL_PATH)/CURL/include/ \
    $(LOCAL_PATH)/OPENSSL/include/ \

LOCAL_SRC_FILES := \
    $(wildcard $(LOCAL_PATH)/XDL/src/*.c*) \
    $(wildcard $(LOCAL_PATH)/IMGUI/src/*.c*) \
    $(wildcard $(LOCAL_PATH)/IMGUI/src/backends/*.c*) \
    $(wildcard $(LOCAL_PATH)/*.c*) \

LOCAL_STATIC_LIBRARIES := \
    dobby \
    curl \
    openssl \
    crypto \

include $(BUILD_SHARED_LIBRARY)