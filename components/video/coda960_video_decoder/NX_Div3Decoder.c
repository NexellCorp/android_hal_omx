#define	LOG_TAG				"NX_DIV3DEC"

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"

static int32_t MakeDiv3DecodeSpecificInfo( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_U8 *pIn, OMX_S32 inSize, OMX_U8 *pOut )
{
	unsigned char *pbHeader = pOut;
	int nMetaData = inSize;
	unsigned char *pbMetaData = pIn;
	int retSize = 0;
	if( !nMetaData )
	{
		PUT_LE32(pbHeader, MKTAG('C', 'N', 'M', 'V'));	//signature 'CNMV'
		PUT_LE16(pbHeader, 0x00);						//version
		PUT_LE16(pbHeader, 0x20);						//length of header in bytes
		PUT_LE32(pbHeader, MKTAG('D', 'I', 'V', '3'));	//codec FourCC
		PUT_LE16(pbHeader, pDecComp->width);			//width
		PUT_LE16(pbHeader, pDecComp->height);			//height
		PUT_LE32(pbHeader, 0);							//frame rate
		PUT_LE32(pbHeader, 0);							//time scale(?)
		PUT_LE32(pbHeader, 1);							//number of frames in file
		PUT_LE32(pbHeader, 0);							//unused
		retSize += 32;
	}
	else
	{
		PUT_BE32(pbHeader, nMetaData);
		retSize += 4;
		memcpy(pbHeader, pbMetaData, nMetaData);
		retSize += nMetaData;
	}
	return retSize;
}

static int MakeDiv3Stream( OMX_U8 *pIn, OMX_S32 inSize, OMX_U8 *pOut )
{
	PUT_LE32(pOut,inSize);
	PUT_LE32(pOut,0);
	PUT_LE32(pOut,0);
	memcpy( pOut, pIn, inSize );
	return (inSize + 12);
}

int NX_DecodeDiv3Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	int inSize = 0;
	OMX_BYTE inData;
	NX_V4L2DEC_IN decIn;
	NX_V4L2DEC_OUT decOut;
	int ret = 0;

	UNUSED_PARAM(pOutQueue);

	if( pDecComp->bFlush )
	{
		flushVideoCodec( pDecComp );
		pDecComp->bFlush = OMX_FALSE;
		pDecComp->inFlushFrameCount = 0;
		pDecComp->bIsFlush = OMX_TRUE;
		pDecComp->tmpInputBufferIndex = 0;
	}

	NX_PopQueue( pInQueue, (void**)&pInBuf );
	if( pInBuf == NULL ){
		return 0;
	}

	inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;
	pDecComp->inFrameCount++;

	TRACE("pInBuf->nFlags = 0x%08x, size = %ld\n", (int)pInBuf->nFlags, pInBuf->nFilledLen );

	if( pInBuf->nFlags & OMX_BUFFERFLAG_EOS )
	{
		DbgMsg("=========================> Receive Endof Stream Message (%ld)\n", pInBuf->nFilledLen);

		pDecComp->bStartEoS = OMX_TRUE;
		if( inSize <= 0)
		{
			pInBuf->nFilledLen = 0;
			pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);
			return 0;
		}
	}

	//	Push Input Time Stamp
	if( !(pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) )
		PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );


	//	Step 2. Find First Key Frame & Do Initialize VPU
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		OMX_S32 size = 0;

		size = MakeDiv3DecodeSpecificInfo( pDecComp, pDecComp->pExtraData, pDecComp->nExtraDataSize, pDecComp->tmpInputBuffer );
		size += MakeDiv3Stream( inData, inSize, pDecComp->tmpInputBuffer + size );
		//	Initialize VPU
		ret = InitializeCodaVpu(pDecComp, pDecComp->tmpInputBuffer, size );
		if( 0 != ret )
		{
			ErrMsg("VPU initialized Failed!!!!\n");
			goto Exit;
		}

		pDecComp->bNeedKey = OMX_FALSE;
		pDecComp->bInitialized = OMX_TRUE;
		decOut.dispIdx = -1;
	}
	else
	{
		if( pDecComp->bIsFlush )
		{
			if( pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
			{
				goto Exit;
			}
			inSize = MakeDiv3Stream( inData, inSize, pDecComp->tmpInputBuffer + pDecComp->tmpInputBufferIndex );

			pDecComp->tmpInputBufferIndex = pDecComp->tmpInputBufferIndex + inSize;

			pDecComp->inFlushFrameCount++;

			if( FLUSH_FRAME_COUNT == pDecComp->inFlushFrameCount)
			{
				pDecComp->inFlushFrameCount = 0;
				pDecComp->bIsFlush = OMX_FALSE;
				inData = pDecComp->tmpInputBuffer;
				inSize = pDecComp->tmpInputBufferIndex;
				pDecComp->tmpInputBufferIndex = 0;
			}
			else
			{
				goto Exit;
			}
		}

		else
		{
			inSize = MakeDiv3Stream( inData, inSize, pDecComp->tmpInputBuffer );
			inData = pDecComp->tmpInputBuffer;
		}

		decIn.strmBuf = inData;
		decIn.strmSize = inSize;
		decIn.timeStamp = pInBuf->nTimeStamp;
		decIn.eos = 0;
		ret = NX_V4l2DecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
	}

	TRACE(">>> [%06ld/%06ld] decOut : dispIdx(%d) decIdx(%d) \n",
		pDecComp->inFrameCount, pDecComp->outFrameCount, decOut.dispIdx, decOut.decIdx );


	if( ret==VID_ERR_NONE && decOut.dispIdx >= 0 && ( decOut.dispIdx < NX_OMX_MAX_BUF ) )
	{
		if( OMX_TRUE == pDecComp->bEnableThumbNailMode )
		{
			//	Thumbnail Mode
			NX_VID_MEMORY_INFO *pImg = &decOut.hImg;
			NX_PopQueue( pOutQueue, (void**)&pOutBuf );

			CopySurfaceToBufferYV12( (OMX_U8 *)pImg->pBuffer[0], pOutBuf->pBuffer, pDecComp->width, pDecComp->height );

			NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
			pOutBuf->nFilledLen = pDecComp->width * pDecComp->height * 3 / 2;
			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
				pOutBuf->nFlags     = pInBuf->nFlags;
			}
			TRACE("ThumbNail Mode : pOutBuf->nAllocLen = %ld, pOutBuf->nFilledLen = %ld\n", pOutBuf->nAllocLen, pOutBuf->nFilledLen );
			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
		else
		{
			if( pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DISPLAY_FRAME] != PIC_TYPE_I )
			{
				OMX_TICKS timestamp;
				OMX_U32 flag;
				PopVideoTimeStamp(pDecComp, &timestamp, &flag );
				NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
				goto Exit;
			}

			int32_t OutIdx = 0;
			if( (OMX_FALSE == pDecComp->bInterlaced) && (OMX_FALSE == pDecComp->bOutBufCopy) )
			{
				OutIdx = decOut.dispIdx;
			}
			else // OMX_TRUE == pDecComp->bInterlaced, pDecComp->bOutBufCopy
			{
				OutIdx = GetUsableBufferIdx(pDecComp);
			}

			pDecComp->isOutIdr = OMX_TRUE;
			//	Native Window Buffer Mode
			//	Get Output Buffer Pointer From Output Buffer Pool
			pOutBuf = pDecComp->pOutputBuffers[OutIdx];

			if( pDecComp->outBufferUseFlag[OutIdx] == 0 )
			{
				OMX_TICKS timestamp;
				OMX_U32 flag;
				PopVideoTimeStamp(pDecComp, &timestamp, &flag );
				NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
				DbgMsg("Unexpected Buffer Handling!!!! Goto Exit\n");
				goto Exit;
			}
			pDecComp->outBufferValidFlag[OutIdx] = 1;
			pDecComp->outBufferUseFlag[OutIdx] = 0;
			pDecComp->curOutBuffers --;

			pOutBuf->nFilledLen = sizeof(struct private_handle_t);
			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
				pOutBuf->nFlags     = pInBuf->nFlags;
			}
			TRACE("nTimeStamp = %lld\n", pOutBuf->nTimeStamp/1000);

			if( OMX_TRUE == pDecComp->bOutBufCopy )
			{
				OutBufCopy( pDecComp, &decOut );
			}

			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
	}

Exit:
	pInBuf->nFilledLen = 0;
	pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);

	return 0;
}
