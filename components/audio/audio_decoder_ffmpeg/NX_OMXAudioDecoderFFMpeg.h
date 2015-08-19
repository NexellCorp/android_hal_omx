#ifndef __NX_OMXAudioDecoderFFMpeg_h__
#define __NX_OMXAudioDecoderFFMpeg_h__

#ifdef NX_DYNAMIC_COMPONENTS
//	This Function need for dynamic registration
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent);
#else
//	static registration
OMX_ERRORTYPE NX_FFMpegAudioDecoder_ComponentInit (OMX_HANDLETYPE hComponent);
#endif

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXBasePort.h>
#include <NX_OMXBaseComponent.h>
#include <NX_OMXSemaphore.h>
#include <NX_OMXQueue.h>

#include <hardware/gralloc.h>

#ifdef __cplusplus
extern "C"{
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>

#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif

#define FFDEC_AUD_VER_MAJOR			0
#define FFDEC_AUD_VER_MINOR			1
#define FFDEC_AUD_VER_REVISION		0
#define FFDEC_AUD_VER_NSTEP			0

#define	FFDEC_AUD_NUM_PORT			2
#define	FFDEC_AUD_INPORT_INDEX		0
#define	FFDEC_AUD_OUTPORT_INDEX		1

#define FFDEC_AUD_INPORT_MIN_BUF_CNT	8
//#define FFDEC_AUD_INPORT_MIN_BUF_SIZE	(64*1024)
#define	FFDEC_AUD_INPORT_MIN_BUF_SIZE	(1024*32)

#define FFDEC_AUD_OUTPORT_MIN_BUF_CNT	8
//#define FFDEC_AUD_OUTPORT_MIN_BUF_SIZE	(16*1536*2*2)
#define	FFDEC_AUD_OUTPORT_MIN_BUF_SIZE	(1024*1024)

#ifdef LOLLIPOP
#define OMX_IndexParamAudioAc3	(OMX_IndexVendorStartUnused + 0xE0000 + 0x00)
#define	OMX_IndexParamAudioDTS	(OMX_IndexVendorStartUnused + 0xE0000 + 0x01)
#define	OMX_IndexParamAudioFLAC	(OMX_IndexVendorStartUnused + 0xE0000 + 0x02)
#define OMX_AUDIO_CodingAC3		(OMX_AUDIO_CodingVendorStartUnused + 0xE0000 + 0x00)
#define OMX_AUDIO_CodingDTS		(OMX_AUDIO_CodingVendorStartUnused + 0xE0000 + 0x01)
#define OMX_AUDIO_CodingFLAC	(OMX_AUDIO_CodingVendorStartUnused + 0xE0000 + 0x02)
#else
#define OMX_IndexParamAudioAc3	(OMX_IndexVendorStartUnused + 0x00)
#define	OMX_IndexParamAudioDTS	(OMX_IndexVendorStartUnused + 0x01)
#define	OMX_IndexParamAudioFLAC	(OMX_IndexVendorStartUnused + 0x02)

#define OMX_AUDIO_CodingAC3		(OMX_AUDIO_CodingVendorStartUnused + 0x00)
#define OMX_AUDIO_CodingDTS		(OMX_AUDIO_CodingVendorStartUnused + 0x01)
#define OMX_AUDIO_CodingFLAC	(OMX_AUDIO_CodingVendorStartUnused + 0x02)
#endif

/** AC3 params */
typedef struct OMX_AUDIO_PARAM_AC3TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
} OMX_AUDIO_PARAM_AC3TYPE;

typedef struct OMX_AUDIO_PARAM_DTSTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
} OMX_AUDIO_PARAM_DTSTYPE;

typedef struct OMX_AUDIO_PARAM_FLACTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
} OMX_AUDIO_PARAM_FLACTYPE;


//	Define Transform Template Component Type
typedef struct NX_FFDEC_AUDIO_COMP_TYPE{
	NX_BASECOMPONENTTYPE		//	Nexell Base Component Type	
	/*					Buffer Thread							*/
	pthread_t					hBufThread;
	pthread_mutex_t				hBufMutex;
	NX_THREAD_CMD				eCmdBufThread;

	union{
		OMX_AUDIO_PARAM_AC3TYPE ac3Type;
		OMX_AUDIO_PARAM_DTSTYPE dtsType;
		OMX_AUDIO_PARAM_FLACTYPE flacType;
		OMX_AUDIO_PARAM_RATYPE raType;
		OMX_AUDIO_PARAM_WMATYPE wmaType;
		OMX_AUDIO_PARAM_MP3TYPE mp3Type;
	}inPortType;

	/*				Audio Format				*/
	OMX_AUDIO_CODINGTYPE		inCodingType;
	OMX_AUDIO_PARAM_PCMMODETYPE	outPortType;

	//	for decoding
	OMX_BOOL					bFlush;
	OMX_TICKS					nPrevTimeStamp;
	OMX_U32						nSampleCount;

	//	FFMPEG codec context
	AVCodec						*hAudioCodec;
	AVCodecContext				*avctx;

	//	FFMPEG Extra Data
	OMX_U8						*pExtraData;
	OMX_S32						nExtraDataSize;	

}NX_FFDEC_AUDIO_COMP_TYPE;

#endif	//	__NX_OMXAudioDecoderFFMpeg_h__
