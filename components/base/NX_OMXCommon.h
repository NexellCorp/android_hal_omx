#include <OMX_Core.h>
#include <OMX_Component.h>

#ifndef __NX_OMXCommon_h__
#define __NX_OMXCommon_h__

void NX_OMXSetVersion(OMX_VERSIONTYPE *pVer);
//	Buffer Tool
void NX_InitializeOutputBuffer(OMX_BUFFERHEADERTYPE *pOutBuf);
void NX_InitializeInputBuffer(OMX_BUFFERHEADERTYPE *pInBuf);

#endif	//	__NX_OMXCommon_h__
