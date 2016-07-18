#ifndef __NX_OMXVideoDecoderFFMpeg_h__
#define __NX_OMXVideoDecoderFFMpeg_h__

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXBasePort.h>
#include <NX_OMXBaseComponent.h>
#include <NX_OMXSemaphore.h>
#include <NX_OMXQueue.h>

#include <hardware/gralloc.h>
#include <media/hardware/MetadataBufferType.h>

#if ARM64
#include <nx_video_api_64.h>
#else
#include <nx_video_api.h>
#endif

#ifdef NX_DYNAMIC_COMPONENTS
//	This Function need for dynamic registration
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent);
#else
//	static registration
OMX_ERRORTYPE NX_VidDecComponentInit (OMX_HANDLETYPE hComponent);
#endif


#define	VIDDEC_NUM_PORT			2
#define	VIDDEC_INPORT_INDEX		0
#define	VIDDEC_OUTPORT_INDEX	1

#define	VIDENC_INPORT_MIN_BUF_CNT	6
#define	VIDENC_INPORT_MIN_BUF_SIZE	(1920*1088*4)

#define	VIDENC_OUTPORT_MIN_BUF_CNT	8
#define	VIDENC_OUTPORT_MIN_BUF_SIZE	(4*1024*1024)

#define	VIDENC_DEF_FRAMERATE		(30)
#define	VIDENC_DEF_BITRATE			(3*1024*1024)

#ifdef ARCH_S5P6818
#define VIDENC_CSC_THREAD			1
#else
#define VIDENC_CSC_THREAD			1
#endif


typedef struct _NX_VINPUT_INFO NX_VINPUT_INFO;
struct _NX_VINPUT_INFO{
	uint32_t key;
	uint32_t flag;
	int64_t timestamp;
};

//	Define Video Decoder Component Type
typedef struct tNX_VIDENC_COMP_TYPE{
	NX_BASECOMPONENTTYPE		//	Nexell Base Component Type
	/*					Buffer Thread							*/
	pthread_t				hBufThread;
	pthread_mutex_t			hBufMutex;
	NX_THREAD_CMD			eCmdBufThread;

	//		Video Format
	OMX_VIDEO_PARAM_PORTFORMATTYPE	inputFormat;
	OMX_VIDEO_PARAM_PORTFORMATTYPE	outputFormat;

	//
	OMX_BOOL					bSendCodecSpecificInfo;
	OMX_VIDEO_PARAM_MPEG4TYPE	omxMp4EncParam;
	OMX_VIDEO_PARAM_AVCTYPE   	omxAVCEncParam;
	OMX_VIDEO_PARAM_H263TYPE	omxH263EncParam;

	//	Android Native Buffer Flasg
	OMX_BOOL					bUseNativeBuffer;
	OMX_BOOL					bMetaDataInBuffers;

	//	Encoder parameters
	uint32_t					encWidth;
	uint32_t					encHeight;
	uint32_t					encKeyInterval;		//	GOP
	uint32_t					encFrameRate;
	uint32_t					encBitRate;
	uint32_t					encIntraRefreshMbs;

	OMX_BUFFERHEADERTYPE		*pPrevInputBuffer;

	//	Sequence Data Buffer
	OMX_U8						pSeqBuf[1024];
	int32_t						seqBufSize;
	OMX_BOOL					bCodecSpecificInfo;

	//	Color Space Converter Destination Buffer
	NX_VID_MEMORY_HANDLE		hCSCMem;

	//	Encoder Handle
	NX_VID_ENC_HANDLE			hVpuCodec;
	OMX_S32						vpuCodecId;

	OMX_BYTE					pPictureBuf;

	//	multi-thread for CSC
	int32_t						threadNum;
}NX_VIDENC_COMP_TYPE;

#endif	//	__NX_OMXVideoDecoderFFMpeg_h__
