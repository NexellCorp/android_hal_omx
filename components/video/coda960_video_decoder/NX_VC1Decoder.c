#define	LOG_TAG				"NX_VC1DEC"
#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"

static int32_t MakeVC1DecodeSpecificInfo( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp )
{
	int retSize=0;
	OMX_U8 *pbHeader = pDecComp->codecSpecificData;
	OMX_U8 *pbMetaData = pDecComp->pExtraData;
	OMX_S32 nMetaData = pDecComp->nExtraDataSize;
	if( pDecComp->codecType.wmvType.eFormat == OMX_VIDEO_WMVFormat8 || pDecComp->codecType.wmvType.eFormat == OMX_VIDEO_WMVFormat9 || pDecComp->bXMSWMVType )
	{
		DbgMsg("MakeVC1DecodeSpecificInfo() WMV Mode.(%ldx%ld)\n", pDecComp->width, pDecComp->height);
		PUT_LE32(pbHeader, ((0xC5 << 24)|0));
		retSize += 4; //version
		PUT_LE32(pbHeader, nMetaData);
		retSize += 4;

		memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
		retSize += nMetaData;

		PUT_LE32(pbHeader, pDecComp->height);
		retSize += 4;
		PUT_LE32(pbHeader, pDecComp->width);
		retSize += 4;
		PUT_LE32(pbHeader, 12);
		retSize += 4;
		PUT_LE32(pbHeader, 2 << 29 | 1 << 28 | 0x80 << 24 | 1 << 0);
		retSize += 4; // STRUCT_B_FRIST (LEVEL:3|CBR:1:RESERVE:4:HRD_BUFFER|24)
		PUT_LE32(pbHeader, 0);
		retSize += 4; // hrd_rate
		PUT_LE32(pbHeader, 0);
		retSize += 4; // frameRate
		pDecComp->codecSpecificDataSize = retSize;
	}
	else	//	VC1
	{
		DbgMsg("MakeVC1DecodeSpecificInfo() VC1 Mode.\n");
		memcpy(pDecComp->codecSpecificData, pbMetaData, nMetaData); //OpaqueDatata
		pDecComp->codecSpecificDataSize = nMetaData;
	}

	return pDecComp->codecSpecificDataSize;
}

static int MakeVC1PacketData( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_S32 format, OMX_U8 *pIn, OMX_S32 inSize, OMX_U8 *pOut, OMX_S32 key, OMX_S32 time )
{
	OMX_S32 size=0;
	OMX_U8 *p = pIn;
	UNUSED_PARAM(time);
	if( format == OMX_VIDEO_WMVFormat8 || format == OMX_VIDEO_WMVFormat9 || pDecComp->bXMSWMVType )
	{
		PUT_LE32( pOut, (inSize | ((key)?0x80000000:0)) );
		size += 4;
		PUT_LE32(pOut, 0);
		size += 4;
		memcpy(pOut, pIn, inSize);
		size += inSize;
	}
	else	//	VC1
	{
		if (p[0] != 0 || p[1] != 0 || p[2] != 1) // check start code as prefix (0x00, 0x00, 0x01)
		{
			*pOut++ = 0x00;
			*pOut++ = 0x00;
			*pOut++ = 0x01;
			*pOut++ = 0x0D;
			size = 4;
			memcpy(pOut, pIn, inSize);
			size += inSize;
		}
		else
		{
			if ( p[3] != 0x0F )
			{
				memcpy(pOut, pIn, inSize);
				size = inSize; // no extra header size, there is start code in input stream.
			}
			else
			{
				int32_t i = 0, iReInitFlg = 0;
				OMX_U8  *pCfg = pDecComp->codecSpecificData;

				for (i=0 ; i<pDecComp->codecSpecificDataSize ; i++)
				{
					if ( *p++ != *pCfg++ )	break;
				}

				if ( i < pDecComp->codecSpecificDataSize )
				{
					// int32_t iRead1, iRead2;
					pCfg = pDecComp->codecSpecificData;
					p = pIn;

					// Sequence Layer
					if ( (pCfg[0] & 0xC6) != (p[0] & 0xC6) )		// Profile & ColorDiff_Fmt Check
						iReInitFlg = 1;
					else if( (pCfg[1] & 1) != (p[1] & 1) )			// PostProcFlg Check
						iReInitFlg = 1;
					else if( (pCfg[2] != p[2]) || (pCfg[3] != p[3]) || (pCfg[4] != p[4]) )		// Max_Codec_Size Check
						iReInitFlg = 1;
					else if ( (pCfg[5] & 0xF4) != (p[5] & 0xF4) )		// PullDown, Interlace, TFCNTRFlg, FINTERPFlg, PSF
						iReInitFlg = 1;
					else
					{
						int32_t iEntryFlg = 0;
						uint32_t uPreFourByte = (uint32_t)-1;

						// Search Entry Layer
						do
						{
							if ( (pCfg >= pDecComp->codecSpecificData + pDecComp->codecSpecificDataSize) || (uPreFourByte == 0x0000010D) )	break;
							if ( uPreFourByte == 0x0000010E )
							{
								iEntryFlg = 1;
								break;
							}
							uPreFourByte = (uPreFourByte << 8) + (*pCfg++);
						} while(1);

						uPreFourByte = (uint32_t)-1;
						do
						{
							if ( (p >= pIn + inSize) || (uPreFourByte == 0x0000010D) )
							{
								iEntryFlg = 0;
								break;
							}
							if ( uPreFourByte == 0x0000010E )
							{
								if ( iEntryFlg == 0 )
								{
									iReInitFlg = 1;
								}
								break;
							}
							uPreFourByte = (uPreFourByte << 8) + (*p++);
						} while(1);

						if ( (iEntryFlg == 1) && (iReInitFlg == 0) )
						{
							if ( (pCfg[0] & 0x3F) != (p[0] & 0x3F) )
								iReInitFlg = 1;
							else if ( (pCfg[1] & 0xF8) != (p[1] & 0xF8) )
								iReInitFlg = 1;
						}
					}
				}

				if ( iReInitFlg == 0 )
				{
					size = inSize - pDecComp->codecSpecificDataSize;
					memcpy(pOut, pIn + pDecComp->codecSpecificDataSize, size);
				}
				else
				{
					TRACE("Sequence header is Changed. And VC1 shall be reinitialized!!! \n");
					return -1;
				}
			}
		}
	}
	return size;
}

int NX_DecodeVC1Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	OMX_S32 inSize = 0, key;
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

	NX_PopQueue( pInQueue, (void**)&pInBuf );
	if( pInBuf == NULL ){
		return 0;
	}

	inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;
	key = pInBuf->nFlags & OMX_BUFFERFLAG_SYNCFRAME;
	pDecComp->inFrameCount ++;

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

	if( OMX_FALSE == pDecComp->bInitialized )
	{
		if( pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
		{
			if( pDecComp->nExtraDataSize <= 0 )
			{
				pDecComp->nExtraDataSize = inSize;
			}
			if( pDecComp->pExtraData != NULL )
			{
				free(pDecComp->pExtraData);
			}
			pDecComp->pExtraData = malloc(pDecComp->nExtraDataSize);
			memcpy(pDecComp->pExtraData, inData, inSize);
			DbgMsg("Copy Extra Data (%ld)\n", inSize );

			{
				OMX_U8 *buf = pDecComp->pExtraData;
				DbgMsg("DumpData (%6ld) : 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x\n",
					inSize,
					buf[ 0],buf[ 1],buf[ 2],buf[ 3],buf[ 4],buf[ 5],buf[ 6],buf[ 7],
					buf[ 8],buf[ 9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],
					buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23] );
			}

			goto Exit;
		}
	}

	//	Push Input Time Stamp
	PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );

	if( OMX_FALSE == pDecComp->bInitialized )
	{
		int size;
		if( pDecComp->codecSpecificData )
		{
			free(pDecComp->codecSpecificData);
			pDecComp->codecSpecificData = NULL;
		}
		pDecComp->codecSpecificData = malloc(pDecComp->nExtraDataSize + 128);
		size = MakeVC1DecodeSpecificInfo( pDecComp );
		memcpy( pDecComp->tmpInputBuffer, pDecComp->codecSpecificData, size );
		size += MakeVC1PacketData( pDecComp, pDecComp->codecType.wmvType.eFormat, inData, inSize, pDecComp->tmpInputBuffer+size, key, (int)(pInBuf->nTimeStamp/1000ll) );

		//	Initialize VPU
		{
			OMX_U8 *buf = pDecComp->tmpInputBuffer;
			DbgMsg("DumpData (%6d) : 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x\n",
				size,
				buf[ 0],buf[ 1],buf[ 2],buf[ 3],buf[ 4],buf[ 5],buf[ 6],buf[ 7],
				buf[ 8],buf[ 9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],
				buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23] );
		}
		ret = InitializeCodaVpu(pDecComp, pDecComp->tmpInputBuffer, size );
		if( 0 > ret )
		{
			ErrMsg("VPU initialized Failed!!!!\n");
			goto Exit;
		}
		else if( ret > 0 )
		{
			ret = 0;
			goto Exit;
		}

		pDecComp->bNeedKey = OMX_FALSE;
		pDecComp->bInitialized = OMX_TRUE;
		//decIn.strmBuf = pDecComp->tmpInputBuffer;
		//decIn.strmSize = 0;
		//decIn.timeStamp = pInBuf->nTimeStamp;
		//decIn.eos = 0;
		//ret = NX_V4l2DecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
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
			inSize = MakeVC1PacketData( pDecComp, pDecComp->codecType.wmvType.eFormat, inData, inSize, pDecComp->tmpInputBuffer + pDecComp->tmpInputBufferIndex,  key, (int)(pInBuf->nTimeStamp/1000ll) );
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
			inSize = MakeVC1PacketData( pDecComp, pDecComp->codecType.wmvType.eFormat, inData, inSize, pDecComp->tmpInputBuffer, key, (int)(pInBuf->nTimeStamp/1000ll) );
			inData = pDecComp->tmpInputBuffer;
		}

		decIn.strmBuf = inData;
		decIn.strmSize = inSize;
		decIn.timeStamp = pInBuf->nTimeStamp;
		decIn.eos = 0;

#if 1	// add hcjun 2018_01_04
		// only vc1 check skip frame.
		if ( (pDecComp->codecType.wmvType.eFormat != OMX_VIDEO_WMVFormat8 && pDecComp->codecType.wmvType.eFormat != OMX_VIDEO_WMVFormat9 && !pDecComp->bXMSWMVType) &&
			 (inData[0] == 0 && inData[1] == 0 && inData[2] == 1) ) // vc1 check start code as prefix (0x00, 0x00, 0x01)
		{
			int32_t piFrameType = -1;
			NX_V4L2DEC_IN decInTmp;

			memset(&decInTmp,  0, sizeof(NX_V4L2DEC_IN));

			decInTmp.strmBuf = inData + 4;
			decInTmp.strmSize = inSize - 4;
			decInTmp.timeStamp = pInBuf->nTimeStamp;
			decInTmp.eos = 0;

			ret = NX_DecGetFrameType(pDecComp->hVpuCodec, &decInTmp, pDecComp->videoCodecId, &piFrameType);
			if( (ret == 0) && (piFrameType == PIC_TYPE_SKIP) )
			{
				OMX_TICKS timestamp;
				OMX_U32 flag;
				PopVideoTimeStamp(pDecComp, &timestamp, &flag );
				goto Exit;
			}
		}
		ret = NX_V4l2DecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
#else
		ret = NX_V4l2DecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
#endif
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
			int32_t OutIdx = 0;
			if( (OMX_FALSE == pDecComp->bInterlaced) && (OMX_FALSE == pDecComp->bOutBufCopy) )
			{
				OutIdx = decOut.dispIdx;
			}
			else // OMX_TRUE == pDecComp->bInterlaced, pDecComp->bOutBufCopy
			{
				OutIdx = GetUsableBufferIdx(pDecComp);
			}


			if( pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DISPLAY_FRAME] != PIC_TYPE_I &&
				pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DISPLAY_FRAME] != PIC_TYPE_VC1_BI )
			{
				OMX_TICKS timestamp;
				OMX_U32 flag;
				PopVideoTimeStamp(pDecComp, &timestamp, &flag );
				NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
				goto Exit;
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
