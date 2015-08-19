LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

OMX_DEBUG := 0

NX_OMX_CFLAGS := -Wall -fpic -pipe -O2

NX_HW_TOP := $(TOP)/hardware/samsung_slsi/slsiap/
NX_HW_INCLUDE := $(NX_HW_TOP)/include
NX_LINUX_INCLUDE := $(TOP)/linux/platform/$(TARGET_CPU_VARIANT2)/library/include

NX_OMX_TOP := $(NX_HW_TOP)/omx
NX_OMX_COMM := $(NX_OMX_TOP)/common
NX_OMX_CORE := $(NX_OMX_TOP)/core
NX_OMX_COMP := $(NX_OMX_TOP)/components
NX_OMX_PLUGIN := $(NX_OMX_TOP)/libstagefrighthw

NX_OMX_INCLUDES := \
	$(TOP)/system/core/include \
	$(NX_OMX_COMM) \
	$(NX_OMX_CORE)/inc \
	$(NX_OMX_COMP)/base \
	$(NX_LINUX_INCLUDE)

#call to common
include $(NX_OMX_COMM)/Android.mk

#call to core
include $(NX_OMX_CORE)/Android.mk

#call to stagefrighthw
include $(NX_OMX_PLUGIN)/Android.mk

#call to components
include $(NX_OMX_COMP)/Android.mk

#call extractor
ifeq ($(EN_FFMPEG_EXTRACTOR),true)
include $(NX_OMX_TOP)/extractor/ffmpeg/Android.mk
endif
