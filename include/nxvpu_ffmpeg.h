#ifndef	__NXVPU_FFMPEG_H__
#define	__NXVPU_FFMPEG_H__

#include <libnxmem.h>
#include <NX_VpuTypes.h>


#ifdef __cplusplus
extern "C"{
#endif

void *NX_FFMPEG_VidDecOpen( int codecType );
int NX_FFMPEG_VidDecInit( void *handle , unsigned char *seqInfo, int seqSize, int width, int height );
int NX_FFMPEG_VidDecFrame(void *handle, NX_VPU_DEC_IN *decIn, NX_VPU_DEC_OUT *decOut );
int NX_FFMPEG_VidDecClrDspFlag( void *handle, VIDALLOCMEMORY *hFrame, int frameIdx );
int NX_FFMPEG_VidDecClose( void* handle );

void *NX_FFMPEG_VidEncOpen( int codecType );
int NX_FFMPEG_VidEncInit( void *handle );
int NX_FFMPEG_VidEncFrame( void *handle );
int NX_FFMPEG_VidEncClose( void *handle );

#ifdef __cplusplus
};
#endif


#endif	//	__NXVPU_FFMPEG_H__
