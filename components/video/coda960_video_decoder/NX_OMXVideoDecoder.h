#ifndef __NX_OMXVideoDecoder_h__
#define __NX_OMXVideoDecoder_h__

#ifdef NX_DYNAMIC_COMPONENTS
//	This Function need for dynamic registration
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent);
#else
//	static registration
OMX_ERRORTYPE NX_VideoDecoder_ComponentInit (OMX_HANDLETYPE hComponent);
#endif

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Video.h>
#include <NX_OMXBasePort.h>
#include <NX_OMXBaseComponent.h>
#include <NX_OMXSemaphore.h>
#include <NX_OMXQueue.h>

#include <hardware/gralloc.h>
#include <cutils/native_handle.h>
#include <gralloc_priv.h>
#include <media/hardware/MetadataBufferType.h>
#include <sys/mman.h>

#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>
#include <linux/media-bus-format.h>

#include <nx_video_api.h>
#include <nx-scaler.h>

enum {
	//  Decoders
	NX_AVC_DEC      = V4L2_PIX_FMT_H264,         // H.264( AVC )
	NX_WVC1_DEC     = V4L2_PIX_FMT_WVC1,         // WVC1
	NX_WMV9_DEC     = V4L2_PIX_FMT_WMV9,         // WMV9
	NX_MP2_DEC      = V4L2_PIX_FMT_MPEG2,        // Mpeg2 Video
	NX_MP4_DEC      = V4L2_PIX_FMT_MPEG4,        // Mpeg4 Video
	NX_H263_DEC     = V4L2_PIX_FMT_H263,         // H.263
	NX_DIV3_DEC     = V4L2_PIX_FMT_DIV3,         // Divx 3.11(MS Mpeg4 V3)
	NX_DIV4_DEC     = V4L2_PIX_FMT_DIV4,
	NX_DIV5_DEC     = V4L2_PIX_FMT_DIV5,
	NX_DIV6_DEC     = V4L2_PIX_FMT_DIV6,
	NX_DIVX_DEC     = V4L2_PIX_FMT_DIVX,
	NX_XVID_DEC     = V4L2_PIX_FMT_XVID,
	NX_RV8_DEC      = V4L2_PIX_FMT_RV8,         // Real Video
	NX_RV9_DEC      = V4L2_PIX_FMT_RV9,
	NX_FLV_DEC      = V4L2_PIX_FMT_FLV1,        // Flv Video
	NX_VP8_DEC      = V4L2_PIX_FMT_VP8,         // VP8
};

enum {
	IN_PORT			= 0,
	OUT_PORT		= 1
};


#define FFDEC_VID_VER_MAJOR			0
#define FFDEC_VID_VER_MINOR			1
#define FFDEC_VID_VER_REVISION		0
#define FFDEC_VID_VER_NSTEP			0

#define	VPUDEC_VID_NUM_PORT			2
#define	VPUDEC_VID_INPORT_INDEX		0
#define	VPUDEC_VID_OUTPORT_INDEX	1

#define	VID_INPORT_MIN_BUF_CNT		6					//	Max 6 Avaliable
#define	VID_INPORT_MIN_BUF_SIZE		(1024*1024*4)		//	32 Mbps( 32Mbps, 1 fps )

//	Default Native Buffer Mode's buffers & buffer size
#define	VID_OUTPORT_MIN_BUF_CNT_THUMB	4
#define	VID_OUTPORT_MIN_BUF_CNT			12				//	Max Avaiable Frames

#define	VID_OUTPORT_MIN_BUF_CNT_H264_UNDER720P	22			//	~720p
#define	VID_OUTPORT_MIN_BUF_CNT_H264_1080P		12			//	1080p

#define	VID_OUTPORT_MIN_BUF_CNT_INTERLACE		6			// hardware deinterlace

#define	VID_OUTPORT_MIN_BUF_SIZE	(4*1024)			//	Video Memory Structure Size

#define	VID_TEMP_IN_BUF_SIZE		(4*1024*1024)

#define FLUSH_FRAME_COUNT			2 		//Number of input frames after flush

#ifndef UNUSED_PARAM
#define	UNUSED_PARAM(X)		X=X
#endif

#ifndef ALIGN
#define  ALIGN(X,N) ( (X+N-1) & (~(N-1)) )
#endif

//
//	DEBUG FLAGS
//
#define	DEBUG_ANDROID	1
#define	DEBUG_BUFFER	0
#define	DEBUG_FUNC		0
#define	TRACE_ON		0
#define	DEBUG_FLUSH		0
#define	DEBUG_STATE		0
#define	DEBUG_PARAM		0

#if DEBUG_BUFFER
#define	DbgBuffer(fmt,...)	DbgMsg(fmt, ##__VA_ARGS__)
#else
#define DbgBuffer(fmt,...)	do{}while(0)
#endif

#if	TRACE_ON
#define	TRACE(fmt,...)		DbgMsg(fmt, ##__VA_ARGS__)
#else
#define	TRACE(fmt,...)		do{}while(0)
#endif

#if	DEBUG_FUNC
#define	FUNC_IN				DbgMsg("%s() In\n", __FUNCTION__)
#define	FUNC_OUT			DbgMsg("%s() OUT\n", __FUNCTION__)
#else
#define	FUNC_IN				do{}while(0)
#define	FUNC_OUT			do{}while(0)
#endif

#if DEBUG_STATE
#define	DBG_STATE(fmt,...)		DbgMsg(fmt, ##__VA_ARGS__)
#else
#define	DBG_STATE(fmt,...)		do{}while(0)
#endif

#if DEBUG_FLUSH
#define	DBG_FLUSH(fmt,...)		DbgMsg(fmt, ##__VA_ARGS__)
#else
#define	DBG_FLUSH(fmt,...)		do{}while(0)
#endif

#if DEBUG_PARAM
#define	DBG_PARAM(fmt,...)		DbgMsg(fmt, ##__VA_ARGS__)
#else
#define	DBG_PARAM(fmt,...)		do{}while(0)
#endif

#define VID_ERR_NONE		0
#define	VID_ERR_INIT		-1

struct OutBufferTimeInfo{
	OMX_TICKS			timestamp;
	OMX_U32				flag;
};

typedef struct tNX_VIDDEC_VIDEO_COMP_TYPE NX_VIDDEC_VIDEO_COMP_TYPE;

//	Define Transform Template Component Type
struct tNX_VIDDEC_VIDEO_COMP_TYPE{
	NX_BASECOMPONENTTYPE		//	Nexell Base Component Type
	/*					Buffer Thread							*/
	pthread_t					hBufThread;
	pthread_mutex_t				hBufMutex;
	NX_THREAD_CMD				eCmdBufThread;

	/*				Video Format				*/
	OMX_VIDEO_PARAM_PORTFORMATTYPE	inputFormat;
	OMX_VIDEO_PARAM_PORTFORMATTYPE	outputFormat;

	// Management Output Buffer
	OMX_S32						outBufferUseFlag[NX_OMX_MAX_BUF];	//	Output Buffer Use Flag( Flag for Decoding )
	OMX_S32						outBufferValidFlag[NX_OMX_MAX_BUF];	//	Valid Buffer Flag
	OMX_S32						outUsableBuffers;					//	Max Allocated Buffers or Max Usable Buffers
	OMX_S32						outUsableBufferIdx;
	OMX_S32						curOutBuffers;						//	Currently Queued Buffer Counter
	OMX_S32						minRequiredFrameBuffer;				//	Minimum H/W Required FrameBuffer( Sequence Output )
	OMX_S32						outBufferable;						//	Display Buffers
	struct OutBufferTimeInfo	outTimeStamp[NX_OMX_MAX_BUF];		//	Output Timestamp

	OMX_U32						outBufferAllocSize;					//	Native Buffer Mode vs ThumbnailMode
	OMX_U32						numOutBuffers;						//

	OMX_BOOL					isOutIdr;

	union {
		OMX_VIDEO_PARAM_AVCTYPE avcType;
		OMX_VIDEO_PARAM_MPEG2TYPE mpeg2Type;
		OMX_VIDEO_PARAM_MPEG4TYPE mpeg4Type;
		OMX_VIDEO_PARAM_H263TYPE h263Type;
		OMX_VIDEO_PARAM_WMVTYPE wmvType;
	} codecType;

	//	for decoding
	OMX_BOOL					bFlush;
	NX_V4L2DEC_HANDLE			hVpuCodec;
	OMX_S32						videoCodecId;
	OMX_BOOL					bInitialized;
	OMX_BOOL					bNeedKey;
	OMX_BOOL					bStartEoS;
	OMX_BOOL					frameDelay;
	OMX_U16						rvFrameCnt;

	int							(*DecodeFrame)(NX_VIDDEC_VIDEO_COMP_TYPE *, NX_QUEUE *, NX_QUEUE *);

	//	Decoder Temporary Buffer
	OMX_U8						tmpInputBuffer[VID_TEMP_IN_BUF_SIZE];
	OMX_S32						tmpInputBufferIndex;

	OMX_S32						width, height;
	OMX_S32						dsp_width, dsp_height;

	OMX_BOOL					bUseNativeBuffer;
	OMX_BOOL					bEnableThumbNailMode;
	OMX_BOOL					bMetaDataInBuffers;

	//	Extra Informations &
	OMX_U8						*codecSpecificData;
	OMX_S32						codecSpecificDataSize;

	//	FFMPEG Parser Data
	OMX_U8						*pExtraData;
	OMX_S32						nExtraDataSize;
	OMX_S32						codecTag;

	//	Native WindowBuffer
	NX_VID_MEMORY_INFO			vidFrameBuf[MAX_FRAME_BUFFER_NUM];	//	Video Buffer Info
	NX_VID_MEMORY_HANDLE		hVidFrameBuf[MAX_FRAME_BUFFER_NUM];

	//	x-ms-wmv
	OMX_BOOL					bXMSWMVType;

	//	for Debugging
	OMX_S32						inFrameCount;
	OMX_S32						outFrameCount;
	OMX_S32						instanceId;

	OMX_BOOL					bNeedSequenceData;

	OMX_BOOL					bInterlaced;		// 0 : Progressive, 1 : SW_Interlaced, 2 : 3D_Interlaced
	void						*hDeinterlace;

	OMX_BOOL					bPortReconfigure;
	OMX_BOOL					bIsPortDisable;
	NX_SEMAPHORE				*hPortCtrlSem;

	OMX_S32						inFlushFrameCount;
	OMX_BOOL					bIsFlush;

	OMX_S32						hScaler;
	OMX_BOOL					bOutBufCopy;
};


void InitVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
void PushVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS timestamp, OMX_U32 flag );
int PopVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS *timestamp, OMX_U32 *flag );
int flushVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
int openVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
void closeVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
void DeInterlaceFrame( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_V4L2DEC_OUT *pDecOut );
int GetUsableBufferIdx( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp );
int32_t OutBufCopy( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_V4L2DEC_OUT *pDecOut );

#endif	//	__NX_OMXVideoDecoder_h__
