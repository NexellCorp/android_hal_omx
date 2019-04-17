LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

NX_OMX_CFLAGS := -Wall -fpic -pipe -O0


ANDROID_VERSION_STR := $(PLATFORM_VERSION)
ANDROID_VERSION := $(firstword $(ANDROID_VERSION_STR))
ifeq ($(ANDROID_VERSION), 9)
LOCAL_VENDOR_MODULE := true
else
LOCAL_MODULE_TAGS := optional
endif

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
	NX_OMXPlugin.cpp

LOCAL_C_INCLUDES += \
	$(NX_INCLUDES) \
	$(TOP)/frameworks/native/include/media/openmax \
	$(TOP)/frameworks/native/include/media/hardware

LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Core \
	libutils \
	libcutils \
	libdl \
	libui \
	liblog

ifeq ($(ANDROID_VERSION), 9)
LOCAL_HEADER_LIBRARIES := media_plugin_headers
endif

LOCAL_CFLAGS := $(NX_OMX_CFLAGS)

LOCAL_CFLAGS += -DNO_OPENCORE

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_MODULE:= libstagefrighthw

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)
