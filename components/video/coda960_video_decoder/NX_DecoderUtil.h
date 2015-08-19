#ifndef __NX_DecodeUtil_h__
#define __NX_DecodeUtil_h__

#include "NX_OMXVideoDecoder.h"

#ifndef MKTAG
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

#define PUT_LE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>16); \
	*_p++ = (unsigned char)((_var)>>24); 

#define PUT_BE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>24);  \
	*_p++ = (unsigned char)((_var)>>16);  \
	*_p++ = (unsigned char)((_var)>>8); \
	*_p++ = (unsigned char)((_var)>>0); 

#define PUT_LE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);  


#define PUT_BE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>0);  


//	Profiles & Levels
#define MAX_DEC_SUPPORTED_AVC_PROFILES		3
#define MAX_DEC_SUPPORTED_AVC_LEVELS		14

#define	MAX_DEC_SUPPORTED_MPEG4_PROFILES	2
#define	MAX_DEC_SUPPORTED_MPEG4_LEVELS		8

#define MAX_DEC_SUPPORTED_MPEG2_PROFILES	3
#define	MAX_DEC_SUPPORTED_MPEG2_LEVELS		3

// add in H.263 by kshblue (14.07.04)
#define	MAX_DEC_SUPPORTED_H263_PROFILES		2
#define	MAX_DEC_SUPPORTED_H263_LEVELS		8

extern const OMX_VIDEO_AVCPROFILETYPE gstDecSupportedAVCProfiles[MAX_DEC_SUPPORTED_AVC_PROFILES];
extern const OMX_VIDEO_AVCLEVELTYPE gstDecSupportedAVCLevels[MAX_DEC_SUPPORTED_AVC_LEVELS];

extern const OMX_VIDEO_MPEG4PROFILETYPE gstDecSupportedMPEG4Profiles[MAX_DEC_SUPPORTED_MPEG4_PROFILES];
extern const OMX_VIDEO_MPEG4LEVELTYPE gstDecSupportedMPEG4Levels[MAX_DEC_SUPPORTED_MPEG4_LEVELS];

extern const OMX_VIDEO_H263PROFILETYPE gstDecSupportedH263Profiles[MAX_DEC_SUPPORTED_H263_PROFILES];
extern const OMX_VIDEO_H263LEVELTYPE gstDecSupportedH263Levels[MAX_DEC_SUPPORTED_H263_LEVELS];

#ifdef __cplusplus
extern "C"{
#endif

// each Codec's NX_XXXX_Decoder.c
int NX_DecodeAvcFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeMpeg4Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeMpeg2Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeDiv3Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeRVFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeVC1Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int NX_DecodeVP8Frame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);

//	AVC Max Frame Buffer Size Tools
int AVCFindMinimumBufferSize(OMX_VIDEO_AVCLEVELTYPE level, int width, int height);

//	NX_OMX_VideoDecoder.c
int InitializeCodaVpu(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, unsigned char *buf, int size);

//	Copy Surface YV12 to General YV12
int CopySurfaceToBufferYV12( uint8_t *srcY, uint8_t *srcU, uint8_t *srcV, uint8_t *dst, uint32_t strideY, uint32_t strideUV, uint32_t width, uint32_t height );


#ifdef __cplusplus
}
#endif

#endif	//	__DecodeFrame_h__
