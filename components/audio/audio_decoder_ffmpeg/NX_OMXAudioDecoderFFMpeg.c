#define	LOG_TAG				"NX_FFDEC_AUDIO"

#include <assert.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <NX_OMXBasePort.h>
#include <NX_OMXBaseComponent.h>
#include <OMX_AndroidTypes.h>
#include "NX_OMXAudioDecoderFFMpeg.h"
#include <system/graphics.h>

#define	DEBUG_ANDROID	1
#define	DEBUG_BUFFER	0
#define	DEBUG_FUNC		0
#define	TRACE_ON		0

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

#define	UNUSED_PARAM(X)		X=X


//	Default Recomanded Functions for Implementation Components
static OMX_ERRORTYPE NX_FFAudDec_GetParameter (OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE NX_FFAudDec_SetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE NX_FFAudDec_UseBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer);
static OMX_ERRORTYPE NX_FFAudDec_ComponentDeInit(OMX_HANDLETYPE hComponent);
static void          NX_FFAudDec_BufferMgmtThread( void *arg );


static void NX_FFAudDec_CommandProc( NX_BASE_COMPNENT *pBaseComp, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData );
static int openAudioCodec(NX_FFDEC_AUDIO_COMP_TYPE *pDecComp);
static void closeAudioCodec(NX_FFDEC_AUDIO_COMP_TYPE *pDecComp);
static int decodeAudioFrame(NX_FFDEC_AUDIO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue);



static OMX_S32		gstNumInstance = 0;
static OMX_S32		gstMaxInstance = 5;




#ifdef NX_DYNAMIC_COMPONENTS
//	This Function need for dynamic registration
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent)
#else
//	static registration
OMX_ERRORTYPE NX_FFMpegAudioDecoder_ComponentInit (OMX_HANDLETYPE hComponent)
#endif
{
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_U32 i=0;
	NX_FFDEC_AUDIO_COMP_TYPE *pDecComp;

	FUNC_IN;

	TRACE("%s()++ gstNumInstance = %ld, gstMaxInstance = %ld\n", __func__, gstNumInstance, gstMaxInstance);

	if( gstNumInstance >= gstMaxInstance )
		return OMX_ErrorInsufficientResources;

	pDecComp = (NX_FFDEC_AUDIO_COMP_TYPE *)NxMalloc(sizeof(NX_FFDEC_AUDIO_COMP_TYPE));
	if( pDecComp == NULL ){
		return OMX_ErrorInsufficientResources;
	}

	///////////////////////////////////////////////////////////////////
	//					Component configuration
	NxMemset( pDecComp, 0, sizeof(NX_FFDEC_AUDIO_COMP_TYPE) );
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
	for( i=0; i<FFDEC_AUD_NUM_PORT ; i++ ){
		pDecComp->pPort[i] = (OMX_PTR)NxMalloc(sizeof(NX_BASEPORTTYPE));
		NxMemset( pDecComp->pPort[i], 0, sizeof(NX_BASEPORTTYPE) );
		pDecComp->pBufQueue[i] = (OMX_PTR)NxMalloc(sizeof(NX_QUEUE));
	}
	pDecComp->nNumPort = FFDEC_AUD_NUM_PORT;
	//	Input port configuration
	pDecComp->pInputPort = (NX_BASEPORTTYPE *)pDecComp->pPort[FFDEC_AUD_INPORT_INDEX];
	pDecComp->pInputPortQueue = (NX_QUEUE *)pDecComp->pBufQueue[FFDEC_AUD_INPORT_INDEX];
	NX_InitQueue(pDecComp->pInputPortQueue, NX_OMX_MAX_BUF);
	pPort = pDecComp->pInputPort;
	NX_OMXSetVersion( &pPort->stdPortDef.nVersion );
	NX_InitOMXPort( &pPort->stdPortDef, FFDEC_AUD_INPORT_INDEX, OMX_DirInput, OMX_TRUE, OMX_PortDomainAudio );
	pPort->stdPortDef.nBufferCountActual = FFDEC_AUD_INPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferCountMin    = FFDEC_AUD_INPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferSize        = FFDEC_AUD_INPORT_MIN_BUF_SIZE;

	//	Output port configuration
	pDecComp->pOutputPort = (NX_BASEPORTTYPE *)pDecComp->pPort[FFDEC_AUD_OUTPORT_INDEX];
	pDecComp->pOutputPortQueue = (NX_QUEUE *)pDecComp->pBufQueue[FFDEC_AUD_OUTPORT_INDEX];
	NX_InitQueue(pDecComp->pOutputPortQueue, NX_OMX_MAX_BUF);
	pPort = pDecComp->pOutputPort;
	NX_OMXSetVersion( &pPort->stdPortDef.nVersion );
	NX_InitOMXPort( &pPort->stdPortDef, FFDEC_AUD_OUTPORT_INDEX, OMX_DirOutput, OMX_TRUE, OMX_PortDomainAudio );
	pPort->stdPortDef.nBufferCountActual = FFDEC_AUD_OUTPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferCountMin    = FFDEC_AUD_OUTPORT_MIN_BUF_CNT;
	pPort->stdPortDef.nBufferSize        = FFDEC_AUD_OUTPORT_MIN_BUF_SIZE;
	pPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingPCM;

	 //  Set default output port configure
	NX_OMXSetVersion( &pDecComp->outPortType.nVersion );                            //  Input Port Type
	pDecComp->outPortType.nPortIndex    = FFDEC_AUD_OUTPORT_INDEX;
	pDecComp->outPortType.nChannels = 2;
	pDecComp->outPortType.eNumData  = OMX_NumericalDataSigned;
	pDecComp->outPortType.eEndian   = OMX_EndianLittle;
	pDecComp->outPortType.bInterleaved = OMX_TRUE;
	pDecComp->outPortType.nBitPerSample = 16;
	pDecComp->outPortType.ePCMMode  = OMX_AUDIO_PCMModeLinear;

	//					End Port configurations
	///////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////
	//
	//	Registration OMX Standard Functions
	//
	//	Base overrided functions
	pComp->GetComponentVersion    = NX_BaseGetComponentVersion    ;
	pComp->SendCommand            = NX_BaseSendCommand            ;
	pComp->GetConfig              = NX_BaseGetConfig              ;
	pComp->SetConfig              = NX_BaseSetConfig              ;
	pComp->GetExtensionIndex      = NX_BaseGetExtensionIndex      ;
	pComp->GetState               = NX_BaseGetState               ;
	pComp->ComponentTunnelRequest = NX_BaseComponentTunnelRequest ;
	pComp->SetCallbacks           = NX_BaseSetCallbacks           ;
	pComp->UseEGLImage            = NX_BaseUseEGLImage            ;
	pComp->ComponentRoleEnum      = NX_BaseComponentRoleEnum      ;
	pComp->AllocateBuffer         = NX_BaseAllocateBuffer         ;
	pComp->FreeBuffer             = NX_BaseFreeBuffer             ;
	pComp->EmptyThisBuffer        = NX_BaseEmptyThisBuffer        ;
	pComp->FillThisBuffer         = NX_BaseFillThisBuffer         ;

	//	Specific implemented functions
	pComp->GetParameter           = NX_FFAudDec_GetParameter      ;
	pComp->SetParameter           = NX_FFAudDec_SetParameter      ;
	pComp->UseBuffer              = NX_FFAudDec_UseBuffer         ;
	pComp->ComponentDeInit        = NX_FFAudDec_ComponentDeInit   ;
	//
	///////////////////////////////////////////////////////////////////

	//	Registration Command Procedure
	pDecComp->cbCmdProcedure = NX_FFAudDec_CommandProc;			//	Command procedure

	//	Create command thread
	NX_InitQueue( &pDecComp->cmdQueue, NX_MAX_QUEUE_ELEMENT );
	pDecComp->hSemCmd = NX_CreateSem( 0, NX_MAX_QUEUE_ELEMENT );
	pDecComp->hSemCmdWait = NX_CreateSem( 0, 1 );
	pDecComp->eCmdThreadCmd = NX_THREAD_CMD_RUN;
	pthread_create( &pDecComp->hCmdThread, NULL, (void*)&NX_BaseCommandThread , pDecComp ) ;
	NX_PendSem( pDecComp->hSemCmdWait );

	pDecComp->compName = strdup("OMX.NX.AUDIO_DECODER.FFMPEG");
	pDecComp->compRole = strdup("audio_decoder.ac3");			//	Default Component Role

	//	Buffer
	pthread_mutex_init( &pDecComp->hBufMutex, NULL );
	pDecComp->hBufAllocSem = NX_CreateSem(0, 256);
	pDecComp->hBufFreeSem = NX_CreateSem(0, 256);

	//	Initialize FFMPEG Codec
	pDecComp->hAudioCodec = NULL;
	pDecComp->avctx = NULL;

	av_register_all();

	gstNumInstance ++;

	FUNC_OUT;
	TRACE("%s()-- gstNumInstance = %ld, gstMaxInstance = %ld\n", __func__, gstNumInstance, gstMaxInstance);

	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_FFAudDec_ComponentDeInit(OMX_HANDLETYPE hComponent)
{
	OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_FFDEC_AUDIO_COMP_TYPE *pDecComp = (NX_FFDEC_AUDIO_COMP_TYPE *)pComp->pComponentPrivate;
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

	// add hcjun(2017_11_09)
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

	if(pDecComp->hBufCtrlSem)
	{
		NX_PostSem( pDecComp->hBufCtrlSem );
	}
	if(pDecComp->hBufChangeSem)
	{
		NX_PostSem( pDecComp->hBufChangeSem );
	}

	pthread_join( pDecComp->hCmdThread, NULL );
	NX_DeinitQueue( &pDecComp->cmdQueue );
	//	Destroy Semaphore
	NX_DestroySem( pDecComp->hSemCmdWait );
	NX_DestroySem( pDecComp->hSemCmd );

	//	Destroy port resource
	for( i=0; i<FFDEC_AUD_NUM_PORT ; i++ ){
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

	if( pDecComp->pExtraData )
		free(pDecComp->pExtraData);

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



static OMX_ERRORTYPE NX_FFAudDec_GetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex,OMX_PTR ComponentParamStruct)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_FFDEC_AUDIO_COMP_TYPE *pDecComp = (NX_FFDEC_AUDIO_COMP_TYPE *)pStdComp->pComponentPrivate;
	TRACE("%s(0x%08x) In\n", __FUNCTION__, nParamIndex);
	switch( (int)nParamIndex )
	{
		case OMX_IndexParamStandardComponentRole:
		{
			OMX_PARAM_COMPONENTROLETYPE *pInRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParamStruct;
			pInRole->nSize = sizeof(OMX_PARAM_COMPONENTROLETYPE);
			pInRole->nVersion = pDecComp->hComp->nVersion;
			strcpy( (OMX_STRING)pInRole->cRole, (OMX_STRING)pDecComp->compRole );
			break;
		}
		//  Audio parameters and configurations
		case OMX_IndexParamAudioPcm:
		{
			OMX_AUDIO_PARAM_PCMMODETYPE *pPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE *)ComponentParamStruct;
			if( pPcmMode->nPortIndex!= FFDEC_AUD_OUTPORT_INDEX){
				return OMX_ErrorBadPortIndex;
			}
			NxMemcpy( pPcmMode, &pDecComp->outPortType, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE) );
			break;
		}
		case OMX_IndexParamAudioAc3:
		{
			OMX_AUDIO_PARAM_AC3TYPE *pAc3Mode = (OMX_AUDIO_PARAM_AC3TYPE *)ComponentParamStruct;
			if( pAc3Mode->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			NxMemcpy( ComponentParamStruct, &pDecComp->inPortType, sizeof(OMX_AUDIO_PARAM_AC3TYPE) );
			break;
		}
		case OMX_IndexParamAudioDTS:
		{
			OMX_AUDIO_PARAM_DTSTYPE *pDtsMode = (OMX_AUDIO_PARAM_DTSTYPE *)ComponentParamStruct;
			if( pDtsMode->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			NxMemcpy( ComponentParamStruct, &pDecComp->inPortType, sizeof(OMX_AUDIO_PARAM_DTSTYPE) );
			break;
		}
		case OMX_IndexParamAudioFLAC:
		{
			OMX_AUDIO_PARAM_FLACTYPE *pFlacMode = (OMX_AUDIO_PARAM_FLACTYPE *)ComponentParamStruct;
			if( pFlacMode->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			NxMemcpy( ComponentParamStruct, &pDecComp->inPortType, sizeof(OMX_AUDIO_PARAM_FLACTYPE) );
			break;
		}
		case OMX_IndexParamAudioAPE:
		{
			OMX_AUDIO_PARAM_APETYPE *pApeMode = (OMX_AUDIO_PARAM_APETYPE *)ComponentParamStruct;
			if( pApeMode->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			NxMemcpy( ComponentParamStruct, &pDecComp->inPortType, sizeof(OMX_AUDIO_PARAM_APETYPE) );
			break;
		}
		case OMX_IndexParamAudioRa:
		{
			OMX_AUDIO_PARAM_RATYPE *pRaMode = (OMX_AUDIO_PARAM_RATYPE*)ComponentParamStruct;
			if( pRaMode->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			NxMemcpy( ComponentParamStruct, &pDecComp->inPortType, sizeof(OMX_AUDIO_PARAM_RATYPE) );
			break;
		}
		case OMX_IndexParamAudioWma:
		{
			OMX_AUDIO_PARAM_WMATYPE *pWmaMode = (OMX_AUDIO_PARAM_WMATYPE*)ComponentParamStruct;
			if( pWmaMode->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			NxMemcpy( ComponentParamStruct, &pDecComp->inPortType, sizeof(OMX_AUDIO_PARAM_WMATYPE) );
			break;
		}
		case OMX_IndexParamAudioMp3:
		{
			OMX_AUDIO_PARAM_MP3TYPE *pMp3Mode = (OMX_AUDIO_PARAM_MP3TYPE*)ComponentParamStruct;
			DbgMsg("%s, ++  OMX_IndexParamAudioMp3\n", __FUNCTION__);
			if( pMp3Mode->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			DbgMsg("%s, --  OMX_IndexParamAudioMp3\n", __FUNCTION__);
			NxMemcpy( ComponentParamStruct, &pDecComp->inPortType, sizeof(OMX_AUDIO_PARAM_MP3TYPE) );
			break;
		}
		default :
			return NX_BaseGetParameter( hComp, nParamIndex, ComponentParamStruct );
	}
	return OMX_ErrorNone;
}

static OMX_ERRORTYPE NX_FFAudDec_SetParameter (OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex, OMX_PTR ComponentParamStruct)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComp;
	NX_FFDEC_AUDIO_COMP_TYPE *pDecComp = (NX_FFDEC_AUDIO_COMP_TYPE *)pStdComp->pComponentPrivate;

	TRACE("%s(0x%08x) In\n", __FUNCTION__, nParamIndex);

	switch( (int)nParamIndex )
	{
		case OMX_IndexParamStandardComponentRole:
		{
			OMX_PARAM_COMPONENTROLETYPE *pInRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParamStruct;

			DbgMsg("pInRole = %s\n", pInRole->cRole);
			if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.ac3"  )  )
			{
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
                pDecComp->inCodingType = OMX_AUDIO_CodingAC3;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingAC3;
			}
			else if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.ape" ) )
			{
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
				pDecComp->inCodingType = OMX_AUDIO_CodingAPE;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingAPE;
				pDecComp->pInputPort->stdPortDef.nBufferSize = FFDEC_AUD_INPORT_APE_MIN_BUF_SIZE;

				pDecComp->pOutputPort->stdPortDef.nBufferCountActual = FFDEC_AUD_OUTPORT_APE_MIN_BUF_CNT;
				pDecComp->pOutputPort->stdPortDef.nBufferCountMin    = FFDEC_AUD_OUTPORT_APE_MIN_BUF_CNT;
				pDecComp->pOutputPort->stdPortDef.nBufferSize        = FFDEC_AUD_OUTPORT_APE_MIN_BUF_SIZE;
			}
			else if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.dts" ) )
			{
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
				pDecComp->inCodingType = OMX_AUDIO_CodingDTS;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingDTS;
			}
			else if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.mp2" ) )
			{
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
				pDecComp->inCodingType = OMX_AUDIO_CodingMP3;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
			}
			else if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.mpeg" ) || !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.mp3" ) )
			{
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
				pDecComp->inCodingType = OMX_AUDIO_CodingMP3;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
			}
			else if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.flac" ) )
			{
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
				pDecComp->inCodingType = OMX_AUDIO_CodingFLAC;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingFLAC;
			}
			else if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.ra" ) )
			{
				//	Real audio
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
				pDecComp->inCodingType = OMX_AUDIO_CodingRA;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingRA;
			}
			else if( !strcmp( (OMX_STRING)pInRole->cRole, "audio_decoder.wma" ) || !strcmp((OMX_STRING)pInRole->cRole, "audio_decoder.x-ms-wma") )
			{
				//	Wma audio
				if( pDecComp->compRole )
					free(pDecComp->compRole);
				pDecComp->compRole = strdup((OMX_STRING)pInRole->cRole);
				pDecComp->inCodingType = OMX_AUDIO_CodingWMA;
				pDecComp->pInputPort->stdPortDef.format.audio.eEncoding = OMX_AUDIO_CodingWMA;
			}
			else
			{
				//	Not Support
				NX_ErrMsg("Error: %s(): in role = %s\n", __FUNCTION__, (OMX_STRING)pInRole->cRole );
				return OMX_ErrorBadParameter;
			}
			break;
		}
		case OMX_IndexParamAudioPortFormat:
		{
			//OMX_AUDIO_PARAM_PORTFORMATTYPE *pAudFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)ComponentParamStruct;
			break;
		}
		case OMX_IndexParamAudioPcm:
		{
			OMX_AUDIO_PARAM_PCMMODETYPE *pPcmType = (OMX_AUDIO_PARAM_PCMMODETYPE *)ComponentParamStruct;

			if( pPcmType->nPortIndex != FFDEC_AUD_OUTPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			pDecComp->outPortType.nChannels = FFMIN(2, pPcmType->nChannels);
			pDecComp->outPortType.nSamplingRate = pPcmType->nSamplingRate;
			break;
		}
		case OMX_IndexParamAudioAc3:
		{
			OMX_AUDIO_PARAM_AC3TYPE *pAc3Type = (OMX_AUDIO_PARAM_AC3TYPE *)ComponentParamStruct;

			if( pAc3Type->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}

			memcpy( &pDecComp->inPortType.ac3Type, pAc3Type, sizeof(OMX_AUDIO_PARAM_AC3TYPE) );

			//  Modify Out Port Information
			//          pDecComp->outPortType.nChannels = pAc3Type->nChannels;
			pDecComp->outPortType.nChannels = FFMIN(2, pAc3Type->nChannels);
			pDecComp->outPortType.nSamplingRate = pAc3Type->nSampleRate;
			DbgMsg("%s() OK. (Channels=%ld, SamplingRate=%ld, BitRate=%ld)\n", __FUNCTION__, pAc3Type->nChannels, pAc3Type->nSampleRate, pAc3Type->nBitRate );
			break;
		}
		case OMX_IndexParamAudioDTS:
		{
			OMX_AUDIO_PARAM_DTSTYPE *pDtsType = (OMX_AUDIO_PARAM_DTSTYPE *)ComponentParamStruct;

			if( pDtsType->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}

			memcpy( &pDecComp->inPortType.dtsType, pDtsType, sizeof(OMX_AUDIO_PARAM_DTSTYPE) );

			pDecComp->outPortType.nChannels = FFMIN(2, pDtsType->nChannels);
			pDecComp->outPortType.nSamplingRate = pDtsType->nSampleRate;
			DbgMsg("%s() OK. (Channels=%ld, SamplingRate=%ld, BitRate=%ld)\n", __FUNCTION__, pDtsType->nChannels, pDtsType->nSampleRate, pDtsType->nBitRate );
			break;
		}
		case OMX_IndexParamAudioAPE:
		{
			OMX_AUDIO_PARAM_APETYPE *pApeType = (OMX_AUDIO_PARAM_APETYPE *)ComponentParamStruct;

			if( pApeType->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}

			memcpy( &pDecComp->inPortType.apeType, pApeType, sizeof(OMX_AUDIO_PARAM_APETYPE) );

			pDecComp->outPortType.nChannels = FFMIN(2, pApeType->nChannels);
			pDecComp->outPortType.nSamplingRate = pApeType->nSampleRate;
			DbgMsg("%s() OK. (Channels=%ld, SamplingRate=%ld, BitRate=%ld)\n", __FUNCTION__, pApeType->nChannels, pApeType->nSampleRate, pApeType->nBitRate );
			break;
		}
		case OMX_IndexParamAudioFLAC:
		{
			OMX_AUDIO_PARAM_FLACTYPE *pFlacType = (OMX_AUDIO_PARAM_FLACTYPE *)ComponentParamStruct;

			if( pFlacType->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}

			memcpy( &pDecComp->inPortType.flacType, pFlacType, sizeof(OMX_AUDIO_PARAM_FLACTYPE) );

			pDecComp->outPortType.nChannels = FFMIN(2, pFlacType->nChannels);
			pDecComp->outPortType.nSamplingRate = pFlacType->nSampleRate;
			DbgMsg("%s() OK. (Channels=%ld, SamplingRate=%ld, BitRate=%ld)\n", __FUNCTION__, pFlacType->nChannels, pFlacType->nSampleRate, pFlacType->nBitRate );
			break;
		}
		case OMX_IndexParamAudioRa:
		{
			OMX_AUDIO_PARAM_RATYPE *pRaType = (OMX_AUDIO_PARAM_RATYPE *)ComponentParamStruct;

			if( pRaType->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}
			memcpy( &pDecComp->inPortType.raType, pRaType, sizeof(OMX_AUDIO_PARAM_RATYPE) );
			//	Modify Out Port Information
			pDecComp->outPortType.nChannels = FFMIN(2, pRaType->nChannels);
			pDecComp->outPortType.nSamplingRate = pRaType->nSamplingRate;
			TRACE( "%s() OK. OMX_IndexParamAudioRa,, (Channels=%ld, SamplingRate=%ld)\n", __FUNCTION__, pRaType->nChannels, pRaType->nSamplingRate );
			break;
		}
		case OMX_IndexParamAudioWma:
		{
			OMX_AUDIO_PARAM_WMATYPE *pWmaType = (OMX_AUDIO_PARAM_WMATYPE *)ComponentParamStruct;

			if( pWmaType->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}

			memcpy( &pDecComp->inPortType.wmaType, pWmaType, sizeof(OMX_AUDIO_PARAM_WMATYPE) );

			//	Modify Out Port Information
			pDecComp->outPortType.nChannels = FFMIN(2, pWmaType->nChannels);
			pDecComp->outPortType.nSamplingRate = pWmaType->nSamplingRate;

			TRACE("%s() OK. (Channels=%hu, BitRate=%ld)\n", __FUNCTION__, pWmaType->nChannels, pWmaType->nBitRate );
			break;
		}
		case OMX_IndexParamAudioMp3:
		{
			OMX_AUDIO_PARAM_MP3TYPE *pMp3Type = (OMX_AUDIO_PARAM_MP3TYPE *)ComponentParamStruct;

			if( pMp3Type->nPortIndex != FFDEC_AUD_INPORT_INDEX){
				NX_ErrMsg("%s Bad port index.(%d)\n", __FUNCTION__, nParamIndex);
				return OMX_ErrorBadPortIndex;
			}

			memcpy( &pDecComp->inPortType.mp3Type, pMp3Type, sizeof(OMX_AUDIO_PARAM_MP3TYPE) );

			//	Modify Out Port Information
			pDecComp->outPortType.nChannels = FFMIN(2, pMp3Type->nChannels);
			pDecComp->outPortType.nSamplingRate = pMp3Type->nSampleRate;

			TRACE("%s() OK. (Channels=%ld, BitRate=%ld)\n", __FUNCTION__, pMp3Type->nChannels, pMp3Type->nBitRate );
			break;
		}
		case OMX_IndexAudioDecoderFFMpegExtradata:
		{
			OMX_U8 *pExtraData = ComponentParamStruct;
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
static OMX_ERRORTYPE NX_FFAudDec_UseBuffer (OMX_HANDLETYPE hComponent, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer)
{
	OMX_COMPONENTTYPE *pStdComp = (OMX_COMPONENTTYPE *)hComponent;
	NX_FFDEC_AUDIO_COMP_TYPE *pDecComp = (NX_FFDEC_AUDIO_COMP_TYPE *)pStdComp->pComponentPrivate;
	NX_BASEPORTTYPE *pPort = NULL;
	OMX_BUFFERHEADERTYPE         **pPortBuf = NULL;
	OMX_U32 i=0;

	DbgBuffer( "%s() In.(PortNo=%ld)\n", __FUNCTION__, nPortIndex );

	if( nPortIndex >= pDecComp->nNumPort ){
		return OMX_ErrorBadPortIndex;
	}

	if( OMX_StateLoaded != pDecComp->eCurState || OMX_StateIdle != pDecComp->eNewState )
		return OMX_ErrorIncorrectStateTransition;

	if( 0 ==nPortIndex ){
		pPort = pDecComp->pInputPort;
		pPortBuf = pDecComp->pInputBuffers;
	}else{
		pPort = pDecComp->pOutputPort;
		pPortBuf = pDecComp->pOutputBuffers;
	}

	// add hcjun(2017_11_09)
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

	DbgBuffer( "%s() : pPort->stdPortDef.nBufferSize = %ld\n", __FUNCTION__, pPort->stdPortDef.nBufferSize);

	if( pPort->stdPortDef.nBufferSize > nSizeBytes )
		return OMX_ErrorBadParameter;

	for( i=0 ; i<pPort->stdPortDef.nBufferCountActual ; i++ ){
		if( NULL == pPortBuf[i] )
		{
			//	Allocate Actual Data
			pPortBuf[i] = NxMalloc( sizeof(OMX_BUFFERHEADERTYPE) );
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





//////////////////////////////////////////////////////////////////////////////
//
//						Command Execution Thread
//
static OMX_ERRORTYPE NX_FFAudDec_StateTransition( NX_FFDEC_AUDIO_COMP_TYPE *pDecComp, OMX_STATETYPE eCurState, OMX_STATETYPE eNewState )
{
	OMX_U32 i=0;
	OMX_ERRORTYPE eError = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE *pPort = NULL;
	NX_QUEUE *pQueue = NULL;
	OMX_BUFFERHEADERTYPE *pBufHdr = NULL;
	OMX_BOOL bAbendState = OMX_FALSE;

	TRACE( "%s() In : eCurState %d -> eNewState %d \n", __FUNCTION__, eCurState, eNewState );

	//	Check basic errors
	if( eCurState == eNewState ){
		NX_ErrMsg("%s:line(%d) : %s() same state\n", __FILE__, __LINE__, __FUNCTION__ );
		return OMX_ErrorSameState;
	}
	if( OMX_StateInvalid==eCurState || OMX_StateInvalid==eNewState ){
		NX_ErrMsg("%s:line(%d) : %s() Invalid state\n", __FILE__, __LINE__, __FUNCTION__ );
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
						//	Buffer Allocation must be done before transferring to Loaded Idle.
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
				//	Create buffer mangement semaphore
				pDecComp->hBufChangeSem = NX_CreateSem(0, FFDEC_AUD_NUM_PORT*1024);
				//	Create buffer management thread
				pDecComp->eCmdBufThread = NX_THREAD_CMD_PAUSE;
				//	Create buffer management thread
				pthread_create( &pDecComp->hBufThread, NULL, (void*)&NX_FFAudDec_BufferMgmtThread , pDecComp );

				//	Open Audio Decoder
				TRACE("Wait BufferCtrlSem");
//				NX_PendSem(pDecComp->hBufCtrlSem);
				if( OMX_TRUE == bAbendState )
				{
					pDecComp->eCurState = eNewState;
					DbgBuffer( "%s() Waring:.. bAbendState == TRUE\n", __FUNCTION__);
					return eError;
				}
				openAudioCodec( pDecComp );

				//	Wait thread creation
				pDecComp->eCurState = eNewState;

				TRACE("OMX_StateLoaded --> OMX_StateIdle");

				break;
			case OMX_StateWaitForResources:
			case OMX_StateExecuting:
			case OMX_StatePause:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
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
				pthread_join( pDecComp->hBufThread, NULL );
				NX_DestroySem( pDecComp->hBufChangeSem );
				NX_DestroySem( pDecComp->hBufCtrlSem );
				pDecComp->hBufChangeSem = NULL;
				pDecComp->hBufCtrlSem = NULL;

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
				}

				closeAudioCodec( pDecComp );

				pDecComp->eCurState = eNewState;
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
				break;
			case OMX_StatePause:
				//	Step 1. Check in/out buffer queue.
				//	Step 2. If buffer is not ready in the queue, return error.
				//	Step 3. Craete buffer management thread.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateWaitForResources:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
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
					if( OMX_DirInput == pPort->eDir ){
						do{
							pBufHdr = NULL;
							if( 0 == NX_PopQueue( pQueue, (void**)&pBufHdr ) ){
								pDecComp->pCallbacks->EmptyBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBufHdr );
							}else{
								break;
							}
						}while(1);
					}else if( OMX_DirOutput == pPort->eDir ){
						do{
							pBufHdr = NULL;
							if( 0 == NX_PopQueue( pQueue, (void**)&pBufHdr ) ){
								pDecComp->pCallbacks->FillBufferDone( pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pBufHdr );
							}else{
								break;
							}
						}while(1);
					}
				}
				pthread_mutex_unlock( &pDecComp->hBufMutex );
				//	Step 3. Exit buffer management thread.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StatePause:
				//	Step 1. Stop buffer processing using buffer management semaphore
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateLoaded:
			case OMX_StateWaitForResources:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else if( OMX_StatePause==eCurState ){
		switch( eNewState )
		{
			case OMX_StateIdle:
				//	Step 1. Flushing buffers.
				//	Step 2. Exit buffer management thread.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateExecuting:
				//	Step 1. Start buffer processing.
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateLoaded:
			case OMX_StateWaitForResources:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else if( OMX_StateWaitForResources==eCurState ){
		switch( eNewState )
		{
			case OMX_StateLoaded:
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateIdle:
				pDecComp->eCurState = eNewState;
				break;
			case OMX_StateExecuting:
			case OMX_StatePause:
				return OMX_ErrorIncorrectStateTransition;
			default:
				break;
		}
	}else{
		//	Error
		return OMX_ErrorUndefined;
	}
	return eError;
}


static void NX_FFAudDec_CommandProc( NX_BASE_COMPNENT *pBaseComp, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData )
{
	NX_FFDEC_AUDIO_COMP_TYPE *pDecComp = (NX_FFDEC_AUDIO_COMP_TYPE *)pBaseComp;
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
				eEvent = OMX_EventError;
				nData1 = OMX_ErrorSameState;
				break;
			}

			eError = NX_FFAudDec_StateTransition( pDecComp, pDecComp->eCurState, nParam1 );
			nData1 = OMX_CommandStateSet;
			nData2 = nParam1;				//	Must be set new state (OMX_STATETYPE)
			TRACE("NX_FFAudDec_CommandProc : OMX_CommandStateSet(nData1=%ld, nData2=%ld)\n", nData1, nData2 );
			break;
		}
		case OMX_CommandFlush:       // Flush the data queue(s) of a component
		{
			OMX_BUFFERHEADERTYPE* pBuf = NULL;
			TRACE("%s() : Flush Start ( nParam1=%ld )\n", __FUNCTION__, nParam1 );

			pthread_mutex_lock( &pDecComp->hBufMutex );

			if( nParam1 == FFDEC_AUD_INPORT_INDEX || nParam1 == OMX_ALL )
			{
				do{
					if( pDecComp->pInputPortQueue->curElements > 0 ){
						//	Flush buffer
						NX_PopQueue( pDecComp->pInputPortQueue, (void**)&pBuf );
						pBuf->nFilledLen = 0;
						pDecComp->pCallbacks->EmptyBufferDone(pStdComp, pStdComp->pApplicationPrivate, pBuf);
					}else{
						break;
					}
				}while(1);
				SendEvent( (NX_BASE_COMPNENT*)pDecComp, eEvent, OMX_CommandFlush, FFDEC_AUD_INPORT_INDEX, pCmdData );
			}

			if( nParam1 == FFDEC_AUD_OUTPORT_INDEX || nParam1 == OMX_ALL )
			{
				do{
					if( NX_GetQueueCnt(pDecComp->pOutputPortQueue) > 0 ){
						//	Flush buffer
						NX_PopQueue( pDecComp->pOutputPortQueue, (void**)&pBuf );
						pBuf->nFilledLen = 0;
						pDecComp->pCallbacks->FillBufferDone(pStdComp, pStdComp->pApplicationPrivate, pBuf);
					}else{
						break;
					}
				}while(1);
				SendEvent( (NX_BASE_COMPNENT*)pDecComp, eEvent, OMX_CommandFlush, FFDEC_AUD_OUTPORT_INDEX, pCmdData );
			}

			if( nParam1 == OMX_ALL )	//	Output Port Flushing
			{
				SendEvent( (NX_BASE_COMPNENT*)pDecComp, eEvent, OMX_CommandFlush, OMX_ALL, pCmdData );
			}
			pDecComp->bFlush = OMX_TRUE;

			pthread_mutex_unlock( &pDecComp->hBufMutex );
			TRACE("%s() : Flush End\n", __FUNCTION__ );
			return ;
		}
		//	Openmax spec v1.1.2 : 3.4.4.3 Non-tunneled Port Disablement and Enablement.
		case OMX_CommandPortDisable: // Disable a port on a component.
		{
			//	Check Parameter
			if( OMX_ALL != nParam1 || FFDEC_AUD_NUM_PORT>=nParam1 ){
				//	Bad parameter
				eEvent = OMX_EventError;
				nData1 = OMX_ErrorBadPortIndex;
				NX_ErrMsg(" Errror : OMX_ErrorBadPortIndex(%d) : %d: %s", nParam1, __FILE__, __LINE__);
				break;
			}

			//	Step 1. The component shall return the buffers with a call to EmptyBufferDone/FillBufferDone,
//			NX_PendSem( pDecComp->hBufCtrlSem );
			if( OMX_ALL == nParam1 ){
				//	Disable all ports
			}else{
				pDecComp->pInputPort->stdPortDef.bEnabled = OMX_FALSE;
				//	Specific port
			}
			TRACE("NX_FFAudDec_CommandProc : OMX_CommandPortDisable \n");
//			NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		case OMX_CommandPortEnable:  // Enable a port on a component.
		{
//			NX_PendSem( pDecComp->hBufCtrlSem );
			TRACE("NX_FFAudDec_CommandProc : OMX_CommandPortEnable \n");
			pDecComp->pInputPort->stdPortDef.bEnabled = OMX_FALSE;
//			NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		case OMX_CommandMarkBuffer:  // Mark a component/buffer for observation
		{
//			NX_PendSem( pDecComp->hBufCtrlSem );
			TRACE("NX_FFAudDec_CommandProc : OMX_CommandMarkBuffer \n");
//			NX_PostSem( pDecComp->hBufCtrlSem );
			break;
		}
		default:
		{
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
static void NX_FFAudDec_Transform(NX_FFDEC_AUDIO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{
	decodeAudioFrame( pDecComp, pInQueue, pOutQueue );
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
static void NX_FFAudDec_BufferMgmtThread( void *arg )
{
	NX_FFDEC_AUDIO_COMP_TYPE *pDecComp = (NX_FFDEC_AUDIO_COMP_TYPE *)arg;
	TRACE( "enter %s() loop\n", __FUNCTION__ );

	NX_PostSem( pDecComp->hBufCtrlSem );					//	Thread Creation Semaphore
	while( NX_THREAD_CMD_EXIT != pDecComp->eCmdBufThread )
	{
		NX_PendSem( pDecComp->hBufCtrlSem );				//	Thread Control Semaphore
		while( NX_THREAD_CMD_RUN == pDecComp->eCmdBufThread ){
			NX_PendSem( pDecComp->hBufChangeSem );			//	wait buffer management command
			if( NX_THREAD_CMD_RUN != pDecComp->eCmdBufThread ){
				break;
			}

			//	check decoding
			if( NX_GetQueueCnt( pDecComp->pInputPortQueue ) > 0 && NX_GetQueueCnt( pDecComp->pOutputPortQueue ) > 0 ) {
				pthread_mutex_lock( &pDecComp->hBufMutex );
				NX_FFAudDec_Transform(pDecComp, pDecComp->pInputPortQueue, pDecComp->pOutputPortQueue);
				pthread_mutex_unlock( &pDecComp->hBufMutex );
			}
		}
	}
	//	Release or Return buffer's
	TRACE("exit buffer management thread.\n");
}

//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//					FFMPEG Audio Decoder Handler
//

//	0 is OK, other Error.
static int openAudioCodec(NX_FFDEC_AUDIO_COMP_TYPE *pDecComp)
{
	enum AVCodecID codecId = CODEC_ID_NONE;
	int channels = 2;
	int sampleRate = 44100;
	OMX_S32 blockAlign = 0;
	OMX_U32 bitRate = 0;

	//	audio coding type to ffmpeg codec id
	switch( (int)pDecComp->inCodingType ){
		case CODEC_ID_PCM_S16LE:
			codecId = CODEC_ID_PCM_S16LE;
			channels = pDecComp->inPortType.flacType.nChannels;
			sampleRate = pDecComp->inPortType.flacType.nSampleRate;
			DbgMsg("Audio coding type RAW, channels=%d, samplingrate=%d\n", channels, sampleRate);
			break;
		case OMX_AUDIO_CodingAC3:
			codecId = CODEC_ID_AC3;
			channels = pDecComp->inPortType.ac3Type.nChannels;
			sampleRate = pDecComp->inPortType.ac3Type.nSampleRate;
			DbgMsg("Audio coding type AC3, channels=%d, samplingrate=%d\n", (int)channels, (int)sampleRate);
			break;
		case OMX_AUDIO_CodingDTS:
			codecId = CODEC_ID_DTS;
			channels = pDecComp->inPortType.dtsType.nChannels;
			sampleRate = pDecComp->inPortType.dtsType.nSampleRate;
			DbgMsg("Audio coding type DTS, channels=%d, samplingrate=%d\n", channels, sampleRate);
			break;
		case OMX_AUDIO_CodingAPE:
			codecId = CODEC_ID_APE;
			channels = pDecComp->inPortType.apeType.nChannels;
			sampleRate = pDecComp->inPortType.apeType.nSampleRate;
			DbgMsg("Audio coding type APE, channels=%d, samplingrate=%d\n", channels, sampleRate);
			break;
		case OMX_AUDIO_CodingFLAC:
			codecId = CODEC_ID_FLAC;
			channels = pDecComp->inPortType.flacType.nChannels;
			sampleRate = pDecComp->inPortType.flacType.nSampleRate;
			DbgMsg("Audio coding type FLAC, channels=%d, samplingrate=%d\n", channels, sampleRate);
			break;
		case OMX_AUDIO_CodingRA:
			codecId = CODEC_ID_COOK;
			channels = pDecComp->inPortType.raType.nChannels;
			sampleRate = pDecComp->inPortType.raType.nSamplingRate;
			blockAlign = pDecComp->inPortType.raType.nBitsPerFrame;
			DbgMsg("Audio coding type COOK, channels=%d, samplingrate=%d, blockAlign=%ld\n", channels, sampleRate, blockAlign);
			break;
		case OMX_AUDIO_CodingWMA:
			if( pDecComp->inPortType.wmaType.eFormat == OMX_AUDIO_WMAFormat7)
			{
				codecId = CODEC_ID_WMAV2;
				DbgMsg("WMAV2\n");
			}
			else if( pDecComp->inPortType.wmaType.eFormat == OMX_AUDIO_WMAFormat8 )
			{
				codecId = CODEC_ID_WMAPRO;
				DbgMsg("CODEC_ID_WMAPRO\n");
			}
			else if( pDecComp->inPortType.wmaType.eFormat == OMX_AUDIO_WMAFormat9 )
			{
				codecId = CODEC_ID_WMALOSSLESS;
				DbgMsg("CODEC_ID_WMALOSSLESS\n");
			}
			channels = pDecComp->inPortType.wmaType.nChannels;
			sampleRate = pDecComp->inPortType.wmaType.nSamplingRate;
			bitRate = pDecComp->inPortType.wmaType.nBitRate;
			blockAlign = pDecComp->inPortType.wmaType.nBlockAlign;
			DbgMsg("Audio coding type WMA, channels=%d, samplingrate=%d, blockAlign=%ld, codecId = %d\n", channels, sampleRate, blockAlign, codecId);
			break;
		case OMX_AUDIO_CodingMP3:
			codecId = CODEC_ID_MP3;
			channels = pDecComp->inPortType.mp3Type.nChannels;
			sampleRate = pDecComp->inPortType.mp3Type.nSampleRate;
			bitRate = pDecComp->inPortType.mp3Type.nBitRate;
			DbgMsg("Audio coding type MP3, channels=%d, samplingrate=%d, codecId = %d\n", channels, sampleRate, codecId);
			break;
		default:
			//DbgMsg("Audio coding type WMA, channels=%d, samplingrate=%d, codecId = %d\n", channels, sampleRate, codecId);
			NX_ErrMsg("Error Coding Type None(%x)!!!\n", pDecComp->inCodingType );
			return -1;
	}

	pDecComp->hAudioCodec = avcodec_find_decoder( codecId );
	if( pDecComp->hAudioCodec == NULL )
	{
		NX_ErrMsg("Error avcodec_find_decoder() is NULL!!!\n" );
		return -1;
	}

	pDecComp->avctx = avcodec_alloc_context3(pDecComp->hAudioCodec);
	pDecComp->avctx->codec_id = codecId;

	//	Default Codec Configuration
	if (1)
	{
		pDecComp->avctx->workaround_bugs   = 1;
		pDecComp->avctx->lowres            = 0;
		if(pDecComp->avctx->lowres > pDecComp->hAudioCodec->max_lowres){
			DbgMsg("The maximum value for lowres supported by the decoder is %d",
				pDecComp->hAudioCodec->max_lowres);
			pDecComp->avctx->lowres= pDecComp->hAudioCodec->max_lowres;
		}
		pDecComp->avctx->idct_algo         = 0;
		pDecComp->avctx->skip_frame        = AVDISCARD_DEFAULT;
		pDecComp->avctx->skip_idct         = AVDISCARD_DEFAULT;
		pDecComp->avctx->skip_loop_filter  = AVDISCARD_DEFAULT;
		pDecComp->avctx->error_concealment = 3;

		if(pDecComp->avctx->lowres) pDecComp->avctx->flags |= CODEC_FLAG_EMU_EDGE;
		if(pDecComp->hAudioCodec->capabilities & CODEC_CAP_DR1)
			pDecComp->avctx->flags |= CODEC_FLAG_EMU_EDGE;
	}

	pDecComp->avctx->channels = channels;
	pDecComp->avctx->sample_rate = sampleRate;
	pDecComp->avctx->request_channel_layout = FFMIN(2, channels);
	pDecComp->avctx->block_align = blockAlign;
	pDecComp->avctx->bit_rate = bitRate;

	if(codecId == CODEC_ID_APE)
	{
		pDecComp->avctx->bits_per_coded_sample = 16;
	}

	TRACE("channels = %d, request_channels = %d, block_align=%d\n", pDecComp->avctx->channels, pDecComp->avctx->request_channels, pDecComp->avctx->block_align );

	if( pDecComp->pExtraData )
	{
		pDecComp->avctx->extradata = pDecComp->pExtraData;
		pDecComp->avctx->extradata_size = pDecComp->nExtraDataSize;
	}
	if( avcodec_open2( pDecComp->avctx, pDecComp->hAudioCodec, NULL ) < 0 )
	{
		DbgMsg("avcodec_open() failed!!!!");
		NX_ErrMsg("%s(%d) avcodec_open() failed.(CodecID=%d, CodingType=%d)\n", __FILE__, __LINE__, codecId, pDecComp->inCodingType);
		return -1;
	}

	return 0;
}

static void closeAudioCodec(NX_FFDEC_AUDIO_COMP_TYPE *pDecComp)
{
	if( pDecComp->avctx ){

		if( pDecComp->pExtraData )
		{
			free(pDecComp->pExtraData);
			pDecComp->pExtraData = NULL;
		}

		avcodec_close( pDecComp->avctx );
		av_free(pDecComp->avctx);
	}
}

static int AudioConvert(int in_channels, int in_nb_samples, int in_sample_fmt,
						int in_sample_rate, int in_linesize, char **in_buf,char **out_buf, int *out_linesize)
{
	struct SwrContext *swr_ctx;
	OMX_S64 src_ch_layout = AV_CH_LAYOUT_STEREO, dst_ch_layout = AV_CH_LAYOUT_SURROUND;
	int src_rate = 0, dst_rate = 0;
	int src_nb_channels = 0, dst_nb_channels = 0;
	int src_linesize = 0, dst_linesize = 0;
	int src_nb_samples = 0, dst_nb_samples = 0, max_dst_nb_samples = 0;
	enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_NONE, dst_sample_fmt = AV_SAMPLE_FMT_NONE;
	int ret = 0;
	unsigned int dst_bufsize = 0;

	/* create resampler context */
	swr_ctx = swr_alloc();
	if (!swr_ctx)
	{
		NX_ErrMsg("Could not allocate resampler context\n");
		return	-1;
	}

	/* set options */
	if(2 < in_channels)
		src_ch_layout = AV_CH_LAYOUT_5POINT1;
	else if(2 > in_channels)
		src_ch_layout = AV_CH_LAYOUT_MONO;
	else
		src_ch_layout = AV_CH_LAYOUT_STEREO;

	src_rate = in_sample_rate;
	src_sample_fmt = in_sample_fmt;

	if(2 > in_channels)
		dst_ch_layout = src_ch_layout;
	else
		dst_ch_layout = AV_CH_LAYOUT_STEREO;

	dst_rate = src_rate;
	dst_sample_fmt = AV_SAMPLE_FMT_S16;

	av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

	/* initialize the resampling context */
	if ((ret = swr_init(swr_ctx)) < 0)
	{
		NX_ErrMsg("Failed to initialize the resampling context\n");
		return	-1;
	}

	/* allocate source and destination samples buffers */
	src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
	src_linesize	= in_linesize;

	/* compute the number of converted samples: buffering is avoided
	* ensuring that the output buffer will contain at least all the
	* converted input samples */
	src_nb_samples = in_nb_samples;
	max_dst_nb_samples = dst_nb_samples =
			av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

	/* buffer is going to be directly written to a rawaudio file, no alignment */
	dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);

	/* compute destination number of samples */
	dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) +
									src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
	dst_nb_samples = swr_convert(swr_ctx, (unsigned char **)out_buf, dst_nb_samples, (unsigned char const**)in_buf, src_nb_samples);

	dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                             dst_nb_samples, dst_sample_fmt, 1);
	*out_linesize = dst_bufsize;

    swr_free(&swr_ctx);

	return	0;
}



//	0 is OK, other Error.
static int decodeAudioFrame(NX_FFDEC_AUDIO_COMP_TYPE *pDecComp, NX_QUEUE *pInQueue, NX_QUEUE *pOutQueue)
{

	OMX_BUFFERHEADERTYPE* pInBuf = NULL, *pOutBuf = NULL;
	OMX_S32 outSize = 0, inSize =0, usedByte=0, writtenSize=0;
	OMX_BYTE inData, outData;
	OMX_S32 allocSize;
	AVPacket avpkt;
	AVFrame *decoded_frame = NULL;
	int got_frame = 0;


	if( pDecComp->bFlush )
	{
		if(pDecComp->avctx)
			avcodec_flush_buffers(pDecComp->avctx);
		pDecComp->bFlush = OMX_FALSE;
		pDecComp->nPrevTimeStamp = -1;
		pDecComp->nSampleCount = 0;
	}

	av_init_packet(&avpkt);

	NX_PopQueue( pOutQueue, (void**)&pOutBuf );
	if( pOutBuf == NULL ){
		DbgMsg("pOutBuf = %p", pOutBuf);
		return -1;
	}

	NX_PopQueue( pInQueue, (void**)&pInBuf );
	if( pInBuf == NULL ){
		DbgMsg("pInBuf = %p", pInBuf);
		pOutBuf->nFilledLen = 0;
		pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);
		return 0;
	}

	inData = pInBuf->pBuffer;
	inSize = pInBuf->nFilledLen;
	outData = pOutBuf->pBuffer;
	allocSize = pOutBuf->nAllocLen;

	do{
		outSize = 0;
		got_frame = 0;
		avpkt.data = inData;
		avpkt.size = inSize;

		if (!decoded_frame)
		{
            if (!(decoded_frame = avcodec_alloc_frame()))
			{
                DbgMsg("Could not allocate audio frame\n");
                return -1;
            }
        }
		else
		{
            avcodec_get_frame_defaults(decoded_frame);
		}

		usedByte = avcodec_decode_audio4( pDecComp->avctx, decoded_frame, (int *)&got_frame, &avpkt );

		if( got_frame )
		{
            /* if a frame has been decoded, output it */
            int data_size = av_samples_get_buffer_size(NULL, pDecComp->avctx->channels,
                                                       decoded_frame->nb_samples,
                                                       pDecComp->avctx->sample_fmt, 1);

			outSize = data_size;
        }

		if( usedByte < 0 ){
			break;
		}

		if(outSize>0)
		{
			AudioConvert(pDecComp->avctx->channels, decoded_frame->nb_samples, pDecComp->avctx->sample_fmt,
						pDecComp->avctx->sample_rate, outSize, (char **)&decoded_frame->data[0], (char **)&outData, (int *)&outSize);
		}


		//  Update In Buffer
		inSize -= usedByte;
		inData += usedByte;

		writtenSize += outSize;
		outData += outSize;
		allocSize -= outSize;

	}while( inSize > 0 );

	avcodec_free_frame(&decoded_frame);

	pOutBuf->nFilledLen = writtenSize;

	//  Time Stamp Update
	pOutBuf->nFlags = pInBuf->nFlags;

	if( pDecComp->nPrevTimeStamp != pInBuf->nTimeStamp )
	{
		pDecComp->nPrevTimeStamp = pInBuf->nTimeStamp;
		pOutBuf->nTimeStamp = pInBuf->nTimeStamp;
		pDecComp->nSampleCount = 0;
	}
	else
	{
		OMX_TICKS sampleDuration = (1000000ll*(OMX_S64)pDecComp->nSampleCount) / pDecComp->avctx->sample_rate;
		pOutBuf->nTimeStamp = pInBuf->nTimeStamp + sampleDuration;
	}

	if( pOutBuf->nFlags & OMX_BUFFERFLAG_EOS )
	{
		pOutBuf->nFlags = OMX_BUFFERFLAG_EOS;
		pOutBuf->nTimeStamp = -1;
	}

	TRACE("%s outTimeStamp = %lld,  inTimeStamp = %lld, nFilledLen = %lu, nFlags=0x%08lx", __func__, pOutBuf->nTimeStamp, pInBuf->nTimeStamp, pOutBuf->nFilledLen, pOutBuf->nFlags );

	pDecComp->nSampleCount += outSize/4;		//	Output Fixed in 16bit 2channel.

	pDecComp->pCallbacks->EmptyBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pInBuf);
	pDecComp->pCallbacks->FillBufferDone(pDecComp->hComp, pDecComp->hComp->pApplicationPrivate, pOutBuf);

	return 0;
}

//
//////////////////////////////////////////////////////////////////////////////
