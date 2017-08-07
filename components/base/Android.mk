LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
	NX_OMXBaseComponent.c \
	NX_OMXBasePort.c \
	NX_OMXCommon.c

LOCAL_C_INCLUDES += \
	$(TOP)/hardware/nexell/s5pxx18/omx/include \
	$(TOP)/hardware/nexell/s5pxx18/omx/core/inc

LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Common\
	libdl \
	liblog

LOCAL_CFLAGS := $(NX_OMX_CFLAGS)

LOCAL_CFLAGS += -DNO_OPENCORE

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_MODULE:= libNX_OMX_Base

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)
