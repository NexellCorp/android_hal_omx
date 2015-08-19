#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXBasePort.h>

OMX_ERRORTYPE NX_InitOMXPort(OMX_PARAM_PORTDEFINITIONTYPE *pPort, OMX_U32 nPortIndex, OMX_DIRTYPE eDir, OMX_BOOL bEnabled, OMX_PORTDOMAINTYPE eDomain)
{
	pPort->nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	pPort->nVersion.nVersion = 0;
	pPort->nPortIndex = nPortIndex;
	pPort->eDir = eDir;
	pPort->bEnabled = bEnabled;
	pPort->eDomain = eDomain;
	pPort->bPopulated = OMX_FALSE;
	return OMX_ErrorNone;
}

