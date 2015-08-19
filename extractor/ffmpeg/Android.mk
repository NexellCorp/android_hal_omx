ifeq ($(EN_FFMPEG_EXTRACTOR),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

NX_HW_TOP 		:= $(TOP)/hardware/samsung_slsi/slsiap
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
	$(TOP)/system/core/include							\
	$(FFMPEG_PATH)/include

LOCAL_CFLAGS += -D__STDC_CONSTANT_MACROS=1 -D__STDINT_LIMITS=1

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_STATIC_LIBRARY)

endif	# EN_FFMPEG_EXTRACTOR
