//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module      : Queue Module
//	File        : 
//	Description : Thread safe Queue moudle
//	Author      : Seong-O Park (ray@nexell.co.kr)
//	History     :
//------------------------------------------------------------------------------
#ifndef __NX_OMXQueue_h__
#define __NX_OMXQueue_h__

#include <pthread.h>

#define NX_MAX_QUEUE_ELEMENT	128

typedef struct NX_QUEUE{
	unsigned int head;
	unsigned int tail;
	unsigned int maxElement;
	unsigned int curElements;
	int bEnabled;
	void *pElements[NX_MAX_QUEUE_ELEMENT];
	pthread_mutex_t	hMutex;
}NX_QUEUE;

int NX_InitQueue( NX_QUEUE *pQueue, unsigned int maxNumElement );
int NX_PushQueue( NX_QUEUE *pQueue, void *pElement );
int NX_PopQueue( NX_QUEUE *pQueue, void **pElement );
int NX_GetNextQueuInfo( NX_QUEUE *pQueue, void **pElement );
unsigned int NX_GetQueueCnt( NX_QUEUE *pQueue );
void NX_DeinitQueue( NX_QUEUE *pQueue );

#endif	//	__NX_OMXQueue_h__
