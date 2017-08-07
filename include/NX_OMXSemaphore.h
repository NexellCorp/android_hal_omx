//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module      : Semaphore Module.
//	File        : 
//	Description :
//	Author      : Seong-O Park (ray@nexell.co.kr)
//	History     :
//------------------------------------------------------------------------------
#ifndef __NX_Semaphore_h__
#define __NX_Semaphore_h__

#define		NX_ESEM_TIMEOUT			ETIMEDOUT			//	Timeout
#define		NX_ESEM					-1
#define		NX_ESEM_OVERFLOW		-2					//	Exceed Max Value

typedef struct _NX_SEMAPHORE NX_SEMAPHORE;

struct _NX_SEMAPHORE{
	unsigned int nValue;
	unsigned int nMaxValue;
	pthread_cond_t hCond;
	pthread_mutex_t hMutex;
};

NX_SEMAPHORE *NX_CreateSem( unsigned int initValue, unsigned int maxValue );
void NX_DestroySem( NX_SEMAPHORE *hSem );
int NX_PendSem( NX_SEMAPHORE *hSem );
int NX_PostSem( NX_SEMAPHORE *hSem );
//int32 NX_PendTimedSem( NX_SEMAPHORE *hSem, uint32 milliSeconds );

#endif	//	__NX_Semaphore_h__
