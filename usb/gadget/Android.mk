LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.usb.gadget@1.0-service-osprey
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_INIT_RC := android.hardware.usb.gadget@1.0-service-osprey.rc
LOCAL_SRC_FILES := service.cpp UsbGadget.cpp
LOCAL_SHARED_LIBRARIES := \
    libbase \
    libcutils \
    libhidlbase \
    liblog \
    libutils \
    android.hardware.usb.gadget@1.0
include $(BUILD_EXECUTABLE)
