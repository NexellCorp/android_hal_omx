ifeq ($(EN_FFMPEG_AUDIO_DEC),true)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo "This is LOLLIPOP!!!"
LOCAL_CFLAGS += -DLOLLIPOP=1
endif

NX_HW_TOP 		:= $(TOP)/hardware/samsung_slsi/slsiap
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
	$(OMX_TOP)/components/base \
	$(FFMPEG_PATH)/include

LOCAL_SHARED_LIBRARIES := \
	libNX_OMX_Common \
	libNX_OMX_Base \
	libdl \
	liblog \
	libhardware \

LOCAL_LDFLAGS += \
	-L$(FFMPEG_PATH)/libs	\
	-lavutil-2.1.4 			\
	-lavcodec-2.1.4  		\
	-lavformat-2.1.4		\
	-lavdevice-2.1.4		\
	-lavfilter-2.1.4		\
	-lswresample-2.1.4

LOCAL_CFLAGS += $(NX_OMX_CFLAGS)

LOCAL_CFLAGS += -DNX_DYNAMIC_COMPONENTS

LOCAL_MODULE:= libNX_OMX_AUDIO_DECODER_FFMPEG

include $(BUILD_SHARED_LIBRARY)

endif	# EN_FFMPEG_AUDIO_DEC
