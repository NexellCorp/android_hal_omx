#define	LOG_TAG				"NX_VP8DEC"

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"
#include "NX_AVCUtil.h"

static int VP8CheckPortReconfiguration( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_BYTE inBuf, OMX_S32 inSize )
{
	if ( (inBuf != NULL) && (inSize > 0) )
	{
		int32_t w,h;	//	width, height, left, top, right, bottom
		OMX_BYTE pbyStrm = inBuf;
		uint32_t readSizeBit = 0;
		uint32_t remainSizeBit = inSize * 8;

		do
		{

			GetBitContext gb;
			remainSizeBit = remainSizeBit - readSizeBit;
			if( remainSizeBit < 6*8 )
				break;
			if( readSizeBit > 1024*8 )
				break;
			init_get_bits(&gb, pbyStrm, remainSizeBit);
			pbyStrm++;

			OMX_BYTE data[10];
			data[0] = (OMX_BYTE)get_bits(&gb, 8);
			data[1] = (OMX_BYTE)(OMX_BYTE)get_bits(&gb, 8);
			data[2] = (OMX_BYTE)get_bits(&gb, 8);
			data[3] = (OMX_BYTE)get_bits(&gb, 8);
			data[4] = (OMX_BYTE)get_bits(&gb, 8);
			data[5] = (OMX_BYTE)get_bits(&gb, 8);
			data[6] = (OMX_BYTE)get_bits(&gb, 8);
			data[7] = (OMX_BYTE)get_bits(&gb, 8);
			data[8] = (OMX_BYTE)get_bits(&gb, 8);
			data[9] = (OMX_BYTE)get_bits(&gb, 8);
			readSizeBit = readSizeBit + 8*10;

			// vet via sync code
			if ( (int32_t)data[3] == 0x9d && (int32_t)data[4] == 0x01 && (int32_t)data[5] == 0x2a )
			{
				{
					w = ((int32_t)data[6] | ((int32_t)data[7] << 8)) & 0x3fff;
            		h = ((int32_t)data[8] | ((int32_t)data[9] << 8)) & 0x3fff;
					{
						if( pDecComp->width != w || pDecComp->height != h )
						{
							DbgMsg("New Video Resolution = %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, w, h);

							//	Change Port Format & Resolution Information
							pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth  = pDecComp->width  = w;
							pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = pDecComp->height = h;

							//	Native Mode
							if( pDecComp->bUseNativeBuffer )
							{
								pDecComp->pOutputPort->stdPortDef.nBufferSize = 4096;
							}
							else
							{
								pDecComp->pOutputPort->stdPortDef.nBufferSize = ((((w+15)>>4)<<4) * (((h+15)>>4)<<4))*3/2;
							}

							//	Need Port Reconfiguration
							SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventPortSettingsChanged, OMX_DirOutput, 0, NULL );
							pDecComp->bPortReconfigure = OMX_TRUE;
							if( OMX_TRUE == pDecComp->bInitialized )
							{
								pDecComp->bInitialized = OMX_FALSE;
								InitVideoTimeStamp(pDecComp);
								closeVideoCodec(pDecComp);
								openVideoCodec(pDecComp);
							}
							pDecComp->pOutputPort->stdPortDef.bEnabled = OMX_FALSE;
							return 1;
						}
						else
						{
							// DbgMsg("Video Resolution = %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, w, h);
							return 0;
						}
					}
					break;
				}
			}
		} while(1);
	}

	return 0;
}

int NX_DecodeVP8Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	int inSize = 0;
	OMX_BYTE inData;
	NX_V4L2DEC_IN decIn;
	NX_V4L2DEC_OUT decOut;
	int ret = 0;

	UNUSED_PARAM(pOutQueue);

	memset(&decIn,  0, sizeof(decIn)  );

	if( pDecComp->bFlush )
	{
		flushVideoCodec( pDecComp );
		pDecComp->bFlush = OMX_FALSE;
		pDecComp->inFlushFrameCount = 0;
		pDecComp->bIsFlush = OMX_TRUE;
		pDecComp->tmpInputBufferIndex = 0;
	}

	// Get Next Queue Information
	NX_PopQueue( pInQueue, (void**)&pInBuf );
	if( pInBuf == NULL ){
		return 0;
	}

	inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;
	pDecComp->inFrameCount++;

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
	TRACE("pInBuf->nTimeStamp = %lld, pInBuf->nFilledLen = %lu\n", pInBuf->nTimeStamp/1000, pInBuf->nFilledLen);
	PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );

	//	Step 2. Find First Key Frame & Do Initialize VPU
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		int initBufSize = 0;
		unsigned char *initBuf = NULL;

		if( pDecComp->codecSpecificDataSize == 0 && pDecComp->nExtraDataSize>0 )
		{
			initBufSize = inSize + pDecComp->nExtraDataSize;
			initBuf = (unsigned char *)malloc( initBufSize );
			memcpy( initBuf, pDecComp->pExtraData, pDecComp->nExtraDataSize );
			memcpy( initBuf + pDecComp->nExtraDataSize, inData, inSize );
		}
		else
		{
			initBufSize = inSize + pDecComp->codecSpecificDataSize;
			initBuf = (unsigned char *)malloc( initBufSize );
			memcpy( initBuf, pDecComp->codecSpecificData, pDecComp->codecSpecificDataSize );
			memcpy( initBuf + pDecComp->codecSpecificDataSize, inData, inSize );
		}

		if( pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
		{
			ret = 0;
			goto Exit;
		}

		//	Initialize VPU
		ret = InitializeCodaVpu(pDecComp, initBuf, initBufSize );

		free( initBuf );

		if( 0 > ret )
		{
			ErrMsg("VPU initialized Failed!!!!\n");
			goto Exit;
		}
		else if( ret > 0  )
		{
			ret = 0;
			goto Exit;
		}

		pDecComp->bNeedKey = OMX_FALSE;
		pDecComp->bInitialized = OMX_TRUE;

		//decIn.strmBuf = inData;
		//decIn.strmSize = 0;
		//decIn.timeStamp = pInBuf->nTimeStamp;
		//decIn.eos = 0;
		//ret = NX_V4l2DecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
		decOut.dispIdx = -1;
	}
	else
	{
		if( VP8CheckPortReconfiguration( pDecComp, inData, inSize ) )
		{
			pDecComp->bNeedSequenceData = OMX_FALSE;
			if( pDecComp->codecSpecificData )
				free( pDecComp->codecSpecificData );
			pDecComp->codecSpecificData = malloc(inSize);
			memcpy( pDecComp->codecSpecificData, inData, inSize );
			pDecComp->codecSpecificDataSize = inSize;
			goto Exit;
		}

		if( pDecComp->bIsFlush )
		{
			if( pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
			{
				goto Exit;
			}
			memcpy( pDecComp->tmpInputBuffer + pDecComp->tmpInputBufferIndex, inData, inSize );
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
#if 0
			if( pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DECODED_FRAME] != PIC_TYPE_I )
			{
				NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
				goto Exit;
			}

#endif
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
				ErrMsg("Unexpected Buffer Handling!!!! Goto Exit\n");
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
			TRACE("pOutBuf->nTimeStamp = %lld\n", pOutBuf->nTimeStamp/1000);

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

	return ret;
}
