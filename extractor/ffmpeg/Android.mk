ifeq ($(EN_FFMPEG_EXTRACTOR),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

NX_HW_TOP 		:= $(TOP)/hardware/nexell/s5pxx18
OMX_TOP			:= $(NX_HW_TOP)/omx
FFMPEG_PATH		:= $(OMX_TOP)/codec/ffmpeg

LOCAL_MODULE	:= libNX_FFMpegExtractor

ANDROID_VERSION_STR := $(PLATFORM_VERSION)
ANDROID_VERSION := $(firstword $(ANDROID_VERSION_STR))
ifeq ($(ANDROID_VERSION), 9)
LOCAL_MODULE_RELATIVE_PATH := extractors
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -DPIE
LOCAL_CFLAGS += -DENABLE_FFMPEG_EXTRACTOR
else
LOCAL_MODULE_TAGS := optional
endif

ifeq ($(ANDROID_VERSION), 9)
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavutil.so
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavcodec.so
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavformat.so
LOCAL_MODULE_OWNER := arm
endif

ifeq ($(ANDROID_VERSION), 9)
LOCAL_SRC_FILES :=			\
		FFmpegExtractorExport.cpp \
		FFmpegExtractor.cpp	\
		ffmpeg_source.cpp	\
		ffmpeg_utils.cpp
else
LOCAL_SRC_FILES :=			\
		FFmpegExtractor.cpp	\
		ffmpeg_source.cpp	\
		ffmpeg_utils.cpp
endif


ifeq ($(ANDROID_VERSION), 9)
LOCAL_C_INCLUDES :=										\
	$(TOP)/frameworks/av/include						\
	$(TOP)/frameworks/av/include/media					\
	$(TOP)/frameworks/av/media/libstagefright/include	\
	$(TOP)/frameworks/av/media/libmedia/include			\
	$(TOP)/frameworks/native/libs/binder/include		\
	$(TOP)/system/media/audio/include					\
	$(TOP)/system/core/base/include						\
	$(TOP)/system/core/include
else
LOCAL_C_INCLUDES :=										\
	$(TOP)/frameworks/av/include/media/stagefright		\
	$(TOP)/frameworks/av/media/libstagefright/include	\
	$(TOP)/system/core/include
endif

LOCAL_C_INCLUDES_32 += \
	$(FFMPEG_PATH)/32bit/include

LOCAL_C_INCLUDES_64 += \
	$(FFMPEG_PATH)/64bit/include

ifeq ($(ANDROID_VERSION), 9)
LOCAL_SHARED_LIBRARIES := \
 		liblog \
 		libstagefright_foundation \
 		libcutils \
 		libutils \
 		libmediaextractor \
 		libc \
 		libbinder
endif


LOCAL_CFLAGS += -D__STDC_CONSTANT_MACROS=1 -D__STDINT_LIMITS=1

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_32_BIT_ONLY := true

ifeq ($(ANDROID_VERSION), 9)
include $(BUILD_SHARED_LIBRARY)
else
include $(BUILD_STATIC_LIBRARY)
endif

#############################################################################################
# ffmpeg avi extractor(pie)

include $(CLEAR_VARS)

ANDROID_VERSION_STR := $(PLATFORM_VERSION)
ANDROID_VERSION := $(firstword $(ANDROID_VERSION_STR))
ifeq ($(ANDROID_VERSION), 9)

NX_HW_TOP			:= $(TOP)/hardware/nexell/s5pxx18
OMX_TOP			:= $(NX_HW_TOP)/omx
FFMPEG_PATH		:= $(OMX_TOP)/codec/ffmpeg

LOCAL_MODULE	:= libNX_FFMpegAVIExtractor

LOCAL_MODULE_RELATIVE_PATH := extractors
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -DPIE
LOCAL_CFLAGS += -DENABLE_FFMPEG_EXTRACTOR

LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavutil.so
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavcodec.so
LOCAL_LDLIBS += $(FFMPEG_PATH)/32bit/libs/libavformat.so
LOCAL_MODULE_OWNER := arm

LOCAL_SRC_FILES :=			\
		FFmpegAVIExtractorExport.cpp \
		FFmpegExtractor.cpp	\
		ffmpeg_source.cpp	\
		ffmpeg_utils.cpp

LOCAL_C_INCLUDES :=										\
	$(TOP)/frameworks/av/include						\
	$(TOP)/frameworks/av/include/media					\
	$(TOP)/frameworks/av/media/libstagefright/include	\
	$(TOP)/frameworks/av/media/libmedia/include			\
	$(TOP)/frameworks/native/libs/binder/include		\
	$(TOP)/system/media/audio/include					\
	$(TOP)/system/core/base/include						\
	$(TOP)/system/core/include

LOCAL_C_INCLUDES_32 += \
	$(FFMPEG_PATH)/32bit/include

LOCAL_C_INCLUDES_64 += \
	$(FFMPEG_PATH)/64bit/include

LOCAL_SHARED_LIBRARIES := \
 		liblog \
 		libstagefright_foundation \
 		libcutils \
 		libutils \
 		libmediaextractor \
 		libc \
 		libbinder


LOCAL_CFLAGS += -D__STDC_CONSTANT_MACROS=1 -D__STDINT_LIMITS=1

ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

LOCAL_CFLAGS += -Wno-multichar -Werror -Wno-error=deprecated-declarations -Wall

LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)

endif	# ifeq ($(ANDROID_VERSION), 9)  #ffmpeg avi extractor(pie)

endif	# EN_FFMPEG_EXTRACTOR
