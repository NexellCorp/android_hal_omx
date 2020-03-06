#define	LOG_TAG				"NX_AVCDEC"

#include <utils/Log.h>

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"

//	From NX_AVCUtil
int avc_get_video_size(unsigned char *buf, int buf_size, int *width, int *height);

#if 0
static int AVCCheckPortReconfiguration( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_BYTE inBuf, OMX_S32 inSize )
{
	if ( (inBuf != NULL) && (inSize > 0) )
	{
		int32_t w = 0 ,h = 0;	//	width, height, left, top, right, bottom
		OMX_BYTE pbyStrm = inBuf;
		uint32_t uPreFourByte = (uint32_t)-1;

		do
		{
			if ( pbyStrm >= (inBuf + inSize) )		break;
			uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;

			if ( uPreFourByte == 0x00000001 || uPreFourByte<<8 == 0x00000100 )
			{
				// SPS start code
				if ( (pbyStrm[0] & 0x1F) == 7 )
				{
					pbyStrm = ( uPreFourByte == 0x00000001 ) ? ( pbyStrm - 4 ) : ( pbyStrm - 3 );
					if( avc_get_video_size( pbyStrm, inSize - (pbyStrm - inBuf), &w, &h ) )
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
#else
static int AVCCheckPortReconfiguration( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_BYTE inBuf, OMX_S32 inSize )
{
	if ( (inBuf != NULL) && (inSize > 0) )
	{
		int32_t w = 0 ,h = 0;	//	width, height, left, top, right, bottom
		OMX_BYTE pbyStrm = inBuf;
		uint32_t uPreFourByte = (uint32_t)-1;

		do
		{
			if ( pbyStrm >= (inBuf + inSize) )		break;
			uPreFourByte = (uPreFourByte << 8) + *pbyStrm++;

			if ( uPreFourByte == 0x00000001 || uPreFourByte<<8 == 0x00000100 )
			{
				// SPS start code
				if ( (pbyStrm[0] & 0x1F) == 7 )
				{
					pbyStrm = ( uPreFourByte == 0x00000001 ) ? ( pbyStrm - 4 ) : ( pbyStrm - 3 );
					if( avc_get_video_size( pbyStrm, inSize - (pbyStrm - inBuf), &w, &h ) )
					{
						if( pDecComp->width != w || pDecComp->height != h )
						{
							DbgMsg("New Video Resolution = %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, w, h);

							pDecComp->bPortReconfigure 		= OMX_TRUE;
							pDecComp->PortReconfigureWidth 	= w;
							pDecComp->PortReconfigureHeight = h;

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
#endif

int NX_DecodeAvcFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
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

	//	Get Next Queue Information
	NX_PopQueue( pInQueue, (void**)&pInBuf );
	if( pInBuf == NULL ){
		return 0;
	}

	inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;
	pDecComp->inFrameCount++;

	TRACE("pInBuf->nFlags = 0x%08x, size = %ld\n", (int)pInBuf->nFlags, pInBuf->nFilledLen );


	//	Check End Of Stream
	if( pInBuf->nFlags & OMX_BUFFERFLAG_EOS )
	{
		DbgMsg("=========================> Receive Endof Stream Message (%ld)\n", pInBuf->nFilledLen);

		pDecComp->bStartEoS = OMX_TRUE;
		if( inSize <= 0)
		{
			goto Exit;
		}
	}

	if( (OMX_FALSE == pDecComp->bNeedSequenceData) && (pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) && (OMX_FALSE == pDecComp->bIsFlush)  )
	{
		pDecComp->bNeedSequenceData = OMX_TRUE;
	}

	//	Step 1. Found Sequence Information
	if( OMX_TRUE == pDecComp->bNeedSequenceData && pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
	{
		pDecComp->bNeedSequenceData = OMX_FALSE;
		DbgMsg("Copy Extra Data (%d)\n", inSize );
		AVCCheckPortReconfiguration( pDecComp, inData, inSize );

		if(pDecComp->codecSpecificDataSize > 0)
		{
			memcpy( pDecComp->tmpInputBuffer, pDecComp->codecSpecificData, pDecComp->codecSpecificDataSize );
			if( pDecComp->codecSpecificData )
				free( pDecComp->codecSpecificData );
			pDecComp->codecSpecificData = malloc(inSize + pDecComp->codecSpecificDataSize);
			memcpy( pDecComp->codecSpecificData, pDecComp->tmpInputBuffer, pDecComp->codecSpecificDataSize );
			memcpy( pDecComp->codecSpecificData + pDecComp->codecSpecificDataSize, inData, inSize );
			pDecComp->codecSpecificDataSize += inSize;
		}
		else
		{
			pDecComp->codecSpecificData = malloc(inSize);
			pDecComp->codecSpecificDataSize = inSize;
			memcpy( pDecComp->codecSpecificData, inData, inSize );
		}


			if( ( inSize>4 && inData[0]==0 && inData[1]==0 && inData[2]==0 && inData[3]==1 && ((inData[4]&0x0F)==0x07) ) ||
				( inSize>4 && inData[0]==0 && inData[1]==0 && inData[2]==1 && ((inData[3]&0x0F)==0x07) ) )
			{
				int w,h;	//	width, height, left, top, right, bottom
				if( avc_get_video_size( pDecComp->codecSpecificData, pDecComp->codecSpecificDataSize, &w, &h ) )
				{
					if( pDecComp->width != w || pDecComp->height != h )
					{
						//	Need Port Reconfiguration
						SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventPortSettingsChanged, OMX_DirOutput, 0, NULL );

						// Change Port Format
						pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth = w;
						pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = h;

						//	Native Mode
						if( pDecComp->bUseNativeBuffer )
						{
							pDecComp->pOutputPort->stdPortDef.nBufferSize = 4096;
						}
						else
						{
							pDecComp->pOutputPort->stdPortDef.nBufferSize = ((((w+15)>>4)<<4) * (((h+15)>>4)<<4))*3/2;
						}
						goto Exit;
					}
				}
			}
		goto Exit;
	}

	// {
	// 	OMX_U8 *buf = pInBuf->pBuffer;
	// 	DbgMsg("nFilledLen(%7d),TimeStamp(%lld),Flags(0x%08x), Data: 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x\n",
	// 		pInBuf->nFilledLen, pInBuf->nTimeStamp, pInBuf->nFlags,
	// 		buf[ 0],buf[ 1],buf[ 2],buf[ 3],buf[ 4],buf[ 5],buf[ 6],buf[ 7],
	// 		buf[ 8],buf[ 9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],
	// 		buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23] );
	// }

	//	Push Input Time Stamp
	if( !(pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) )
		PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );

	//	Step 2. Find First Key Frame & Do Initialize VPU
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		int initBufSize;
		unsigned char *initBuf;

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

		if( OMX_TRUE == pDecComp->bNeedSequenceData )
		{
			if( AVCCheckPortReconfiguration( pDecComp, initBuf, initBufSize ) )
			{
				pDecComp->bNeedSequenceData = OMX_FALSE;
				if( pDecComp->codecSpecificData )
					free( pDecComp->codecSpecificData );
				pDecComp->codecSpecificData = malloc(initBufSize);
				memcpy( pDecComp->codecSpecificData, initBuf, initBufSize );
				pDecComp->codecSpecificDataSize = initBufSize;
				goto Exit;
			}
			else
			{
				pDecComp->bNeedSequenceData = OMX_FALSE;
				if( pDecComp->codecSpecificData )
					free( pDecComp->codecSpecificData );
				pDecComp->codecSpecificData = malloc(initBufSize);
				memcpy( pDecComp->codecSpecificData, initBuf, initBufSize );
				pDecComp->codecSpecificDataSize = initBufSize;
				goto Exit;
			}
		}

		//	Initialize VPU
		ret = InitializeCodaVpu(pDecComp, initBuf, initBufSize );

		if(pDecComp->bPortReconfigure)
		{
			int32_t width = 0, height = 0;
			//	Need Port Reconfiguration
			SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventPortSettingsChanged, OMX_DirOutput, 0, NULL );

			width = pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth;
			height = pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight;

			// Change Port Format
			pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth = width;
			pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = height;

			if(pDecComp->codecSpecificDataSize)
			{
				free(pDecComp->codecSpecificData);
				pDecComp->codecSpecificDataSize = 0;
			}

			pDecComp->codecSpecificData = (unsigned char *)malloc( initBufSize );
			pDecComp->codecSpecificDataSize = initBufSize;
			memcpy(pDecComp->codecSpecificData, initBuf, pDecComp->codecSpecificDataSize);
			free( initBuf );

			//	Native Mode
			if( pDecComp->bUseNativeBuffer )
			{
				pDecComp->pOutputPort->stdPortDef.nBufferSize = 4096;
			}
			else
			{
				pDecComp->pOutputPort->stdPortDef.nBufferSize = ((((width+15)>>4)<<4) * (((height+15)>>4)<<4))*3/2;
			}

			if( OMX_TRUE == pDecComp->bInitialized )
			{
				pDecComp->bInitialized = OMX_FALSE;
				InitVideoTimeStamp(pDecComp);
				closeVideoCodec(pDecComp);
				openVideoCodec(pDecComp);
			}
			pDecComp->pOutputPort->stdPortDef.bEnabled = OMX_FALSE;
			goto Exit;
		}

		free( initBuf );

		if( VID_ERR_INIT == ret )
		{
			ErrMsg("VPU initialized Failed!!!!\n");
			goto Exit;
		}

		pDecComp->bNeedKey = OMX_FALSE;
		pDecComp->bInitialized = OMX_TRUE;

		ret = 0;
		decOut.dispIdx = -1;
	}
	else
	{
		if( AVCCheckPortReconfiguration( pDecComp, inData, inSize ) )
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
	TRACE("Output Buffer : ColorFormat(0x%08x), NatvieBuffer(%d), Thumbnail(%d), MetaDataInBuffer(%d), ret(%d)\n",
		pDecComp->outputFormat.eColorFormat, pDecComp->bUseNativeBuffer, pDecComp->bEnableThumbNailMode, pDecComp->bMetaDataInBuffers, ret );

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
			pOutBuf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
			TRACE("ThumbNail Mode : pOutBuf->nAllocLen = %ld, pOutBuf->nFilledLen = %ld, 0x%08x\n",
					pOutBuf->nAllocLen, pOutBuf->nFilledLen, (int32_t)pOutBuf->nFlags );
			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
			goto Exit;
		}
		else
		{
			int32_t OutIdx = 0;
			if( (OMX_FALSE == pDecComp->bInterlaced) && (OMX_FALSE == pDecComp->bOutBufCopy) && (pDecComp->bUseNativeBuffer == OMX_TRUE) )
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
				ErrMsg("Unexpected Buffer Handling!!!! Goto Exit(%ld,%d)\n", pDecComp->curOutBuffers, decOut.dispIdx);
				goto Exit;
			}
			else if (pDecComp->bUseNativeBuffer == OMX_FALSE)	 //use in cts
			{
				NX_VID_MEMORY_INFO *pImg = &decOut.hImg;
				CopySurfaceToBufferYV12( (OMX_U8 *)pImg->pBuffer[0], pOutBuf->pBuffer, pDecComp->width, pDecComp->height );
				pOutBuf->nFilledLen = pDecComp->width * pDecComp->height * 3 / 2;
				NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
			}
			else
			{
				DbgBuffer("curOutBuffers(%ld),idx(%d)\n", pDecComp->curOutBuffers, decOut.dispIdx);
				pOutBuf->nFilledLen = sizeof(struct private_handle_t);
			}
			pDecComp->outBufferValidFlag[OutIdx] = 1;
			pDecComp->outBufferUseFlag[OutIdx] = 0;
			pDecComp->curOutBuffers --;
			
			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
				pOutBuf->nFlags     = pInBuf->nFlags;
			}
			pOutBuf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
			TRACE("Native Mode : pOutBuf->nTimeStamp = %lld\n", pOutBuf->nTimeStamp/1000);

			if( OMX_TRUE == pDecComp->bInterlaced )
			{
				DeInterlaceFrame( pDecComp, &decOut );
			}else if( OMX_TRUE == pDecComp->bOutBufCopy )
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
