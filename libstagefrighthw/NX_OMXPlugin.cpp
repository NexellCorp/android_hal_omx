/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "NXOMXPlugin"

#include <dlfcn.h>
#include "NX_OMXPlugin.h"
#include <HardwareAPI.h>

#define DEBUG_OMXPLUGIN

#ifdef DEBUG_OMXPLUGIN
#define	TRACE	ALOGV
#define	DbgMsg	ALOGD
#define	ErrMsg	ALOGE
#else
#define	TRACE(fmt,...)      do{}while(0)
#define	DbgMsg(fmt,...)     do{}while(0)
#define	ErrMsg(fmt,...)     do{}while(0)
#endif

#define MAJOR           0
#define MINOR           90
#define REVISIONM       0
#define RESERVATION     0

namespace android {

OMXPluginBase *createOMXPlugin() {
    return new NX_OMXPlugin;
}

NX_OMXPlugin::NX_OMXPlugin()
    : mLibHandle(dlopen("libNX_OMX_Core.so", RTLD_NOW)),
      mInit(NULL),
      mDeinit(NULL),
      mComponentNameEnum(NULL),
      mGetHandle(NULL),
      mFreeHandle(NULL),
      mGetRolesOfComponentHandle(NULL)
{
    DbgMsg("\t ------------------------------------------------------\n");
    DbgMsg("\t -                                                    -\n");
    DbgMsg("\t -          Nexell Open MAX IL Plugin                 -\n");
    DbgMsg("\t -                                                    -\n");
    DbgMsg("\t -          Version: %d.%d.%d.%d                      -\n",MAJOR, MINOR, REVISIONM, RESERVATION);
    DbgMsg("\t ------------------------------------------------------\n");

    if (mLibHandle != NULL) {
        mInit = (InitFunc)dlsym(mLibHandle, "NX_OMX_Init");
        mDeinit = (DeinitFunc)dlsym(mLibHandle, "NX_OMX_Deinit");

        mComponentNameEnum =
            (ComponentNameEnumFunc)dlsym(mLibHandle, "NX_OMX_ComponentNameEnum");

        mGetHandle = (GetHandleFunc)dlsym(mLibHandle, "NX_OMX_GetHandle");
        mFreeHandle = (FreeHandleFunc)dlsym(mLibHandle, "NX_OMX_FreeHandle");

        mGetRolesOfComponentHandle =
            (GetRolesOfComponentFunc)dlsym(mLibHandle, "NX_OMX_GetRolesOfComponent");

        (*mInit)();
    }
}

NX_OMXPlugin::~NX_OMXPlugin()
{
    if (mLibHandle != NULL) {
        if( mDeinit )
        {
            (*mDeinit)();
        }

        dlclose(mLibHandle);
        mLibHandle = NULL;
    }
}

OMX_ERRORTYPE NX_OMXPlugin::makeComponentInstance(const char *name,const OMX_CALLBACKTYPE *callbacks,OMX_PTR appData,OMX_COMPONENTTYPE **component)
{
    if (mLibHandle == NULL) {
        return OMX_ErrorUndefined;
    }

    TRACE("\t %s() called\n",__FUNCTION__);

	return (*mGetHandle)(
            reinterpret_cast<OMX_HANDLETYPE *>(component),
            const_cast<char *>(name),
            appData, const_cast<OMX_CALLBACKTYPE *>(callbacks));
}

OMX_ERRORTYPE NX_OMXPlugin::destroyComponentInstance(OMX_COMPONENTTYPE *component)
{
    if (mLibHandle == NULL) {
        return OMX_ErrorUndefined;
    }

	TRACE("\t %s() called\n",__FUNCTION__);

    return (*mFreeHandle)(reinterpret_cast<OMX_HANDLETYPE *>(component));
}

OMX_ERRORTYPE NX_OMXPlugin::enumerateComponents(OMX_STRING name,size_t size,OMX_U32 index)
{
    if (mLibHandle == NULL) {
        return OMX_ErrorUndefined;
    }

    TRACE("\t %s() called. size=%zu, index=%u\n",__FUNCTION__, size, index);

	return (*mComponentNameEnum)(name, size, index);
}

OMX_ERRORTYPE NX_OMXPlugin::getRolesOfComponent(const char *name,Vector<String8> *roles)
{
    TRACE("\t %s() ++\n",__FUNCTION__);
    roles->clear();

    if (mLibHandle == NULL) {
        return OMX_ErrorUndefined;
    }

    OMX_U32 numRoles;
    OMX_ERRORTYPE err = (*mGetRolesOfComponentHandle)(
            const_cast<OMX_STRING>(name), &numRoles, NULL);

    if (err != OMX_ErrorNone) {
        return err;
    }

    if (numRoles > 0) {
        OMX_U8 **array = new OMX_U8 *[numRoles];
        for (OMX_U32 i = 0; i < numRoles; ++i) {
            array[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
        }

        OMX_U32 numRoles2 = numRoles;     // modified by kshblue(14.07.04)
        err = (*mGetRolesOfComponentHandle)(
                const_cast<OMX_STRING>(name), &numRoles2, array);

		if( err != OMX_ErrorNone )
		{
			ErrMsg("Error : %s(%d) : error != OMX_ErrorNone\n", __FILE__, __LINE__);
		}
		if( numRoles != numRoles2 )
		{
			ErrMsg("Error : %s(%d) : numRoles != numRoles2\n", __FILE__, __LINE__);
		}

        for (OMX_U32 i = 0; i < numRoles; ++i) {
            String8 s((const char *)array[i]);
            roles->push(s);

            delete[] array[i];
            array[i] = NULL;
        }

        delete[] array;
        array = NULL;
    }

    TRACE("\t %s() --\n",__FUNCTION__);
    return OMX_ErrorNone;
}

}  // namespace android
