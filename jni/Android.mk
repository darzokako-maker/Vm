LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := PolyfieldStealthMenu
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS    := -llog -lGLESv2 -lEGL

include $(BUILD_SHARED_LIBRARY)

