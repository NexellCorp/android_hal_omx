//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module     : Debug Module
//	File       :
//	Description:
//	Author     : RayPark
//	History    :
//------------------------------------------------------------------------------
#ifndef __NX_DebugMsg_h__
#define __NX_DebugMsg_h__

#define NX_DEBUG

#define	NX_DBG_DISABLE		0
#define NX_DBG_ERROR		1
#define NX_DBG_WARNING		2
#define NX_DBG_INFO			3
#define NX_DBG_DEBUG		4
#define NX_DBG_TRACE		5


void NX_DbgSetPrefix( const char *pPrefix );
void NX_DbgSetLevel( unsigned int TargetLevel );
void NX_RelMsg( unsigned int level, const char *format, ... );
void NX_DbgTrace( const char *format, ... );
void NX_ErrMsg( const char *format, ... );


//	for android
#include <cutils/log.h>
#ifdef NX_DEBUG
#define	NX_LOGE(fmt...)			ALOGE(fmt)
#define	NX_LOGV(fmt...)			ALOGV(fmt)
#define	NX_LOGD(fmt...)			ALOGD(fmt)
#define	NX_LOGI(fmt...)			ALOGI(fmt)
#define	NX_LOGW(fmt...)			ALOGW(fmt)

#ifndef DbgMsg
#define	DbgMsg(fmt...)			ALOGD(fmt)
#endif	//	DbgMsg
#ifndef	ErrMsg
#define	ErrMsg(fmt...)			ALOGE(fmt)
#endif	//	ErrMsg

#else
#define	NX_LOGE(fmt...)
#define	NX_LOGV(fmt...)
#define	NX_LOGD(fmt...)
#define	NX_LOGI(fmt...)
#define	NX_LOGW(fmt...)

#ifndef DbgMsg
#define	DbgMsg(fmt...)
#endif	//	DbgMsg
#ifndef	ErrMsg
#define	ErrMsg(fmt...)
#endif	//	ErrMsg
#endif

#endif	//	__NX_DebugMsg_h__
