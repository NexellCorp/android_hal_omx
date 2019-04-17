LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_CFLAGS :=

ANDROID_VERSION_STR := $(PLATFORM_VERSION)
ANDROID_VERSION := $(firstword $(ANDROID_VERSION_STR))
ifeq ($(ANDROID_VERSION), 9)
LOCAL_VENDOR_MODULE := true
else
LOCAL_MODULE_TAGS := optional
endif

ifeq "7" "$(ANDROID_VERSION)"
$( === This is NOUGAT ===)
#LOCAL_CFLAGS += -DNOUGAT=1
endif

LOCAL_CFLAGS += -DOUT_BUF_COPY=0

NX_HW_TOP := $(TOP)/hardware/nexell/s5pxx18
NX_HW_INCLUDE := $(NX_HW_TOP)/include
NX_LIBRARY_TOP := $(TOP)/device/nexell/library

OMX_TOP := $(NX_HW_TOP)/omx

LOCAL_SRC_FILES:= \
	NX_AVCDecoder.c \
	NX_MPEG2Decoder.c \
	NX_MPEG4Decoder.c \
	NX_XvidDecoder.c \
	NX_Div3Decoder.c \
	NX_RVDecoder.c \
	NX_VC1Decoder.c \
	NX_VP8Decoder.c \
	NX_DecoderUtil.c \
	NX_AVCUtil.c \
	NX_OMXVideoDecoder.c

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/include \
	$(TOP)/hardware/libhardware/include \
	$(NX_HW_TOP)/gralloc \
	$(NX_HW_INCLUDE)

LOCAL_C_INCLUDES += \
	$(OMX_TOP)/include \
	$(OMX_TOP)/core/inc \
	$(OMX_TOP)/components/base \
	$(NX_LIBRARY_TOP)/nx-video-api/src/include \
	$(NX_LIBRARY_TOP)/nx-video-api/src \
	$(NX_LIBRARY_TOP)/nx-scaler

LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Common \
	libNX_OMX_Base \
	libdl \
	liblog \
	libhardware \
	libnx_video_api \
	libnx_scaler \
	libion \
	libutils \
	libcutils

ifeq ($(ANDROID_VERSION), 9)
LOCAL_HEADER_LIBRARIES := media_plugin_headers
endif

LOCAL_CFLAGS +=  -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_CFLAGS += $(NX_OMX_CFLAGS)

LOCAL_CFLAGS += -DNX_DYNAMIC_COMPONENTS -DUSE_ION_ALLOCATOR

LOCAL_MODULE:= libNX_OMX_VIDEO_DECODER

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)
