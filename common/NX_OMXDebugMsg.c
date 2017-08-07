//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module     :
//	File       :
//	Description:
//	Author     : RayPark
//	History    :
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <NX_OMXDebugMsg.h>

static unsigned int gst_TargetDebugLevel = NX_DBG_INFO;	//	Error
static char gst_Prefix[128] = "[NX_OXM]";

void NX_DbgSetPrefix( const char *pPrefix )
{
	strcpy( gst_Prefix, pPrefix );
}

void NX_DbgSetLevel( unsigned int TargetLevel )
{
	printf("%s >>> NX_DbgSetLevel : from %d to %d\n", gst_Prefix, gst_TargetDebugLevel, TargetLevel);
	gst_TargetDebugLevel = TargetLevel;
}

void NX_RelMsg( unsigned int level, const char *format, ... )
{
	char buffer[1024];
	if( level > gst_TargetDebugLevel )	return;

	{
		va_list marker;
		va_start(marker, format);
		vsprintf(buffer, format, marker);
//		printf( "%s %s", gst_Prefix, buffer );
		ALOGD( "%s", buffer );
		va_end(marker);
	}
}

void NX_ErrMsg( const char *format, ... )
{
	char buffer[1024];
	va_list marker;
	va_start(marker, format);
	vsprintf(buffer, format, marker);
//	printf( "%s %s", gst_Prefix, buffer );
	ALOGE( "%s", buffer );
	va_end(marker);
}

void NX_DbgTrace( const char *format, ... )
{
#ifdef NX_DEBUG
	char buffer[1024];
	if( NX_DBG_TRACE > gst_TargetDebugLevel )	return;
	{
		va_list marker;
		va_start(marker, format);
		vsprintf(buffer, format, marker);
//		printf( "%s %s", gst_Prefix, buffer );
		ALOGD( "%s", buffer );
		va_end(marker);
	}
#endif
}
