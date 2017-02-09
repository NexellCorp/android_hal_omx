#define	LOG_TAG				"NX_MP2DEC"

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"
#include "NX_AVCUtil.h"

static int Mpeg2CheckPortReconfiguration( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_BYTE inBuf, OMX_S32 inSize )
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

			OMX_BYTE data[4];
			data[0] = (OMX_BYTE)get_bits(&gb, 8);
			data[1] = (OMX_BYTE)get_bits(&gb, 8);
			data[2] = (OMX_BYTE)get_bits(&gb, 8);
			data[3] = (OMX_BYTE)get_bits(&gb, 8);
			readSizeBit = readSizeBit + 8*4;

			// SPS start code
			if ( (int32_t)data[0] == 0x00 && (int32_t)data[1] == 0x00 && (int32_t)data[2] == 0x01 && (int32_t)data[3] == 0xb3 )
			{
				{
					w = get_bits(&gb, 12);
					h = get_bits(&gb, 12);
					readSizeBit = readSizeBit + 12*2;
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
int NX_DecodeMpeg2Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	int inSize = 0;
	OMX_BYTE inData;
	NX_VID_DEC_IN decIn;
	NX_VID_DEC_OUT decOut;
	int ret = 0;

	UNUSED_PARAM(pOutQueue);

	memset(&decIn,  0, sizeof(decIn)  );

	if( pDecComp->bFlush )
	{
		flushVideoCodec( pDecComp );
		pDecComp->bFlush = OMX_FALSE;
	}

	NX_PopQueue( pInQueue, (void**)&pInBuf );
	if( pInBuf == NULL ){
		return 0;
	}

	inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;
	pDecComp->inFrameCount++;

	TRACE("[%6ld]pInBuf->nFlags = 0x%08x\n", pDecComp->inFrameCount, (int)pInBuf->nFlags );

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

	//	Step 1. Found Sequence Information
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		if( pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
		{
			DbgMsg("Copy Extra Data (%d)\n", inSize );
			Mpeg2CheckPortReconfiguration( pDecComp, inData, inSize );
			if( pDecComp->codecSpecificData )
				free(pDecComp->codecSpecificData);
			pDecComp->codecSpecificData = malloc(inSize);
			memcpy( pDecComp->codecSpecificData, inData, inSize );
			pDecComp->codecSpecificDataSize = inSize;
			goto Exit;
		}
	}

	//{
	//	OMX_U8 *buf = pInBuf->pBuffer;
	//	DbgMsg("pInBuf : Size(%7d) Flag(0x%08x) : 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x\n", pInBuf->nFilledLen, pInBuf->nFlags,
	//		buf[ 0],buf[ 1],buf[ 2],buf[ 3],buf[ 4],buf[ 5],buf[ 6],buf[ 7],
	//		buf[ 8],buf[ 9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],
	//		buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23] );
	//}

	//	Push Input Time Stamp
	TRACE("pInBuf->nTimeStamp = %lld, pInBuf->nFilledLen = %d\n", pInBuf->nTimeStamp/1000, pInBuf->nFilledLen);
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
			if( Mpeg2CheckPortReconfiguration( pDecComp, initBuf, initBufSize ) )
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

		//decIn.strmBuf = inData;
		//decIn.strmSize = 0;
		//decIn.timeStamp = pInBuf->nTimeStamp;
		//decIn.eos = 0;
		//ret = NX_VidDecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
		ret = 0;
		decOut.outImgIdx = -1;
	}
	else
	{
		if( Mpeg2CheckPortReconfiguration( pDecComp, inData, inSize ) )
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
	TRACE("decOut : readPos = %d, writePos = %d\n", decOut.strmReadPos, decOut.strmWritePos );

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
			TRACE("ThumbNail Mode : pOutBuf->nAllocLen = %ld, pOutBuf->nFilledLen = %ld\n", pOutBuf->nAllocLen, pOutBuf->nFilledLen );
			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
		else
		{
			int32_t OutIdx = ( pDecComp->bInterlaced == 0 ) ? ( decOut.outImgIdx ) : ( GetUsableBufferIdx(pDecComp) );

			//if( pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DECODED_FRAME] != PIC_TYPE_I )
			//{
			//	NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
			//	goto Exit;
			//}
			pDecComp->isOutIdr = OMX_TRUE;

			//	Native Window Buffer Mode
			//	Get Output Buffer Pointer From Output Buffer Pool
			pOutBuf = pDecComp->pOutputBuffers[OutIdx];

			if( pDecComp->outBufferUseFlag[OutIdx] == 0 )
			{
				OMX_TICKS timestamp;
				OMX_U32 flag;
				PopVideoTimeStamp(pDecComp, &timestamp, &flag );
				NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
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
