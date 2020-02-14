ifeq ($(EN_FFMPEG_AUDIO_DEC),true)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

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

NX_HW_TOP 		:= $(TOP)/hardware/nexell/s5pxx18
NX_HW_INCLUDE	:= $(NX_HW_TOP)/include
OMX_TOP			:= $(NX_HW_TOP)/omx
FFMPEG_PATH		:= $(OMX_TOP)/codec/ffmpeg

LOCAL_SRC_FILES:= \
	NX_OMXAudioDecoderFFMpeg.c

LOCAL_C_INCLUDES += \
	$(TOP)/system/core/include \
	$(TOP)/hardware/libhardware/include \
	$(NX_HW_INCLUDE) \
	$(OMX_TOP)/include \
	$(OMX_TOP)/core/inc \
	$(OMX_TOP)/components/base

LOCAL_C_INCLUDES_32 += \
	$(FFMPEG_PATH)/32bit/include

LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Common \
	libNX_OMX_Base \
	libdl \
	liblog \
	libhardware \

ifeq ($(ANDROID_VERSION), 9)
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavutil.so
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavcodec.so
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavformat.so
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libswresample.so
LOCAL_MODULE_OWNER := arm
else
LOCAL_LDFLAGS_32 += \
	-L$(FFMPEG_PATH)/32bit/libs	\
	-lavutil 			\
	-lavcodec  		\
	-lavformat		\
	-lavdevice		\
	-lavfilter		\
	-lswresample
endif

LOCAL_CFLAGS += $(NX_OMX_CFLAGS)

LOCAL_CFLAGS += -DNX_DYNAMIC_COMPONENTS

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_MODULE:= libNX_OMX_AUDIO_DECODER_FFMPEG

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)

endif	# EN_FFMPEG_AUDIO_DEC
