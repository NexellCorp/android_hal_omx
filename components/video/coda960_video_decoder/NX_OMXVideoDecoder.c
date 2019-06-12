#define	LOG_TAG				"NX_VIDDEC"

#include <assert.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXBasePort.h>
#include <NX_OMXBaseComponent.h>
#include <OMX_AndroidTypes.h>
#include <system/graphics.h>
#include <cutils/properties.h>
#include <linux/media-bus-format.h>

#include <stdbool.h>

#include "NX_OMXVideoDecoder.h"
#include "NX_DecoderUtil.h"

//	Default Recomanded Functions for Implementation Components
static OMX_ERRORTYPE NX_VidDec_GetConfig(OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure);
static OMX_ERRORTYPE NX_VidDec_GetParameter (OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE NX_VidDec_SetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE NX_VidDec_UseBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer);
static OMX_ERRORTYPE NX_VidDec_AllocateBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** pBuffer, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes);
static OMX_ERRORTYPE NX_VidDec_FreeBuffer (OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE NX_VidDec_FillThisBuffer(OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE NX_VidDec_ComponentDeInit(OMX_HANDLETYPE hComponent);
static void          NX_VidDec_BufferMgmtThread( void *arg );

static void NX_VidDec_CommandProc( NX_BASE_COMPNENT *pDecComp, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData );


int flushVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
int openVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
void closeVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
int decodeVideoFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);
int processEOS(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);
int processEOSforFlush(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp);

static OMX_S32		gstNumInstance = 0;
static OMX_S32		gstMaxInstance = 3;


#ifdef NX_DYNAMIC_COMPONENTS
//	This Function need for dynamic registration
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent)
#else
//	static registration
OMX_ERRORTYPE NX_VideoDecoder_ComponentInit (OMX_HANDLETYPE hComponent)
#endif
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_U32 i=0;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp;

	FUNC_IN;

	TRACE("%s()++ gstNumInstance = %ld, gstMaxInstance = %ld\n", __func__, gstNumInstance, gstMaxInstance);

	if( gstNumInstance >= gstMaxInstance )
	{
		ErrMsg( "%s() : Instance Creation Failed = %ld\n", __func__, gstNumInstance );
		return OMX_ErrorInsufficientResources;
	}

	pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)NxMalloc(sizeof(NX_VIDDEC_VIDEO_COMP_TYPE));
	if( pDecComp == NULL ){
		return OMX_ErrorInsufficientResources;
	}

	///////////////////////////////////////////////////////////////////
	//					Component configuration
	NxMemset( pDecComp, 0, sizeof(NX_VIDDEC_VIDEO_COMP_TYPE) );
	pComp->pComponentPrivate = pDecComp;

	//	Initialize Base Component Informations
	if( OMX_ErrorNone != (eError=NX_BaseComponentInit( pComp )) ){
		NxFree( pDecComp );
		return eError;
	}
	NX_OMXSetVersion( &pComp->nVersion );
	//					End Component configuration
	///////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////
	//						Port configurations
	//	Create ports
	for( i=0; i<VPUDEC_VID_NUM_PORT ; i++ )
	{
		pDecComp->pPort[i] = (OMX_PTR)NxMalloc(sizeof(NX_BASEPORTTYPE));
		NxMemset( pDecComp->pPort[i], 0, sizeof(NX_BASEPORTTYPE) );
		pDecComp->pBufQueue[i] = (OMX_PTR)NxMalloc(sizeof(NX_QUEUE));
	}
	pDecComp->nNumPort = VPUDEC_VID_NUM_PORT;
	//	Input port configuration
	pDecComp->pInputPort = (NX_BASEPORTTYPE *)pDecComp->pPort[VPUDEC_VID_INPORT_INDEX];
	pDecComp->pInputPortQueue = (NX_QUEUE *)pDecComp->pBufQueue[VPUDEC_VID_INPORT_INDEX];
	NX_InitQueue(pDecComp->pInputPortQueue, NX_OMX_MAX_BUF);
	pPort = pDecComp->pInputPort;

	NX_OMXSetVersion( &pPort->stdPortDef.nVersion );
	NX_InitOMXPort( &pPort->stdPortDef, VPUDEC_VID_INPORT_INDEX, OMX_DirInput, OMX_TRUE, OMX_PortDomainVideo );
	pPort->stdPortDef.nBufferCountActual = VID_INPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferCountMin    = VID_INPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferSize        = VID_INPORT_MIN_BUF_SIZE;

	//	Output port configuration
	pDecComp->pOutputPort = (NX_BASEPORTTYPE *)pDecComp->pPort[VPUDEC_VID_OUTPORT_INDEX];
	pDecComp->pOutputPortQueue = (NX_QUEUE *)pDecComp->pBufQueue[VPUDEC_VID_OUTPORT_INDEX];
	NX_InitQueue(pDecComp->pOutputPortQueue, NX_OMX_MAX_BUF);
	pPort = pDecComp->pOutputPort;
	NX_OMXSetVersion( &pPort->stdPortDef.nVersion );
	NX_InitOMXPort( &pPort->stdPortDef, VPUDEC_VID_OUTPORT_INDEX, OMX_DirOutput, OMX_TRUE, OMX_PortDomainVideo );
	pPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferSize        = VID_OUTPORT_MIN_BUF_SIZE;
	//					End Port configurations
	///////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////
	//
	//	Registration OMX Standard Functions
	//
	//	Base overrided functions
	pComp->GetComponentVersion    = NX_BaseGetComponentVersion    ;
	pComp->SendCommand            = NX_BaseSendCommand            ;
	pComp->SetConfig              = NX_BaseSetConfig              ;
	pComp->GetExtensionIndex      = NX_BaseGetExtensionIndex      ;
	pComp->GetState               = NX_BaseGetState               ;
	pComp->ComponentTunnelRequest = NX_BaseComponentTunnelRequest ;
	pComp->SetCallbacks           = NX_BaseSetCallbacks           ;
	pComp->UseEGLImage            = NX_BaseUseEGLImage            ;
	pComp->ComponentRoleEnum      = NX_BaseComponentRoleEnum      ;
	pComp->EmptyThisBuffer        = NX_BaseEmptyThisBuffer        ;

	//	Specific implemented functions
	pComp->GetConfig              = NX_VidDec_GetConfig           ;
	pComp->GetParameter           = NX_VidDec_GetParameter        ;
	pComp->SetParameter           = NX_VidDec_SetParameter        ;
	pComp->UseBuffer              = NX_VidDec_UseBuffer           ;
	pComp->AllocateBuffer         = NX_VidDec_AllocateBuffer      ;
	pComp->FreeBuffer             = NX_VidDec_FreeBuffer          ;
	pComp->FillThisBuffer         = NX_VidDec_FillThisBuffer      ;
	pComp->ComponentDeInit        = NX_VidDec_ComponentDeInit     ;
	//
	///////////////////////////////////////////////////////////////////

	//	Registration Command Procedure
	pDecComp->cbCmdProcedure = NX_VidDec_CommandProc;			//	Command procedure

	//	Create command thread
	NX_InitQueue( &pDecComp->cmdQueue, NX_MAX_QUEUE_ELEMENT );
	pDecComp->hSemCmd = NX_CreateSem( 0, NX_MAX_QUEUE_ELEMENT );
	pDecComp->hSemCmdWait = NX_CreateSem( 0, 1 );
	pDecComp->eCmdThreadCmd = NX_THREAD_CMD_RUN;
	pthread_create( &pDecComp->hCmdThread, NULL, (void*)&NX_BaseCommandThread , pDecComp ) ;
	NX_PendSem( pDecComp->hSemCmdWait );

	pDecComp->compName = strdup("OMX.NX.VIDEO_DECODER");
	pDecComp->compRole = strdup("video_decoder");			//	Default Component Role

	//	Buffer
	pthread_mutex_init( &pDecComp->hBufMutex, NULL );
	pDecComp->hBufAllocSem = NX_CreateSem(0, 16);
	pDecComp->hBufFreeSem = NX_CreateSem(0, 16);

	pDecComp->hVpuCodec = NULL;		//	Initialize Video Process Unit Handler
	pDecComp->videoCodecId = -1;
	pDecComp->bInitialized = OMX_FALSE;
	pDecComp->bNeedKey = OMX_FALSE;
	pDecComp->frameDelay = 0;
	pDecComp->bStartEoS = OMX_FALSE;
	pDecComp->codecSpecificData = NULL;
	pDecComp->codecSpecificDataSize = 0;
	pDecComp->outBufferable = 1;
	pDecComp->outUsableBuffers = 0;
	pDecComp->curOutBuffers = 0;
	pDecComp->minRequiredFrameBuffer = 1;

	//	Set Video Output Port Information
	pDecComp->outputFormat.eColorFormat = OMX_COLOR_FormatYUV420Planar;
	pDecComp->bUseNativeBuffer = OMX_FALSE;

	//	FIXME : TODO
	pDecComp->bEnableThumbNailMode = OMX_FALSE;

	pDecComp->bMetaDataInBuffers = OMX_FALSE;

	pDecComp->outBufferAllocSize = 0;
	pDecComp->numOutBuffers = 0;

	//	for WMV new MX Player
	pDecComp->bXMSWMVType = OMX_FALSE;

	//	Debugging Tool
	pDecComp->inFrameCount = 0;
	pDecComp->outFrameCount = 0;
	pDecComp->bNeedSequenceData = OMX_TRUE;

	pDecComp->instanceId = gstNumInstance;

	gstNumInstance ++;

	FUNC_OUT;
	TRACE("%s()-- gstNumInstance = %ld, gstMaxInstance = %ld\n", __func__, gstNumInstance, gstMaxInstance);

	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_VidDec_ComponentDeInit(OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pComp->pComponentPrivate;
	OMX_U32 i=0;
	FUNC_IN;

	TRACE("%s()++ gstNumInstance = %ld, gstMaxInstance = %ld\n", __func__, gstNumInstance, gstMaxInstance);

	//	prevent duplacation
	if( NULL == pComp->pComponentPrivate )
		return OMX_ErrorNone;

	if( (OMX_StateLoaded == pDecComp->eCurState && OMX_StateIdle == pDecComp->eNewState) ||
		(OMX_StateIdle == pDecComp->eCurState && OMX_StateLoaded == pDecComp->eNewState) )
	{
		pDecComp->bAbendState = OMX_TRUE;
		DbgMsg("%s(): Waring. bAbendState: True\n", __func__);
	}
	
	// Destroy command thread
	pDecComp->eCmdThreadCmd = NX_THREAD_CMD_EXIT;
	NX_PostSem( pDecComp->hSemCmdWait );
	NX_PostSem( pDecComp->hSemCmd );

	// add hcjun(2017_10_25)
	for( i=0 ; i<pDecComp->nNumPort ; i++ )
	{
		if(pDecComp->bBufAllocPend[i] == OMX_TRUE)
		{
			NX_PostSem(pDecComp->hBufAllocSem);
			DbgMsg("%s(): Waring. BufAllocPend: Port(%lu)\n", __func__,i );
		}
	}
	for( i=0 ; i<pDecComp->nNumPort ; i++ )
	{
		if(pDecComp->bBufFreePend[i] == OMX_TRUE)
		{
			NX_PostSem(pDecComp->hBufFreeSem);
			DbgMsg("%s(): Waring. bBufFreePend: Port(%lu)\n", __func__,i );
		}
	}

	pthread_join( pDecComp->hCmdThread, NULL );
	NX_DeinitQueue( &pDecComp->cmdQueue );
	//	Destroy Semaphore
	NX_DestroySem( pDecComp->hSemCmdWait );
	NX_DestroySem( pDecComp->hSemCmd );

	//	Destroy port resource
	for( i=0; i<VPUDEC_VID_NUM_PORT ; i++ ){
		if( pDecComp->pPort[i] ){
			NxFree(pDecComp->pPort[i]);
		}
		if( pDecComp->pBufQueue[i] ){
			//	Deinit Queue
			NX_DeinitQueue( pDecComp->pBufQueue[i] );
			NxFree( pDecComp->pBufQueue[i] );
		}
	}

	NX_BaseComponentDeInit( hComponent );

	//	Buffer
	pthread_mutex_destroy( &pDecComp->hBufMutex );
	NX_DestroySem(pDecComp->hBufAllocSem);
	NX_DestroySem(pDecComp->hBufFreeSem);

	if( pDecComp->codecSpecificData )
	{
		free( pDecComp->codecSpecificData );
	}

	//
	if( pDecComp->compName )
		free(pDecComp->compName);
	if( pDecComp->compRole )
		free(pDecComp->compRole);

	if( pDecComp ){
		NxFree(pDecComp);
		pComp->pComponentPrivate = NULL;
	}

	gstNumInstance --;
	FUNC_OUT;
	TRACE("%s()-- gstNumInstance = %ld, gstMaxInstance = %ld\n", __func__, gstNumInstance, gstMaxInstance);
	return OMX_ErrorNone;
}


static OMX_ERRORTYPE NX_VidDec_GetConfig(OMX_HANDLETYPE hComp, OMX_INDEXTYPE nConfigIndex, OMX_PTR pComponentConfigStructure)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	TRACE("%s(0x%08x) In\n", __FUNCTION__, nConfigIndex);

	switch ( nConfigIndex )
	{
		case OMX_IndexConfigCommonOutputCrop:
		{
			OMX_CONFIG_RECTTYPE *pRect = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
		    if ((pRect->nPortIndex != IN_PORT) && (pRect->nPortIndex != OUT_PORT))
			{
		        return OMX_ErrorBadPortIndex;
		    }
		    pRect->nTop = 0;
		    pRect->nLeft = 0;
			if( pRect->nPortIndex == IN_PORT )
			{
				pRect->nWidth = pDecComp->pInputPort->stdPortDef.format.video.nFrameWidth;
				pRect->nHeight = pDecComp->pInputPort->stdPortDef.format.video.nFrameHeight;
			}
			else
			{
				pRect->nWidth = pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth;
				pRect->nHeight = pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight;

				if( (pDecComp->dsp_width > 0 && pDecComp->dsp_height > 0) &&
					(pRect->nWidth != (OMX_U32)pDecComp->dsp_width || pRect->nHeight != (OMX_U32)pDecComp->dsp_height) )
				{
					DbgMsg("Change Crop = %lu x %lu --> %ld x %ld\n", pRect->nWidth, pRect->nHeight, pDecComp->dsp_width, pDecComp->dsp_height);
					pRect->nWidth = pDecComp->dsp_width;
					pRect->nHeight = pDecComp->dsp_height;
				}
			}
			TRACE("%s() : width(%ld), height(%ld)\n ", __func__, pRect->nWidth, pRect->nHeight );
			break;
		}
		default:
		    return NX_BaseGetConfig(hComp, nConfigIndex, pComponentConfigStructure);
	}
	return OMX_ErrorNone;
}


static OMX_ERRORTYPE NX_VidDec_GetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	TRACE("%s(0x%08x) In\n", __FUNCTION__, nParamIndex);
	switch( nParamIndex )
	{
		case OMX_IndexParamVideoPortFormat:
		{
			OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParamStruct;
			TRACE("%s() : OMX_IndexParamVideoPortFormat : port Index = %ld, format index = %ld\n", __FUNCTION__, pVideoFormat->nPortIndex, pVideoFormat->nIndex );
			if( pVideoFormat->nPortIndex == IN_PORT )	//	Input Information
			{
				//	Set from component Role
				memcpy( pVideoFormat, &pDecComp->inputFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
			}
			else
			{
				switch ( pVideoFormat->nIndex )
				{
					case 0:
						memcpy( pVideoFormat, &pDecComp->outputFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
						pDecComp->outputFormat.nPortIndex= OUT_PORT;
						pVideoFormat->eColorFormat = OMX_COLOR_FormatYUV420Planar;     break;
					// case 1:
					// 	pVideoFormat->eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar; break;
					default:
						return OMX_ErrorNoMore;
				}
			}
			break;
		}
		case OMX_IndexAndroidNativeBufferUsage:
		{
			struct GetAndroidNativeBufferUsageParams *pBufUsage = (struct GetAndroidNativeBufferUsageParams *)ComponentParamStruct;
			if( pBufUsage->nPortIndex != OUT_PORT )
				return OMX_ErrorBadPortIndex;
			pBufUsage->nUsage |= (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);	//	Enable Native Buffer Usage
			break;
		}
		case OMX_IndexParamPortDefinition:
		{
			OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParamStruct;
			OMX_ERRORTYPE error = OMX_ErrorNone;

			error = NX_BaseGetParameter( hComp, nParamIndex, ComponentParamStruct );
			if( error != OMX_ErrorNone )
			{
				DbgMsg("Error NX_BaseGetParameter() failed !!! for OMX_IndexParamPortDefinition \n");
				return error;
			}
			if( pPortDef->nPortIndex == OUT_PORT )
			{
				pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YV12;

				if(pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth > 0 && pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight > 0)
				{
					if( (pPortDef->format.video.nFrameWidth  != pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth) ||
						(pPortDef->format.video.nFrameHeight  != pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight) )
					{
						DbgMsg("OutputPort Video Resolution = %ld x %ld --> %ld x %ld\n", pPortDef->format.video.nFrameWidth, pPortDef->format.video.nFrameHeight,
										pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth, pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight);
						pPortDef->format.video.nFrameWidth = pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth;
						pPortDef->format.video.nFrameHeight = pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight;
					}

					pPortDef->format.video.nStride = pDecComp->pOutputPort->stdPortDef.format.video.nFrameWidth;
					pPortDef->format.video.nSliceHeight = pDecComp->pOutputPort->stdPortDef.format.video.nFrameHeight;
				}
			}
			break;
		}
		case OMX_IndexParamVideoProfileLevelQuerySupported:
		{
			OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)ComponentParamStruct;
			int32_t profileIdx, levelIdx;
			if( profileLevel->nPortIndex > OUT_PORT )
			{
				return OMX_ErrorBadPortIndex;
			}

			if( pDecComp->videoCodecId == NX_AVC_DEC )
			{
				if( profileLevel->nProfileIndex >= (MAX_DEC_SUPPORTED_AVC_PROFILES*MAX_DEC_SUPPORTED_AVC_LEVELS) )
				{
					return OMX_ErrorNoMore;
				}
				profileIdx = profileLevel->nProfileIndex / MAX_DEC_SUPPORTED_AVC_PROFILES;
				levelIdx = profileLevel->nProfileIndex % MAX_DEC_SUPPORTED_AVC_LEVELS;
				profileLevel->eProfile = gstDecSupportedAVCProfiles[profileIdx];
				profileLevel->eLevel = gstDecSupportedAVCLevels[levelIdx];
			}
			else if( pDecComp->videoCodecId == NX_MP4_DEC )
			{
				if( profileLevel->nProfileIndex >= MAX_DEC_SUPPORTED_MPEG4_PROFILES*MAX_DEC_SUPPORTED_MPEG4_LEVELS )
				{
					return OMX_ErrorNoMore;
				}
				profileIdx = profileLevel->nProfileIndex / MAX_DEC_SUPPORTED_MPEG4_PROFILES;
				levelIdx = profileLevel->nProfileIndex % MAX_DEC_SUPPORTED_MPEG4_LEVELS;
				profileLevel->eProfile = gstDecSupportedMPEG4Profiles[profileIdx];
				profileLevel->eLevel = gstDecSupportedMPEG4Levels[levelIdx];
			}
			else if( pDecComp->videoCodecId == NX_H263_DEC )     // add by kshblue (14.07.04)
			{
				if( profileLevel->nProfileIndex >= MAX_DEC_SUPPORTED_H263_PROFILES*MAX_DEC_SUPPORTED_H263_LEVELS )
				{
					return OMX_ErrorNoMore;
				}
				profileIdx = profileLevel->nProfileIndex / MAX_DEC_SUPPORTED_H263_PROFILES;
				levelIdx = profileLevel->nProfileIndex % MAX_DEC_SUPPORTED_H263_LEVELS;
				profileLevel->eProfile = gstDecSupportedH263Profiles[profileIdx];
				profileLevel->eLevel = gstDecSupportedH263Levels[levelIdx];
			}
			else
			{
				return OMX_ErrorNoMore;
			}
			break;
		}
		default :
			return NX_BaseGetParameter( hComp, nParamIndex, ComponentParamStruct );
	}
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_VidDec_SetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;

	DBG_PARAM("%s(0x%08x) In\n", __FUNCTION__, nParamIndex);

	switch( nParamIndex )
	{
		case OMX_IndexParamStandardComponentRole:
		{
			OMX_PARAM_COMPONENTROLETYPE *pInRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParamStruct;
			if( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.avc"  )  )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingAVC;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_AVC_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.mpeg4") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_MP4_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.x-flv") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_FLV_DEC;
				pDecComp->codecTag = MKTAG('f','l','v','1');
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.mp43") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_MP4_DEC;
				pDecComp->codecTag = MKTAG('m','p','4','3');
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.mpeg2") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_MP2_DEC;
			}
			else if ( (!strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.h263") )  ||
					  (!strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.3gpp")) )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingH263;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_H263_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.x-ms-wmv") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingWMV;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_WMV9_DEC;
				pDecComp->bXMSWMVType = OMX_TRUE;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.wvc1") || !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.vc1") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingWMV;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_WVC1_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.x-pn-realvideo") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingRV;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_RV9_DEC;
			}
			else if ( !strcmp( (OMX_STRING)pInRole->cRole, "video_decoder.vp8") )
			{
				//	Set Input Format
				pDecComp->inputFormat.eCompressionFormat = OMX_VIDEO_CodingVP8;
				pDecComp->inputFormat.eColorFormat = OMX_COLOR_FormatUnused;
				pDecComp->inputFormat.nPortIndex= IN_PORT;
				pDecComp->videoCodecId = NX_VP8_DEC;
			}
			else
			{
				//	Error
				ErrMsg("Error: %s(): in role = %s\n", __FUNCTION__, (OMX_STRING)pInRole->cRole );
				return OMX_ErrorBadParameter;
			}

			//	Set Output Fomrmat
			pDecComp->outputFormat.eCompressionFormat = OMX_VIDEO_CodingUnused;
			pDecComp->outputFormat.eColorFormat = OMX_COLOR_FormatYUV420Planar;
			pDecComp->outputFormat.nPortIndex= OUT_PORT;

			if( pDecComp->compRole ){
				free ( pDecComp->compRole );
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
			}
			DbgMsg("%s(): Set new role : in role = %s, comp role = %s\n", __FUNCTION__, (OMX_STRING)pInRole->cRole, pDecComp->compRole );
			break;
		}

		case OMX_IndexParamVideoPortFormat:
		{
			OMX_VIDEO_PARAM_PORTFORMATTYPE *pVideoFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParamStruct;
			if( pVideoFormat->nPortIndex == IN_PORT ){
				memcpy( &pDecComp->inputFormat, pVideoFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
			}else{
				memcpy( &pDecComp->outputFormat, pVideoFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE) );
			}
			break;
		}

		case OMX_IndexParamVideoAvc:
		{
			OMX_VIDEO_PARAM_AVCTYPE *pAvcParam = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParamStruct;
			DBG_PARAM("%s(): In, (nParamIndex=0x%08x)OMX_IndexParamVideoAvc\n", __FUNCTION__, nParamIndex );
			if( pAvcParam->eProfile > OMX_VIDEO_AVCProfileHigh )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoAvc failed!! Cannot support profile(%d)\n", pAvcParam->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			if( pAvcParam->eLevel > OMX_VIDEO_AVCLevel51 )		//	???
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoAvc failed!! Cannot support profile(%d)\n", pAvcParam->eLevel);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoMpeg2:
		{
			OMX_VIDEO_PARAM_MPEG2TYPE *pMpeg2Param = (OMX_VIDEO_PARAM_MPEG2TYPE *)ComponentParamStruct;
			DBG_PARAM("%s(): In, (nParamIndex=0x%08x)OMX_IndexParamVideoMpeg2\n", __FUNCTION__, nParamIndex );
			if( pMpeg2Param->eProfile != OMX_VIDEO_MPEG2ProfileSimple || pMpeg2Param->eProfile != OMX_VIDEO_MPEG2ProfileMain || pMpeg2Param->eProfile != OMX_VIDEO_MPEG2ProfileHigh )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg2 failed!! Cannot support profile(%d)\n", pMpeg2Param->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			if( pMpeg2Param->eLevel != OMX_VIDEO_MPEG2LevelLL || pMpeg2Param->eLevel != OMX_VIDEO_MPEG2LevelML || pMpeg2Param->eLevel != OMX_VIDEO_MPEG2LevelHL )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg2 failed!! Cannot support level(%d)\n", pMpeg2Param->eLevel);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoMpeg4:
		{
			OMX_VIDEO_PARAM_MPEG4TYPE *pMpeg4Param = (OMX_VIDEO_PARAM_MPEG4TYPE *)ComponentParamStruct;
			DBG_PARAM("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_MPEG4TYPE\n", __FUNCTION__, nParamIndex );
			if( pMpeg4Param->eProfile >  OMX_VIDEO_MPEG4ProfileAdvancedSimple )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg4 failed!! Cannot support profile(%d)\n", pMpeg4Param->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			if( pMpeg4Param->eLevel > OMX_VIDEO_MPEG4Level5 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoMpeg4 failed!! Cannot support level(%d)\n", pMpeg4Param->eLevel);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoH263:     // modified by kshblue (14.07.04)
		{
			OMX_VIDEO_PARAM_H263TYPE *pH263Param = (OMX_VIDEO_PARAM_H263TYPE *)ComponentParamStruct;
			DBG_PARAM("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_H263TYPE\n", __FUNCTION__, nParamIndex );
			if( pH263Param->eProfile != OMX_VIDEO_H263ProfileBaseline || pH263Param->eProfile != OMX_VIDEO_H263ProfileISWV2 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoH263 failed!! Cannot support profile(%d)\n", pH263Param->eProfile);
				return OMX_ErrorPortsNotCompatible;
			}
			if ( pH263Param->eLevel > OMX_VIDEO_H263Level70 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoH263 failed!! Cannot support level(%d)\n", pH263Param->eLevel);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}

		case OMX_IndexParamVideoWmv:
		{
			OMX_VIDEO_PARAM_WMVTYPE *pWmvParam = (OMX_VIDEO_PARAM_WMVTYPE *)ComponentParamStruct;
			DBG_PARAM("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_WMVTYPE\n", __FUNCTION__, nParamIndex );
			if( pWmvParam->eFormat > OMX_VIDEO_WMVFormat9 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoWmv failed!! Cannot support format(%d)\n", pWmvParam->eFormat);
				return OMX_ErrorPortsNotCompatible;
			}
			memcpy( &pDecComp->codecType.wmvType, pWmvParam, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
			if( (0 == pWmvParam->eFormat) && (pDecComp->bXMSWMVType == OMX_TRUE)  )
			{
				pDecComp->bXMSWMVType = OMX_FALSE;
			}
			break;
		}

		case OMX_IndexParamVideoRv:
		{
			OMX_VIDEO_PARAM_RVTYPE *pRvParam = (OMX_VIDEO_PARAM_RVTYPE *)ComponentParamStruct;
			DBG_PARAM("%s(): In, (nParamIndex=0x%08x)OMX_VIDEO_PARAM_RVTYPE\n", __FUNCTION__, nParamIndex );
			if( pRvParam->eFormat > OMX_VIDEO_RVFormatG2 )
			{
				ErrMsg("NX_VidDec_SetParameter() : OMX_IndexParamVideoRv failed!! Cannot support format(%d)\n", pRvParam->eFormat);
				return OMX_ErrorPortsNotCompatible;
			}
			break;
		}
		case OMX_IndexVideoDecoderCodecTag:
		{
			OMX_VIDEO_CODECTAG *pCodecTag = (OMX_VIDEO_CODECTAG *)ComponentParamStruct;
			pDecComp->codecTag = pCodecTag->nCodecTag;
			break;
		}
		case OMX_IndexParamPortDefinition:
		{
			OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParamStruct;
			OMX_ERRORTYPE error = OMX_ErrorNone;
			error = NX_BaseSetParameter( hComp, nParamIndex, ComponentParamStruct );
			if( error != OMX_ErrorNone )
			{
				DbgMsg("Error NX_BaseSetParameter() failed !!! for OMX_IndexParamPortDefinition \n");
				return error;
			}
			if( pPortDef->nPortIndex == IN_PORT )
			{
				//	Set Input Width & Height
				pDecComp->width = pPortDef->format.video.nFrameWidth;
				pDecComp->height = pPortDef->format.video.nFrameHeight;

				if( pDecComp->bEnableThumbNailMode )
				{
					pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT_THUMB;
					pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT_THUMB;
				}
				else
				{
					DbgMsg("CodecID(%ld) NX_AVC_DEC(%d)\n", pDecComp->videoCodecId, NX_AVC_DEC);

					if( pDecComp->videoCodecId == NX_MP2_DEC )
					{
						pDecComp->bInterlaced = OMX_FALSE;

						if(pDecComp->bInterlaced)
						{
							pDecComp->pOutputPort->stdPortDef.nBufferCountMin = VID_OUTPORT_MIN_BUF_CNT_INTERLACE;
							pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT_INTERLACE;
						}
						else
						{
							pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT;
							pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT;
						}
					}
					else if( pDecComp->videoCodecId == NX_AVC_DEC )
					{
						int32_t MBs = ((pDecComp->width+15)>>4)*((pDecComp->height+15)>>4);
						//	Under 720p
						if(MBs <= ((1280>>4)*(720>>4)))
						{
							pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT_H264_UNDER720P;
							pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT_H264_UNDER720P;
						}
						//	1080p
						else
						{
							pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT_H264_1080P;
							pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT_H264_1080P;
						}
					}
					else
					{
						pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT;
						pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT;
					}
				}

				pDecComp->pOutputPort->stdPortDef.nBufferSize = pDecComp->width*pDecComp->height*3/2;

				if( (((pDecComp->width+15)>>4) * ((pDecComp->height+15)>>4) ) > ((1920>>4)*(1088>>4)) )
				{
					DbgMsg("Cannot Support Video Resolution : Max(1920x1080), Input(%ldx%ld)\n", pDecComp->width, pDecComp->height);
					return OMX_ErrorUnsupportedSetting;
				}
			}
			break;
		}

		//
		//	For Android Extension
		//
		case OMX_IndexEnableAndroidNativeBuffers:
		{
			struct EnableAndroidNativeBuffersParams *pEnNativeBuf = (struct EnableAndroidNativeBuffersParams *)ComponentParamStruct;
			if ( pEnNativeBuf->nPortIndex != OUT_PORT )
				return OMX_ErrorBadPortIndex;
			pDecComp->bUseNativeBuffer = pEnNativeBuf->enable;
			DbgMsg("Native buffer flag is %s!!", (pDecComp->bUseNativeBuffer==OMX_TRUE)?"Enabled":"Disabled");

			if ( pDecComp->bUseNativeBuffer == OMX_FALSE )
			{
				pDecComp->bEnableThumbNailMode = OMX_TRUE;
			}
			else
			{
				pDecComp->bEnableThumbNailMode = OMX_FALSE;
			}
			break;
		}
		//case OMX_IndexStoreMetaDataInBuffers:
		//{
		//	struct StoreMetaDataInBuffersParams *pParam = (struct StoreMetaDataInBuffersParams *)ComponentParamStruct;
		//	DbgMsg("%s() : OMX_IndexStoreMetaDataInBuffers : port(%ld), flag(%d)\n", __FUNCTION__, pParam->nPortIndex, pParam->bStoreMetaData );
		//	pDecComp->bMetaDataInBuffers = pParam->bStoreMetaData;
		//	break;
		//}
		case OMX_IndexVideoDecoderThumbnailMode:
		{
			//	Thumbnail Mode
			pDecComp->bEnableThumbNailMode = OMX_TRUE;

			//	Reconfigure Output Buffer Information for Thubmail Mode.
			pDecComp->pOutputPort->stdPortDef.nBufferCountActual = VID_OUTPORT_MIN_BUF_CNT_THUMB;
			pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = VID_OUTPORT_MIN_BUF_CNT_THUMB;
			pDecComp->pOutputPort->stdPortDef.nBufferSize        = pDecComp->width*pDecComp->height*3/2;
			break;
		}

		//
		//	Private Extractor ( FFMPEGExtractor )
		//
		case OMX_IndexVideoDecoderExtradata:
		{
			OMX_U8 *pExtraData = (OMX_U8 *)ComponentParamStruct;
			OMX_S32 extraSize = *((OMX_S32*)pExtraData);

			if( pDecComp->pExtraData )
			{
				free(pDecComp->pExtraData);
				pDecComp->pExtraData = NULL;
				pDecComp->nExtraDataSize = 0;
			}
			pDecComp->nExtraDataSize = extraSize;

			pDecComp->pExtraData = (OMX_U8*)malloc( extraSize );
			memcpy( pDecComp->pExtraData, pExtraData+4, extraSize );
			break;
		}

		default :
			return NX_BaseSetParameter( hComp, nParamIndex, ComponentParamStruct );
	}
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_VidDec_UseBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_BUFFERHEADERTYPE         **pPortBuf = NULL;
	OMX_U32 i=0;

	DbgBuffer( "[%ld] %s() In.(PortNo=%ld), state(%d)\n", pDecComp->instanceId, __FUNCTION__, nPortIndex , pDecComp->eNewState);

	if( nPortIndex >= pDecComp->nNumPort ){
		return OMX_ErrorBadPortIndex;
	}

	if( OMX_StateIdle != pDecComp->eNewState && OMX_StateExecuting != pDecComp->eNewState )
		return OMX_ErrorIncorrectStateTransition;

	if( IN_PORT == nPortIndex ){
		pPort = pDecComp->pInputPort;
		pPortBuf = pDecComp->pInputBuffers;
	}else{
		pPort = pDecComp->pOutputPort;
		pPortBuf = pDecComp->pOutputBuffers;
	}

	// add hcjun(2017_10_25)
	// If an error occurs during FreeBuffer(buffer free), it is pending.
	for( i=0 ; i<pDecComp->nNumPort ; i++ )
	{
		if(pDecComp->bBufFreePend[i] == OMX_TRUE)
		{
			NX_PostSem(pDecComp->hBufFreeSem);
			pDecComp->bBufFreePend[i] = OMX_FALSE;
			DbgMsg("%s(): Waring. bBufFreePend: Port(%lu)\n", __func__,i );
		}
	}

	DbgBuffer( "%s() : pPort->stdPortDef.nBufferSize = %ld, nSizeBytes = %ld\n", __FUNCTION__, pPort->stdPortDef.nBufferSize, nSizeBytes);

	if( pDecComp->bUseNativeBuffer == OMX_FALSE && pPort->stdPortDef.nBufferSize > nSizeBytes )
		return OMX_ErrorBadParameter;

	for( i=0 ; i<pPort->stdPortDef.nBufferCountActual ; i++ ){
		if( NULL == pPortBuf[i] )
		{
			//	Allocate Actual Data
			pPortBuf[i] = (OMX_BUFFERHEADERTYPE*)NxMalloc( sizeof(OMX_BUFFERHEADERTYPE) );
			if( NULL == pPortBuf[i] )
				return OMX_ErrorInsufficientResources;
			NxMemset( pPortBuf[i], 0, sizeof(OMX_BUFFERHEADERTYPE) );
			pPortBuf[i]->nSize = sizeof(OMX_BUFFERHEADERTYPE);
			NX_OMXSetVersion( &pPortBuf[i]->nVersion );
			pPortBuf[i]->pBuffer = pBuffer;
			pPortBuf[i]->nAllocLen = nSizeBytes;
			pPortBuf[i]->pAppPrivate = pAppPrivate;
			pPortBuf[i]->pPlatformPrivate = pStdComp;
			if( OMX_DirInput == pPort->stdPortDef.eDir ){
				pPortBuf[i]->nInputPortIndex = pPort->stdPortDef.nPortIndex;
			}else{
				pPortBuf[i]->nOutputPortIndex = pPort->stdPortDef.nPortIndex;
				pDecComp->outUsableBuffers ++;
				DbgBuffer( "[%ld] %s() : outUsableBuffers(%ld), pPort->nAllocatedBuf(%ld), pBuffer(0x%08x)\n",
					pDecComp->instanceId, __FUNCTION__, pDecComp->outUsableBuffers, nSizeBytes, (unsigned int)pBuffer);
			}
			pPort->nAllocatedBuf ++;
			if( pPort->nAllocatedBuf == pPort->stdPortDef.nBufferCountActual ){
				pPort->stdPortDef.bPopulated = OMX_TRUE;
				pPort->eBufferType = NX_BUFHEADER_TYPE_USEBUFFER;
				NX_PostSem(pDecComp->hBufAllocSem);
			}
			*ppBufferHdr = pPortBuf[i];
			return OMX_ErrorNone;
		}
	}
	return OMX_ErrorInsufficientResources;
}

static OMX_ERRORTYPE NX_VidDec_AllocateBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** pBuffer, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_BUFFERHEADERTYPE         **pPortBuf = NULL;
	OMX_U32 i=0;

	if( nPortIndex >= pDecComp->nNumPort )
		return OMX_ErrorBadPortIndex;

	if( OMX_StateLoaded != pDecComp->eCurState || OMX_StateIdle != pDecComp->eNewState )
		return OMX_ErrorIncorrectStateTransition;

	if( IN_PORT == nPortIndex ){
		pPort = pDecComp->pInputPort;
		pPortBuf = pDecComp->pInputBuffers;
	}else{
		pPort = pDecComp->pOutputPort;
		pPortBuf = pDecComp->pOutputBuffers;
	}

	if( pPort->stdPortDef.nBufferSize > nSizeBytes )
		return OMX_ErrorBadParameter;

	for( i=0 ; i<pPort->stdPortDef.nBufferCountActual ; i++ ){
		if( NULL == pPortBuf[i] ){
			//	Allocate Actual Data
			pPortBuf[i] = NxMalloc( sizeof(OMX_BUFFERHEADERTYPE) );
			if( NULL == pPortBuf[i] )
				return OMX_ErrorInsufficientResources;
			NxMemset( pPortBuf[i], 0, sizeof(OMX_BUFFERHEADERTYPE) );
			pPortBuf[i]->nSize = sizeof(OMX_BUFFERHEADERTYPE);
			NX_OMXSetVersion( &pPortBuf[i]->nVersion );
			pPortBuf[i]->pBuffer = NxMalloc( nSizeBytes );
			if( NULL == pPortBuf[i]->pBuffer )
				return OMX_ErrorInsufficientResources;
			pPortBuf[i]->nAllocLen = nSizeBytes;
			pPortBuf[i]->pAppPrivate = pAppPrivate;
			pPortBuf[i]->pPlatformPrivate = pStdComp;
			if( OMX_DirInput == pPort->stdPortDef.eDir ){
				pPortBuf[i]->nInputPortIndex = pPort->stdPortDef.nPortIndex;
			}else{
				pPortBuf[i]->nOutputPortIndex = pPort->stdPortDef.nPortIndex;
			}
			pPort->nAllocatedBuf ++;
			if( pPort->nAllocatedBuf == pPort->stdPortDef.nBufferCountActual ){
				pPort->stdPortDef.bPopulated = OMX_TRUE;
				pPort->eBufferType = NX_BUFHEADER_TYPE_ALLOCATED;
				TRACE("%s(): port:%ld, %ld buffer allocated\n", __FUNCTION__, nPortIndex, pPort->nAllocatedBuf);
				NX_PostSem(pDecComp->hBufAllocSem);
			}
			*pBuffer = pPortBuf[i];
			return OMX_ErrorNone;
		}
	}
	pPort->stdPortDef.bPopulated = OMX_TRUE;
	*pBuffer = pPortBuf[0];
	return OMX_ErrorInsufficientResources;
}

static OMX_ERRORTYPE NX_VidDec_FreeBuffer (OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pStdComp->pComponentPrivate;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_BUFFERHEADERTYPE **pPortBuf = NULL;
	OMX_U32 i=0;

	DbgBuffer("[%ld] %s(), Port(%lu) IN\n", pDecComp->instanceId, __FUNCTION__, nPortIndex );

	if( nPortIndex >= pDecComp->nNumPort )
	{
		return OMX_ErrorBadPortIndex;
	}

	if( OMX_StateLoaded != pDecComp->eNewState && OMX_StateExecuting != pDecComp->eNewState )
	{
		SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventError, OMX_ErrorPortUnpopulated, nPortIndex, NULL );
	}

	// add hcjun(2017_10_25)
	// If an error occurs during UseBuffer(buffer allocation), it is pending.
	for( i=0 ; i<pDecComp->nNumPort ; i++ )
	{
		if(pDecComp->bBufAllocPend[i] == OMX_TRUE)
		{
			NX_PostSem(pDecComp->hBufAllocSem);
			pDecComp->bBufAllocPend[i] = OMX_FALSE;			
			DbgMsg("%s(): Waring. BufAllocPend: Port(%lu)\n", __func__,i );
		}
	}

	if( IN_PORT == nPortIndex ){
		pPort = pDecComp->pInputPort;
		pPortBuf = pDecComp->pInputBuffers;
	}else{
		pPort = pDecComp->pOutputPort;
		pPortBuf = pDecComp->pOutputBuffers;
	}

	//	If not transitioning to OMX_StateLoaded ????
	if( NULL == pBuffer ){
		return OMX_ErrorBadParameter;
	}

	for( i=0 ; i<pPort->stdPortDef.nBufferCountActual ; i++ ){
		if( NULL != pBuffer && pBuffer == pPortBuf[i] ){
			if( pPort->eBufferType == NX_BUFHEADER_TYPE_ALLOCATED ){
				NxFree(pPortBuf[i]->pBuffer);
				pPortBuf[i]->pBuffer = NULL;
			}
			NxFree(pPortBuf[i]);
			pPortBuf[i] = NULL;
			pPort->nAllocatedBuf --;

			DbgBuffer( "[%ld] %s() : pPort->nAllocatedBuf(%ld), pBuffer(0x%08x)\n",
					pDecComp->instanceId, __FUNCTION__, pPort->nAllocatedBuf, (unsigned int)pBuffer);
			if( 0 == pPort->nAllocatedBuf ){
				pPort->stdPortDef.bPopulated = OMX_FALSE;
				NX_PostSem(pDecComp->hBufFreeSem);
			}

			if(pPort->nAllocatedBuf == 0)
			{
				if(	pDecComp->bIsPortDisable == OMX_TRUE )
				{
					NX_PostSem( pDecComp->hPortCtrlSem );
				}
			}

			return OMX_ErrorNone;
		}
	}

	DbgBuffer("[%ld] %s() OUT\n", pDecComp->instanceId, __FUNCTION__ );

	return OMX_ErrorInsufficientResources;
}

static OMX_ERRORTYPE NX_VidDec_FillThisBuffer(OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer)
{
	OMX_S32 foundBuffer=1, i;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)((OMX_COMPONENTTYPE *)hComp)->pComponentPrivate;

	pthread_mutex_lock( &pDecComp->hBufMutex );

	FUNC_IN;

	//	Push data to command buffer
	assert( NULL != pDecComp->pOutputPort );
	assert( NULL != pDecComp->pOutputPortQueue );

	if( pDecComp->eCurState == pDecComp->eNewState ){
		if( !(OMX_StateIdle == pDecComp->eCurState || OMX_StatePause == pDecComp->eCurState || OMX_StateExecuting == pDecComp->eCurState) ){
			pthread_mutex_unlock( &pDecComp->hBufMutex );
			return OMX_ErrorIncorrectStateOperation;
		}
	}else{
		if( (OMX_StateIdle==pDecComp->eNewState) && (OMX_StateExecuting==pDecComp->eCurState || OMX_StatePause==pDecComp->eCurState) ){
			pthread_mutex_unlock( &pDecComp->hBufMutex );
			return OMX_ErrorIncorrectStateOperation;
		}
	}

	if( pDecComp->bEnableThumbNailMode == OMX_TRUE )
	{
		//	Normal Output Memory Mode
		if( 0 != NX_PushQueue( pDecComp->pOutputPortQueue, pBuffer ) ){
			pthread_mutex_unlock( &pDecComp->hBufMutex );
			NX_PostSem( pDecComp->hBufChangeSem );		//	Send Buffer Change Event
			return OMX_ErrorUndefined;
		}
		NX_PostSem( pDecComp->hBufChangeSem );		//	Send Buffer Change Event
	}
	else
	{
		//	Native Window Buffer Mode
		for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
		{
			//	Find Matching Buffer Pointer
			if( pDecComp->pOutputBuffers[i] == pBuffer )
			{
				if( pDecComp->outBufferValidFlag[i] )
				{
					if ( (OMX_FALSE == pDecComp->bInterlaced) && (OMX_FALSE == pDecComp->bOutBufCopy) )
					{
						NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, i );
					}

					pDecComp->outBufferValidFlag[i] = 0;
				}

				if( pDecComp->outBufferUseFlag[i] )
				{
					DbgMsg("===========> outBufferUseFlag Already We Have a Output Buffer!!!\n");
				}
				pDecComp->outBufferUseFlag[i] = 1;		//	Check
				pDecComp->curOutBuffers ++;
				DbgBuffer("curOutBuffers = %ld, index(%ld)", pDecComp->curOutBuffers, i);
				foundBuffer = 1;
			}
		}

		if( !foundBuffer )
		{
			DbgMsg("Discard Buffer = %p\n", pBuffer);
			pBuffer->nFilledLen = 0;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBuffer);
		}
		NX_PostSem( pDecComp->hBufChangeSem );		//	Send Buffer Change Event
	}
	FUNC_OUT;
	pthread_mutex_unlock( &pDecComp->hBufMutex );
	return OMX_ErrorNone;
}



//////////////////////////////////////////////////////////////////////////////
//
//						Command Execution Thread
//
static OMX_ERRORTYPE NX_VidDec_StateTransition( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_STATETYPE eCurState, OMX_STATETYPE eNewState )
{
	OMX_U32 i=0;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE *pPort = NULL;
	NX_QUEUE *pQueue = NULL;
	OMX_BUFFERHEADERTYPE *pBufHdr = NULL;
	OMX_BOOL bAbendState = OMX_FALSE;

	DBG_STATE( "%s() In : eCurState %d -> eNewState %d \n", __FUNCTION__, eCurState, eNewState );

	//	Check basic errors
	if( eCurState == eNewState ){
		ErrMsg("%s:line(%d) : %s() same state\n", __FILE__, __LINE__, __FUNCTION__ );
		return OMX_ErrorSameState;
	}
	if( OMX_StateInvalid==eCurState || OMX_StateInvalid==eNewState ){
		ErrMsg("%s:line(%d) : %s() Invalid state\n", __FILE__, __LINE__, __FUNCTION__ );
		return OMX_ErrorInvalidState;
	}

	if( OMX_StateLoaded == eCurState ){
		switch( eNewState )
		{
			//	CHECKME
			case OMX_StateIdle:
				//	Wait buffer allocation event
				for( i=0 ; i<pDecComp->nNumPort ; i++ ){
					pPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pDecComp->pPort[i];
					if( OMX_TRUE == pPort->bEnabled ){
						//	Note :
						//	Buffer Allocation must be done in the process of migrating to Loaded Idle.
						//	On Android, however, this process does not seem to go through.
						//	If it is originally standardized, it must wait until the buffer allocation is done in this process.
						pDecComp->bBufAllocPend[i] = OMX_TRUE;
						DbgBuffer( "%s() NX_PendSem(pDecComp->hBufAllocSem) (port(%lu) +++\n", __FUNCTION__, i);
						NX_PendSem(pDecComp->hBufAllocSem);
						DbgBuffer( "%s() NX_PendSem(pDecComp->hBufAllocSem) (port(%lu) ---\n", __FUNCTION__, i);
						pDecComp->bBufAllocPend[i] = OMX_FALSE;

						bAbendState = pDecComp->bAbendState;

						if( OMX_TRUE == bAbendState )
						{
							pDecComp->eCurState = eNewState;
							DbgBuffer( "%s() Waring: bAbendState == TRUE\n", __FUNCTION__);
							return eError;
						}

					}
					//
					//	TODO : Need exit check.
					//
				}
				//	Start buffer management thread
				pDecComp->eCmdBufThread = NX_THREAD_CMD_PAUSE;
				//	Create buffer control semaphore
				pDecComp->hBufCtrlSem = NX_CreateSem(0, 1);
				//	Create port control semaphore
				pDecComp->hPortCtrlSem = NX_CreateSem(0, 1);
				//	Create buffer mangement semaphore
				pDecComp->hBufChangeSem = NX_CreateSem(0, VPUDEC_VID_NUM_PORT*1024);
				//	Create buffer management thread
				pDecComp->eCmdBufThread = NX_THREAD_CMD_PAUSE;
				//	Clear EOS Flag
				pDecComp->bStartEoS = OMX_FALSE;
				//	Create buffer management thread
				pthread_create( &pDecComp->hBufThread, NULL, (void*)&NX_VidDec_BufferMgmtThread , pDecComp );

				//	Open FFMPEG Video Decoder
				DBG_STATE("Wait BufferCtrlSem");
				//NX_PendSem(pDecComp->hBufCtrlSem);

				//	Wait thread creation
				pDecComp->eCurState = eNewState;

				DBG_STATE("OMX_StateLoaded --> OMX_StateIdle");

				break;
			default:
				return OMX_ErrorIncorrectStateTransition;
		}
	}else if( OMX_StateIdle == eCurState ){
		switch( eNewState )
		{
			case OMX_StateLoaded:
			{
				//	Exit buffer management thread
				pDecComp->eCmdBufThread = NX_THREAD_CMD_EXIT;
				NX_PostSem( pDecComp->hBufChangeSem );
				NX_PostSem( pDecComp->hBufCtrlSem );
				NX_PostSem( pDecComp->hPortCtrlSem);
				pthread_join( pDecComp->hBufThread, NULL );
				NX_DestroySem( pDecComp->hBufChangeSem );
				NX_DestroySem( pDecComp->hBufCtrlSem );
				NX_DestroySem( pDecComp->hPortCtrlSem);
				pDecComp->hBufChangeSem = NULL;
				pDecComp->hBufCtrlSem = NULL;
				pDecComp->hPortCtrlSem = NULL;

				//	Wait buffer free
				for( i=0 ; i<pDecComp->nNumPort ; i++ ){
					pPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pDecComp->pPort[i];
					if( OMX_TRUE == pPort->bEnabled ){
						pDecComp->bBufFreePend[i] = OMX_TRUE;
						DbgBuffer( "%s() NX_PendSem(pDecComp->hBufFreeSem) (port(%lu) +++\n", __FUNCTION__, i);
						NX_PendSem(pDecComp->hBufFreeSem);
						DbgBuffer( "%s() NX_PendSem(pDecComp->hBufFreeSem) (port(%lu) ---\n", __FUNCTION__, i);
						pDecComp->bBufFreePend[i] = OMX_FALSE;

						bAbendState = pDecComp->bAbendState;

						if( OMX_TRUE == bAbendState )
						{
							pDecComp->eCurState = eNewState;
							DbgBuffer( "%s() Waring: bAbendState == TRUE\n", __FUNCTION__);
							return eError;
						}

					}
					//
					//	TODO : Need exit check.
					//
					if( OMX_DirOutput == pPort->eDir )
					{
						pDecComp->outUsableBuffers = 0;
					}
				}

				closeVideoCodec( pDecComp );

				pDecComp->eCurState = eNewState;
				pDecComp->bStartEoS = OMX_FALSE;
				DBG_STATE("OMX_StateIdle --> OMX_StateLoaded");
				break;
			}
			case OMX_StateExecuting:
				//	Step 1. Check in/out buffer queue.
				//	Step 2. If buffer is not ready in the queue, return error.
				//	Step 3. Craete buffer management thread.
				//	Step 4. Start buffer processing.
				pDecComp->eCmdBufThread = NX_THREAD_CMD_RUN;
				NX_PostSem( pDecComp->hBufCtrlSem );
				pDecComp->eCurState = eNewState;
				openVideoCodec( pDecComp );
				DBG_STATE("OMX_StateIdle --> OMX_StateExecuting");
				break;
			case OMX_StatePause:
				//	Step 1. Check in/out buffer queue.
				//	Step 2. If buffer is not ready in the queue, return error.
				//	Step 3. Craete buffer management thread.
				pDecComp->eCurState = eNewState;
				DBG_STATE("OMX_StateIdle --> OMX_StatePause");
				break;
			default:
				return OMX_ErrorIncorrectStateTransition;
		}
	}else if( OMX_StateExecuting == eCurState ){
		switch( eNewState )
		{
			case OMX_StateIdle:
				//	Step 1. Stop buffer processing
				pDecComp->eCmdBufThread = NX_THREAD_CMD_PAUSE;
				//	Write dummy
				NX_PostSem( pDecComp->hBufChangeSem );
				//	Step 2. Flushing buffers.
				//	Return buffer to supplier.
				pthread_mutex_lock( &pDecComp->hBufMutex );
				for( i=0 ; i<pDecComp->nNumPort ; i++ ){
					pPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pDecComp->pPort[i];
					pQueue = (NX_QUEUE*)pDecComp->pBufQueue[i];
					if( OMX_DirInput == pPort->eDir )
					{
						do{
							pBufHdr = NULL;
							if( 0 == NX_PopQueue( pQueue, (void**)&pBufHdr ) )
							{
								pDecComp->pCallbacks->EmptyBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBufHdr );
							}
							else
							{
								break;
							}
						}while(1);
					}
					else if( OMX_DirOutput == pPort->eDir )
					{
						//	Thumbnail Mode
						if( pDecComp->bEnableThumbNailMode == OMX_TRUE )
						{
							do{
								pBufHdr = NULL;
								if( 0 == NX_PopQueue( pQueue, (void**)&pBufHdr ) )
								{
									pDecComp->pCallbacks->FillBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBufHdr );
								}
								else
								{
									break;
								}
							}while(1);
						}
						else
						{
							//	Native Windowbuffer Mode
							for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
							{
								//	Find Matching Buffer Pointer
								if( pDecComp->outBufferUseFlag[i] )
								{
									pDecComp->pCallbacks->FillBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pDecComp->pOutputBuffers[i] );
									pDecComp->outBufferUseFlag[i] = 0;
									pDecComp->curOutBuffers --;
								}
							}
						}
					}
				}
				pthread_mutex_unlock( &pDecComp->hBufMutex );
				//	Step 3. Exit buffer management thread.
				pDecComp->eCurState = eNewState;
				DBG_STATE("OMX_StateExecuting --> OMX_StateIdle");
				break;
			case OMX_StatePause:
				//	Step 1. Stop buffer processing using buffer management semaphore
				pDecComp->eCurState = eNewState;
				DBG_STATE("OMX_StateExecuting --> OMX_StatePause");
				break;
			default:
				return OMX_ErrorIncorrectStateTransition;
		}
	}else if( OMX_StatePause==eCurState ){
		switch( eNewState )
		{
			case OMX_StateIdle:
				//	Step 1. Flushing buffers.
				//	Step 2. Exit buffer management thread.
				pDecComp->eCurState = eNewState;
				DBG_STATE("OMX_StatePause --> OMX_StateIdle");
				break;
			case OMX_StateExecuting:
				//	Step 1. Start buffer processing.
				pDecComp->eCurState = eNewState;
				DBG_STATE("OMX_StatePause --> OMX_StateExecuting");
				break;
			default:
				return OMX_ErrorIncorrectStateTransition;
		}
	}else if( OMX_StateWaitForResources==eCurState ){
		switch( eNewState )
		{
			case OMX_StateLoaded:
				DBG_STATE("OMX_StateWaitForResources --> OMX_StateLoaded");
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateIdle:
				DBG_STATE("OMX_StateWaitForResources --> OMX_StateIdle");
				pDecComp->eCurState = eNewState;
				break;
			default:
				return OMX_ErrorIncorrectStateTransition;
		}
	}else{
		//	Error
		return OMX_ErrorUndefined;
	}
	return eError;
}


static void FlushVideoInputPort( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_PTR pCmdData )
{
	OMX_BUFFERHEADERTYPE* pBuf = NULL;
	OMX_COMPONENTTYPE *pStdComp = pDecComp->hComp;
	DBG_FLUSH("Current Input QueueBuffer Size = %d\n", NX_GetQueueCnt(pDecComp->pInputPortQueue));
	do{
		if( NX_GetQueueCnt(pDecComp->pInputPortQueue) > 0 ){
			//	Flush buffer
			if( 0 == NX_PopQueue( pDecComp->pInputPortQueue, (void**)&pBuf ) )
			{
				pBuf->nFilledLen = 0;
				pDecComp->pCallbacks->EmptyBufferDone(pStdComp, pStdComp->pApplicationPrivate, pBuf);
			}
		}else{
			break;
		}
	}while(1);
	SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventCmdComplete, OMX_CommandFlush, VPUDEC_VID_INPORT_INDEX, pCmdData );
}

static void FlushVideoOutputPort( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_PTR pCmdData )
{
	OMX_BUFFERHEADERTYPE* pBuf = NULL;
	OMX_COMPONENTTYPE *pStdComp = pDecComp->hComp;
	if( OMX_TRUE == pDecComp->bEnableThumbNailMode )
	{
		do{
			if( NX_GetQueueCnt(pDecComp->pOutputPortQueue) > 0 ){
				//	Flush buffer
				NX_PopQueue( pDecComp->pOutputPortQueue, (void**)&pBuf );
				pDecComp->pCallbacks->FillBufferDone(pStdComp, pStdComp->pApplicationPrivate, pBuf);
			}else{
				break;
			}
		}while(1);
	}
	else
	{
		OMX_S32 i;
		DBG_FLUSH("++ pDecComp->curOutBuffers = %ld\n", pDecComp->curOutBuffers);
		for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
		{
			//	Find Matching Buffer Pointer
			if( pDecComp->outBufferUseFlag[i] )
			{
				pDecComp->pOutputBuffers[i]->nFilledLen = 0;
				pDecComp->pCallbacks->FillBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pDecComp->pOutputBuffers[i] );
				pDecComp->outBufferUseFlag[i] = 0;
				pDecComp->curOutBuffers -- ;
				DBG_FLUSH("pDecComp->pOutputBuffers[%2ld] = %p\n",  i, pDecComp->pOutputBuffers[i]);
			}

			if( pDecComp->outBufferValidFlag[i] )
			{
					pDecComp->outBufferValidFlag[i] = 0;
			}

		}
		DBG_FLUSH("-- pDecComp->curOutBuffers = %ld\n", pDecComp->curOutBuffers);
	}
	SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventCmdComplete, OMX_CommandFlush, VPUDEC_VID_OUTPORT_INDEX, pCmdData );
}



static void NX_VidDec_CommandProc( NX_BASE_COMPNENT *pBaseComp, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData )
{
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)pBaseComp;
	OMX_ERRORTYPE eError=OMX_ErrorNone;
	OMX_EVENTTYPE eEvent = OMX_EventCmdComplete;
	OMX_COMPONENTTYPE *pStdComp = pDecComp->hComp;
	OMX_U32 nData1 = 0, nData2 = 0;

	TRACE("%s() : In( Cmd = %d )\n", __FUNCTION__, Cmd );

	switch( Cmd )
	{
		//	If the component successfully transitions to the new state,
		//	it notifies the IL client of the new state via the OMX_EventCmdComplete event,
		//	indicating OMX_CommandStateSet for nData1 and the new state for nData2.
		case OMX_CommandStateSet:    // Change the component state
		{
			if( pDecComp->eCurState == nParam1 ){
				//
				eEvent = OMX_EventError;
				nData1 = OMX_ErrorSameState;
				break;
			}

			eError = NX_VidDec_StateTransition( pDecComp, pDecComp->eCurState, nParam1 );
			nData1 = OMX_CommandStateSet;
			nData2 = nParam1;				//	Must be set new state (OMX_STATETYPE)
			TRACE("%s : OMX_CommandStateSet(nData1=%ld, nData2=%ld)\n", __FUNCTION__, nData1, nData2 );
			break;
		}
		case OMX_CommandFlush:       // Flush the data queue(s) of a component
		{
			DBG_FLUSH("%s() : Flush( nParam1=%ld )\n", __FUNCTION__, nParam1 );
			DBG_FLUSH("%s() : Flush lock ++\n", __FUNCTION__ );
			pthread_mutex_lock( &pDecComp->hBufMutex );

			//	Input Port Flushing
			if( nParam1 == VPUDEC_VID_INPORT_INDEX || nParam1 == OMX_ALL )
			{
				FlushVideoInputPort( pDecComp, pCmdData );
			}
			//	Input Port Flushing
			if( nParam1 == VPUDEC_VID_OUTPORT_INDEX || nParam1 == OMX_ALL )
			{
				processEOSforFlush(pDecComp);
				FlushVideoOutputPort(pDecComp, pCmdData );
			}
			if( nParam1 == OMX_ALL )	//	Output Port Flushing
			{
				SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventCmdComplete, OMX_CommandFlush, OMX_ALL, pCmdData );
			}

			pDecComp->bNeedKey = OMX_TRUE;
			pDecComp->bFlush = OMX_TRUE;
			pDecComp->bStartEoS = OMX_FALSE;
			pthread_mutex_unlock( &pDecComp->hBufMutex );
			DBG_FLUSH("%s() : Flush unlock --\n", __FUNCTION__ );
			return ;
		}
		//	Openmax spec v1.1.2 : 3.4.4.3 Non-tunneled Port Disablement and Enablement.
		case OMX_CommandPortDisable: // Disable a port on a component.
		{
			//	Check Parameter
			if( OMX_ALL != nParam1 && nParam1>=VPUDEC_VID_NUM_PORT ){
				//	Bad parameter
				eEvent = OMX_EventError;
				nData1 = OMX_ErrorBadPortIndex;
				ErrMsg(" Errror : OMX_ErrorBadPortIndex(%ld) : %s: %d", nParam1, __FILE__, __LINE__);
				break;
			}
			pthread_mutex_lock( &pDecComp->hBufMutex );
			//	Step 1. The component shall return the buffers with a call to EmptyBufferDone/FillBufferDone,
			//NX_PendSem( pDecComp->hBufCtrlSem );
			if( 0 == nParam1 )
			{
				pDecComp->pInputPort->stdPortDef.bEnabled = OMX_FALSE;
				FlushVideoInputPort(pDecComp, pCmdData );
			}
			else if ( 1 == nParam1 )
			{
				OMX_BUFFERHEADERTYPE *pBuf;
				if(OMX_TRUE == pDecComp->bEnableThumbNailMode)
				{
					do{
						if(NX_GetQueueCnt(pDecComp->pOutputPortQueue) > 0){
							//	Flush buffer
							NX_PopQueue(pDecComp->pOutputPortQueue,(void**)&pBuf);
							pDecComp->pCallbacks->FillBufferDone(pStdComp,pStdComp->pApplicationPrivate,pBuf);
						}
						else{
							break;
						}
					} while(1);
				}
				else
				{
					OMX_S32 i;
					DBG_FLUSH("++ pDecComp->curOutBuffers = %ld\n",pDecComp->curOutBuffers);
					for(i=0 ; i<NX_OMX_MAX_BUF ; i++)
					{
						//	Find Matching Buffer Pointer
						if(pDecComp->outBufferUseFlag[i])
						{
							pDecComp->pOutputBuffers[i]->nFilledLen = 0;
							pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp,pDecComp->hComp->pApplicationPrivate,pDecComp->pOutputBuffers[i]);
							pDecComp->outBufferUseFlag[i] = 0;
							pDecComp->curOutBuffers -- ;
							DBG_FLUSH("pDecComp->pOutputBuffers[%2ld] = %p\n",i,pDecComp->pOutputBuffers[i]);
						}
					}
					DBG_FLUSH("-- pDecComp->curOutBuffers = %ld\n",pDecComp->curOutBuffers);
				}
				pDecComp->pOutputPort->stdPortDef.bEnabled = OMX_FALSE;
				pDecComp->outUsableBuffers = 0;

			}
			else if( OMX_ALL == nParam1 ){
				//	Disable all ports
				FlushVideoInputPort(pDecComp, pCmdData );
				FlushVideoOutputPort(pDecComp, pCmdData );
				pDecComp->pInputPort->stdPortDef.bEnabled = OMX_FALSE;
				pDecComp->pOutputPort->stdPortDef.bEnabled = OMX_FALSE;
			}
			eEvent = OMX_EventCmdComplete;
			nData1 = OMX_CommandPortDisable;
			nData2 = nParam1;
			//NX_PostSem( pDecComp->hBufCtrlSem );
			pDecComp->bIsPortDisable = OMX_TRUE;
			
			if( 0 != pDecComp->pOutputPort->nAllocatedBuf)
			{
				NX_PendSem( pDecComp->hPortCtrlSem );
			}

			pDecComp->bIsPortDisable = OMX_FALSE;
			pthread_mutex_unlock( &pDecComp->hBufMutex );

			break;
		}
		case OMX_CommandPortEnable:  // Enable a port on a component.
		{
			//NX_PendSem( pDecComp->hBufCtrlSem );
			if( nParam1 == 0 )
				pDecComp->pInputPort->stdPortDef.bEnabled = OMX_TRUE;
			else if( nParam1 == 1 )
				pDecComp->pOutputPort->stdPortDef.bEnabled = OMX_TRUE;
			else if( nParam1 == OMX_ALL )
			{
				pDecComp->pInputPort->stdPortDef.bEnabled = OMX_TRUE;
				pDecComp->pOutputPort->stdPortDef.bEnabled = OMX_TRUE;
			}
			eEvent = OMX_EventCmdComplete;
			nData1 = OMX_CommandPortEnable;
			nData2 = nParam1;

			if( pDecComp->eNewState == OMX_StateExecuting ){
				//openVideoCodec(pDecComp);
			}
			NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		case OMX_CommandMarkBuffer:  // Mark a component/buffer for observation
		{
			//NX_PendSem( pDecComp->hBufCtrlSem );
			TRACE("%s : OMX_CommandMarkBuffer \n", __FUNCTION__);
			//NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		default:
		{
			DbgMsg("%s() : Unknown command( Cmd = %d )\n", __FUNCTION__, Cmd );
			break;
		}
	}

	SendEvent( (NX_BASE_COMPNENT*)pDecComp, eEvent, nData1, nData2, pCmdData );
}


//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//						Buffer Management Thread
//

//
//	NOTE :
//		1. Input buffer is not processed when EOS is input.
//
static int NX_VidDec_Transform(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	return decodeVideoFrame( pDecComp, pInQueue, pOutQueue );
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Buffer control semaphore is synchronize state of component and state of buffer thread.
//
//	Transition Action
//	1. OMX_StateLoaded   --> OMX_StateIdle      ;
//		==> Buffer management thread create, initialize buf change sem & buf control sem,
//			thread state set to NX_THREAD_CMD_PAUSE
//	2. OMX_StateIdle     --> OMX_StateExecuting ;
//		==> Set state to NX_THREAD_CMD_RUN and post control semaphore
//	3. OMX_StateIdle     --> OMX_StatePause     ;
//		==> Noting.
//	4. OMX_StatePause    --> OMX_StateExecuting ;
//		==> Set state to NX_THREAD_CMD_RUN and post control semaphore
//	5. OMX_StateExcuting --> OMX_StatePause     ;
//		==> Set state to NX_THREAD_CMD_PAUSE and post dummy buf change semaphore
//	6. OMX_StateExcuting --> OMX_StateIdle      ;
//		==> Set state to NX_THREAD_CMD_PAUSE and post dummy buf change semaphore and
//			return all buffers on the each port.
//	7. OMX_StateIdle     --> OMX_Loaded         ;
//
/////////////////////////////////////////////////////////////////////////////////////////////////////
static void NX_VidDec_BufferMgmtThread( void *arg )
{
	int err = 0;
	NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp = (NX_VIDDEC_VIDEO_COMP_TYPE *)arg;
	TRACE( "enter %s() loop\n", __FUNCTION__ );

	NX_PostSem( pDecComp->hBufCtrlSem );					//	Thread Creation Semaphore
	while( NX_THREAD_CMD_EXIT != pDecComp->eCmdBufThread )
	{
		TRACE("[%ld] ========================> Wait Buffer Control Signal!!!\n", pDecComp->instanceId);
		NX_PendSem( pDecComp->hBufCtrlSem );				//	Thread Control Semaphore
		TRACE("[%ld] ========================> Released Buffer Control Signal!!!\n", pDecComp->instanceId);
		while( NX_THREAD_CMD_RUN == pDecComp->eCmdBufThread ){
			NX_PendSem( pDecComp->hBufChangeSem );			//	wait buffer management command
			if( NX_THREAD_CMD_RUN != pDecComp->eCmdBufThread ){
				break;
			}

			if( pDecComp->pOutputPort->stdPortDef.bEnabled != OMX_TRUE || pDecComp->pInputPort->stdPortDef.bEnabled != OMX_TRUE )
			{
				//	Break Out & Wait Thread Control Signal
				NX_PostSem( pDecComp->hBufChangeSem );
				break;
			}

			pthread_mutex_lock( &pDecComp->hBufMutex );
			if( pDecComp->bEnableThumbNailMode == OMX_TRUE )
			{
				//	check decoding
				if( pDecComp->bStartEoS == OMX_FALSE && NX_GetQueueCnt( pDecComp->pInputPortQueue ) > 0 && NX_GetQueueCnt( pDecComp->pOutputPortQueue ) > 0 ) {
					if( 0 != (err=NX_VidDec_Transform(pDecComp, pDecComp->pInputPortQueue, pDecComp->pOutputPortQueue)) )
					{
						pthread_mutex_unlock( &pDecComp->hBufMutex );
						ErrMsg(("NX_VidDec_Transform failed!!!\n"));
						goto ERROR_EXIT;
					}
				}
				if( pDecComp->bStartEoS == OMX_TRUE && NX_GetQueueCnt( pDecComp->pOutputPortQueue ) > 0 )
				{
					processEOS( pDecComp );
				}
			}
			else
			{
				// ALOGD("pDecComp->curOutBuffers=%d, pDecComp->minRequiredFrameBuffer=%d\n", pDecComp->curOutBuffers, pDecComp->minRequiredFrameBuffer);
				//	check decoding
				if( pDecComp->bStartEoS == OMX_FALSE && NX_GetQueueCnt( pDecComp->pInputPortQueue ) > 0 && (pDecComp->curOutBuffers > pDecComp->minRequiredFrameBuffer) ) {
					if( 0 != (err=NX_VidDec_Transform(pDecComp, pDecComp->pInputPortQueue, pDecComp->pOutputPortQueue)) )
					{
						pthread_mutex_unlock( &pDecComp->hBufMutex );
						ErrMsg(("NX_VidDec_Transform failed!!!\n"));
						goto ERROR_EXIT;
					}
				}
				if( pDecComp->bStartEoS == OMX_TRUE && (pDecComp->curOutBuffers > pDecComp->minRequiredFrameBuffer) )
				{
					processEOS( pDecComp );
				}
			}
			pthread_mutex_unlock( &pDecComp->hBufMutex );
		}
	}

ERROR_EXIT:
	//	Release or Return buffer's
	TRACE("exit buffer management thread.\n");
	if( err != 0 )
	{
		SendEvent( (NX_BASE_COMPNENT*)pDecComp, OMX_EventError, 0, 0, NULL );
	}
}

//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//					FFMPEG Video Decoder Handler
//

void CodecTagToMp4Class( OMX_S32 codecTag, OMX_S32 *codecId, OMX_S32 *mp4Class )
{
	*mp4Class = 0;

	switch( codecTag )
	{
		case MKTAG('D','I','V','X'):case MKTAG('d','i','v','x'):
			*mp4Class = 5;
			*codecId = NX_DIVX_DEC;
			break;
		case MKTAG('D','I','V','4'):case MKTAG('d','i','v','4'):
			*mp4Class = 5;
			*codecId = NX_DIV4_DEC;
			break;
		case MKTAG('D','X','5','0'):case MKTAG('d','x','5','0'):
		case MKTAG('D','I','V','5'):case MKTAG('d','i','v','5'):
			*codecId = NX_DIV5_DEC;
		case MKTAG('D','I','V','6'):case MKTAG('d','i','v','6'):
			*mp4Class = 1;
			*codecId = NX_DIV6_DEC;
			break;
		case MKTAG('X','V','I','D'):case MKTAG('x','v','i','d'):
			*mp4Class = 2;
			*codecId = NX_XVID_DEC;
			break;
		case MKTAG('F','L','V','1'):case MKTAG('f','l','v','1'):
			*mp4Class = 256;
			break;
		case MKTAG('D','I','V','3'):case MKTAG('d','i','v','3'):
		case MKTAG('M','P','4','3'):case MKTAG('m','p','4','3'):
		case MKTAG('M','P','G','3'):case MKTAG('m','p','g','3'):
		case MKTAG('D','V','X','3'):case MKTAG('d','v','x','3'):
		case MKTAG('A','P','4','1'):case MKTAG('a','p','4','1'):
			*codecId = NX_DIV3_DEC;
			break;
		case MKTAG('R','V','3','0'):case MKTAG('r','v','3','0'):
			*codecId = NX_RV8_DEC;
			break;
		case MKTAG('R','V','4','0'):case MKTAG('r','v','4','0'):
			*codecId = NX_RV9_DEC;
			break;
		case MKTAG('W','M','V','3'):case MKTAG('w','m','v','3'):
			*codecId = NX_WMV9_DEC;
			break;
		case MKTAG('W','V','C','1'):case MKTAG('w','v','c','1'):
			*codecId = NX_WVC1_DEC;
			break;
		default:
			*mp4Class = 0;
			break;
	}
}

//
//
//			Codec Management Functions
//
//

//	0 is OK, other Error.
int openVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	OMX_S32 mp4Class = 0;
	FUNC_IN;

	pDecComp->isOutIdr = OMX_FALSE;

	//	FIXME : Move to Port SetParameter Part
	CodecTagToMp4Class( pDecComp->codecTag, &pDecComp->videoCodecId, &mp4Class );

	switch( pDecComp->videoCodecId )
	{
		case NX_AVC_DEC:
			pDecComp->DecodeFrame = NX_DecodeAvcFrame;
			break;
		case NX_MP2_DEC:
			pDecComp->DecodeFrame = NX_DecodeMpeg2Frame;
			break;
		case NX_MP4_DEC:
		case NX_H263_DEC:
		case NX_FLV_DEC:
		case NX_DIV4_DEC:
		case NX_DIV5_DEC:
		case NX_DIV6_DEC:
		case NX_DIVX_DEC:
			pDecComp->DecodeFrame = NX_DecodeMpeg4Frame;
			break;
		case NX_XVID_DEC:
			pDecComp->DecodeFrame = NX_DecodeXvidFrame;
			break;
		case NX_DIV3_DEC:
			pDecComp->DecodeFrame = NX_DecodeDiv3Frame;
			break;
		case NX_WVC1_DEC:
		case NX_WMV9_DEC:
			pDecComp->DecodeFrame = NX_DecodeVC1Frame;
			break;
		case NX_RV8_DEC:
		case NX_RV9_DEC:
			pDecComp->DecodeFrame = NX_DecodeRVFrame;
			break;
		case NX_VP8_DEC:
			pDecComp->DecodeFrame = NX_DecodeVP8Frame;
			break;
		default:
			pDecComp->DecodeFrame = NULL;
			break;
	}

	DbgMsg("CodecID = %ld, mp4Class = %ld, Tag = %c%c%c%c\n\n", pDecComp->videoCodecId, mp4Class,
		(char)(pDecComp->codecTag&0xFF),
		(char)((pDecComp->codecTag>>8)&0xFF),
		(char)((pDecComp->codecTag>>16)&0xFF),
		(char)((pDecComp->codecTag>>24)&0xFF) );
	pDecComp->hVpuCodec = NX_V4l2DecOpen( pDecComp->videoCodecId );

	if( NULL == pDecComp->hVpuCodec ){
		ErrMsg("%s(%d) NX_V4l2DecOpen() failed.(CodecID=%ld)\n", __FILE__, __LINE__, pDecComp->videoCodecId);
		FUNC_OUT;
		return -1;
	}
	InitVideoTimeStamp( pDecComp );

	pDecComp->tmpInputBufferIndex = 0;

	FUNC_OUT;
	return 0;
}

void closeVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	FUNC_IN;
	if( NULL != pDecComp->hVpuCodec ){
		if ( NULL != pDecComp->hDeinterlace )
		{
			pDecComp->bInterlaced = OMX_FALSE;
			pDecComp->hDeinterlace = NULL;
		}

		if(pDecComp->bOutBufCopy)
		{
			if(pDecComp->hScaler)
			{
				nx_scaler_close(pDecComp->hScaler);
				pDecComp->hScaler = 0;
			}
		}

		if(pDecComp->bInitialized == OMX_TRUE)
		{
 			NX_V4l2DecFlush( pDecComp->hVpuCodec );
		}
		NX_V4l2DecClose( pDecComp->hVpuCodec );
		pDecComp->bInitialized = OMX_FALSE;
		pDecComp->bNeedKey = OMX_TRUE;
		pDecComp->hVpuCodec = NULL;
		pDecComp->isOutIdr = OMX_FALSE;
		if(pDecComp->bPortReconfigure)
			pDecComp->bPortReconfigure = OMX_FALSE;
		else
			pDecComp->codecSpecificDataSize = 0;
		pDecComp->bNeedSequenceData = 0;
		FUNC_OUT;
	}
	FUNC_OUT;
}

int flushVideoCodec(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	FUNC_IN;
	InitVideoTimeStamp(pDecComp);
	if( NULL != pDecComp->hVpuCodec )
	{
		if(pDecComp->bInitialized == OMX_TRUE)
		{
			NX_V4l2DecFlush( pDecComp->hVpuCodec );
		}
		pDecComp->isOutIdr = OMX_FALSE;
	}
	FUNC_OUT;
	return 0;
}

int InitializeCodaVpu(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, unsigned char *buf, int size)
{
	int ret = -1;
	FUNC_IN;
	if( pDecComp->hVpuCodec )
	{
		int32_t iNumCurRegBuf, i;
		int32_t nativeBufWidth = 0;
		int32_t nativeBufHeight = 0;
		NX_V4L2DEC_SEQ_IN seqIn;
		NX_V4L2DEC_SEQ_OUT seqOut;
		memset( &seqIn, 0, sizeof(seqIn) );
		memset( &seqOut, 0, sizeof(seqOut) );
		seqIn.width   = pDecComp->width;
		seqIn.height  = pDecComp->height;
		seqIn.seqBuf = buf;
		seqIn.seqSize = size;

		iNumCurRegBuf = pDecComp->outUsableBuffers;

		if ( VID_ERR_NONE != (ret = NX_V4l2DecParseVideoCfg( pDecComp->hVpuCodec, &seqIn, &seqOut )) )
		{
			ALOGE("%s : NX_V4l2DecParseVideoCfg() is failed!!,(ret=%d)\n", __func__, ret);
			ret = VID_ERR_INIT;
			return ret;
		}

		if( pDecComp->bUseNativeBuffer == OMX_TRUE )
		{
			DbgMsg("[%ld] Native Buffer Mode : iNumCurRegBuf=%d, ExtraSize = %ld, MAX_FRAME_BUFFER_NUM = %d\n",
				pDecComp->instanceId, iNumCurRegBuf, pDecComp->codecSpecificDataSize, MAX_FRAME_BUFFER_NUM );

#if OUT_BUF_COPY
			pDecComp->bOutBufCopy = OMX_TRUE;
			DbgMsg("%s : OutBufCopy Mode\n", __func__);
#else
			pDecComp->bOutBufCopy = OMX_FALSE;

			char value[PROPERTY_VALUE_MAX];
			if (property_get("videobufcopy.mode", value, "0") )
			{
				if ( !strcmp(value, "1") )
				{
					pDecComp->bOutBufCopy = OMX_TRUE;
					DbgMsg("%s : OutBufCopy Mode\n", __func__);
				}
			}
#endif
			//	Translate Gralloc Memory Buffer Type To Nexell Video Memory Type
			for( i=0 ; i<iNumCurRegBuf ; i++ )
			{
				struct private_handle_t const *handle = NULL;
				handle = (struct private_handle_t const *)pDecComp->pOutputBuffers[i]->pBuffer;

				ALOGV("%s: handle->width = %d, handle->height = %d \n", __func__, handle->width, handle->height);
				nativeBufWidth = handle->width;
				nativeBufHeight = handle->height;

				if( handle == NULL )
				{
					ALOGE("%s: Invalid Buffer!!!\n", __func__);
				}
				NX_PrivateHandleToVideoMemory(handle, &pDecComp->vidFrameBuf[i]);

				pDecComp->hVidFrameBuf[i] = &pDecComp->vidFrameBuf[i];
			}

			if ( (OMX_FALSE == pDecComp->bInterlaced) && (OMX_FALSE == pDecComp->bOutBufCopy) )
			{
				seqIn.numBuffers = iNumCurRegBuf;
				seqIn.imgFormat	= pDecComp->vidFrameBuf[0].format;
				seqIn.pMemHandle = &pDecComp->hVidFrameBuf[0];
			}
			else if ( OMX_TRUE == pDecComp->bInterlaced )
			{
				seqIn.imgFormat = V4L2_PIX_FMT_YVU420;
				seqIn.numBuffers = iNumCurRegBuf - seqOut.minBuffers;
			}
			else  //OMX_TRUE == pDecComp->bOutBufCopy
			{
				pDecComp->hScaler = scaler_open();
				if(pDecComp->hScaler < 0)
				{
					ALOGE("%s : Nscaler_open is failed!!,(hScaler=%ld)\n", __func__, pDecComp->hScaler);
					ret = VID_ERR_INIT;
					return ret;
				}
				seqIn.imgFormat = V4L2_PIX_FMT_YVU420;
				seqIn.numBuffers = iNumCurRegBuf - seqOut.minBuffers;
			}
		}
		else
		{
			seqIn.imgFormat = V4L2_PIX_FMT_YVU420;
			seqIn.numBuffers = seqOut.minBuffers + 3;
		}

		seqIn.width = seqOut.width;
		seqIn.height = seqOut.height;
		seqIn.imgPlaneNum = 3;

		ret = NX_V4l2DecInit( pDecComp->hVpuCodec, &seqIn );
		pDecComp->bInitialized = OMX_TRUE;
		if(VID_ERR_NONE != ret)
		{
			ALOGE("%s : NX_V4l2DecInit() is failed!!,(ret=%d)\n", __func__, ret);
			ret = VID_ERR_INIT;
			return ret;
		}

		pDecComp->minRequiredFrameBuffer = seqOut.minBuffers;
		pDecComp->outUsableBufferIdx = -1;
		DbgMsg("[%ld] <<<<<<<<<< InitializeCodaVpu(Min=%ld, %dx%d) (ret = %d) >>>>>>>>>\n",
			pDecComp->instanceId, pDecComp->minRequiredFrameBuffer, seqOut.width, seqOut.height, ret );


		//	Update Output Port Information
		{
			OMX_PARAM_PORTDEFINITIONTYPE *pOutPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pDecComp->pPort[VPUDEC_VID_OUTPORT_INDEX];
			OMX_VIDEO_PORTDEFINITIONTYPE *pVideoFormat = &pOutPort->format.video;
			pVideoFormat->eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YV12;
			pVideoFormat->cMIMEType = "video/raw";
			pVideoFormat->nFrameWidth = seqOut.dispInfo.dispRight;
			pVideoFormat->nFrameHeight = seqOut.dispInfo.dispBottom;
			pDecComp->dsp_width = seqOut.dispInfo.dispRight;
			pDecComp->dsp_height = seqOut.dispInfo.dispBottom;
			if( OMX_TRUE == pDecComp->bUseNativeBuffer )
			{
				pVideoFormat->nStride = ALIGN(seqOut.width, 32);
				pVideoFormat->nSliceHeight = ALIGN(seqOut.height, 16);

				if( (seqOut.width != nativeBufWidth) || (seqOut.height != nativeBufHeight) )
				{
					int32_t diff = seqOut.height - seqOut.dispInfo.dispBottom;
					DbgMsg(">>>>>><<< diff(%d) = seqOut.height(%d) - seqOut.dispInfo.dispBottom(%d)\n",diff, seqOut.height, seqOut.dispInfo.dispBottom);
					if(16 <= diff)	//16 align <= diff
					{
						DbgMsg("[%ld] <<< seqOut.width(%d) != nativeBufWidth(%d), seqOut.height(%d) != nativeBufHeight(%d)\n",
							pDecComp->instanceId, seqOut.width, nativeBufWidth, seqOut.height, nativeBufHeight);
						pVideoFormat->nFrameWidth = seqOut.width;
						pVideoFormat->nFrameHeight = seqOut.height;
						pDecComp->bPortReconfigure = OMX_TRUE;
					}
				}
			}
			else
			{
				pVideoFormat->nStride = seqOut.width;
				pVideoFormat->nSliceHeight = seqOut.height;
			}
		}
	}
	FUNC_OUT;
	return ret;
}

void InitVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	OMX_S32 i;
	for( i=0 ;i<NX_OMX_MAX_BUF; i++ )
	{
		pDecComp->outTimeStamp[i].flag = (OMX_U32)-1;
	}
}

void PushVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS timestamp, OMX_U32 flag )
{
	OMX_S32 i=0;
	for( i=0 ;i<NX_OMX_MAX_BUF; i++ )
	{
		if( pDecComp->outTimeStamp[i].flag == (OMX_U32)-1 )
		{
			pDecComp->outTimeStamp[i].timestamp = timestamp;
			pDecComp->outTimeStamp[i].flag = flag;
			break;
		}
	}
}

int PopVideoTimeStamp(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, OMX_TICKS *timestamp, OMX_U32 *flag )
{
	OMX_S32 i=0;
	OMX_TICKS minTime = 0x7FFFFFFFFFFFFFFFll;
	OMX_S32 minIdx = -1;
	for( i=0 ;i<NX_OMX_MAX_BUF; i++ )
	{
		if( pDecComp->outTimeStamp[i].flag != (OMX_U32)-1 )
		{
			if( minTime > pDecComp->outTimeStamp[i].timestamp )
			{
				minTime = pDecComp->outTimeStamp[i].timestamp;
				minIdx = i;
			}
		}
	}
	if( minIdx != -1 )
	{
		*timestamp = pDecComp->outTimeStamp[minIdx].timestamp;
		*flag      = pDecComp->outTimeStamp[minIdx].flag;
		pDecComp->outTimeStamp[minIdx].flag = (OMX_U32)-1;
		return 0;
	}
	else
	{
		DbgMsg("Cannot Found Time Stamp!!!");
		return -1;
	}
}


//	0 is OK, other Error.
int decodeVideoFrame(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	if( pDecComp->DecodeFrame )
	{
		return pDecComp->DecodeFrame( pDecComp, pInQueue, pOutQueue );
	}
	else{
		return 0;
	}
}

int processEOS(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	OMX_BUFFERHEADERTYPE *pOutBuf = NULL;
	OMX_S32 i;
	OMX_S32 ret;
	NX_V4L2DEC_IN decIn;
	NX_V4L2DEC_OUT decOut;

	if( pDecComp->hVpuCodec && pDecComp->bInitialized )
	{
		decIn.strmBuf = 0;
		decIn.strmSize = 0;
		decIn.timeStamp = 0;
		decIn.eos = 1;

		ret = NX_V4l2DecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
		if( ret==VID_ERR_NONE && decOut.dispIdx >= 0 && ( decOut.dispIdx < NX_OMX_MAX_BUF ) )
		{
			int32_t outIdx = 0;
			if( (OMX_FALSE == pDecComp->bInterlaced) && (OMX_FALSE == pDecComp->bOutBufCopy) )
			{
				outIdx = decOut.dispIdx;
			}
			else // TRUE == bInterlaced,bOutBufCopy
			{
				outIdx = GetUsableBufferIdx(pDecComp);
			}

      		pOutBuf = pDecComp->pOutputBuffers[outIdx];

			if( pDecComp->bEnableThumbNailMode == OMX_TRUE )
			{
				//	Thumbnail Mode
				NX_VID_MEMORY_INFO *pImg = &decOut.hImg;
				NX_PopQueue( pDecComp->pOutputPortQueue, (void**)&pOutBuf );

				CopySurfaceToBufferYV12( (OMX_U8 *)pImg->pBuffer[0], pOutBuf->pBuffer, pDecComp->width, pDecComp->height );

				NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );

				pOutBuf->nFilledLen = pDecComp->width * pDecComp->height * 3 / 2;
			}
			else
			{
				//	Native Window Buffer Mode
				//	Get Output Buffer Pointer From Output Buffer Pool
				if( pDecComp->outBufferUseFlag[outIdx] == 0 )
				{
					NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
					DbgMsg("[EOS]Unexpected Buffer Handling!!!! Goto Exit\n");

					for(i=0; i<NX_OMX_MAX_BUF; i++)
					{
						outIdx = GetUsableBufferIdx(pDecComp);
						if( pDecComp->outBufferUseFlag[outIdx] != 0 )
						{
							DbgMsg("[EOS] DummyBuf Send !!!\n");
							pOutBuf = pDecComp->pOutputBuffers[outIdx];
							pDecComp->outBufferUseFlag[outIdx] = 0;
							pDecComp->curOutBuffers --;
							pOutBuf->nFilledLen = 0;
							break;
						}
					}
				}
				else
				{
					pDecComp->outBufferUseFlag[outIdx] = 0;
					pDecComp->curOutBuffers --;

					pOutBuf->nFilledLen = sizeof(struct private_handle_t);
				}

				if( OMX_TRUE == pDecComp->bInterlaced )
				{
					DeInterlaceFrame( pDecComp, &decOut );
				}
				else if( OMX_TRUE == pDecComp->bOutBufCopy )
				{
					OutBufCopy( pDecComp, &decOut );
				}
			}

			if( 0 != PopVideoTimeStamp(pDecComp, &pOutBuf->nTimeStamp, &pOutBuf->nFlags )  )
			{
				pOutBuf->nTimeStamp = -1;
				pOutBuf->nFlags     = OMX_BUFFERFLAG_ENDOFFRAME;	//	Frame Flag
			}
			TRACE("[%6ld/%6ld]Send Buffer after EOS Receive( Delayed Frame. ), Flag = 0x%08x\n",
				pDecComp->inFrameCount, pDecComp->outFrameCount, (unsigned int)pOutBuf->nFlags);
			pDecComp->outFrameCount++;
			pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);

			return 0;
		}
	}

	//	Real EOS Send
	if ( pDecComp->bEnableThumbNailMode == OMX_TRUE )
	{
		if( NX_GetQueueCnt(pDecComp->pOutputPortQueue) > 0)
		{
			if (NX_PopQueue( pDecComp->pOutputPortQueue, (void**)&pOutBuf ) == 0)
			{
				pOutBuf->nTimeStamp = 0;
				pOutBuf->nFlags = OMX_BUFFERFLAG_EOS;
				pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
			}
		}
	}
	else
	{
		for( i=0 ; i<NX_OMX_MAX_BUF ; i++ )
		{
			if( pDecComp->outBufferUseFlag[i] )
			{
				pOutBuf = pDecComp->pOutputBuffers[i];
				pDecComp->outBufferUseFlag[i] = 0;
				pDecComp->curOutBuffers --;
				pOutBuf->nFilledLen = 0;
				pOutBuf->nTimeStamp = 0;
				pOutBuf->nFlags     = OMX_BUFFERFLAG_EOS;
				pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
				break;
			}
		}
	}
	return 0;
}


int processEOSforFlush(NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp)
{
	if( pDecComp->hVpuCodec && pDecComp->bInitialized )
	{
		int32_t ret;
		NX_V4L2DEC_IN decIn;
		NX_V4L2DEC_OUT decOut;
		do{
			decIn.strmBuf = 0;
			decIn.strmSize = 0;
			decIn.timeStamp = 0;
			decIn.eos = 1;
			ret = NX_V4l2DecDecodeFrame( pDecComp->hVpuCodec, &decIn, &decOut );
			if( ret==VID_ERR_NONE && decOut.dispIdx >= 0 && ( decOut.dispIdx < NX_OMX_MAX_BUF ) )
			{
				if( pDecComp->bEnableThumbNailMode != OMX_TRUE )
				{
					NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, decOut.dispIdx );
				}
			}
		}while(ret==VID_ERR_NONE && decOut.dispIdx >= 0);
	}
	return 0;
}

void DeInterlaceFrame( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_V4L2DEC_OUT *pDecOut )
{
	UNUSED_PARAM(pDecOut);
	if ( OMX_FALSE == pDecComp->bInterlaced )	return;
}

int GetUsableBufferIdx( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp )
{
	int32_t i, OutIdx;

	for ( i=1 ; i<NX_OMX_MAX_BUF ; i++ )
	{
		OutIdx = pDecComp->outUsableBufferIdx + i;
		if ( OutIdx >= NX_OMX_MAX_BUF ) OutIdx -= NX_OMX_MAX_BUF;
		if ( pDecComp->outBufferUseFlag[OutIdx] ) break;
	}

	if ( i == NX_OMX_MAX_BUF )
		return -1;

	pDecComp->outUsableBufferIdx = OutIdx;

	return OutIdx;
}

int32_t OutBufCopy( NX_VIDDEC_VIDEO_COMP_TYPE *pDecComp, NX_V4L2DEC_OUT *pDecOut )
{
	int ret = 0;
	struct nx_scaler_context scalerCtx;
	struct rect	crop;

	memset(&scalerCtx,0,sizeof(struct nx_scaler_context));

	NX_VID_MEMORY_INFO *pInImg = &pDecOut->hImg;
	NX_VID_MEMORY_INFO *pOutImg = pDecComp->hVidFrameBuf[pDecComp->outUsableBufferIdx];

	crop.x = 0;
	crop.y = 0;
	crop.width = pDecComp->dsp_width;
	crop.height = pDecComp->dsp_height;

	// scaler crop
	scalerCtx.crop.x = crop.x;
	scalerCtx.crop.y = crop.y;
	scalerCtx.crop.width = crop.width;
	scalerCtx.crop.height = crop.height;

	// scaler src
	scalerCtx.src_plane_num = 1;
	scalerCtx.src_width = pInImg->width;
	scalerCtx.src_height = pInImg->height;
	scalerCtx.src_code = MEDIA_BUS_FMT_YUYV8_2X8;
	scalerCtx.src_fds[0] = pInImg->sharedFd[0];
	scalerCtx.src_stride[0] = pInImg->stride[0];
	scalerCtx.src_stride[1] = pInImg->stride[1];
	scalerCtx.src_stride[2] = pInImg->stride[2];

	// scaler dst
	scalerCtx.dst_plane_num = 1;
	scalerCtx.dst_width = pOutImg->width;
	scalerCtx.dst_height = pOutImg->height;
	scalerCtx.dst_code = MEDIA_BUS_FMT_YUYV8_2X8;
	scalerCtx.dst_fds[0] = pOutImg->sharedFd[0];
	scalerCtx.dst_stride[0] = pOutImg->stride[0];
	scalerCtx.dst_stride[1] = pOutImg->stride[1];
	scalerCtx.dst_stride[2] = pOutImg->stride[2];

	ret =  nx_scaler_run(pDecComp->hScaler, &scalerCtx);

	if (  ret < 0 )
	{
		ErrMsg("nx_scaler_run() Fail, Handle = %lx, return = %d \n", pDecComp->hScaler, ret );
	}

	NX_V4l2DecClrDspFlag( pDecComp->hVpuCodec, NULL, pDecOut->dispIdx );

	return ret;
}

//
//////////////////////////////////////////////////////////////////////////////
