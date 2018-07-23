#define	LOG_TAG				"NX_AVCDEC"

#include <utils/Log.h>

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"

//	From NX_AVCUtil
int avc_get_video_size(unsigned char *buf, int buf_size, int *width, int *height, int *coded_width, int *coded_height);

static int AVCCheckPortReconfiguration( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_BYTE inBuf, OMX_S32 inSize )
{
	if ( (inBuf != NULL) && (inSize > 0) )
	{
		int32_t width = 0, height = 0;	//	width, height, left, top, right, bottom
		OMX_BYTE pbyStrm = inBuf;
		uint32_t uPreFourByte = (uint32_t)-1;

		int coded_height = 0, coded_width = 0;

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
					if( avc_get_video_size( pbyStrm, inSize - (pbyStrm - inBuf), &width, &height, &coded_width, &coded_height ) )
					{
						if( (pDecComp->width == width && pDecComp->height == height) && (OMX_TRUE == pDecComp->bEnableThumbNailMode) )
						{
							break;
						}
						else if( pDecComp->width != width || pDecComp->height != height ||
							( (coded_width > 0 && coded_height > 0) && (width != coded_width || height != coded_height) ) )
						{
							if( (pDecComp->width != width || pDecComp->height != height) && (width == coded_width && height == coded_height) )
							{
								//	Change Port Format & Resolution Information
								DbgMsg("New Video Resolution = %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, width, height);
								pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth  = pDecComp->width  = width;
								pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = pDecComp->height = height;
							}
							else if( (coded_width > 0 && coded_height > 0) && (width != coded_width || height != coded_height) )
							{
								if( pDecComp->width != coded_width || pDecComp->height != coded_height )
								{
									int32_t diff = coded_height - pDecComp->height;
									if(16 > diff) //16 align
									{
										break;
									}

									//	Change Port Format & Resolution Information
									DbgMsg("New Video Resolution. = %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, coded_width, coded_height);
									pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth  = pDecComp->width  = coded_width;
									pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = pDecComp->height = coded_height;
								}
								else
								{
									break;
								}
							}

							pDecComp->dsp_width  = width;
							pDecComp->dsp_height = height;
							pDecComp->coded_width  = coded_width;
							pDecComp->coded_height = coded_height;

							//	Native Mode
							if( pDecComp->bUseNativeBuffer )
							{
								pDecComp->pOutputPort->stdPortDef.nBufferSize = 4096;
							}
							else
							{
								pDecComp->pOutputPort->stdPortDef.nBufferSize = ((((width+15)>>4)<<4) * (((height+15)>>4)<<4))*3/2;
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

int NX_DecodeAvcFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	int inSize = 0;
	OMX_BYTE inData;
	NX_VID_DEC_IN decIn;
	NX_VID_DEC_OUT decOut;
	int ret = 0;

	UNUSED_PARAM(pOutQueue);

	if( pDecComp->bFlush )
	{
		flushVideoCodec( pDecComp );
		pDecComp->bFlush = OMX_FALSE;
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

	//	Step 1. Found Sequence Information
	if( OMX_TRUE == pDecComp->bNeedSequenceData && pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
	{
		if( pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
		{
			int32_t bPortReconfig = 0;
			pDecComp->bNeedSequenceData = OMX_FALSE;
			DbgMsg("Copy Extra Data (%d)\n", inSize );
			bPortReconfig = AVCCheckPortReconfiguration( pDecComp, inData, inSize );
			if( pDecComp->codecSpecificData )
				free( pDecComp->codecSpecificData );
			pDecComp->codecSpecificData = malloc(inSize);
			memcpy( pDecComp->codecSpecificData + pDecComp->codecSpecificDataSize, inData, inSize );
			pDecComp->codecSpecificDataSize += inSize;

			if(!bPortReconfig)
			{
				if( ( inSize>4 && inData[0]==0 && inData[1]==0 && inData[2]==0 && inData[3]==1 && ((inData[4]&0x0F)==0x07) ) ||
					( inSize>4 && inData[0]==0 && inData[1]==0 && inData[2]==1 && ((inData[3]&0x0F)==0x07) ) )
				{
					int width = 0,height = 0,coded_width=0, coded_height=0;	//	width, height, left, top, right, bottom
					if( avc_get_video_size( pDecComp->codecSpecificData, pDecComp->codecSpecificDataSize, &width, &height, &coded_width, &coded_height ) )
					{
						if( (pDecComp->width == width && pDecComp->height == height) && (OMX_TRUE == pDecComp->bEnableThumbNailMode) )
						{
							goto Exit;
						}
						if( pDecComp->width != width || pDecComp->height != height ||
								( (coded_width > 0 && coded_height > 0) && (width != coded_width || height != coded_height) ) )
						{
							if( pDecComp->width != width || pDecComp->height != height )
							{
								//	Change Port Format & Resolution Information
								DbgMsg("New Video Resolution.. = %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, width, height);
								pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth  = width;
								pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = height;
							}
							else if( (coded_width > 0 && coded_height > 0) && (width != coded_width || height != coded_height) )
							{
								if( pDecComp->width != coded_width || pDecComp->height != coded_height )
								{
									int32_t diff = coded_height - pDecComp->height;
									if(16 > diff) //16 align
									{
										goto Exit;
									}

									//	Change Port Format & Resolution Information
									DbgMsg("New Video Resolution...= %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, coded_width, coded_height);
									pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth  = coded_width;
									pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = coded_height;
								}
								else
								{
									goto Exit;
								}
							}

							//	Need Port Reconfiguration
							SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventPortSettingsChanged, OMX_DirOutput, 0, NULL );

							//	Native Mode
							if( pDecComp->bUseNativeBuffer )
							{
								pDecComp->pOutputPort->stdPortDef.nBufferSize = 4096;
							}
							else
							{
								pDecComp->pOutputPort->stdPortDef.nBufferSize = ((((width+15)>>4)<<4) * (((height+15)>>4)<<4))*3/2;
							}
							goto Exit;
						}
					}
				}
			}

			goto Exit;
		}
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

		// Because of CTS
#if 0
		decIn.strmBuf = inData;
		//decIn.strmSize = 0;
		decIn.strmSize = initBufSize;
		decIn.timeStamp = pInBuf->nTimeStamp;
		decIn.eos = 0;
		ret = NX_VidDecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
#else
		ret = 0;
		decOut.outImgIdx = -1;
#endif
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

		decIn.strmBuf = inData;
		decIn.strmSize = inSize;
		decIn.timeStamp = pInBuf->nTimeStamp;
		decIn.eos = 0;
		ret = NX_VidDecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
	}

	TRACE(">>> [%06ld/%06ld] decOut : outImgIdx(%d) decIdx(%d) readPos(%d), writePos(%d) \n",
		pDecComp->inFrameCount, pDecComp->outFrameCount, decOut.outImgIdx, decOut.outDecIdx, decOut.strmReadPos, decOut.strmWritePos );
	TRACE("Output Buffer : ColorFormat(0x%08x), NatvieBuffer(%d), Thumbnail(%d), MetaDataInBuffer(%d), ret(%d)\n",
			pDecComp->outputFormat.eColorFormat, pDecComp->bUseNativeBuffer, pDecComp->bEnableThumbNailMode, pDecComp->bMetaDataInBuffers, ret );

	if( ret==VID_ERR_NONE && decOut.outImgIdx >= 0 && ( decOut.outImgIdx < NX_OMX_MAX_BUF ) )
	{
		if( OMX_TRUE == pDecComp->bEnableThumbNailMode )
		{
			//	Thumbnail Mode
			NX_VID_MEMORY_INFO *pImg = &decOut.outImg;
			NX_PopQueue( pOutQueue, (void**)&pOutBuf );
			CopySurfaceToBufferYV12( (OMX_U8*)pImg->luVirAddr, (OMX_U8*)pImg->cbVirAddr, (OMX_U8*)pImg->crVirAddr,
				pOutBuf->pBuffer, pImg->luStride, pImg->cbStride, pDecComp->width, pDecComp->height );

			NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
			pOutBuf->nFilledLen = pDecComp->width * pDecComp->height * 3 / 2;
			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
				pOutBuf->nFlags     = pInBuf->nFlags;
			}
			pOutBuf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
			TRACE("ThumbNail Mode : pOutBuf->nAllocLen = %ld, pOutBuf->nFilledLen = %ld, 0x%08x\n",
					pOutBuf->nAllocLen, pOutBuf->nFilledLen, pOutBuf->nFlags );
			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
			goto Exit;
		}
		else
		{
			int32_t OutIdx = 0;
			if( (pDecComp->bInterlaced == 0) && (0 == pDecComp->bOutBufCopy) )
			{
				OutIdx = decOut.outImgIdx;
			}
			else if( (pDecComp->bInterlaced != 0) || (1 == pDecComp->bOutBufCopy))
			{
				OutIdx = GetUsableBufferIdx(pDecComp);
			}

			// if( pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DECODED_FRAME] != PIC_TYPE_I )
			// {
			// 	OMX_TICKS timestamp;
			// 	OMX_U32 flag;
			// 	PopVideoTimeStamp(pDecComp, &timestamp, &flag );
			// 	NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
			// 	goto Exit;
			// }
			pDecComp->isOutIdr = OMX_TRUE;

			//	Native Window Buffer Mode
			//	Get Output Buffer Pointer From Output Buffer Pool
			pOutBuf = pDecComp->pOutputBuffers[OutIdx];
			// if( OMX_TRUE == pDecComp->bMetaDataInBuffers )
			// {
			// 	uint32_t *pOutBufType = pDecComp->pOutputBuffers[decOut.outImgIdx];
			// 	*pOutBufType = kMetadataBufferTypeGrallocSource;
			// 	pOutBuf = (OMX_BUFFERHEADERTYPE*)(((unsigned char*)pDecComp->pOutputBuffers[decOut.outImgIdx])+4);
			// 	DbgMsg("~~~~~~~~~~~~~~~~~~~ Outbuffer Data Type ~~~~~~~~~~~~~~~~~~~~~");
			// }

			if( pDecComp->outBufferUseFlag[OutIdx] == 0 )
			{
				OMX_TICKS timestamp;
				OMX_U32 flag;
				PopVideoTimeStamp(pDecComp, &timestamp, &flag );
				NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
				ErrMsg("Unexpected Buffer Handling!!!! Goto Exit(%ld,%d)\n", pDecComp->curOutBuffers, decOut.outImgIdx);
				goto Exit;
			}
			else
			{
				DbgBuffer("curOutBuffers(%ld),idx(%d)\n", pDecComp->curOutBuffers, decOut.outImgIdx);
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
			pOutBuf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
			TRACE("Native Mode : pOutBuf->nTimeStamp = %lld\n", pOutBuf->nTimeStamp/1000);

			if( (pDecComp->bInterlaced == 0) && (1 == pDecComp->bOutBufCopy) )
			{
				OutBufCopy( pDecComp, &decOut );
			}

			DeInterlaceFrame( pDecComp, &decOut );
			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
	}

Exit:
	pInBuf->nFilledLen = 0;
	pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);

	return ret;
}
