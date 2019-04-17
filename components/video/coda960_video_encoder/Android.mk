LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

NX_HW_TOP := $(TOP)/hardware/nexell/s5pxx18
NX_HW_INCLUDE := $(NX_HW_TOP)/include
NX_LIBRARY_TOP := $(TOP)/device/nexell/library

ANDROID_VERSION_STR := $(PLATFORM_VERSION)
ANDROID_VERSION := $(firstword $(ANDROID_VERSION_STR))
ifeq ($(ANDROID_VERSION), 9)
LOCAL_VENDOR_MODULE := true
else
LOCAL_MODULE_TAGS := optional
endif

ifeq "7" "$(ANDROID_VERSION)"
$( === This is NOUGAT ===)
LOCAL_CFLAGS += -DNOUGAT=1
endif


OMX_TOP := $(NX_HW_TOP)/omx
#RATECONTROL_PATH := $(NX_LINUX_TOP)/library/lib/ratecontrol

LOCAL_SRC_FILES:= \
	NX_OMXVideoEncoder.c

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/include \
	$(TOP)/hardware/libhardware/include \
	$(NX_HW_TOP)/gralloc \
	$(NX_HW_INCLUDE)

LOCAL_C_INCLUDES += \
	$(OMX_TOP)/include \
	$(OMX_TOP)/core/inc \
	$(OMX_TOP)/codec/video/coda960 \
	$(OMX_TOP)/components/base \
	$(NX_LIBRARY_TOP)/nx-video-api/src/include \
	$(NX_LIBRARY_TOP)/nx-video-api/src \
	$(NX_LIBRARY_TOP)/nx-csc

LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Common \
	libNX_OMX_Base \
	libdl \
	liblog \
	libhardware \
	libnx_video_api \
	libion \
	libutils \
	libnx_csc

ifeq ($(ANDROID_VERSION), 9)
LOCAL_HEADER_LIBRARIES := media_plugin_headers
endif

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall
LOCAL_CFLAGS += $(NX_OMX_CFLAGS)
LOCAL_CFLAGS += -DNX_DYNAMIC_COMPONENTS  -DUSE_ION_ALLOCATOR

LOCAL_MODULE := libNX_OMX_VIDEO_ENCODER

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)
