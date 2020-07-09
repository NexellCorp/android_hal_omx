/*
 * Copyright (C) 2017 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "FFmpegMKVExtractor"
#include <utils/Log.h>

#include <media/MediaExtractor.h>
#include "FFmpegExtractor.h"

namespace android {

extern "C" {
// This is the only symbol that needs to be exported
__attribute__ ((visibility ("default")))
MediaExtractor::ExtractorDef GETEXTRACTORDEF() {
    return {
        MediaExtractor::EXTRACTORDEF_VERSION,
        UUID("abbedd92-38c4-4904-a4c1-b3f45f123458"),
        1,
        "FFMPEG Matroska Extractor",
        [](
                DataSourceBase *source,
                float *confidence,
                void **,
                MediaExtractor::FreeMetaFunc *) -> MediaExtractor::CreatorFunc {
            if (SniffMatroskaFFMPEG(source, confidence)) {
                return [](
                        DataSourceBase *source,
                        void *) -> MediaExtractor* {
                    return new FFmpegExtractor(source);};
            }
            return NULL;
        }
    };
}

} // extern "C"

} // namespace android
