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

static int MakeVP8DecoderSpecificInfo( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp )
{
	int retSize = 0;
	OMX_U8 *pbHeader = pDecComp->tmpInputBuffer;

	PUT_LE32(pbHeader, MKTAG('D', 'K', 'I', 'F'));		//signature 'DKIF'
	PUT_LE16(pbHeader, 0x00);							//version
	PUT_LE16(pbHeader, 0x20);							//length of header in bytes
	PUT_LE32(pbHeader, MKTAG('V', 'P', '8', '0'));		//codec FourCC
	PUT_LE16(pbHeader, pDecComp->width);				//width
	PUT_LE16(pbHeader, pDecComp->height);				//height
	PUT_LE32(pbHeader, 0);								//frame rate
	PUT_LE32(pbHeader, 0);								//time scale(?)
	PUT_LE32(pbHeader, 1);								//number of frames in file
	PUT_LE32(pbHeader, 0);								//unused
	retSize += 32;
	return 	retSize;
}

static int MakeVP8Stream( OMX_U8 *pIn, OMX_S32 inSize, OMX_U8 *pOut )
{
	PUT_LE32(pOut, inSize);								//frame_chunk_len
	PUT_LE32(pOut, 0);									//time stamp
	PUT_LE32(pOut, 0);
	memcpy( pOut, pIn, inSize );
	return ( inSize + 12 );
}

int NX_DecodeVP8Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
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
	TRACE("pInBuf->nTimeStamp = %lld, pInBuf->nFilledLen = %d\n", pInBuf->nTimeStamp/1000, pInBuf->nFilledLen);
	PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );

	//	Step 2. Find First Key Frame & Do Initialize VPU
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		int initBufSize = 0;
		unsigned char *initBuf = NULL;
		OMX_S32 size = 0;

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

		size = MakeVP8DecoderSpecificInfo( pDecComp );
		size += MakeVP8Stream( initBuf, initBufSize, pDecComp->tmpInputBuffer + size );
		//	Initialize VPU
		ret = InitializeCodaVpu(pDecComp, pDecComp->tmpInputBuffer, size );
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
		decOut.outImgIdx = -1;
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

		inSize = MakeVP8Stream( inData, inSize, pDecComp->tmpInputBuffer );
		decIn.strmBuf = pDecComp->tmpInputBuffer;
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
#if 0
			if( pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DECODED_FRAME] != PIC_TYPE_I )
			{
				NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
				goto Exit;
			}

#endif
			pDecComp->isOutIdr = OMX_TRUE;

			//	Native Window Buffer Mode
			//	Get Output Buffer Pointer From Output Buffer Pool
			pOutBuf = pDecComp->pOutputBuffers[decOut.outImgIdx];

			if( pDecComp->outBufferUseFlag[decOut.outImgIdx] == 0 )
			{
				OMX_TICKS timestamp;
				OMX_U32 flag;
				PopVideoTimeStamp(pDecComp, &timestamp, &flag );
				NX_VidDecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.outImgIdx );
				ErrMsg("Unexpected Buffer Handling!!!! Goto Exit\n");
				goto Exit;
			}
			pDecComp->outBufferValidFlag[decOut.outImgIdx] = 1;
			pDecComp->outBufferUseFlag[decOut.outImgIdx] = 0;
			pDecComp->curOutBuffers --;

			pOutBuf->nFilledLen = sizeof(struct private_handle_t);
			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
				pOutBuf->nFlags     = pInBuf->nFlags;
			}
			TRACE("pOutBuf->nTimeStamp = %lld\n", pOutBuf->nTimeStamp/1000);
			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		}
	}

Exit:
	pInBuf->nFilledLen = 0;
	pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);

	return ret;
}
