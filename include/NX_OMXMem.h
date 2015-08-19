//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module     : OAL Memory Module
//	File       : 
//	Description:
//	Author     : RayPark
//	History    :
//------------------------------------------------------------------------------
#ifndef __NX_OMXMem_h__
#define __NX_OMXMem_h__

#include <stdlib.h>
#include <memory.h>

#if 1	//	Android/Linux/Windows CE etc
#define	NxMalloc			malloc
#define	NxFree				free
#define NxRealloc			realloc
#define NxMemset			memset
#define NxMemcpy			memcpy

#else	//	Custom Debug Allocator or other Allocator

#endif

#endif	//	__NX_OMXMem_h__
