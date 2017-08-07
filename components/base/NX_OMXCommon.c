#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXBaseComponent.h>
#include <NX_OMXCommon.h>

#ifndef UNUSED_PARAM
#define	UNUSED_PARAM(X)		X=X
#endif

void NX_OMXSetVersion(OMX_VERSIONTYPE *pVer)
{
	pVer->s.nVersionMajor = NXOMX_VER_MAJOR;
	pVer->s.nVersionMinor = NXOMX_VER_MINOR;
	pVer->s.nRevision = NXOMX_VER_REVISION;
	pVer->s.nStep = NXOMX_VER_NSTEP;
}

void NX_InitializeOutputBuffer(OMX_BUFFERHEADERTYPE *pOutBuf)
{
	pOutBuf->nFilledLen = 0;
	pOutBuf->hMarkTargetComponent = 0;
	pOutBuf->pMarkData = NULL;
	pOutBuf->nTickCount = 0;
	pOutBuf->nTimeStamp = 0;
	pOutBuf->nFlags = 0;
}

void NX_InitializeInputBuffer(OMX_BUFFERHEADERTYPE *pInBuf)
{
	UNUSED_PARAM(pInBuf);
}
