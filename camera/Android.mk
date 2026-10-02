LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
    frameworks/native/include \
    frameworks/av/camera/include \
    system/media/camera/include

LOCAL_SRC_FILES := \
    Camera3Shim.cpp

LOCAL_SHARED_LIBRARIES := \
    libhardware \
    liblog \
    libcutils \
    libui \
    libcamera_client \
    libcamera_metadata \
    libutils \
    libdl

LOCAL_STATIC_LIBRARIES := \
    libarect

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := false

include $(BUILD_SHARED_LIBRARY)
