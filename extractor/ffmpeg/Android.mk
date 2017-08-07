ifeq ($(EN_FFMPEG_EXTRACTOR),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

NX_HW_TOP 		:= $(TOP)/hardware/nexell/s5pxx18
OMX_TOP			:= $(NX_HW_TOP)/omx
FFMPEG_PATH		:= $(OMX_TOP)/codec/ffmpeg

LOCAL_MODULE	:= libNX_FFMpegExtractor
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES :=			\
        FFmpegExtractor.cpp	\
		ffmpeg_source.cpp	\
		ffmpeg_utils.cpp

LOCAL_C_INCLUDES :=										\
	$(TOP)/frameworks/av/include/media/stagefright		\
	$(TOP)/frameworks/av/media/libstagefright/include	\
	$(TOP)/system/core/include

LOCAL_C_INCLUDES_32 += \
	$(FFMPEG_PATH)/32bit/include

LOCAL_C_INCLUDES_64 += \
	$(FFMPEG_PATH)/64bit/include


LOCAL_CFLAGS += -D__STDC_CONSTANT_MACROS=1 -D__STDINT_LIMITS=1

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_32_BIT_ONLY := true

include $(BUILD_STATIC_LIBRARY)

endif	# EN_FFMPEG_EXTRACTOR
