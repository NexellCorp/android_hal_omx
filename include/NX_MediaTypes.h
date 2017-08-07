//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module     : OMX Types
//	File       : 
//	Description:
//	Author     : RayPark
//	History    :
//------------------------------------------------------------------------------
#ifndef __NX_OMXTypes_h__
#define __NX_OMXTypes_h__

typedef unsigned char		uint8;
typedef signed char			int8;

typedef unsigned short		uint16;
typedef signed short		int16;

typedef unsigned int		uint32;
typedef signed int			int32;

#ifdef WIN32
typedef unsigned __int64	uint64;
typedef signed __int64		int64;
#else
typedef unsigned long long	uint64;
typedef signed long long	int64;
#endif

#ifndef NULL
#define NULL		0
#endif

#ifndef FALSE
#define FALSE		0
#endif

#ifndef TRUE
#define TRUE		(0==0)
#endif

typedef int					boolean;

#endif	//	__NX_OMXTypes_h__
