#define	LOG_TAG				"NX_XVIDDEC"

#include <assert.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"
#include "NX_AVCUtil.h"

#define VIDOBJ_START_CODE		0x00000100	/* ..0x0000011f  */
#define VIDOBJLAY_START_CODE	0x00000120	/* ..0x0000012f */
#define VISOBJSEQ_START_CODE	0x000001b0
#define VISOBJSEQ_STOP_CODE		0x000001b1	/* ??? */
#define USERDATA_START_CODE		0x000001b2
#define GRPOFVOP_START_CODE		0x000001b3
/*#define VIDSESERR_ERROR_CODE  0x000001b4 */
#define VISOBJ_START_CODE		0x000001b5
#define VOP_START_CODE			0x000001b6
/*#define STUFFING_START_CODE	0x000001c3 */
#define VIDOBJLAY_AR_EXTPAR				15
#define VIDOBJLAY_SHAPE_RECTANGULAR		0
#define VIDOBJLAY_SHAPE_BINARY			1
#define VIDOBJLAY_SHAPE_BINARY_ONLY		2
#define VIDOBJLAY_SHAPE_GRAYSCALE		3

static const uint8_t log2_tab_16[16] =  { 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };

static uint32_t __inline log2bin(uint32_t value)
{
	int n = 0;
	if (value & 0xffff0000)
	{
	   	value >>= 16;
	   	n += 16;
	}
	if (value & 0xff00)
	{
		value >>= 8;
		n += 8;
	}
	if (value & 0xf0)
	{
		value >>= 4;
		n += 4;
	}
	return n + log2_tab_16[value];
}

static int XvidCheckPortReconfiguration( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_BYTE inBuf, OMX_S32 inSize )
{

	if ( (inBuf != NULL) && (inSize > 0) )
	{
		OMX_BYTE pbyStrm = inBuf;
		uint32_t remainSizeBit = inSize * 8;
		GetBitContext gb;

		init_get_bits(&gb, pbyStrm, remainSizeBit);
		pbyStrm++;

		uint32_t start_code = 0;
		uint32_t tmp = 0;
		uint32_t vol_ver_id = 0;
		uint32_t dec_ver_id = 1;
		int32_t aspect_ratio = 0;
		uint32_t time_inc_resolution = 0;
		uint32_t time_inc_bits = 0;
		uint32_t shape = 0;
		uint32_t readMarker = 0;

		do
		{
			if(gb.index > 300)
			{
				return 0;
				// break;
			}
			start_code = (uint32_t)show_bits(&gb, 32);

			if( start_code == VISOBJSEQ_START_CODE)	/* visual_object_sequence_start_code */
			{
				skip_bits(&gb, 32);
				tmp = get_bits(&gb, 8);	/* profile_and_level_indication */
			}
			else if( start_code == VIDOBJ_START_CODE)		//video_object_start_code
			{
				skip_bits(&gb, 32);
				start_code = (uint32_t)show_bits(&gb, 32);	//video_object_layer_start_code
				if( start_code == VIDOBJLAY_START_CODE)
				{
					skip_bits(&gb, 32);
					tmp = get_bits(&gb, 1);	/* random_accessible_vol */
					tmp = get_bits(&gb, 8);   /* video_object_type_indication */

					if (get_bits(&gb, 1))	/* is_object_layer_identifier */
					{
						vol_ver_id = get_bits(&gb, 4);	/* video_object_layer_verid */
						get_bits(&gb, 3);	/* video_object_layer_priority */
					}
					else
					{
						vol_ver_id = dec_ver_id;
					}

					aspect_ratio = get_bits(&gb, 4);
					if (aspect_ratio == VIDOBJLAY_AR_EXTPAR)	/* aspect_ratio_info */
					{
						tmp = get_bits(&gb, 8);	/* par_width */
						tmp = get_bits(&gb, 8);	/* par_height */
					}

					if (get_bits(&gb, 1))	/* vol_control_parameters */
					{
						tmp = get_bits(&gb, 2);	/* chroma_format */
						tmp = get_bits(&gb, 1);	/* low_delay */

						if (get_bits(&gb, 1))	/* vbv_parameters */
						{
							uint32_t bitrate = 0;
							uint32_t buffer_size = 0;
							uint32_t occupancy = 0;

							bitrate = get_bits(&gb,15) << 15;	/* first_half_bit_rate */
							readMarker = get_bits(&gb, 1);		// READ_MARKER
							if(!readMarker)
							{
								break;
							}
							bitrate |= get_bits(&gb,15);		/* latter_half_bit_rate */
							readMarker = get_bits(&gb, 1);				// READ_MARKER
							if(!readMarker)
							{
								break;
							}

							buffer_size = get_bits(&gb, 15) << 3;	/* first_half_vbv_buffer_size */
							readMarker = get_bits(&gb, 1);				// READ_MARKER
							if(!readMarker)
							{
								break;
							}

							buffer_size |= get_bits(&gb, 3);		/* latter_half_vbv_buffer_size */

							occupancy = get_bits(&gb, 11) << 15;	/* first_half_vbv_occupancy */
							readMarker = get_bits(&gb, 1);				// READ_MARKER
							if(!readMarker)
							{
								break;
							}

							occupancy |= get_bits(&gb, 15);	/* latter_half_vbv_occupancy */
							readMarker = get_bits(&gb, 1);				// READ_MARKER
							if(!readMarker)
							{
								break;
							}
						}
					}

					shape = get_bits(&gb, 2);	/* video_object_layer_shape */

					if (shape == VIDOBJLAY_SHAPE_GRAYSCALE && vol_ver_id != 1)
					{
						tmp = get_bits(&gb, 4);	/* video_object_layer_shape_extension */
					}

					readMarker = get_bits(&gb, 1);				// READ_MARKER
					if(!readMarker)
					{
						break;
					}

					time_inc_resolution = get_bits(&gb, 16);	/* vop_time_increment_resolution */

					if (time_inc_resolution > 0)
					{
						tmp = log2bin(time_inc_resolution-1);
						time_inc_bits = ((tmp)<(1)?(tmp):(1));
					}
					else
					{
						/* for "old" xvid compatibility, set time_inc_bits = 1 */
						time_inc_bits = 1;
					}

					readMarker = get_bits(&gb, 1);				// READ_MARKER
					if(!readMarker)
					{
						break;
					}

					if (get_bits(&gb, 1))	/* fixed_vop_rate */
					{
						get_bits(&gb, time_inc_bits);	/* fixed_vop_time_increment */
					}

					if (shape != VIDOBJLAY_SHAPE_BINARY_ONLY)
					{
						if (shape == VIDOBJLAY_SHAPE_RECTANGULAR)
						{
							uint32_t width, height;

							readMarker = get_bits(&gb, 1);				// READ_MARKER
							if(!readMarker)
							{
								break;
							}

							width = get_bits(&gb, 13);	/* video_object_layer_width */
							readMarker = get_bits(&gb, 1);				// READ_MARKER
							if(!readMarker)
							{
								break;
							}

							height = get_bits(&gb, 13);	/* video_object_layer_height */
							readMarker = get_bits(&gb, 1);				// READ_MARKER
							if(!readMarker)
							{
								break;
							}

							if( (uint32_t)pDecComp->width != width || (uint32_t)pDecComp->height != height )
							{
								DbgMsg("New Video Resolution = %ld x %ld --> %d x %d\n", pDecComp->width, pDecComp->height, width, height);

								//	Change Port Format & Resolution Information
								pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth  = pDecComp->width  = width;
								pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight = pDecComp->height = height;
								pDecComp->dsp_width  = width;
								pDecComp->dsp_height = height;

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
								if( (OMX_TRUE == pDecComp->bInitialized) )
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
								return 0;
							}
						}
					} //VIDOBJLAY_START_CODE
				} //VIDOBJ_START_CODE
			}
			else
			{
				skip_bits(&gb, 8);
			}

		}while(1);
	}
	return 0;
}


int NX_DecodeXvidFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
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

	if( (OMX_FALSE == pDecComp->bNeedSequenceData) && (pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) && (OMX_TRUE == pDecComp->bIsFlush)  )
	{
		pDecComp->bNeedSequenceData = OMX_TRUE;

		if(pDecComp->codecSpecificDataSize > 0)
		{
			if( pDecComp->codecSpecificData )
			{
				free( pDecComp->codecSpecificData );
				pDecComp->codecSpecificData = NULL;
				pDecComp->codecSpecificDataSize = 0;
			}
		}
	}

	//	Step 1. Found Sequence Information
	if( OMX_TRUE == pDecComp->bNeedSequenceData && pInBuf->nFlags & OMX_BUFFERFLAG_CODECCONFIG )
	{
		pDecComp->bNeedSequenceData = OMX_FALSE;
		DbgMsg("Copy Extra Data (%d)\n", inSize );
		XvidCheckPortReconfiguration( pDecComp, inData, inSize );

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

		goto Exit;
	}

	// {
	// 	OMX_U8 *buf = pInBuf->pBuffer;
	// 	DbgMsg("pInBuf : Size(%7d) Flag(0x%08x) : 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x, 0x%02x%02x%02x%02x\n", pInBuf->nFilledLen, pInBuf->nFlags,
	// 		buf[ 0],buf[ 1],buf[ 2],buf[ 3],buf[ 4],buf[ 5],buf[ 6],buf[ 7],
	// 		buf[ 8],buf[ 9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15],
	// 		buf[16],buf[17],buf[18],buf[19],buf[20],buf[21],buf[22],buf[23] );
	// }

	//	Push Input Time Stamp
	PushVideoTimeStamp(pDecComp, pInBuf->nTimeStamp, pInBuf->nFlags );

	//	Step 2. Find First Key Frame & Do Initialize VPU
	if( OMX_FALSE == pDecComp->bInitialized )
	{
		int32_t initBufSize = 0;
		if( XvidCheckPortReconfiguration( pDecComp, inData, inSize ) )
		{
			pDecComp->bNeedSequenceData = OMX_FALSE;
			if( pDecComp->codecSpecificData )
				free( pDecComp->codecSpecificData );
			pDecComp->codecSpecificData = malloc(inSize);
			memcpy( pDecComp->codecSpecificData, inData, inSize );
			pDecComp->codecSpecificDataSize = inSize;
			goto Exit;
		}

		initBufSize = inSize + pDecComp->codecSpecificDataSize;
		unsigned char *initBuf = (unsigned char *)malloc( initBufSize );
		memcpy( initBuf, pDecComp->codecSpecificData, pDecComp->codecSpecificDataSize );
		memcpy( initBuf + pDecComp->codecSpecificDataSize, inData, inSize );

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

		ret = 0;
		decOut.dispIdx = -1;
	}
	else
	{
		if( XvidCheckPortReconfiguration( pDecComp, inData, inSize ) )
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
			int32_t OutIdx = 0;
			if( (OMX_FALSE == pDecComp->bInterlaced) && (OMX_FALSE == pDecComp->bOutBufCopy) )
			{
				OutIdx = decOut.dispIdx;
			}
			else // OMX_TRUE == pDecComp->bInterlaced, pDecComp->bOutBufCopy
			{
				OutIdx = GetUsableBufferIdx(pDecComp);
			}


			//if( pDecComp->isOutIdr == OMX_FALSE && decOut.picType[DECODED_FRAME] != PIC_TYPE_I )
			//{
			//	NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
			//	goto Exit;
			//}
			pDecComp->isOutIdr = OMX_TRUE;

			//	Native Window Buffer Mode
			//	Get Output Buffer Pointer From Output Buffer Pool
			pOutBuf = pDecComp->pOutputBuffers[OutIdx];

			if( (pDecComp->outBufferUseFlag[OutIdx] == 0) || (OutIdx < 0) )
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
			TRACE("pOutBuf->nTimeStamp = %lld\n", pOutBuf->nTimeStamp/1000);

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
