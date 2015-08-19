LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

NX_OMX_CFLAGS := -Wall -fpic -pipe -O0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := optional
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

LOCAL_CFLAGS := $(NX_OMX_CFLAGS)

LOCAL_CFLAGS += -DNO_OPENCORE

LOCAL_MODULE:= libstagefrighthw

include $(BUILD_SHARED_LIBRARY)
