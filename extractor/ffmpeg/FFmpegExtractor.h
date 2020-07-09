/*
 * Copyright 2012 Michael Chen <omxcodec@gmail.com>
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
 *
 * Modified by Ray Park <ray@nexell.co.kr>
 */

#ifndef SUPER_EXTRACTOR_H_

#define SUPER_EXTRACTOR_H_

#include <media/stagefright/foundation/ABase.h>

#ifdef PIE
#include <media/MediaExtractor.h>
#include <media/stagefright/MetaDataBase.h>
#else
#include <media/stagefright/MediaExtractor.h>
#endif

#include <utils/threads.h>
#include <utils/KeyedVector.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <limits.h> /* INT_MAX */
#include <sys/time.h>

#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    bool bPend;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} PacketQueue;

namespace android {

class String8;
class FFMpegSource;
class TrackInfo;

class FFmpegExtractor : public MediaExtractor
{
public:
#ifdef PIE
    explicit FFmpegExtractor(DataSourceBase *source);
#else
    FFmpegExtractor(const sp<DataSource> &source);
#endif

    virtual size_t countTracks();
#ifdef PIE
    virtual MediaTrack *getTrack(size_t index);
    virtual status_t getTrackMetaData(MetaDataBase& meta, size_t index, uint32_t flags);
    virtual status_t getMetaData(MetaDataBase& meta);
#else
    virtual sp<IMediaSource> getTrack(size_t index);
    virtual sp<MetaData> getTrackMetaData(size_t index, uint32_t flags);

    virtual sp<MetaData> getMetaData();
#endif

    virtual uint32_t flags() const;

protected:
    virtual ~FFmpegExtractor();

private:
    friend class FFMpegSource;

    class TrackInfo;

    mutable Mutex mExtractorLock;
    mutable Mutex mSeekLock;
#ifdef PIE
    DataSourceBase *mDataSource;
#else
    sp<DataSource> mDataSource;
#endif
    status_t mInitCheck;

    KeyedVector<unsigned, sp<TrackInfo> > mTrackMetas;

    char mFilename[1024];
    int mGenPTS;
    int mVideoDisable;
    int mAudioDisable;
    int mShowStatus;
    int mSeekByBytes;
    int mAutoExit;
    int64_t mStartTime;
    int64_t mDuration;
    int mLoop;
    bool mEOF;
    bool mEOF2;
    size_t mProbePkts;
    int mMyInstanceId;

    int mAbortRequest;
    int mPaused;
    int mLastPaused;
    int mSeekReq;
    int mSeekFlags;
    int64_t mSeekPos;
    int mReadPauseReturn;
    PacketQueue mAudioQ;
    PacketQueue mVideoQ;
    bool mVideoEOSReceived;
    bool mAudioEOSReceived;

	// 20170515 added by hcjun for readEntry Thread
    bool mbreaderEntryExit;
    int64_t mVideoPktTsPrev;
    int64_t mAudioPktTsPrev;
    bool mbMakeEOS;

    AVFormatContext *mFormatCtx;
    int mVideoStreamIdx;
    int mAudioStreamIdx;
    AVStream *mVideoStream;
    AVStream *mAudioStream;
    bool mVideoQInited;
    bool mAudioQInited;
    bool mDefersToCreateVideoTrack;
    bool mDefersToCreateAudioTrack;
    AVBitStreamFilterContext *mVideoBsfc;
    AVBitStreamFilterContext *mAudioBsfc;
    static int decode_interrupt_cb(void *ctx);
    void print_error_ex(const char *filename, int err);
    int initStreams();
    void deInitStreams();
#ifdef PIE
    void buildFileName(DataSourceBase *source);
    bool MakeAVCCodecSpecificData(MetaDataBase &meta, const uint8_t *data, size_t size);
#else
    void buildFileName(const sp<DataSource> &source);
#endif
    void setFFmpegDefaultOpts();
    void printTime(int64_t time);
    int stream_component_open(int stream_index);
    void stream_component_close(int stream_index);
    void packet_queue_init(PacketQueue *q);
    void packet_queue_flush(PacketQueue *q);
    void packet_queue_end(PacketQueue *q);
    void packet_queue_abort(PacketQueue *q);
    int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
    int packet_queue_put(PacketQueue *q, AVPacket *pkt);
    void reachedEOS(enum AVMediaType media_type);
    int stream_seek(int64_t pos, enum AVMediaType media_type);
    int check_extradata(AVCodecContext *avctx);

    bool mReaderThreadStarted;
    pthread_t mReaderThread;
    status_t startReaderThread();
    void stopReaderThread();
    static void *ReaderWrapper(void *me);
    void readerEntry();

	//	20170104 added by ray park for seek monitoring.
	bool exitSeekMonitor;
	pthread_t mSeekMonitorThread;
	int mSeekTimeoutTime;	//	milli seconds
	void startSeekMonitor(int32_t mSec);
	void stopSeekMonitor();
	static void *SeekMonitorThreadStub(void*me);
	void *SeekMonitorThread();

	// 20170515 added by hcjun for readEntry Thread
	int32_t resumeInit();
    int32_t timeStampCheck(AVPacket *pkt);
    void closeStream();

    DISALLOW_EVIL_CONSTRUCTORS(FFmpegExtractor);
};

#ifdef PIE
bool SniffMatroskaFFMPEG( DataSourceBase *source, float *confidence);
bool SniffAVIFFMPEG( DataSourceBase *source, float *confidence);
bool SniffFFMPEG(DataSourceBase *source, float *confidence);
#else
bool SniffFFMPEG(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);
bool SniffAVIFFMPEG(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);
#endif

}  // namespace android

#endif  // SUPER_EXTRACTOR_H_

