#ifndef	__NX_VPUTYPES_H__
#define	__NX_VPUTYPES_H__


#include <libnxmem.h>		//	Nexell Video Memory Allocator



//	Video Codec Type ( API Level )
enum {
	//	Decoders
	NX_AVC_DEC  ,
	NX_MP4_DEC  ,
	NX_AVC_ENC  ,
	NX_MP4_ENC  ,
};


//
//	Encoder Specific APIs
//
typedef struct NX_VPU_DEC_IN{
	unsigned char *inBuf;	//	Manadatory 
	int bufSize;			//	Manadatory : inBuf's size
	int isKey;				//	Optional   : 0 or 1
	long long dts;			//	OPtional   : deocode time.
	long long pts;			//	OPtional   : presentation time.
}NX_VPU_DEC_IN;


typedef struct NX_VPU_DEC_OUT{
	int isOutput;
	int width;
	int height;
	int isKey;
	long long pts;
	VIDALLOCMEMORY outImg;
	int outImgIdx;
}NX_VPU_DEC_OUT;


typedef struct NX_VPU_ENC_OPT{
	int width;					//	Encoding Width
	int height;					//	Encoding Height
	int gop;					//	Group of Picture
	int framerate;				//	Frame Rate
	unsigned int bitrate;		//	Bitrate
}NX_VPU_ENC_OPT;

typedef struct NX_VPU_ENC_IN{
	int forcedKey;				//	forced key frame
	long long timestamp;		//	input frame's timestamp
	unsigned int frameIndex;	//	input frame's index( increment 1 in generally )
	VIDALLOCMEMORY inImg;		//	input image buffer
}NX_VID_ENC_IN;

typedef struct NX_VPU_ENC_OUT{
	unsigned char *outBuf;	//	output buffer's pointer
	int bufSize;			//	outBuf's size(input) and filled size(output)
	int isKey;				//	frame is i frame
	int width;				//	encoded image width
	int height;				//	encoded image height
}NX_VPU_ENC_OUT;


#endif	//	__NX_VPUTYPES_H__