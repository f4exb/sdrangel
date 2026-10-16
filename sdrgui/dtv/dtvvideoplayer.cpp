///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <thread>

#include <QDebug>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>

// C++ headers FFmpeg's hardware contexts include, which can't be in extern "C"
#if defined(_WIN32)
#include <d3d11.h>
#endif
#if defined(__APPLE__)
#include <VideoToolbox/VideoToolbox.h>
#endif

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#if defined(_WIN32)
#include <libavutil/hwcontext_d3d11va.h>
#endif
#if defined(__APPLE__)
#include <libavutil/hwcontext_videotoolbox.h>
#endif
// CUDA frames' context, without the CUDA SDK: its first member is the CUcontext
#ifndef CUDA_VERSION
#define CUDA_VERSION 11000
typedef struct CUctx_st *CUcontext;
typedef struct CUstream_st *CUstream;
#endif
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#ifdef DTV_DEINTERLACE
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#endif
}

// FFmpeg's APIs that differ between the versions built with (4.4 on). AVChannelLayout from FFmpeg 5.1 (the channel
// masks deprecated then, and gone in 7)
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
#define DTV_CH_LAYOUT
#endif
// AVFrame's interlacing flags from 6.1 (its fields deprecated then)
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 7, 100)
#define DTV_FRAME_FLAGS
#endif
// Threaded conversion with sws_scale_frame(), used from 7 (it's in 5, but its threads were new)
#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(8, 0, 100)
#define DTV_SWS_THREADS
#endif

#include "audio/audiofifo.h"
#include "util/profiler.h"

#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>
#endif

#include "dtv/dtvtsfifo.h"
#include "dtv/dtvvideoplayer.h"

std::chrono::steady_clock::duration DTVVideoPlayer::seconds(double s)
{
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(s));
}

// A PES header's PTS (33 bits, 90 kHz), from its 5 bytes
int64_t DTVVideoPlayer::pesPTS(const uint8_t *h)
{
    return ((int64_t) ((h[0] >> 1) & 7) << 30) | ((int64_t) h[1] << 22) | ((int64_t) (h[2] >> 1) << 15)
        | ((int64_t) h[3] << 7) | (h[4] >> 1);
}

// Whether TS packets of a PID were lost before this one: the continuity counter of a good packet isn't one more than
// the last good one's, allowing for those with errors between (whose counters can't be trusted), nor the same (a
// duplicate), without a discontinuity signalled. Updates the last good counter, and the count of those with errors
bool DTVVideoPlayer::continuityLost(const uint8_t *p, bool discontinuity, int& lastCC, int& errorPackets)
{
    const bool error = (p[1] & 0x80) != 0;
    const int cc = p[3] & 0x0f;
    const bool lost = !error && !discontinuity && (lastCC >= 0) && (cc != ((lastCC + 1 + errorPackets) & 0x0f)) && (cc != lastCC);

    if (error)
    {
        errorPackets++;
    }
    else
    {
        lastCC = cc;
        errorPackets = 0;
    }

    return lost;
}

DTVVideoPlayer::DTVVideoPlayer(DTVTSFifo *tsFifo, AudioFifo *audioFifo, QObject *parent) :
    QThread(parent),
    m_tsFifo(tsFifo),
    m_audioFifo(audioFifo),
    m_stop(false),
    m_requestedService(0),
    m_audioMute(false),
    m_audioGain(0.1f),
    m_deinterlace(true),
    m_hardwareDecoding(false),
    m_reselect(false),
    m_selectedRequest(-1),
    m_formatCtx(nullptr),
    m_ioCtx(nullptr),
    m_videoCtx(nullptr),
    m_audioCtx(nullptr),
    m_adCtx(nullptr),
    m_subtitleCtx(nullptr),
    m_frame(nullptr),
    m_videoFrame(nullptr),
    m_transferFrame(nullptr),
    m_hwDeviceCtx(nullptr),
    m_hwDeviceType(AV_HWDEVICE_TYPE_NONE),
    m_hwPixelFormat(-1),
    m_videoGPU(-1),
    m_videoAwaitKey(false),
    m_videoAwaitPackets(0),
    m_packet(nullptr),
    m_videoStream(-1),
    m_videoTimeBaseNum(1),
    m_videoTimeBaseDen(90000),
    m_videoSARNum(0),
    m_videoSARDen(1),
    m_audioStream(-1),
    m_adStream(-1),
    m_subtitleStream(-1),
    m_programIndex(-1),
    m_audioType(Track::Main),
    m_subtitles(false),
    m_subtitleType(Track::Subtitles),
    m_tracksChanged(false),
    m_adSwrCtx(nullptr),
    m_adSwrRate(0),
    m_adStartPts(0.0),
    m_adPid(-1),
    m_fadeGain(1.0f),
    m_muteGain(1.0f),
    m_audioNextPts(NAN),
    m_audioFillAfterWrite(0),
    m_videoPid(-1),
    m_audioPid(-1),
    m_audioPESBytes(0),
    m_audioCC(-1),
    m_audioErrorPackets(0),
    m_framePESPts(-1),
    m_framePESOffset(0),
    m_audioFrameSize(0),
    m_audioDamagedLast(false),
    m_audioPeak(32767),
    m_adControl{0.0, 0, 0},
    m_subtitleShown{0.0, 0.0, nullptr},
    m_videoBytes(0),
    m_videoStop(false),
    m_swsCtx(nullptr),
    m_swsWidth(0),
    m_swsHeight(0),
    m_swsFormat(-1),
    m_frameWidth(0),
    m_frameHeight(0),
    m_filterGraph(nullptr),
    m_bufferSrc(nullptr),
    m_bufferSink(nullptr),
    m_filtered(nullptr),
    m_filterWidth(0),
    m_filterHeight(0),
    m_filterFormat(-1),
    m_filterFrames(nullptr),
    m_gpuDeinterlaceFailed(false),
    m_audioClockValid(false),
    m_audioEndPts(0.0),
    m_audioClockRate(48000),
    m_clockValid(false),
    m_timestampRef(0),
    m_timestampRefValid(false),
    m_clockPts(0.0),
    m_swrCtx(nullptr),
    m_swrRate(0),
    m_audioSampleRate(48000),
    m_audioLatency(0.0),
    m_avOffset(0.0),
    m_info{0, "", "", "", 0, 0, 0, 0, 0, 0, false, 0, 0, false, 0, 0, 0}
{
    setObjectName("DTVVideoPlayer");
    qRegisterMetaType<DTVVideoPlayer::Service>();
    qRegisterMetaType<QList<DTVVideoPlayer::Service>>();
    qRegisterMetaType<DTVVideoPlayer::Tracks>();

    // FFmpeg's log level is the process's: set once. Received streams have errors, which the decoders would log for
    // each damaged picture or packet, flooding the log
    static std::once_flag logLevel;
    std::call_once(logLevel, []() { av_log_set_level(AV_LOG_FATAL); });
}

bool DTVVideoPlayer::canDeinterlace()
{
#ifdef DTV_DEINTERLACE
    return true;
#else
    return false;
#endif
}

DTVVideoPlayer::~DTVVideoPlayer()
{
    stopPlayer();
}

void DTVVideoPlayer::stopPlayer()
{
    m_stop = true;
    wait();
}

void DTVVideoPlayer::setAudioOutput(int sampleRate, double latency)
{
    if (sampleRate > 0) {
        m_audioSampleRate = sampleRate;
    }

    m_audioLatency = std::max(0.0, std::min(1.0, latency));
}

void DTVVideoPlayer::setAudioVolume(int volume)
{
    volume = std::max(0, std::min(100, volume));
    // 0.01 to 1, logarithmic, and 0 silent
    m_audioGain = volume == 0 ? 0.0f : (float) std::pow(10.0, volume * 0.02 - 2.0);
}

void DTVVideoPlayer::setTrackPreferences(const QString& audioLanguage, Track::Type audioType, bool subtitles,
    const QString& subtitleLanguage, Track::Type subtitleType)
{
    QMutexLocker locker(&m_tracksMutex);

    if ((audioLanguage != m_audioLanguage) || (audioType != m_audioType) || (subtitles != m_subtitles)
        || (subtitleLanguage != m_subtitleLanguage) || (subtitleType != m_subtitleType))
    {
        m_audioLanguage = audioLanguage;
        m_audioType = audioType;
        m_subtitles = subtitles;
        m_subtitleLanguage = subtitleLanguage;
        m_subtitleType = subtitleType;
        m_tracksChanged = true;
    }
}

DTVVideoPlayer::Info DTVVideoPlayer::getInfo()
{
    QMutexLocker locker(&m_infoMutex);
    return m_info;
}

void DTVVideoPlayer::run()
{
    m_frame = av_frame_alloc();
    m_videoFrame = av_frame_alloc();
    m_transferFrame = av_frame_alloc();
    m_filtered = av_frame_alloc();
    m_packet = av_packet_alloc();

    while (!m_stop)
    {
        if (!open())
        {
            if (!m_stop) {
                msleep(100);
            }

            continue;
        }

        while (!m_stop && readAndDecode()) {
        }

        close();
    }

    av_buffer_unref(&m_hwDeviceCtx);
    av_packet_free(&m_packet);
    av_frame_free(&m_filtered);
    av_frame_free(&m_transferFrame);
    av_frame_free(&m_videoFrame);
    av_frame_free(&m_frame);
}

// FFmpeg reads the transport stream from the FIFO, waiting for the demodulator as needed
int DTVVideoPlayer::readPacket(void *opaque, uint8_t *buf, int size)
{
    DTVVideoPlayer *player = static_cast<DTVVideoPlayer *>(opaque);

    while (!player->m_stop)
    {
        // Probing for too long, as when all the services are scrambled (FFmpeg gets no packets, so probes on): it
        // ends as at the end of the stream, with what's known from the PMT, and reading goes on after (open())
        if (player->m_probing && player->m_probeTimed && (std::chrono::steady_clock::now() > player->m_probeDeadline))
        {
            player->m_probeStopped = true;
            return AVERROR_EOF;
        }

        PROFILER_START()
        int n = player->m_tsFifo->read(buf, size, 100);
        PROFILER_STOP("DTV video waiting for TS")

        if (n > 0)
        {
            // The probe's time from its first data: waiting for the demodulator to lock (seconds, for DVB-T2 or a
            // weak signal) isn't probing
            if (player->m_probing && !player->m_probeTimed)
            {
                player->m_probeTimed = true;
                player->m_probeDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((int) (maxProbeSeconds * 1000));
            }

            player->m_psi.feed(buf, n);
            player->parseTS(buf, n);
            return n;
        }
    }

    return AVERROR_EOF;
}

bool DTVVideoPlayer::open()
{
    m_formatCtx = avformat_alloc_context();
    m_psi.reset();
    m_adControls.clear();
    m_videoTracks.clear();
    m_tsCarry.clear();
    m_transportStreamId = -1;
    m_timestampRefValid = false;
    uint8_t *ioBuffer = (uint8_t *) av_malloc(m_ioBufferSize);
    m_ioCtx = avio_alloc_context(ioBuffer, m_ioBufferSize, 0, this, &DTVVideoPlayer::readPacket, nullptr, nullptr);
    m_formatCtx->pb = m_ioCtx;
    m_formatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

    // A multiplex carries many services, so limit how much is read to find their parameters
    AVDictionary *options = nullptr;
    av_dict_set(&options, "probesize", "6000000", 0);
    av_dict_set(&options, "analyzeduration", "2000000", 0);
    auto inputFormat = av_find_input_format("mpegts");
    int ret = avformat_open_input(&m_formatCtx, nullptr, inputFormat, &options);
    av_dict_free(&options);

    if (ret < 0)
    {
        qDebug() << "DTVVideoPlayer::open: avformat_open_input failed:" << ret;
        close();
        return false;
    }

    m_probing = true;
    m_probeTimed = false;
    m_probeStopped = false;
    ret = avformat_find_stream_info(m_formatCtx, nullptr);
    m_probing = false;

    if (m_probeStopped)
    {
        // Reading goes on from where probing stopped
        m_ioCtx->eof_reached = 0;
        m_ioCtx->error = 0;
        qDebug("DTVVideoPlayer::open: probing stopped after %.0f s of data: some streams gave no packets (scrambled?)", maxProbeSeconds);
    }

    if ((ret < 0) && !m_probeStopped)
    {
        qDebug() << "DTVVideoPlayer::open: avformat_find_stream_info failed";
        close();
        return false;
    }

    // The multiplex's, as probed: taken a second later, another's (after a quick second retune) could be taken for it
    m_transportStreamId = m_psi.getTransportStreamId();
    m_services = getServices();
    m_servicesChecked = std::chrono::steady_clock::now();
    qDebug() << "DTVVideoPlayer::open:" << m_services.size() << "services";
    emit servicesChanged(m_services);
    m_selectedRequest = -1;

    return true;
}

// The services of the multiplex, named from the SDT if it's been received
QList<DTVVideoPlayer::Service> DTVVideoPlayer::getServices() const
{
    QList<Service> services;

    for (unsigned int i = 0; i < m_formatCtx->nb_programs; i++)
    {
        AVProgram *program = m_formatCtx->programs[i];
        Service service;
        AVDictionaryEntry *name = av_dict_get(program->metadata, "service_name", nullptr, 0);
        AVDictionaryEntry *provider = av_dict_get(program->metadata, "service_provider", nullptr, 0);
        service.m_id = program->id;
        service.m_name = name ? QString::fromUtf8(name->value) : QString("Service %1").arg(program->id);
        service.m_provider = provider ? QString::fromUtf8(provider->value) : QString();
        service.m_hasVideo = false;
        auto channel = m_psi.getChannels().find(program->id);
        service.m_channel = channel != m_psi.getChannels().end() ? channel->second : -1;

        for (unsigned int j = 0; j < program->nb_stream_indexes; j++)
        {
            if (m_formatCtx->streams[program->stream_index[j]]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                service.m_hasVideo = true;
            }
        }

        services.append(service);
    }

    return services;
}

void DTVVideoPlayer::updateServices()
{
    QList<Service> services = getServices();
    bool changed = services.size() != m_services.size();

    for (int i = 0; !changed && (i < services.size()); i++) {
        changed = (services[i].m_id != m_services[i].m_id) || (services[i].m_name != m_services[i].m_name)
            || (services[i].m_provider != m_services[i].m_provider) || (services[i].m_hasVideo != m_services[i].m_hasVideo)
            || (services[i].m_channel != m_services[i].m_channel);
    }

    if (changed)
    {
        m_services = services;
        emit servicesChanged(m_services);
    }

    // The service asked for now here, when another was selected (it wasn't, or wasn't yet): selected. Or the selected
    // programme's streams changed (its PMT updated: a part-time service starting, audio description or subtitles
    // added): selected again if its video's changed or nothing was playing, else its tracks chosen again
    const int requested = m_requestedService;
    AVProgram *program = nullptr;
    bool requestedPresent = false;

    for (unsigned int i = 0; i < m_formatCtx->nb_programs; i++)
    {
        requestedPresent = requestedPresent || ((requested > 0) && (m_formatCtx->programs[i]->id == requested));

        if (m_formatCtx->programs[i]->id == m_selectedProgram) {
            program = m_formatCtx->programs[i];
        }
    }

    if (requestedPresent && (requested != m_selectedProgram))
    {
        m_reselect = true;
        return;
    }

    if (!program) {
        return;
    }

    std::vector<int> streams(program->stream_index, program->stream_index + program->nb_stream_indexes);
    std::vector<std::string> signatures = signaturesOf(streams);

    if ((streams == m_selectedStreams) && (signatures != m_selectedSignatures))
    {
        // The same streams, described differently (FFmpeg updates a stream in place when its PMT changes): a track's
        // language or kind, or a subtitle track's page ids (in its extradata), which its decoder was opened with. The
        // service selected again if its video's changed, else its tracks chosen again (their decoders opened again)
        bool videoChanged = false;

        for (size_t k = 0; k < streams.size(); k++)
        {
            if ((m_formatCtx->streams[streams[k]]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                && ((k >= m_selectedSignatures.size()) || (signatures[k] != m_selectedSignatures[k]))) {
                videoChanged = true;
            }
        }

        if (!(m_videoCtx || m_audioCtx) || videoChanged) {
            m_reselect = true;
        } else {
            m_tracksChanged = true;
        }

        m_selectedSignatures = signatures;
    }

    if (streams != m_selectedStreams)
    {
        auto videoOf = [this](const std::vector<int>& indexes) {
            std::vector<int> video;

            for (int index : indexes)
            {
                if (m_formatCtx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                    video.push_back(index);
                }
            }

            return video;
        };

        if (!(m_videoCtx || m_audioCtx) || (videoOf(streams) != videoOf(m_selectedStreams))) {
            m_reselect = true;
        } else {
            m_tracksChanged = true;
        }

        m_selectedStreams = streams;
        m_selectedSignatures = signatures;
    }
}

// What decides a stream's track and decoder, copied (FFmpeg changes the stream in place)
std::vector<std::string> DTVVideoPlayer::signaturesOf(const std::vector<int>& streams) const
{
    std::vector<std::string> signatures;

    for (int index : streams)
    {
        const AVStream *stream = m_formatCtx->streams[index];
        const AVCodecParameters *par = stream->codecpar;
        const AVDictionaryEntry *language = av_dict_get(stream->metadata, "language", nullptr, 0);
        std::string s = std::to_string((int) par->codec_type) + ":" + std::to_string((int) par->codec_id) + ":"
            + std::to_string(stream->disposition) + ":" + (language ? language->value : "") + ":";

        if (par->extradata && (par->extradata_size > 0)) {
            s.append(reinterpret_cast<const char *>(par->extradata), par->extradata_size);
        }

        signatures.push_back(s);
    }

    return signatures;
}

void DTVVideoPlayer::close()
{
    closeDecoders();

    if (m_formatCtx)
    {
        if (m_formatCtx->iformat) {
            avformat_close_input(&m_formatCtx);
        } else {
            avformat_free_context(m_formatCtx);
        }

        m_formatCtx = nullptr;
    }

    if (m_ioCtx)
    {
        av_freep(&m_ioCtx->buffer);
        avio_context_free(&m_ioCtx);
    }

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_swsFormat = -1;
    }
}

void DTVVideoPlayer::closeDecoders()
{
    stopVideoThread();
    closeDeinterlacer();
    // Buffers still held (by the display) are freed when they're released
    freePool(m_transferPool);
    freePool(m_convertPool);
    m_frameWidth = 0; // So the next frame's size is recorded
    m_frameHeight = 0;
    avcodec_free_context(&m_videoCtx);
    avcodec_free_context(&m_videoSoftwareCtx);
    clearGOP();
    m_videoSoftware = false;
    m_videoFallback = false;
    m_videoDiscard = AVDISCARD_DEFAULT;
    m_lateScore = 0.0;
    m_onTimeFrames = 0;
    m_lastVideoPts = -1e300;
    m_replayShownPts = -1e300;
    m_handoverPts = -1e300;
    m_hwPixelFormat = AV_PIX_FMT_NONE;
    m_hwName.clear();
    m_videoGPU = -1;
    m_videoAwaitKey = false;
    m_videoAwaitPackets = 0;
    m_gpuDeinterlaceFailed = false;
    closeTrackDecoders();
    closeCaptions();    // The video thread's stopped
    m_captionsSeen = false;
    m_captionsListed = false;
    m_videoStream = -1;
    m_videoPid = -1;
    m_programIndex = -1;
    m_scrambled = false;

    QMutexLocker locker(&m_infoMutex);
    m_info.m_scrambled = false;
    m_info.m_width = 0;
    m_info.m_height = 0;
}

AVCodecContext *DTVVideoPlayer::openDecoder(AVStream *stream)
{
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);

    if (!codec) {
        return nullptr;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, stream->codecpar);
    ctx->thread_count = 0; // Automatic
    ctx->pkt_timebase = stream->time_base; // For subtitles' timestamps

    if (avcodec_open2(ctx, codec, nullptr) < 0)
    {
        avcodec_free_context(&ctx);
        return nullptr;
    }

    return ctx;
}

void DTVVideoPlayer::setHardwareDecoding(bool hardware)
{
    if (hardware != m_hardwareDecoding)
    {
        m_hardwareDecoding = hardware;
        m_reselect = true;
    }
}

// The hardware format (ctx->opaque points to it), if offered, else the first software format, as for a profile the
// hardware doesn't decode. File-local, not a member: it's AVCodecContext::get_format, so takes and returns
// AVPixelFormat, which the header can't forward-declare
static AVPixelFormat getHardwareFormat(AVCodecContext *ctx, const AVPixelFormat *formats)
{
    const int hwPixelFormat = *static_cast<const int *>(ctx->opaque);

    for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; format++)
    {
        if (*format == hwPixelFormat) {
            return *format;
        }
    }

    for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; format++)
    {
        const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get(*format);

        if (descriptor && !(descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            return *format;
        }
    }

    return AV_PIX_FMT_NONE;
}

// Hardware decoding with the platform's usual API, if FFmpeg was built with it and the codec supports it
bool DTVVideoPlayer::setupHardwareDecoding(const AVCodec *codec, AVCodecContext *ctx)
{
    // NVDEC first, as its frames can be deinterlaced with bwdif, as in software
    static const AVHWDeviceType types[] = {
#if defined(_WIN32)
        AV_HWDEVICE_TYPE_CUDA, AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_DXVA2,
#elif defined(__APPLE__)
        AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
#else
        AV_HWDEVICE_TYPE_CUDA, AV_HWDEVICE_TYPE_VAAPI,
#endif
    };

    // DTV_HWDEVICE (or, as before, DVBDEMOD_HWDEVICE) in the environment chooses one (e.g. cuda, d3d11va, vaapi), as
    // for testing
    QByteArray forced = qgetenv("DTV_HWDEVICE");

    if (forced.isEmpty()) {
        forced = qgetenv("DVBDEMOD_HWDEVICE");
    }

    const AVHWDeviceType forcedType = forced.isEmpty() ? AV_HWDEVICE_TYPE_NONE : av_hwdevice_find_type_by_name(forced.constData());

    for (AVHWDeviceType type : types)
    {
        if ((forcedType != AV_HWDEVICE_TYPE_NONE) && (type != forcedType)) {
            continue;
        }

        for (int i = 0; ; i++)
        {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);

            if (!config) {
                break;
            }

            if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) || (config->device_type != type)) {
                continue;
            }

            if (m_hwDeviceCtx && (m_hwDeviceType != type)) {
                av_buffer_unref(&m_hwDeviceCtx);
            }

            if (!m_hwDeviceCtx && (av_hwdevice_ctx_create(&m_hwDeviceCtx, type, nullptr, nullptr, 0) < 0)) {
                break; // Not on this computer
            }

            m_hwDeviceType = type;
            ctx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
            ctx->extra_hw_frames = 8; // Frames held by the display, and its deinterlacer
            ctx->opaque = &m_hwPixelFormat;
            ctx->get_format = getHardwareFormat;
            // The GPU does the work. If FFmpeg falls back to software in this context, m_videoSoftwareCtx (with
            // threads) takes over (startSoftwareFallback())
            ctx->thread_count = 1;
            m_hwPixelFormat = config->pix_fmt;
            m_hwName = QString(av_hwdevice_get_type_name(type)).toUpper();
            return true;
        }
    }

    return false;
}

AVCodecContext *DTVVideoPlayer::openVideoDecoder(AVStream *stream)
{
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);

    if (!codec) {
        return nullptr;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, stream->codecpar);
    ctx->thread_count = 0; // Automatic

    if (m_hardwareDecoding) {
        setupHardwareDecoding(codec, ctx);
    } else {
        av_buffer_unref(&m_hwDeviceCtx);
    }

    if (avcodec_open2(ctx, codec, nullptr) < 0)
    {
        avcodec_free_context(&ctx);
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwPixelFormat = AV_PIX_FMT_NONE;
        m_hwName.clear();
        return openDecoder(stream); // Software
    }

    return ctx;
}

// Choose the requested service, or the first with video, its first video stream, and its audio and subtitles
void DTVVideoPlayer::selectService(bool reselect)
{
    int requested = m_requestedService;
    m_selectedRequest = requested;

    // A service asked for that isn't in this multiplex (as just after a retune for it, the old multiplex still being
    // read) doesn't replace one playing: it's selected when it appears (updateServices())
    if ((requested > 0) && (m_videoCtx || m_audioCtx) && !reselect)
    {
        bool present = false;

        for (unsigned int i = 0; (i < m_formatCtx->nb_programs) && !present; i++) {
            present = m_formatCtx->programs[i]->id == requested;
        }

        if (!present) {
            return;
        }
    }

    m_reselect = false; // This applies the latest settings
    closeDecoders();
    m_clockValid = false;
    m_timestampRefValid = false; // Each service has its own timestamps
    m_lastShown = std::chrono::steady_clock::time_point();
    m_intervalMax = 0.0;
    m_intervalLastMax = 0.0;
    m_avSyncAverage = std::nan("");
    m_lastAudioWrite = std::chrono::steady_clock::time_point();

    {
        QMutexLocker locker(&m_infoMutex);
        m_info.m_avSync = std::nan("");
        m_info.m_maxFrameInterval = 0.0;
    }

    {
        std::lock_guard<std::mutex> lock(m_audioClockMutex);
        m_audioClockValid = false;
    }

    // The old service's audio overlaps the new's in time. Left queued, it would delay the new audio for good
    if (m_audioFifo) {
        m_audioFifo->clear();
    }

    // The service requested, else the first with video, else (a radio multiplex) the first with audio
    AVProgram *selected = nullptr;
    int firstVideo = -1, firstAudio = -1;

    for (unsigned int i = 0; i < m_formatCtx->nb_programs; i++)
    {
        AVProgram *program = m_formatCtx->programs[i];

        if (program->id == requested)
        {
            selected = program;
            m_programIndex = i;
            break;
        }

        for (unsigned int j = 0; j < program->nb_stream_indexes; j++)
        {
            const AVMediaType type = m_formatCtx->streams[program->stream_index[j]]->codecpar->codec_type;

            if ((type == AVMEDIA_TYPE_VIDEO) && (firstVideo < 0)) {
                firstVideo = (int) i;
            } else if ((type == AVMEDIA_TYPE_AUDIO) && (firstAudio < 0)) {
                firstAudio = (int) i;
            }
        }
    }

    if (!selected && ((firstVideo >= 0) || (firstAudio >= 0)))
    {
        m_programIndex = firstVideo >= 0 ? firstVideo : firstAudio;
        selected = m_formatCtx->programs[m_programIndex];
    }

    if (selected)
    {
        for (unsigned int j = 0; j < selected->nb_stream_indexes; j++)
        {
            int index = selected->stream_index[j];

            if ((m_formatCtx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) && (m_videoStream < 0)) {
                m_videoStream = index;
            }
        }
    }

    // All the streams are demultiplexed, though only the selected service's are decoded. If the others were discarded,
    // av_read_frame() would keep reading, never returning to see a different service requested, if the service had
    // neither video nor audio (as a data service), or stopped transmitting
    for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++) {
        m_formatCtx->streams[i]->discard = AVDISCARD_DEFAULT;
    }

    if (m_videoStream >= 0)
    {
        const AVStream *stream = m_formatCtx->streams[m_videoStream];
        m_videoTimeBaseNum = stream->time_base.num;
        m_videoTimeBaseDen = stream->time_base.den > 0 ? stream->time_base.den : 90000;
        m_videoSARNum = stream->sample_aspect_ratio.num;
        m_videoSARDen = stream->sample_aspect_ratio.den;
        m_videoPid = stream->id;
        m_videoCtx = openVideoDecoder(m_formatCtx->streams[m_videoStream]);

        if (m_videoCtx && m_videoCtx->hw_device_ctx) {
            m_videoSoftwareCtx = openDecoder(m_formatCtx->streams[m_videoStream]);
        }
    }

    m_tracksChanged = false;
    selectTracks();
    int id = selected ? selected->id : 0;
    m_selectedProgram = selected ? selected->id : -1;
    m_selectedStreams.clear();

    if (selected) {
        m_selectedStreams.assign(selected->stream_index, selected->stream_index + selected->nb_stream_indexes);
    }

    m_selectedSignatures = signaturesOf(m_selectedStreams);
    qDebug() << "DTVVideoPlayer::selectService: requested" << requested << "selected" << id
        << "video stream" << m_videoStream << "audio stream" << m_audioStream;

    {
        // Before the video thread starts, as it changes the decoder if the GPU's falls back to software
        QMutexLocker locker(&m_infoMutex);
        m_info.m_serviceId = id;
        m_info.m_videoCodec = m_videoCtx ? QString(m_videoCtx->codec->long_name) : QString();
        m_info.m_videoDecoder = m_videoCtx ? m_hwName : QString();
    }

    startVideoThread();
    emit serviceSelected(id, m_videoCtx != nullptr, m_audioCtx != nullptr);
}

bool DTVVideoPlayer::readAndDecode()
{
    const bool reselect = m_reselect.exchange(false);

    if ((m_requestedService != m_selectedRequest) || reselect) {
        selectService(reselect);
    } else if (m_tracksChanged.exchange(false)) {
        selectTracks();
    } else if (m_captionsSeen && !m_captionsListed && (m_programIndex >= 0)) {
        addCaptionTrack();
    }

    PROFILER_START()

    if (av_read_frame(m_formatCtx, m_packet) < 0) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();

    if (now - m_servicesChecked > seconds(m_servicesInterval))
    {
        // Another multiplex, after tuning elsewhere: FFmpeg would add its services to the last one's, and keep
        // playing the last service. So start again, as for a new stream
        const int transportStreamId = m_psi.getTransportStreamId();

        if (m_transportStreamId < 0)
        {
            m_transportStreamId = transportStreamId;
        }
        else if ((transportStreamId >= 0) && (transportStreamId != m_transportStreamId))
        {
            qDebug("DTVVideoPlayer::readAndDecode: transport stream id changed from %d to %d: reopening",
                m_transportStreamId, transportStreamId);
            av_packet_unref(m_packet);
            return false;
        }

        updateServices();
        m_servicesChecked = now;
    }

    PROFILER_STOP("DTV video demux (including waiting for TS)")

    if ((m_packet->stream_index == m_videoStream) || (m_packet->stream_index == m_audioStream)
        || (m_packet->stream_index == m_adStream) || (m_packet->stream_index == m_subtitleStream))
    {
        unwrapTimestamps(m_packet);
    }

    if ((m_packet->stream_index == m_videoStream) && m_videoCtx) {
        queueVideoPacket();
    } else if ((m_packet->stream_index == m_audioStream) && m_audioCtx) {
        PROFILER_RESTART()
        decodeAudio();
        PROFILER_STOP("DTV audio decode and output")
    } else if (m_packet->stream_index == m_adStream) {
        PROFILER_RESTART()
        decodeAudioDescription();
        PROFILER_STOP("DTV audio description decode")
    } else if ((m_packet->stream_index == m_subtitleStream) && m_subtitleCtx) {
        PROFILER_RESTART()
        decodeSubtitle();
        PROFILER_STOP("DTV subtitle decode")
    }

    av_packet_unref(m_packet);

    return true;
}

// The service's timestamps made continuous, each the nearest (modulo 2^33, the PTS's wrap) to the last of any of its
// streams. FFmpeg corrects for the wrap from a reference shared by the multiplex's programmes, which a PID shared
// between them (as on Sky's, each programme with its own PTS base) can change for another's streams mid-stream: the
// video's timestamps fell 2^33 (26.5 hours) behind the audio's, so every frame restarted the clock. What FFmpeg added
// doesn't matter here
void DTVVideoPlayer::unwrapTimestamps(AVPacket *packet)
{
    const AVRational timeBase = m_formatCtx->streams[packet->stream_index]->time_base;

    if ((timeBase.num != 1) || (timeBase.den != 90000)) {
        return;
    }

    const int64_t wrap = 1LL << 33;
    auto unwrap = [this, wrap](int64_t& ts) {
        if (ts == AV_NOPTS_VALUE) {
            return;
        }

        if (!m_timestampRefValid)
        {
            m_timestampRef = ts;
            m_timestampRefValid = true;
            return;
        }

        int64_t difference = (ts - m_timestampRef) & (wrap - 1);

        if (difference >= wrap / 2) {
            difference -= wrap;
        }

        ts = m_timestampRef + difference;
    };

    unwrap(packet->dts);
    unwrap(packet->pts);
    const int64_t latest = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;

    if (latest != AV_NOPTS_VALUE) {
        m_timestampRef = latest;
    }
}

// Pass a video packet to the video thread. If it has fallen far behind, rather than wait for it (which would hold up
// the audio, and the TS, which would overflow), the video waiting is dropped, and what follows up to a keyframe, where
// the video thread decodes from again
void DTVVideoPlayer::queueVideoPacket()
{
    if (videoFrameDamaged(m_packet)) {
        m_packet->flags |= AV_PKT_FLAG_CORRUPT;
    }

    const bool key = (m_packet->flags & AV_PKT_FLAG_KEY) != 0;
    uint64_t dropped = 0;

    {
        std::unique_lock<std::mutex> lock(m_videoMutex);

        if (m_videoBytes > m_maxVideoBytes)
        {
            for (AVPacket *packet : m_videoPackets)
            {
                dropped += packet ? 1 : 0;
                av_packet_free(&packet);
            }

            m_videoPackets.clear();
            m_videoBytes = 0;
            m_videoPackets.push_back(nullptr); // The discontinuity, for the video thread
            m_videoResync = true;
            qDebug("DTVVideoPlayer::queueVideoPacket: video fell behind: %llu packets dropped", (unsigned long long) dropped);
        }

        if (m_videoResync && !key)
        {
            dropped++;
        }
        else
        {
            m_videoResync = false;
            AVPacket *packet = av_packet_alloc();
            av_packet_move_ref(packet, m_packet);
            m_videoBytes += packet->size;
            m_videoPackets.push_back(packet);
        }
    }

    m_videoCondition.notify_all();

    if (dropped > 0)
    {
        QMutexLocker locker(&m_infoMutex);
        m_info.m_videoDropped += dropped;
    }
}

void DTVVideoPlayer::startVideoThread()
{
    if (!m_videoCtx) {
        return;
    }

    m_videoStop = false;
    m_videoThread = std::thread(&DTVVideoPlayer::videoThread, this);
}

void DTVVideoPlayer::stopVideoThread()
{
    if (m_videoThread.joinable())
    {
        {
            std::lock_guard<std::mutex> lock(m_videoMutex);
            m_videoStop = true;
        }

        m_videoCondition.notify_all();
        m_videoThread.join();
    }

    std::lock_guard<std::mutex> lock(m_videoMutex);

    for (AVPacket *packet : m_videoPackets) {
        av_packet_free(&packet);
    }

    m_videoPackets.clear();
    m_videoBytes = 0;
    m_videoResync = false;
}

void DTVVideoPlayer::videoThread()
{
#ifdef _WIN32
    // Frames are timed by sleeping, which by default is in 15.6 ms steps on Windows: uneven at 25 or 50 frames/s.
    // The thread mostly sleeps, so it can have priority over decoding and demodulation, to wake on time
    timeBeginPeriod(1);
    struct Restore { ~Restore() { timeEndPeriod(1); } } restore;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif

    while (true)
    {
        AVPacket *packet;

        {
            std::unique_lock<std::mutex> lock(m_videoMutex);
            m_videoCondition.wait(lock, [this] { return m_videoStop || !m_videoPackets.empty(); });

            if (m_videoStop) {
                return;
            }

            packet = m_videoPackets.front();
            m_videoPackets.pop_front();
            m_videoBytes -= packet ? packet->size : 0;
        }

        m_videoCondition.notify_all();

        if (packet) {
            decodeVideo(packet);
        } else {
            videoDiscontinuity();
        }

        av_packet_free(&packet);
    }
}

const AVHWFramesContext *DTVVideoPlayer::framesContext(const AVFrame *frame)
{
    return frame->hw_frames_ctx ? reinterpret_cast<const AVHWFramesContext *>(frame->hw_frames_ctx->data) : nullptr;
}

// If FFmpeg has a filter to deinterlace GPU frames of the format, without copying them to memory (setupDeinterlacer)
#ifdef DTV_DEINTERLACE
bool DTVVideoPlayer::hasGPUDeinterlacer(int format)
{
    return (format == AV_PIX_FMT_CUDA) || (format == AV_PIX_FMT_VAAPI) || (format == AV_PIX_FMT_VIDEOTOOLBOX);
}
#endif

void DTVVideoPlayer::decodeVideo(AVPacket *packet)
{
    // GPU decoders aren't as robust against damaged data as FFmpeg's own: NVDEC read thousands of bytes beyond a
    // corrupt slice's data, and crashed (also in an MPEG-2 keyframe FFmpeg hadn't flagged). So packets flagged with
    // transport errors the receiver couldn't correct, or lost (by queueVideoPacket()), aren't given to them; nor, as
    // NVDEC's MPEG-2 decoding also crashed on an intact picture after some were dropped (those after refer to them), is
    // anything else until an intact keyframe, when the GPU's decoder is flushed and starts again. Meanwhile FFmpeg
    // decodes in software, which conceals the damage, from the GOP's keyframe (its packets replayed): holding the
    // picture instead froze it for 2 s or more, as keyframes, the largest pictures, are the most often damaged. Unless
    // FFmpeg is decoding in software, after all, for a profile the GPU can't decode: then its own decoder takes over
    if (m_videoFallback)
    {
        decodeVideoSoftware(packet);
        return;
    }

    const bool gpuDecoding = m_videoCtx->hw_device_ctx && (m_videoGPU != 0);

    if (gpuDecoding && m_videoSoftwareCtx)
    {
        const bool key = (packet->flags & AV_PKT_FLAG_KEY) != 0;
        const bool corrupt = (packet->flags & AV_PKT_FLAG_CORRUPT) != 0;

        const double pts = packet->pts != AV_NOPTS_VALUE ? packet->pts * av_q2d(AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen}) : NAN;

        // After the handover, until a picture shown after the keyframe: those before it (refering to the GOP before)
        // decoded by software too
        if (m_handoverPts > -1e299)
        {
            if (!std::isnan(pts) && (pts < m_handoverPts))
            {
                decodeVideoSoftware(packet);

                if (corrupt) {
                    return;
                }
            }
            else
            {
                endSoftwareGOP(); // Its last frames before the GPU's
            }
        }

        if (m_videoSoftware)
        {
            if (!key || corrupt || std::isnan(pts))
            {
                decodeVideoSoftware(packet);
                return;
            }

            // An intact keyframe: the GPU decodes from it. Software too, until the pictures shown before it are done
            avcodec_flush_buffers(m_videoCtx);
            m_videoSoftware = false;
            m_handoverPts = pts;
            decodeVideoSoftware(packet);
        }
        else if (corrupt)
        {
            startSoftwareGOP();
            decodeVideoSoftware(packet);
            return;
        }
    }

    if (gpuDecoding && ((packet->flags & AV_PKT_FLAG_CORRUPT)
        || (m_videoAwaitKey && !(packet->flags & AV_PKT_FLAG_KEY) && (m_videoAwaitPackets < m_maxAwaitPackets))))
    {
        m_videoAwaitKey = true;
        m_videoAwaitPackets++;
        QMutexLocker locker(&m_infoMutex);
        m_info.m_videoDropped++;
        return;
    }

    if (m_videoAwaitKey)
    {
        avcodec_flush_buffers(m_videoCtx);
        m_videoAwaitKey = false;
        m_videoAwaitPackets = 0;
    }

    PROFILER_START()

    if (avcodec_send_packet(m_videoCtx, packet) < 0)
    {
        {
            QMutexLocker locker(&m_infoMutex);
            m_info.m_decodeErrors++;
        }

        // Damage the receiver didn't see: decoded in software too
        if (gpuDecoding && m_videoSoftwareCtx)
        {
            startSoftwareGOP();
            decodeVideoSoftware(packet);
        }
        else
        {
            m_videoAwaitKey = gpuDecoding;
        }

        return;
    }

    PROFILER_STOP("DTV video decode")

    // Kept from the keyframe on, for software decoding to replay if the GOP's damaged
    if (gpuDecoding && m_videoSoftwareCtx)
    {
        if (packet->flags & AV_PKT_FLAG_KEY)
        {
            clearGOP();
            m_gopValid = true;
        }

        if (m_gopValid && (m_gopBytes + packet->size <= m_maxGOPBytes))
        {
            m_gopPackets.push_back(av_packet_clone(packet));
            m_gopBytes += packet->size;
        }
        else
        {
            clearGOP(); // Too long a GOP: software decoding would start from the damage
        }
    }

    receiveVideoFrames(m_videoCtx);

    // FFmpeg fell back to software in the GPU's context, which is single threaded: too slow for HD (as HEVC on a GPU
    // that can't decode it). Its own decoder, with threads, takes over
    if ((m_videoGPU == 0) && m_videoCtx->hw_device_ctx && m_videoSoftwareCtx) {
        startSoftwareFallback();
    }
}

void DTVVideoPlayer::clearGOP()
{
    for (AVPacket *packet : m_gopPackets) {
        av_packet_free(&packet);
    }

    m_gopPackets.clear();
    m_gopBytes = 0;
    m_gopValid = false;
}

// Damage, with the GPU decoding: FFmpeg decodes in software from the GOP's keyframe, those frames the GPU had shown
// replayed without being shown again, those it hadn't put out not lost. Counted
void DTVVideoPlayer::startSoftwareGOP()
{
    avcodec_flush_buffers(m_videoSoftwareCtx);
    avcodec_flush_buffers(m_videoCtx);
    m_videoSoftware = true;
    m_handoverPts = -1e300;
    m_replayShownPts = m_lastVideoPts;

    {
        QMutexLocker locker(&m_infoMutex);
        m_info.m_softwareGOPs++;
    }

    replayGOP();
}

// FFmpeg fell back to software in the GPU's context: m_videoSoftwareCtx decodes from now on, from the GOP's keyframe
// (as for damage), the frames already shown not again. The GPU's context isn't used again for the service, so if a
// later sequence could be decoded by the GPU, it isn't (the service selected again would)
void DTVVideoPlayer::startSoftwareFallback()
{
    qDebug("DTVVideoPlayer::startSoftwareFallback: FFmpeg decodes in software: its decoder with threads takes over");
    avcodec_flush_buffers(m_videoSoftwareCtx);
    avcodec_flush_buffers(m_videoCtx); // Its frames let go of
    m_videoFallback = true;
    m_videoSoftware = false;
    m_handoverPts = -1e300;
    m_videoAwaitKey = false;
    m_videoAwaitPackets = 0;
    m_replayShownPts = m_lastVideoPts;
    replayGOP();
}

// The GOP's packets kept, from its keyframe, into the software decoder. The pictures already shown that nothing refers
// to aren't decoded (AVDISCARD_NONREF), so it catches up sooner, rather than the picture freezing then rushing on
void DTVVideoPlayer::replayGOP()
{
    std::vector<AVPacket*> packets;
    packets.swap(m_gopPackets);
    m_gopBytes = 0;
    m_gopValid = false;
    const double timeBase = av_q2d(AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen});

    for (AVPacket *packet : packets)
    {
        // As receiveVideoFrames() decides which were shown
        const double pts = packet->pts != AV_NOPTS_VALUE ? packet->pts * timeBase : NAN;
        const bool shown = !std::isnan(pts) && (pts <= m_replayShownPts) && (pts > m_replayShownPts - m_replayWindow);
        m_videoSoftwareCtx->skip_frame = shown ? AVDISCARD_NONREF : (AVDiscard) m_videoDiscard;
        decodeVideoSoftware(packet);
        av_packet_free(&packet);
    }

    m_videoSoftwareCtx->skip_frame = (AVDiscard) m_videoDiscard;
}

// The video before was dropped, as the video thread fell far behind: the next packet is a keyframe, which a GPU's
// decoder waits for (as after damage), and the GOP kept for software decoding no longer leads to it
void DTVVideoPlayer::videoDiscontinuity()
{
    if (!m_videoCtx) {
        return;
    }

    if (m_videoCtx->hw_device_ctx && (m_videoGPU != 0) && !m_videoFallback) {
        m_videoAwaitKey = true;
    }

    clearGOP();
}

// The decoders skip pictures nothing refers to (AVDISCARD_NONREF), or not (AVDISCARD_DEFAULT)
void DTVVideoPlayer::setVideoDiscard(int discard)
{
    qDebug("DTVVideoPlayer::setVideoDiscard: %s", discard == AVDISCARD_DEFAULT ? "decoding every picture again"
        : "frames late: skipping pictures nothing refers to");
    m_videoDiscard = discard;
    m_videoCtx->skip_frame = (AVDiscard) discard;

    if (m_videoSoftwareCtx) {
        m_videoSoftwareCtx->skip_frame = (AVDiscard) discard;
    }
}

// Shed decoding work while frames are consistently late (the decoder can't keep up), rather than decode pictures to
// drop them, until they've been on time for some seconds. A late frame now and then, as when the timestamps jump,
// doesn't
void DTVVideoPlayer::updateShedding(bool late)
{
    m_lateScore = m_lateScore * m_lateScoreDecay + (late ? 1.0 : 0.0);
    m_onTimeFrames = late ? 0 : m_onTimeFrames + 1;

    if ((m_videoDiscard == AVDISCARD_DEFAULT) && (m_lateScore >= m_shedLateScore)) {
        setVideoDiscard(AVDISCARD_NONREF);
    } else if ((m_videoDiscard != AVDISCARD_DEFAULT) && (m_onTimeFrames >= m_restoreOnTimeFrames)) {
        setVideoDiscard(AVDISCARD_DEFAULT);
    }
}

void DTVVideoPlayer::decodeVideoSoftware(AVPacket *packet)
{
    PROFILER_START()

    if (avcodec_send_packet(m_videoSoftwareCtx, packet) < 0)
    {
        QMutexLocker locker(&m_infoMutex);
        m_info.m_decodeErrors++;
        return;
    }

    PROFILER_STOP("DTV video software decode")
    receiveVideoFrames(m_videoSoftwareCtx);
}

// After the handover to the GPU: the software decoder's last frames (those before the keyframe), then the GPU's
void DTVVideoPlayer::endSoftwareGOP()
{
    avcodec_send_packet(m_videoSoftwareCtx, nullptr);
    receiveVideoFrames(m_videoSoftwareCtx);
    avcodec_flush_buffers(m_videoSoftwareCtx);
    m_videoSoftware = false;
    m_handoverPts = -1e300;
}

// The frames a decoder has ready, deinterlaced and shown
void DTVVideoPlayer::receiveVideoFrames(AVCodecContext *ctx)
{
    PROFILER_START()

    while (true)
    {
        PROFILER_RESTART()

        if (avcodec_receive_frame(ctx, m_videoFrame) != 0) {
            break;
        }

        PROFILER_STOP("DTV video decode")

        // Those already shown, as when a GOP's replayed for software decoding, not again
        const int64_t timestamp = m_videoFrame->best_effort_timestamp != AV_NOPTS_VALUE ? m_videoFrame->best_effort_timestamp : m_videoFrame->pts;

        if (timestamp != AV_NOPTS_VALUE)
        {
            const double pts = timestamp * av_q2d(AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen});

            // In the handover, frames before the keyframe from software, the rest from the GPU
            if ((m_handoverPts > -1e299) && ((ctx == m_videoSoftwareCtx) != (pts < m_handoverPts)))
            {
                av_frame_unref(m_videoFrame);
                continue;
            }

            if (m_replayShownPts > -1e299)
            {
                if ((pts <= m_replayShownPts) && (pts > m_replayShownPts - m_replayWindow))
                {
                    av_frame_unref(m_videoFrame);
                    continue;
                }

                m_replayShownPts = -1e300;
            }

            m_lastVideoPts = pts;
        }

        // Closed captions (A/53), from the picture's user data: the frames come in presentation order, as the
        // captions are sent. Before deinterlacing, which can make two frames of one
        if (const AVFrameSideData *captions = av_frame_get_side_data(m_videoFrame, AV_FRAME_DATA_A53_CC))
        {
            m_captionsSeen = true;

            if (m_captionsEnabled && (timestamp != AV_NOPTS_VALUE)) {
                decodeCaptions(captions, timestamp);
            }
        }

        if (!m_captionsEnabled && m_captionCtx) {
            closeCaptions();
        }

        // GPU frames the display can show stay there, else are copied to memory
        const bool gpu = (m_hwPixelFormat != AV_PIX_FMT_NONE) && (m_videoFrame->format == m_hwPixelFormat);
        const int type = gpu ? gpuFrameType(m_videoFrame) : -1;
        bool keepOnGPU = gpu && m_canShowCallback && (type >= 0) && m_canShowCallback(type);

        // FFmpeg chooses the GPU or software at each sequence header, so which is decoding is known from the frames
        if ((ctx == m_videoCtx) && (m_hwPixelFormat != AV_PIX_FMT_NONE) && ((gpu ? 1 : 0) != m_videoGPU))
        {
            m_videoGPU = gpu ? 1 : 0;
            QMutexLocker locker(&m_infoMutex);
            m_info.m_videoDecoder = gpu ? m_hwName : QString();
        }

#ifdef DTV_DEINTERLACE
        // Without the GPU's deinterlacer, frames are deinterlaced in memory
        if (keepOnGPU && m_deinterlace && m_gpuDeinterlaceFailed && (m_videoFrame->format != AV_PIX_FMT_D3D11)) {
            keepOnGPU = false;
        }

        // GPU frames the display can't show are deinterlaced by FFmpeg's GPU filter, where it has one for them, and
        // copied to memory after, rather than deinterlaced in memory by bwdif, sparing the CPU
        bool copyAfter = gpu && !keepOnGPU && m_deinterlace && !m_gpuDeinterlaceFailed && hasGPUDeinterlacer(m_videoFrame->format);
#else
        const bool copyAfter = false;
#endif

        if (gpu && !keepOnGPU && !copyAfter && !transferFromGPU(m_videoFrame)) {
            continue;
        }

#ifdef DTV_DEINTERLACE
        // D3D11 frames are deinterlaced by the display
        if (m_deinterlace && !(keepOnGPU && (m_videoFrame->format == AV_PIX_FMT_D3D11)))
        {
            const void *frames = m_videoFrame->hw_frames_ctx ? m_videoFrame->hw_frames_ctx->data : nullptr;

            if ((m_videoFrame->width != m_filterWidth) || (m_videoFrame->height != m_filterHeight)
                || (m_videoFrame->format != m_filterFormat) || (frames != m_filterFrames))
            {
                flushDeinterlacer();

                // The GPU's deinterlacer may not be in FFmpeg's build (as bwdif_cuda): then copied to memory, for bwdif
                if (!setupDeinterlacer(m_videoFrame) && m_videoFrame->hw_frames_ctx)
                {
                    m_gpuDeinterlaceFailed = true;
                    copyAfter = false;

                    if (!transferFromGPU(m_videoFrame)) {
                        continue;
                    }

                    setupDeinterlacer(m_videoFrame);
                }
            }

            if (m_filterGraph)
            {
                PROFILER_RESTART()

                if (av_buffersrc_add_frame_flags(m_bufferSrc, m_videoFrame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0)
                {
                    while (av_buffersink_get_frame(m_bufferSink, m_filtered) >= 0)
                    {
                        PROFILER_STOP("DTV video deinterlace")

                        if (!copyAfter || !m_filtered->hw_frames_ctx || transferFromGPU(m_filtered)) {
                            renderFrame(m_filtered);
                        }

                        av_frame_unref(m_filtered);
                        PROFILER_RESTART()
                    }
                }

                PROFILER_STOP("DTV video deinterlace")

                av_frame_unref(m_videoFrame);
                continue;
            }
        }
        else if (m_filterGraph)
        {
            closeDeinterlacer();
        }
#endif
        // Not deinterlaced after all
        if (copyAfter && !transferFromGPU(m_videoFrame)) {
            continue;
        }

        renderFrame(m_videoFrame);
        av_frame_unref(m_videoFrame);
    }
}

bool DTVVideoPlayer::getPooledBuffer(FramePool& pool, AVFrame *frame)
{
    if (!pool.m_pool || (frame->format != pool.m_format) || (frame->width != pool.m_width) || (frame->height != pool.m_height))
    {
        freePool(pool);

        // Laid out as av_frame_get_buffer() would: rows aligned for SIMD, with padding for filters that read beyond them
        const int align = 64;
        int linesize[4];
        ptrdiff_t linesizes[4];
        size_t sizes[4];

        if (av_image_fill_linesizes(linesize, (AVPixelFormat) frame->format, FFALIGN(frame->width, 32)) < 0) {
            return false;
        }

        for (int i = 0; i < 4; i++)
        {
            pool.m_linesize[i] = FFALIGN(linesize[i], align);
            linesizes[i] = pool.m_linesize[i];
        }

        if (av_image_fill_plane_sizes(sizes, (AVPixelFormat) frame->format, FFALIGN(frame->height, 32), linesizes) < 0) {
            return false;
        }

        size_t total = 0;

        for (int i = 0; i < 4; i++)
        {
            pool.m_offset[i] = total;
            total += sizes[i] > 0 ? sizes[i] + align : 0;
        }

        pool.m_pool = av_buffer_pool_init(total + align, nullptr);

        if (!pool.m_pool) {
            return false;
        }

        pool.m_format = frame->format;
        pool.m_width = frame->width;
        pool.m_height = frame->height;
    }

    frame->buf[0] = av_buffer_pool_get(pool.m_pool);

    if (!frame->buf[0]) {
        return false;
    }

    for (int i = 0; i < 4; i++)
    {
        frame->data[i] = pool.m_linesize[i] > 0 ? frame->buf[0]->data + pool.m_offset[i] : nullptr;
        frame->linesize[i] = pool.m_linesize[i];
    }

    frame->extended_data = frame->data;
    return true;
}

void DTVVideoPlayer::freePool(FramePool& pool)
{
    av_buffer_pool_uninit(&pool.m_pool);
    pool.m_format = -1;
    pool.m_width = 0;
    pool.m_height = 0;
}

// Copy a GPU frame to memory, in its place, into a buffer from m_transferPool. False, with the frame released, if it
// couldn't be
bool DTVVideoPlayer::transferFromGPU(AVFrame *frame)
{
    PROFILER_START()
    av_frame_unref(m_transferFrame);
    const AVHWFramesContext *frames = framesContext(frame);
    bool ok = false;

    if (frames)
    {
        // As av_hwframe_transfer_data() would allocate it: of the frames context's size, which may be larger than the
        // frame's (as it is for the GPU's alignment), in its format in memory, then cut to the frame's size
        m_transferFrame->format = frames->sw_format;
        m_transferFrame->width = frames->width;
        m_transferFrame->height = frames->height;
        ok = getPooledBuffer(m_transferPool, m_transferFrame) && (av_hwframe_transfer_data(m_transferFrame, frame, 0) >= 0);
    }

    if (!ok)
    {
        // Into a buffer FFmpeg allocates
        av_frame_unref(m_transferFrame);
        ok = av_hwframe_transfer_data(m_transferFrame, frame, 0) >= 0;
    }

    if (!ok || (av_frame_copy_props(m_transferFrame, frame) < 0))
    {
        av_frame_unref(m_transferFrame);
        av_frame_unref(frame);
        QMutexLocker locker(&m_infoMutex);
        m_info.m_decodeErrors++;
        return false;
    }

    m_transferFrame->width = frame->width;
    m_transferFrame->height = frame->height;
    av_frame_unref(frame);
    av_frame_move_ref(frame, m_transferFrame);
    PROFILER_STOP("DTV video transfer from GPU")
    return true;
}

// bwdif only processes frames that are marked as interlaced, outputting a frame for each frame
bool DTVVideoPlayer::setupDeinterlacer(const AVFrame *frame)
{
    closeDeinterlacer();
#ifdef DTV_DEINTERLACE
    AVRational timeBase = AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen};
    AVRational sar = frame->sample_aspect_ratio;

    if ((sar.num <= 0) || (sar.den <= 0)) {
        sar = AVRational{1, 1};
    }

    char args[256];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        frame->width, frame->height, frame->format, timeBase.num, timeBase.den, sar.num, sar.den);

    // bwdif in memory or with CUDA, the VAAPI or VideoToolbox deinterlacer for their frames
    const char *filter;

    switch (frame->format)
    {
    case AV_PIX_FMT_CUDA: filter = "bwdif_cuda=mode=send_frame:parity=auto:deint=interlaced"; break;
    case AV_PIX_FMT_VAAPI: filter = "deinterlace_vaapi=rate=frame:auto=1"; break;
    case AV_PIX_FMT_VIDEOTOOLBOX: filter = "yadif_videotoolbox=mode=send_frame:parity=auto:deint=interlaced"; break;
    default: filter = "bwdif=mode=send_frame:parity=auto:deint=interlaced"; break;
    }

    // Recorded as set up, even if it fails, so it isn't tried again for each frame
    m_filterWidth = frame->width;
    m_filterHeight = frame->height;
    m_filterFormat = frame->format;
    m_filterFrames = frame->hw_frames_ctx ? frame->hw_frames_ctx->data : nullptr;

    m_filterGraph = avfilter_graph_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    bool ok = m_filterGraph && outputs && inputs
        && (avfilter_graph_create_filter(&m_bufferSrc, avfilter_get_by_name("buffer"), "in", args, nullptr, m_filterGraph) >= 0)
        && (avfilter_graph_create_filter(&m_bufferSink, avfilter_get_by_name("buffersink"), "out", nullptr, nullptr, m_filterGraph) >= 0);

    // GPU frames: the source needs their frames context
    if (ok && frame->hw_frames_ctx)
    {
        AVBufferSrcParameters *parameters = av_buffersrc_parameters_alloc();
        parameters->format = frame->format;
        parameters->hw_frames_ctx = frame->hw_frames_ctx;
        ok = av_buffersrc_parameters_set(m_bufferSrc, parameters) >= 0;
        av_free(parameters);
    }

    if (ok)
    {
        outputs->name = av_strdup("in");
        outputs->filter_ctx = m_bufferSrc;
        outputs->pad_idx = 0;
        outputs->next = nullptr;
        inputs->name = av_strdup("out");
        inputs->filter_ctx = m_bufferSink;
        inputs->pad_idx = 0;
        inputs->next = nullptr;
        ok = (avfilter_graph_parse_ptr(m_filterGraph, filter, &inputs, &outputs, nullptr) >= 0)
            && (avfilter_graph_config(m_filterGraph, nullptr) >= 0);
    }

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    if (!ok)
    {
        qWarning() << "DTVVideoPlayer::setupDeinterlacer: failed:" << filter << args;
        avfilter_graph_free(&m_filterGraph);
        m_bufferSrc = nullptr;
        m_bufferSink = nullptr;
        return false;
    }

    return true;
#else
    (void) frame;
    return false;
#endif
}

// The deinterlacer's last frame (bwdif holds one until the next comes), shown rather than lost, before it's set up
// again for frames of another format or size: as each time software decodes a damaged GOP in place of the GPU, and
// the GPU takes over again
void DTVVideoPlayer::flushDeinterlacer()
{
#ifdef DTV_DEINTERLACE
    if (!m_filterGraph || (av_buffersrc_add_frame_flags(m_bufferSrc, nullptr, 0) < 0)) {
        return;
    }

    while (av_buffersink_get_frame(m_bufferSink, m_filtered) >= 0)
    {
        // A GPU frame the display can't show copied to memory, as it was deinterlaced to be
        const int type = m_filtered->hw_frames_ctx ? gpuFrameType(m_filtered) : -1;
        const bool canShow = !m_filtered->hw_frames_ctx || ((type >= 0) && m_canShowCallback && m_canShowCallback(type));

        if (canShow || transferFromGPU(m_filtered)) {
            renderFrame(m_filtered);
        }

        av_frame_unref(m_filtered);
    }
#endif
}

void DTVVideoPlayer::closeDeinterlacer()
{
#ifdef DTV_DEINTERLACE
    avfilter_graph_free(&m_filterGraph);
#endif
    m_filterGraph = nullptr;
    m_bufferSrc = nullptr;
    m_bufferSink = nullptr;
    m_filterFormat = -1;
    m_filterFrames = nullptr;
}

// The timestamp of the audio being played: of the end of the last audio written, less what is still queued. Muted
// audio is written too (silent), so the clock runs on
bool DTVVideoPlayer::getAudioClock(double& clock)
{
    std::lock_guard<std::mutex> lock(m_audioClockMutex);

    if (!m_audioClockValid || !m_audioFifo || (std::chrono::steady_clock::now() - m_audioWriteTime > seconds(m_audioClockStale))) {
        return false;
    }

    // What's being heard: the end of the audio written, less what's waiting in the FIFO and in the device. Less the
    // A/V offset, so frames are shown that much later
    clock = m_audioEndPts - m_audioFifo->fill() / (double) m_audioClockRate - m_audioLatency - m_avOffset;
    return true;
}

// Wait until the frame is due, so frames are shown evenly rather than as they are decoded, and in step with
// the audio. The audio clock moves in steps, as the audio device takes blocks of samples, so frames are timed by a
// steady clock, steered gradually towards the audio, or jumping to it if they are far apart. Without audio, the
// clock starts with some latency. With audio, frames can be due seconds ahead, as video is sent ahead of its audio.
// Give up waiting if the timestamps jump, as when the receiver reacquires.
// Returns false for a frame too late to show, as showing the frames that have fallen behind in quick succession
// looks worse than skipping them. late: it was too late, or so late the clock restarted (for shedding decoding work)
bool DTVVideoPlayer::waitForPresentation(const AVFrame *frame, bool& late)
{
    late = false;

    // Frames from the deinterlacer have pts, but not best_effort_timestamp
    int64_t timestamp = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;

    if (timestamp == AV_NOPTS_VALUE) {
        return true;
    }

    double pts = timestamp * av_q2d(AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen});
    auto now = std::chrono::steady_clock::now();
    double audio;

    if (!m_clockValid)
    {
        m_clockPts = pts;
        m_clockTime = now + seconds(m_latency);
        m_clockValid = true;
    }

    const bool audioClock = getAudioClock(audio);

    if (audioClock)
    {
        double error = audio - (m_clockPts + std::chrono::duration<double>(now - m_clockTime).count());

        if (std::abs(error) > m_clockJump)
        {
            m_clockPts = audio;
            m_clockTime = now;
        }
        else
        {
            m_clockTime -= seconds(clockGain * error);
        }
    }

    auto due = m_clockTime + seconds(pts - m_clockPts);
    double wait = std::chrono::duration<double>(due - now).count();
    const double maxWait = audioClock ? maxAudioLead : m_maxVideoLead;

    if ((wait > maxWait) || (wait < -m_maxVideoLag))
    {
        late = wait < 0.0;
        m_clockValid = false; // Restart the clock from the next frame
        PROFILER_ADD("DTV video clock restart (count)", 0)
        QMutexLocker locker(&m_infoMutex);
        m_info.m_clockRestarts++;
        return true;
    }

    if (wait < 0.0)
    {
        PROFILER_ADD("DTV video frame late (by)", (qint64) (-wait * 1e9))
        late = wait <= -maxLateness;

        // Without audio to keep to, frames late are the clock's fault (the stream came late, or slower than real time):
        // it starts again from this frame, shown now, with the latency in hand for the next. Skipping them instead
        // froze the picture until they were a second late
        if (!audioClock && late)
        {
            m_clockPts = pts;
            m_clockTime = now + seconds(m_latency);
            PROFILER_ADD("DTV video clock restart (count)", 0)
            QMutexLocker locker(&m_infoMutex);
            m_info.m_clockRestarts++;
            return true;
        }

        return !late;
    }

    // Wake now and then to see if the player is stopping, or the audio clock has jumped (as its timestamps have,
    // after the video's), which a long wait would otherwise miss
    while (!m_stop && !m_videoStop && (now < due))
    {
        std::this_thread::sleep_until(std::min(due, now + std::chrono::milliseconds(20)));
        now = std::chrono::steady_clock::now();

        if (audioClock && getAudioClock(audio)
            && (std::abs(audio - (m_clockPts + std::chrono::duration<double>(now - m_clockTime).count())) > m_clockJump))
        {
            m_clockPts = audio;
            m_clockTime = now;
            due = m_clockTime + seconds(pts - m_clockPts);
            wait = std::chrono::duration<double>(due - now).count();

            if ((wait > maxWait) || (wait < -m_maxVideoLag))
            {
                m_clockValid = false;
                PROFILER_ADD("DTV video clock restart (count)", 0)
                QMutexLocker locker(&m_infoMutex);
                m_info.m_clockRestarts++;
                return true;
            }
        }
    }

    return true;
}

namespace {

// A decoded frame for the display, holding a reference to the AVFrame (and for VAAPI, its DRM mapping) until shown
class FFmpegVideoFrame : public GLVideoFrame
{
public:
    explicit FFmpegVideoFrame(const AVFrame *frame) :
        m_frame(av_frame_clone(frame)),
        m_mapped(nullptr)
    {
    }

    ~FFmpegVideoFrame() override
    {
        av_frame_free(&m_mapped);
        av_frame_free(&m_frame);
    }

    AVFrame *m_frame;
    AVFrame *m_mapped;
};

} // namespace

// The GLVideoFrame type of a GPU frame the display might show (NV12 only), -1 if none
int DTVVideoPlayer::gpuFrameType(const AVFrame *frame) const
{
    const AVHWFramesContext *frames = framesContext(frame);

    if (!frames || (frames->sw_format != AV_PIX_FMT_NV12)) {
        return -1;
    }

    switch (frame->format)
    {
    case AV_PIX_FMT_D3D11: return GLVideoFrame::D3D11;
    case AV_PIX_FMT_CUDA: return GLVideoFrame::CUDA;
    case AV_PIX_FMT_VAAPI: return GLVideoFrame::DMABuf;
    case AV_PIX_FMT_VIDEOTOOLBOX: return GLVideoFrame::IOSurface;
    default: return -1;
    }
}

// A converter from frames of the format to YUV 4:2:0 of the same size. As it doesn't scale, a 4:2:0 format (10 bits,
// or semi-planar) needs nothing but the nearest sample, which is exact and fastest; 4:2:2's chroma is halved vertically,
// by a bilinear filter. With threads where FFmpeg has them for sws_scale_frame()
SwsContext *DTVVideoPlayer::createConverter(int width, int height, int format)
{
    const AVPixFmtDescriptor *descriptor = av_pix_fmt_desc_get((AVPixelFormat) format);
    const bool is420 = descriptor && (descriptor->log2_chroma_w == 1) && (descriptor->log2_chroma_h == 1);
    const int flags = is420 ? SWS_POINT : SWS_FAST_BILINEAR;
#ifdef DTV_SWS_THREADS
    SwsContext *ctx = sws_alloc_context();

    if (ctx)
    {
        av_opt_set_int(ctx, "srcw", width, 0);
        av_opt_set_int(ctx, "srch", height, 0);
        av_opt_set_int(ctx, "src_format", format, 0);
        av_opt_set_int(ctx, "dstw", width, 0);
        av_opt_set_int(ctx, "dsth", height, 0);
        av_opt_set_int(ctx, "dst_format", AV_PIX_FMT_YUV420P, 0);
        av_opt_set_int(ctx, "sws_flags", flags, 0);
        av_opt_set_int(ctx, "threads", 0, 0); // Automatic

        if (sws_init_context(ctx, nullptr, nullptr) >= 0) {
            return ctx;
        }

        sws_freeContext(ctx);
    }
#endif
    return sws_getContext(width, height, (AVPixelFormat) format, width, height, AV_PIX_FMT_YUV420P, flags, nullptr, nullptr, nullptr);
}

// The frame as the display takes it: YUV 4:2:0 in memory (converted if in another format), or a GPU frame's handles
GLVideoFramePtr DTVVideoPlayer::makeFrame(AVFrame *frame)
{
    std::shared_ptr<FFmpegVideoFrame> out;
    const int format = frame->format;

    if ((format == AV_PIX_FMT_YUV420P) || (format == AV_PIX_FMT_YUVJ420P) || (format == AV_PIX_FMT_NV12))
    {
        out = std::make_shared<FFmpegVideoFrame>(frame);
        out->m_type = format == AV_PIX_FMT_NV12 ? GLVideoFrame::NV12 : GLVideoFrame::Planar420;

        for (int i = 0; i < 3; i++)
        {
            out->m_data[i] = out->m_frame->data[i];
            out->m_linesize[i] = out->m_frame->linesize[i];
        }
    }
    else if (format == AV_PIX_FMT_CUDA)
    {
        out = std::make_shared<FFmpegVideoFrame>(frame);
        out->m_type = GLVideoFrame::CUDA;
        out->m_cudaPlane[0] = (uint64_t) (uintptr_t) out->m_frame->data[0];
        out->m_cudaPlane[1] = (uint64_t) (uintptr_t) out->m_frame->data[1];
        out->m_cudaPitch[0] = out->m_frame->linesize[0];
        out->m_cudaPitch[1] = out->m_frame->linesize[1];
        out->m_cudaContext = static_cast<AVCUDADeviceContext *>(framesContext(frame)->device_ctx->hwctx)->cuda_ctx;
        out->m_cudaContextOwner = std::shared_ptr<void>(av_buffer_ref(framesContext(frame)->device_ref), [](void *device) {
            AVBufferRef *ref = static_cast<AVBufferRef *>(device);
            av_buffer_unref(&ref);
        });
    }
#if defined(_WIN32)
    else if (format == AV_PIX_FMT_D3D11)
    {
        out = std::make_shared<FFmpegVideoFrame>(frame);
        AVD3D11VADeviceContext *device = static_cast<AVD3D11VADeviceContext *>(framesContext(frame)->device_ctx->hwctx);
        out->m_type = GLVideoFrame::D3D11;
        out->m_d3d11Device = device->device;
        out->m_d3d11Texture = out->m_frame->data[0];
        out->m_d3d11Index = (int) (intptr_t) out->m_frame->data[1];
        // The decoder uses the device's immediate context in another thread
        out->m_lock = [device]() { device->lock(device->lock_ctx); };
        out->m_unlock = [device]() { device->unlock(device->lock_ctx); };
    }
#endif
    else if (format == AV_PIX_FMT_VAAPI)
    {
        // Exported as DRM PRIME: the Y and UV planes, as separate layers (R8, GR88) or one NV12 layer
        out = std::make_shared<FFmpegVideoFrame>(frame);
        out->m_mapped = av_frame_alloc();
        out->m_mapped->format = AV_PIX_FMT_DRM_PRIME;

        if (av_hwframe_map(out->m_mapped, frame, AV_HWFRAME_MAP_READ) < 0) {
            return nullptr;
        }

        const AVDRMFrameDescriptor *desc = reinterpret_cast<const AVDRMFrameDescriptor *>(out->m_mapped->data[0]);
        const uint32_t r8 = 0x20203852, gr88 = 0x38385247; // DRM_FORMAT_R8, DRM_FORMAT_GR88

        for (int plane = 0; plane < 2; plane++)
        {
            const AVDRMPlaneDescriptor *p;

            if (desc->nb_layers >= 2) {
                p = &desc->layers[plane].planes[0];
            } else if ((desc->nb_layers == 1) && (desc->layers[0].nb_planes >= 2)) {
                p = &desc->layers[0].planes[plane];
            } else {
                return nullptr;
            }

            const AVDRMObjectDescriptor& object = desc->objects[p->object_index];
            out->m_dmaBufFd[plane] = object.fd;
            out->m_dmaBufOffset[plane] = (int) p->offset;
            out->m_dmaBufPitch[plane] = (int) p->pitch;
            out->m_dmaBufFourcc[plane] = plane == 0 ? r8 : gr88;
            out->m_dmaBufModifier[plane] = object.format_modifier;
        }

        out->m_dmaBufHasModifier = desc->objects[0].format_modifier != 0x00FFFFFFFFFFFFFFULL; // Not DRM_FORMAT_MOD_INVALID
        // Which surface (VASurfaceID) of which pool, so the display can keep the planes imported, rather than import
        // them for each frame. A reference to the frames context keeps it (and so its address) while the display holds it
        out->m_dmaBufSurface = (uint64_t) (uintptr_t) frame->data[3];
        out->m_dmaBufPool = frame->hw_frames_ctx->data;
        out->m_dmaBufPoolOwner = std::shared_ptr<void>(av_buffer_ref(frame->hw_frames_ctx), [](void *frames) {
            AVBufferRef *ref = static_cast<AVBufferRef *>(frames);
            av_buffer_unref(&ref);
        });
        out->m_type = GLVideoFrame::DMABuf;
    }
#if defined(__APPLE__)
    else if (format == AV_PIX_FMT_VIDEOTOOLBOX)
    {
        out = std::make_shared<FFmpegVideoFrame>(frame);
        out->m_type = GLVideoFrame::IOSurface;
        out->m_ioSurface = CVPixelBufferGetIOSurface(reinterpret_cast<CVPixelBufferRef>(out->m_frame->data[3]));

        if (!out->m_ioSurface) {
            return nullptr;
        }
    }
#endif
    else
    {
        // Another format in memory, as 4:2:2, or 10 bits: converted to 4:2:0. The converter's kept while the frames'
        // format and size are the same
        if (!m_swsCtx || (frame->width != m_swsWidth) || (frame->height != m_swsHeight) || (format != m_swsFormat))
        {
            sws_freeContext(m_swsCtx);
            m_swsCtx = createConverter(frame->width, frame->height, format);
            m_swsWidth = frame->width;
            m_swsHeight = frame->height;
            m_swsFormat = format;
        }

        if (!m_swsCtx) {
            return nullptr;
        }

        AVFrame *converted = av_frame_alloc();
        converted->format = AV_PIX_FMT_YUV420P;
        converted->width = frame->width;
        converted->height = frame->height;

        if (!getPooledBuffer(m_convertPool, converted) && (av_frame_get_buffer(converted, 0) < 0))
        {
            av_frame_free(&converted);
            return nullptr;
        }

        PROFILER_START()
#ifdef DTV_SWS_THREADS
        // With the converter's threads
        if (sws_scale_frame(m_swsCtx, converted, frame) < 0)
        {
            av_frame_free(&converted);
            return nullptr;
        }
#else
        sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height, converted->data, converted->linesize);
#endif
        PROFILER_STOP("DTV video format conversion")
        av_frame_copy_props(converted, frame);
        out = std::make_shared<FFmpegVideoFrame>(converted);
        av_frame_free(&converted);
        out->m_type = GLVideoFrame::Planar420;

        for (int i = 0; i < 3; i++)
        {
            out->m_data[i] = out->m_frame->data[i];
            out->m_linesize[i] = out->m_frame->linesize[i];
        }
    }

    out->m_width = frame->width;
    out->m_height = frame->height;
    AVRational sar = frame->sample_aspect_ratio;

    if ((sar.num <= 0) || (sar.den <= 0)) {
        sar = AVRational{m_videoSARNum, m_videoSARDen};
    }

    out->m_sampleAspectRatio = (sar.num > 0) && (sar.den > 0) ? av_q2d(sar) : 1.0;

    if ((out->m_sampleAspectRatio < 0.25) || (out->m_sampleAspectRatio > 4.0)) {
        out->m_sampleAspectRatio = 1.0;
    }

    // HD is BT.709, SD BT.601, unless the stream says (UHD's BT.2020)
    if ((frame->colorspace == AVCOL_SPC_BT2020_NCL) || (frame->colorspace == AVCOL_SPC_BT2020_CL)) {
        out->m_matrix = GLVideoFrame::BT2020;
    } else if (frame->colorspace == AVCOL_SPC_BT709) {
        out->m_matrix = GLVideoFrame::BT709;
    } else if ((frame->colorspace == AVCOL_SPC_BT470BG) || (frame->colorspace == AVCOL_SPC_SMPTE170M)) {
        out->m_matrix = GLVideoFrame::BT601;
    } else {
        out->m_matrix = frame->height >= 720 ? GLVideoFrame::BT709 : GLVideoFrame::BT601;
    }

    // Where the chroma samples are, for upsampling 4:2:0: left (MPEG-2, H.264, HEVC's default) unless the stream says
    out->m_chromaLocation = frame->chroma_location == AVCHROMA_LOC_CENTER ? GLVideoFrame::ChromaCenter
        : frame->chroma_location == AVCHROMA_LOC_TOPLEFT ? GLVideoFrame::ChromaTopLeft : GLVideoFrame::ChromaLeft;

    out->m_fullRange = (frame->color_range == AVCOL_RANGE_JPEG) || (format == AV_PIX_FMT_YUVJ420P);
#ifdef DTV_FRAME_FLAGS
    out->m_interlaced = (frame->flags & AV_FRAME_FLAG_INTERLACED) != 0;
    out->m_topFieldFirst = (frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
#else
    out->m_interlaced = frame->interlaced_frame != 0;
    out->m_topFieldFirst = frame->top_field_first != 0;
#endif
    const int64_t timestamp = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
    out->m_timestamp = timestamp != AV_NOPTS_VALUE ? timestamp * av_q2d(AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen}) : 0.0;
    return out;
}

void DTVVideoPlayer::renderFrame(AVFrame *frame)
{
    PROFILER_START()
    bool late;
    bool show = waitForPresentation(frame, late);
    PROFILER_STOP("DTV video waiting to present")
    updateShedding(late);

    if (!show)
    {
        QMutexLocker locker(&m_infoMutex);
        m_info.m_lateFrames++;
        return;
    }

    if ((frame->width != m_frameWidth) || (frame->height != m_frameHeight))
    {
        m_frameWidth = frame->width;
        m_frameHeight = frame->height;
        QMutexLocker locker(&m_infoMutex);
        m_info.m_width = frame->width;
        m_info.m_height = frame->height;
    }

    PROFILER_RESTART()
    GLVideoFramePtr out = makeFrame(frame);
    PROFILER_STOP("DTV video frame for display")

    if (!out) {
        return;
    }

    out->m_subtitle = currentSubtitle(out->m_timestamp);

    PROFILER_RESTART()

    if (m_frameCallback) {
        m_frameCallback(out);
    }

    PROFILER_STOP("DTV video display callback")

#ifdef ENABLE_PROFILER
    // Even intervals between frames mean smooth video: the maximum shows the worst stall
    auto shown = std::chrono::steady_clock::now();

    if (m_profileLastFrame.time_since_epoch().count() != 0) {
        PROFILER_ADD("DTV video interval between frames", (qint64) std::chrono::duration_cast<std::chrono::nanoseconds>(shown - m_profileLastFrame).count())
    }

    m_profileLastFrame = shown;
#endif

    // How far the video is behind the audio being heard, as it's shown, averaged over some frames
    double audio;
    const bool audioClock = getAudioClock(audio);

    if (audioClock)
    {
        const double sync = audio - out->m_timestamp;
        m_avSyncAverage = std::isnan(m_avSyncAverage) || (std::abs(sync - m_avSyncAverage) > 1.0) ? sync
            : m_avSyncAverage + avSyncGain * (sync - m_avSyncAverage);
    }
    else
    {
        m_avSyncAverage = std::nan("");
    }

    // The longest interval between frames, in this 5 s window and the last: stalls
    const auto now = std::chrono::steady_clock::now();

    if (now - m_intervalWindowStart > std::chrono::seconds(5))
    {
        m_intervalLastMax = m_intervalMax;
        m_intervalMax = 0.0;
        m_intervalWindowStart = now;
    }

    if (m_lastShown.time_since_epoch().count() != 0) {
        m_intervalMax = std::max(m_intervalMax, std::chrono::duration<double>(now - m_lastShown).count());
    }

    m_lastShown = now;

    QMutexLocker locker(&m_infoMutex);
    m_info.m_videoFrames++;
    m_info.m_avSync = m_avSyncAverage;
    m_info.m_maxFrameInterval = std::max(m_intervalMax, m_intervalLastMax);
}

// A resampler from a decoded frame's format to 16-bit samples at rate, stereo or mono
SwrContext *DTVVideoPlayer::createResampler(const AVFrame *frame, int rate, bool mono)
{
    SwrContext *swr = swr_alloc();
#ifndef DTV_CH_LAYOUT
    av_opt_set_int(swr, "in_channel_layout", frame->channel_layout ? frame->channel_layout : av_get_default_channel_layout(frame->channels), 0);
    av_opt_set_int(swr, "out_channel_layout", mono ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO, 0);
#else
    AVChannelLayout out;
    av_channel_layout_default(&out, mono ? 1 : 2);
    av_opt_set_chlayout(swr, "in_chlayout", &frame->ch_layout, 0);
    av_opt_set_chlayout(swr, "out_chlayout", &out, 0);
#endif
    av_opt_set_int(swr, "in_sample_rate", frame->sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate", rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", (AVSampleFormat) frame->format, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swr) < 0) {
        swr_free(&swr);
    }

    return swr;
}

SwrContext *DTVVideoPlayer::resamplerFor(SwrContext *&swr, ResamplerInput& input, int rate, bool mono, bool damaged)
{
    ResamplerInput frameInput;
    frameInput.m_format = m_frame->format;
    frameInput.m_rate = m_frame->sample_rate;
#ifndef DTV_CH_LAYOUT
    frameInput.m_channels = m_frame->channels;
    frameInput.m_layout = m_frame->channel_layout;
#else
    frameInput.m_channels = m_frame->ch_layout.nb_channels;
    frameInput.m_layout = m_frame->ch_layout.order == AV_CHANNEL_ORDER_NATIVE ? m_frame->ch_layout.u.mask : 0;
#endif

    // Samples in every channel's plane
    const int planes = av_sample_fmt_is_planar((AVSampleFormat) m_frame->format) ? frameInput.m_channels : 1;
    bool usable = (m_frame->nb_samples > 0) && (frameInput.m_channels > 0) && (frameInput.m_rate > 0) && m_frame->extended_data;

    for (int p = 0; usable && (p < planes); p++) {
        usable = m_frame->extended_data[p] != nullptr;
    }

    if (!usable) {
        return nullptr;
    }

    if (!swr || !(frameInput == input))
    {
        // A damaged frame's format is more likely the damage than a change (and changing back would lose the
        // resampler's history twice)
        if (swr && damaged) {
            return nullptr;
        }

        swr_free(&swr);
        swr = createResampler(m_frame, rate, mono);
        input = frameInput;
    }

    return swr;
}

// Resample to stereo at the audio device's rate, from the next frame's format
void DTVVideoPlayer::setupResampler()
{
    swr_free(&m_swrCtx);
    m_swrRate = m_audioSampleRate;
}

void DTVVideoPlayer::decodeAudio()
{
    if (m_audioCtx && (m_swrRate != m_audioSampleRate)) {
        setupResampler();
    }

    // The demuxer flags packets with transport errors the receiver couldn't correct: those frames that were in them
    const bool damaged = audioFrameDamaged(m_packet, (m_packet->flags & AV_PKT_FLAG_CORRUPT) != 0);
    // Decoders carry some of a frame into the next (MP2's synthesis filter bank, AAC's overlap), so the frame after a
    // damaged one is muted too
    const bool corrupt = damaged || m_audioDamagedLast;
    m_audioDamagedLast = damaged;

    if (!(m_packet->flags & AV_PKT_FLAG_CORRUPT)) {
        m_audioFrameSize = m_packet->size;
    }

    if (avcodec_send_packet(m_audioCtx, m_packet) < 0) {
        return;
    }

    while (avcodec_receive_frame(m_audioCtx, m_frame) == 0)
    {
        // Muted, the audio's written silent, so the video keeps to the audio clock, and the A/V offset still applies
        SwrContext *swr = m_audioFifo && (m_swrRate > 0) ? resamplerFor(m_swrCtx, m_swrInput, m_swrRate, false, corrupt) : nullptr;

        if (!swr && corrupt && m_audioFifo)
        {
            QMutexLocker locker(&m_infoMutex);
            m_info.m_audioConcealed++; // Dropped
        }

        // Silence only fills gaps between frames that were written, not where the audio was muted or dropped
        bool written = false;

        if (swr)
        {
            int maxSamples = (int) av_rescale_rnd(swr_get_delay(swr, m_frame->sample_rate) + m_frame->nb_samples,
                m_swrRate, m_frame->sample_rate, AV_ROUND_UP);
            m_audioBuffer.resize(2 * maxSamples);
            uint8_t *out[1] = {(uint8_t *) m_audioBuffer.data()};
            int samples = swr_convert(swr, out, maxSamples, (const uint8_t **) m_frame->extended_data, m_frame->nb_samples);

            if (samples > 0)
            {
                const bool hasPts = m_frame->best_effort_timestamp != AV_NOPTS_VALUE;
                const double pts = hasPts ? m_frame->best_effort_timestamp * av_q2d(m_formatCtx->streams[m_audioStream]->time_base) : NAN;
                const auto now = std::chrono::steady_clock::now();
                written = true;

                // Audio missing, as when packets were lost: silence in its place, then fade in. Less what the device
                // has already played as silence, if the FIFO ran dry waiting for this frame, which would delay the
                // audio for good
                if (hasPts && !std::isnan(m_audioNextPts) && (pts - m_audioNextPts > m_maxAudioGap) && (pts - m_audioNextPts < 1.0))
                {
                    const double dry = std::chrono::duration<double>(now - m_audioWriteTime).count() - m_audioFillAfterWrite / (double) m_swrRate;
                    const int silence = (int) std::lround(std::max(0.0, pts - m_audioNextPts - std::max(0.0, dry)) * m_swrRate);

                    if (silence > 0)
                    {
                        m_silence.assign(2 * silence, 0);
                        writeAudio(m_silence.data(), silence);
                    }

                    m_muteGain = 0.0f;
                    QMutexLocker locker(&m_infoMutex);
                    m_info.m_audioGaps++;
                }

                // A frame from a PES flagged corrupt, much louder than the last, is damage that wasn't located
                int peak = 0;

                for (int k = 0; k < 2 * samples; k++) {
                    peak = std::max(peak, std::abs((int) m_audioBuffer[k]));
                }

                const bool squawk = (m_packet->flags & AV_PKT_FLAG_CORRUPT) && (peak > 16384) && (peak > 4 * m_audioPeak);
                m_audioPeak = (corrupt || squawk) ? m_audioPeak : peak;

                mixAudioDescription(m_audioBuffer.data(), samples, pts, m_audioMute ? 0.0f : m_audioGain.load(), corrupt || squawk);
                const uint32_t samplesWritten = writeAudio(m_audioBuffer.data(), samples);
                m_audioNextPts = hasPts ? pts + m_frame->nb_samples / (double) m_frame->sample_rate : NAN;

                if (corrupt)
                {
                    QMutexLocker locker(&m_infoMutex);
                    m_info.m_audioConcealed++;
                }

                if (hasPts)
                {
                    m_audioFillAfterWrite = (int) m_audioFifo->fill();
                    std::lock_guard<std::mutex> lock(m_audioClockMutex);
                    // To the end of what was written (all of it, but for a FIFO smaller than a frame)
                    m_audioEndPts = pts + (m_frame->nb_samples / (double) m_frame->sample_rate) * samplesWritten / samples;
                    m_audioClockRate = m_swrRate;
                    m_audioWriteTime = now;
                    m_audioClockValid = true;
                }
            }
        }

        if (!written) {
            m_audioNextPts = NAN;
        }

        av_frame_unref(m_frame);
        QMutexLocker locker(&m_infoMutex);
        m_info.m_audioFrames++;
    }
}

// The audio description is decoded as it arrives, and kept until the main audio of the same time is decoded. Its
// codec isn't known until FFmpeg has seen some of its packets, which only come while a programme has a description
void DTVVideoPlayer::decodeAudioDescription()
{
    if (!m_adCtx)
    {
        AVStream *stream = m_formatCtx->streams[m_adStream];

        if (stream->codecpar->codec_id == AV_CODEC_ID_NONE) {
            return;
        }

        m_adCtx = openDecoder(stream);

        if (!m_adCtx) {
            return;
        }

        qDebug() << "DTVVideoPlayer::decodeAudioDescription:" << m_adCtx->codec->long_name;
    }

    if (m_swrRate <= 0) {
        return;
    }

    if (m_adSwrRate != m_swrRate)
    {
        swr_free(&m_adSwrCtx);
        m_adSwrRate = m_swrRate;
        m_adSamples.clear();
    }

    if (avcodec_send_packet(m_adCtx, m_packet) < 0) {
        return;
    }

    while (avcodec_receive_frame(m_adCtx, m_frame) == 0)
    {
        SwrContext *swr = m_frame->best_effort_timestamp != AV_NOPTS_VALUE
            ? resamplerFor(m_adSwrCtx, m_adSwrInput, m_adSwrRate, true, (m_packet->flags & AV_PKT_FLAG_CORRUPT) != 0) : nullptr;

        if (swr)
        {
            const double pts = m_frame->best_effort_timestamp * av_q2d(m_formatCtx->streams[m_adStream]->time_base);
            const int maxSamples = (int) av_rescale_rnd(swr_get_delay(swr, m_frame->sample_rate) + m_frame->nb_samples,
                m_adSwrRate, m_frame->sample_rate, AV_ROUND_UP);
            const size_t size = m_adSamples.size();
            m_adBuffer.resize(maxSamples);
            uint8_t *out[1] = {(uint8_t *) m_adBuffer.data()};
            const int samples = swr_convert(swr, out, maxSamples, (const uint8_t **) m_frame->extended_data, m_frame->nb_samples);

            if (samples > 0)
            {
                if (m_packet->flags & AV_PKT_FLAG_CORRUPT) {
                    std::fill(m_adBuffer.begin(), m_adBuffer.begin() + samples, 0); // As for the main audio
                }

                m_adSamples.insert(m_adSamples.end(), m_adBuffer.begin(), m_adBuffer.begin() + samples);
            }

            // Start again where the description doesn't follow on
            const double end = m_adStartPts + size / (double) m_adSwrRate;

            if ((size == 0) || (std::abs(pts - end) > 0.1))
            {
                m_adSamples.erase(m_adSamples.begin(), m_adSamples.begin() + size);
                m_adStartPts = pts;
            }

            // At most a few seconds, in case the main audio stops
            const size_t maxSize = 5 * m_adSwrRate;

            if (m_adSamples.size() > maxSize)
            {
                const size_t excess = m_adSamples.size() - maxSize;
                m_adSamples.erase(m_adSamples.begin(), m_adSamples.begin() + excess);
                m_adStartPts += excess / (double) m_adSwrRate;
            }

            m_adTime = std::chrono::steady_clock::now();
        }

        av_frame_unref(m_frame);
        QMutexLocker locker(&m_infoMutex);
        m_info.m_adFrames++;
    }
}

// Applies the volume (gain) to a block of stereo samples, starting at pts seconds, mixing in the audio description
// of the same time, if there is one, faded and panned as its descriptors say. The fade is ramped across the block.
// If mute, the block fades out in a millisecond, as it may be corrupt; after a muted block, the audio fades in
void DTVVideoPlayer::mixAudioDescription(int16_t *samples, int count, double pts, float gain, bool mute)
{
    const bool active = (m_adStream >= 0) && !std::isnan(pts) && (m_adSwrRate == m_swrRate)
        && (std::chrono::steady_clock::now() - m_adTime < seconds(m_audioClockStale));
    int start = count;      // Of the description in the block
    int mixed = 0;          // Description samples mixed

    if (active)
    {
        // The latest control by the end of the block. Timestamps wrap at 2^33 / 90 kHz, which FFmpeg's may not
        const double wrap = 8589934592.0 / 90000.0;
        const double end = std::fmod(pts, wrap) + count / (double) m_swrRate;

        std::deque<ADControl>& controls = m_adControls[m_adPid];

        while (!controls.empty() && ((controls.front().m_pts <= end) || (controls.front().m_pts > end + m_adControlWindow)))
        {
            if (controls.front().m_pts <= end) {
                m_adControl = controls.front();
            }

            controls.pop_front();
        }

        // Drop what's before the block, and find where the description starts in it
        if (!m_adSamples.empty())
        {
            long first = std::lround((pts - m_adStartPts) * m_adSwrRate);

            if (first > 0)
            {
                const long dropped = std::min<long>(first, (long) m_adSamples.size());
                m_adSamples.erase(m_adSamples.begin(), m_adSamples.begin() + dropped);
                m_adStartPts += dropped / (double) m_adSwrRate;
                first -= dropped;
            }

            if (!m_adSamples.empty() && (-first < count))
            {
                start = (int) -first;
                mixed = std::min(count - start, (int) m_adSamples.size());
            }
        }
    }
    else
    {
        m_adControl = ADControl{0.0, 0, 0};
    }

    const double pi = 3.14159265358979323846;
    const double sqrt2 = 1.41421356237309504880;
    // Fade: 0.3 dB steps. Pan: the angle clockwise from the centre, as a position between the speakers, constant power
    const float target = m_adControl.m_fade >= 255 ? 0.0f : std::pow(10.0f, -0.3f * m_adControl.m_fade / 20.0f);
    const double position = std::sin(m_adControl.m_pan * 2.0 * pi / 256.0);
    const float left = (float) (std::cos((position + 1.0) * pi / 4.0) * sqrt2) * gain;
    const float right = (float) (std::sin((position + 1.0) * pi / 4.0) * sqrt2) * gain;
    const float step = (target - m_fadeGain) / count;
    float fade = m_fadeGain;
    const float muteStep = mute ? -1000.0f / m_swrRate : 1000.0f / (5.0f * m_swrRate); // 1 ms out, 5 ms in
    float muteGain = m_muteGain;

    for (int i = 0; i < count; i++)
    {
        fade += step;
        muteGain = std::max(0.0f, std::min(1.0f, muteGain + muteStep));
        float l = samples[2 * i] * fade * gain;
        float r = samples[2 * i + 1] * fade * gain;

        if ((i >= start) && (i - start < mixed))
        {
            const float ad = m_adSamples[i - start];
            l += ad * left;
            r += ad * right;
        }

        l *= muteGain;
        r *= muteGain;

        samples[2 * i] = (int16_t) std::max(-32768.0f, std::min(32767.0f, l));
        samples[2 * i + 1] = (int16_t) std::max(-32768.0f, std::min(32767.0f, r));
    }

    m_fadeGain = target;
    m_muteGain = muteGain;

    if (mixed > 0)
    {
        m_adSamples.erase(m_adSamples.begin(), m_adSamples.begin() + mixed);
        m_adStartPts += mixed / (double) m_adSwrRate;
    }

    if (m_adStream >= 0)
    {
        QMutexLocker locker(&m_infoMutex);
        m_info.m_adFade = m_adControl.m_fade;
    }
}

// A good packet's adaptation field's discontinuity_indicator: its continuity counter may jump (ISO/IEC 13818-1
// 2.4.3.5), so a jump there isn't packets lost. Not trusted in a packet with errors
bool DTVVideoPlayer::signalledDiscontinuity(const uint8_t *p)
{
    return !(p[1] & 0x80) && (p[3] & 0x20) && (p[4] > 0) && (p[5] & 0x80);
}

// Note the PES of the main audio, and which of their payload bytes came in TS packets with errors
// (transport_error_indicator), or were lost (the continuity counter jumped, without a discontinuity signalled)
void DTVVideoPlayer::trackAudioPES(const uint8_t *buf, int size)
{
    if (m_audioPid < 0) {
        return;
    }

    for (int i = 0; i + 188 <= size; i += 188)
    {
        const uint8_t *p = buf + i;

        if ((p[0] != 0x47) || ((((p[1] & 0x1f) << 8) | p[2]) != m_audioPid)) {
            continue;
        }

        const bool discontinuity = signalledDiscontinuity(p);

        // Adaptation only (no payload): its counter doesn't count, but a discontinuity it signals does, for the next
        if (!(p[3] & 0x10))
        {
            if (discontinuity)
            {
                m_audioCC = -1;
                m_audioErrorPackets = 0;
            }

            continue;
        }

        const bool error = (p[1] & 0x80) != 0;
        const bool lost = continuityLost(p, discontinuity, m_audioCC, m_audioErrorPackets);
        const int offset = (p[3] & 0x20) ? 5 + p[4] : 4;
        const int length = std::max(0, 188 - offset);

        if (p[1] & 0x40)
        {
            // A new PES: the last is complete, and any packets lost before this were its end. Its PTS, and its payload
            // after the header, if its header is good
            if (!m_audioPES.empty())
            {
                if (lost) {
                    m_audioPES.back().m_damaged.push_back({m_audioPESBytes, INT_MAX});
                }

                m_audioPES.back().m_length = m_audioPESBytes;
            }

            const uint8_t *pes = p + offset;
            AudioPES next{-1, -1, {}};
            m_audioPESBytes = 0;

            if (!error && (length >= 14) && (pes[0] == 0) && (pes[1] == 0) && (pes[2] == 1) && (pes[7] & 0x80) && (9 + pes[8] <= length))
            {
                next.m_pts = pesPTS(pes + 9);
                m_audioPESBytes = length - 9 - pes[8];
            }

            m_audioPES.push_back(next);

            if (m_audioPES.size() > 64) {
                m_audioPES.pop_front();
            }

            continue;
        }

        if (m_audioPES.empty()) {
            continue;
        }

        if (lost) {
            m_audioPES.back().m_damaged.push_back({m_audioPESBytes, INT_MAX}); // Where the rest of it is isn't known
        }

        if (error) {
            m_audioPES.back().m_damaged.push_back({m_audioPESBytes, m_audioPESBytes + length});
        }

        m_audioPESBytes += length;
    }
}

// Whether each video PES had TS packets with errors, or lost packets (pictures are a PES each in broadcasts), for
// every PID found carrying video (a PES of stream id 0xE0 to 0xEF), from the stream's opening
void DTVVideoPlayer::trackVideoPES(const uint8_t *buf, int size)
{
    for (int i = 0; i + 188 <= size; i += 188)
    {
        const uint8_t *p = buf + i;
        const int pid = ((p[1] & 0x1f) << 8) | p[2];

        if ((p[0] != 0x47) || (pid < 0x20) || (pid == 0x1fff)) {
            continue;
        }

        const bool discontinuity = signalledDiscontinuity(p);

        // Adaptation only: a discontinuity it signals, for the next packet's counter (as for audio)
        if (!(p[3] & 0x10))
        {
            auto tracked = m_videoTracks.find(pid);

            if (discontinuity && (tracked != m_videoTracks.end()))
            {
                tracked->second.m_cc = -1;
                tracked->second.m_errorPackets = 0;
            }

            continue;
        }

        const bool error = (p[1] & 0x80) != 0;
        const int offset = (p[3] & 0x20) ? 5 + p[4] : 4;
        const int length = std::max(0, 188 - offset);
        const uint8_t *pes = p + offset;
        auto it = m_videoTracks.find(pid);

        if (it == m_videoTracks.end())
        {
            // A PID's tracked once a good PES start says it's video
            if (!(p[1] & 0x40) || error || (length < 4) || (pes[0] != 0) || (pes[1] != 0) || (pes[2] != 1)
                || ((pes[3] & 0xf0) != 0xe0)) {
                continue;
            }

            it = m_videoTracks.emplace(pid, VideoTrack()).first;
        }

        VideoTrack& track = it->second;

        // As for audio, allowing for packets with errors since the last good one
        const bool lost = continuityLost(p, discontinuity, track.m_cc, track.m_errorPackets);

        if (p[1] & 0x40)
        {
            // Packets lost before a new PES were the end of the last
            if (lost && !track.m_pes.empty()) {
                track.m_pes.back().m_damaged = true;
            }

            VideoPES next{-1, error, track.m_sequence++};

            if (!error && (length >= 14) && (pes[0] == 0) && (pes[1] == 0) && (pes[2] == 1) && (pes[7] & 0x80) && (9 + pes[8] <= length))
            {
                next.m_pts = pesPTS(pes + 9);
            }

            track.m_pes.push_back(next);

            if (track.m_pes.size() > 64) {
                track.m_pes.pop_front();
            }
        }
        else if ((error || lost) && !track.m_pes.empty())
        {
            track.m_pes.back().m_damaged = true;
        }
    }
}

// If a picture's PES (that with its PTS) was damaged. Its whole PES has been read by the time the demuxer returns it.
// A picture whose PTS isn't found, when a PES whose header had errors (so whose PTS isn't known) came after the last
// matched, is that one's: damaged (the parser's own flag would be on the next picture)
bool DTVVideoPlayer::videoFrameDamaged(const AVPacket *packet)
{
    auto track = m_videoTracks.find(m_videoPid);

    if (track == m_videoTracks.end()) {
        return false;
    }

    std::deque<VideoPES>& pes = track->second.m_pes;

    if (packet->pts != AV_NOPTS_VALUE)
    {
        const int64_t pts = packet->pts & ((1LL << 33) - 1);

        for (auto it = pes.rbegin(); it != pes.rend(); ++it)
        {
            if (it->m_pts == pts)
            {
                track->second.m_matched = std::max(track->second.m_matched, it->m_sequence + 1);
                return it->m_damaged;
            }
        }
    }

    for (VideoPES& p : pes)
    {
        if ((p.m_sequence >= track->second.m_matched) && (p.m_pts == -1) && p.m_damaged)
        {
            p.m_pts = -2;
            track->second.m_matched = p.m_sequence + 1;
            return true;
        }
    }

    return false;
}

// The TS as read, for what's parsed from it besides FFmpeg: whole packets
void DTVVideoPlayer::parseTS(const uint8_t *buf, int size)
{
    const uint8_t *data = buf;
    int length = size;

    if (!m_tsCarry.empty() || (size % 188 != 0))
    {
        m_tsAligned.assign(m_tsCarry.begin(), m_tsCarry.end());
        m_tsAligned.insert(m_tsAligned.end(), buf, buf + size);
        data = m_tsAligned.data();
        length = (int) m_tsAligned.size() / 188 * 188;
        m_tsCarry.assign(m_tsAligned.begin() + length, m_tsAligned.end());
    }

    parseADDescriptors(data, length);
    trackAudioPES(data, length);
    trackVideoPES(data, length);
    trackScrambling(data, length);
}

// Whether the service's video (or its audio, if it has none) is scrambled, from its packets' transport_scrambling_control:
// then it's encrypted (pay TV), can't be decoded, and the display says so, rather than that none is being received
void DTVVideoPlayer::trackScrambling(const uint8_t *buf, int size)
{
    const int pid = m_videoPid >= 0 ? m_videoPid : m_audioPid;

    if (pid < 0) {
        return;
    }

    int scrambled = 0, clear = 0;

    for (int i = 0; i + 188 <= size; i += 188)
    {
        const uint8_t *p = buf + i;

        if ((p[0] != 0x47) || (p[1] & 0x80) || ((((p[1] & 0x1f) << 8) | p[2]) != pid) || !(p[3] & 0x10)) {
            continue;
        }

        (p[3] & 0xc0 ? scrambled : clear)++;
    }

    if ((scrambled + clear > 0) && ((scrambled > clear) != m_scrambled))
    {
        m_scrambled = scrambled > clear;
        QMutexLocker locker(&m_infoMutex);
        m_info.m_scrambled = m_scrambled;
    }
}

uint32_t DTVVideoPlayer::writeAudio(const int16_t *samples, int nbSamples)
{
    // The oldest dropped rather than the newest lost: after an open, the audio FFmpeg read probing comes all at once,
    // and DVB-T2's in a frame's burst. Writing past the FIFO's end lost the newest, mid frame, and left the clock
    // ahead by them. Kept to 3/4 full, so a burst on top fits
    const uint32_t target = m_audioFifo->size() * 3 / 4;
    const uint32_t fill = m_audioFifo->fill();
    const auto now = std::chrono::steady_clock::now();

    if (fill + (uint32_t) nbSamples > target)
    {
        m_audioFifo->drain(std::min(fill, fill + (uint32_t) nbSamples - target));
        QMutexLocker locker(&m_infoMutex);
        m_info.m_audioOverflows++;
    }
    else if ((fill == 0) && (m_lastAudioWrite.time_since_epoch().count() != 0) && (now - m_lastAudioWrite < seconds(m_audioClockStale)))
    {
        // Empty, though audio was written moments ago: the device ran out, and played silence
        QMutexLocker locker(&m_infoMutex);
        m_info.m_audioUnderruns++;
    }

    m_lastAudioWrite = now;

    return m_audioFifo->write((const quint8 *) samples, nbSamples);
}

// If an audio frame was in a damaged part of its PES, or of the next where it ran on into that (frames needn't be
// aligned to PES), or ran on past a PES header with errors. If where it was can't be told, as when its PES header had
// errors, it was damaged if the demuxer flagged it
bool DTVVideoPlayer::audioFrameDamaged(const AVPacket *packet, bool flagged)
{
    if (packet->pts == AV_NOPTS_VALUE) {
        return flagged;
    }

    const int64_t mask = (1LL << 33) - 1;
    const int64_t pts = packet->pts & mask;
    int index = -1;
    int64_t pesAge = 0;

    // The latest PES starting at or before the frame (within a second): the one it starts in, its PTS being that of
    // the first frame starting in it
    for (int i = 0; i < (int) m_audioPES.size(); i++)
    {
        const AudioPES& a = m_audioPES[i];

        if (a.m_pts < 0) {
            continue;
        }

        const int64_t age = (pts - a.m_pts) & mask;

        if ((age < 90000) && ((index < 0) || (age < pesAge)))
        {
            index = i;
            pesAge = age;
        }
    }

    if (index < 0) {
        return flagged;
    }

    const AudioPES& pes = m_audioPES[index];

    if (pes.m_pts != m_framePESPts)
    {
        // The first frame starting in it starts where the last frame checked ended, if that ran on into it from the
        // PES before
        const AudioPES *previous = index > 0 ? &m_audioPES[index - 1] : nullptr;
        const bool follows = previous && (previous->m_pts >= 0) && (previous->m_pts == m_framePESPts) && (previous->m_length >= 0);
        m_framePESOffset = follows ? std::max(0, m_framePESOffset - previous->m_length) : 0;
        m_framePESPts = pes.m_pts;
    }

    const int first = m_framePESOffset;
    const int last = first + packet->size;
    m_framePESOffset = last;

    // A frame after the damage may start with some of it, where the parser found a false frame header there
    const int extra = m_audioFrameSize > 0 ? m_audioFrameSize : 1536;
    auto damaged = [extra](const AudioPES& a, int from, int to) {
        for (const auto& range : a.m_damaged)
        {
            if ((range.first < to) && ((range.second == INT_MAX) || (range.second + extra > from))) {
                return true;
            }
        }

        return false;
    };

    if (damaged(pes, first, last)) {
        return true;
    }

    // Run on into the next PES: damaged where that is, or where its header had errors (so where it is in it can't
    // be told), or ran on past it too
    if ((pes.m_length >= 0) && (last > pes.m_length))
    {
        if (index + 1 >= (int) m_audioPES.size()) {
            return true;
        }

        const AudioPES& next = m_audioPES[index + 1];
        const int nextFirst = std::max(0, first - pes.m_length);
        const int nextLast = last - pes.m_length;

        if ((next.m_pts < 0) || ((next.m_length >= 0) && (nextLast > next.m_length))) {
            return true;
        }

        return damaged(next, nextFirst, nextLast);
    }

    return false;
}

// The fade of the main audio and pan of the description, from the PES headers of an MPEG audio description: the
// AD_descriptor in the PES private data (ETSI TS 101 154 E.1). Read from the TS, as FFmpeg skips PES private data,
// for any PID that has them
void DTVVideoPlayer::parseADDescriptors(const uint8_t *buf, int size)
{
    for (int i = 0; i + 188 <= size; i += 188)
    {
        const uint8_t *p = buf + i;

        if ((p[0] != 0x47) || !(p[1] & 0x40) || !(p[3] & 0x10)) {
            continue; // Not the start of a PES packet
        }

        const int offset = (p[3] & 0x20) ? 5 + p[4] : 4;
        const uint8_t *pes = p + offset;
        const int length = 188 - offset;

        if ((length < 9) || (pes[0] != 0) || (pes[1] != 0) || (pes[2] != 1)) {
            continue;
        }

        const int flags = pes[7];
        const int headerLength = pes[8];
        const uint8_t *h = pes + 9;

        if (!(flags & 0x80) || !(flags & 0x01) || (9 + headerLength > length)) {
            continue; // No PTS, or no extension
        }

        const int64_t pts = pesPTS(h);
        int k = (flags & 0x40) ? 10 : 5;    // PTS and DTS
        k += (flags & 0x20) ? 6 : 0;        // ESCR
        k += (flags & 0x10) ? 3 : 0;        // ES rate
        k += (flags & 0x08) ? 1 : 0;        // Trick mode
        k += (flags & 0x04) ? 1 : 0;        // Additional copy info
        k += (flags & 0x02) ? 2 : 0;        // CRC

        if ((k + 17 > headerLength) || !(h[k] & 0x80)) {
            continue; // No private data
        }

        const uint8_t *descriptor = h + k + 1; // Length, "DTGAD", revision, fade, pan

        if (std::memcmp(descriptor + 1, "DTGAD", 5) != 0) {
            continue;
        }

        std::deque<ADControl>& controls = m_adControls[((p[1] & 0x1f) << 8) | p[2]];
        controls.push_back(ADControl{pts / 90000.0, descriptor[7], descriptor[8]});

        if (controls.size() > 100) {
            controls.pop_front();
        }
    }
}

// All the bitmaps of a page in one image, on the decoder's display (720x576 unless a display definition says)
GLVideoSubtitlePtr DTVVideoPlayer::makeSubtitle(const AVSubtitle& sub, int canvasWidth, int canvasHeight)
{
    int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;

    for (unsigned int i = 0; i < sub.num_rects; i++)
    {
        const AVSubtitleRect *rect = sub.rects[i];

        if ((rect->type == SUBTITLE_BITMAP) && (rect->w > 0) && (rect->h > 0))
        {
            x0 = std::min(x0, rect->x);
            y0 = std::min(y0, rect->y);
            x1 = std::max(x1, rect->x + rect->w);
            y1 = std::max(y1, rect->y + rect->h);
        }
    }

    if (x0 >= x1) {
        return nullptr;
    }

    auto subtitle = std::make_shared<GLVideoSubtitle>();
    subtitle->m_x = x0;
    subtitle->m_y = y0;
    subtitle->m_width = x1 - x0;
    subtitle->m_height = y1 - y0;
    subtitle->m_canvasWidth = canvasWidth > 0 ? canvasWidth : 720;
    subtitle->m_canvasHeight = canvasHeight > 0 ? canvasHeight : 576;
    subtitle->m_rgba.assign((size_t) subtitle->m_width * subtitle->m_height * 4, 0);

    for (unsigned int i = 0; i < sub.num_rects; i++)
    {
        const AVSubtitleRect *rect = sub.rects[i];

        if ((rect->type != SUBTITLE_BITMAP) || (rect->w <= 0) || (rect->h <= 0)) {
            continue;
        }

        const uint32_t *palette = (const uint32_t *) rect->data[1]; // ARGB

        for (int y = 0; y < rect->h; y++)
        {
            const uint8_t *in = rect->data[0] + y * rect->linesize[0];
            uint8_t *out = subtitle->m_rgba.data() + (((size_t) (rect->y - y0 + y) * subtitle->m_width) + (rect->x - x0)) * 4;

            for (int x = 0; x < rect->w; x++)
            {
                const uint32_t c = palette[in[x]];
                const uint32_t a = c >> 24;
                out[4 * x] = (uint8_t) (((c >> 16) & 0xff) * a / 255); // Premultiplied
                out[4 * x + 1] = (uint8_t) (((c >> 8) & 0xff) * a / 255);
                out[4 * x + 2] = (uint8_t) ((c & 0xff) * a / 255);
                out[4 * x + 3] = (uint8_t) a;
            }
        }
    }

    return subtitle;
}

// A page replaces the last, until its time out. An empty page clears the screen
void DTVVideoPlayer::decodeSubtitle()
{
    AVSubtitle sub;
    int got = 0;

    if ((avcodec_decode_subtitle2(m_subtitleCtx, &sub, &got, m_packet) < 0) || !got) {
        return;
    }

    if (sub.pts != AV_NOPTS_VALUE)
    {
        const double pts = sub.pts / (double) AV_TIME_BASE;
        SubtitlePage page;
        page.m_start = pts + sub.start_display_time / 1000.0;
        page.m_end = (sub.end_display_time == 0) || (sub.end_display_time == UINT32_MAX) ? INFINITY : pts + sub.end_display_time / 1000.0;
        page.m_subtitle = makeSubtitle(sub, m_subtitleCtx->width, m_subtitleCtx->height);
        std::lock_guard<std::mutex> lock(m_subtitleMutex);
        m_subtitlePages.push_back(page);

        if (m_subtitlePages.size() > 100) {
            m_subtitlePages.pop_front();
        }
    }

    avsubtitle_free(&sub);
    QMutexLocker locker(&m_infoMutex);
    m_info.m_subtitlePages++;
}

// FFmpeg's CEA-608 decoder, in real time mode (each change to the screen given as it's made, as roll-up captions are
// shown): a page shown from the frame's time until the next. Only CEA-608 (in CEA-708's cc_data, as US broadcasters
// send both): CEA-708's own services aren't decoded
void DTVVideoPlayer::decodeCaptions(const AVFrameSideData *captions, int64_t timestamp)
{
    if (!m_captionCtx)
    {
        const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_EIA_608);

        if (!codec) {
            return;
        }

        m_captionCtx = avcodec_alloc_context3(codec);
        m_captionCtx->pkt_timebase = AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen};
        AVDictionary *options = nullptr;
        av_dict_set(&options, "real_time", "1", 0);
        const int opened = avcodec_open2(m_captionCtx, codec, &options);
        av_dict_free(&options);

        if (opened < 0)
        {
            qWarning("DTVVideoPlayer::decodeCaptions: FFmpeg's CEA-608 decoder couldn't be opened");
            avcodec_free_context(&m_captionCtx);
            m_captionsEnabled = false;
            return;
        }
    }

    AVPacket *packet = av_packet_alloc();

    if (!packet || (av_new_packet(packet, (int) captions->size) < 0))
    {
        av_packet_free(&packet);
        return;
    }

    memcpy(packet->data, captions->data, captions->size);
    packet->pts = timestamp;
    AVSubtitle sub;
    int got = 0;

    if ((avcodec_decode_subtitle2(m_captionCtx, &sub, &got, packet) >= 0) && got)
    {
        QString text;

        for (unsigned int i = 0; i < sub.num_rects; i++)
        {
            if ((sub.rects[i]->type == SUBTITLE_ASS) && sub.rects[i]->ass) {
                text += assText(sub.rects[i]->ass);
            }
        }

        SubtitlePage page;
        page.m_start = timestamp * av_q2d(AVRational{m_videoTimeBaseNum, m_videoTimeBaseDen});
        page.m_end = INFINITY;
        page.m_subtitle = text.trimmed().isEmpty() ? nullptr : makeCaptions(text);

        {
            std::lock_guard<std::mutex> lock(m_subtitleMutex);
            m_subtitlePages.push_back(page);

            if (m_subtitlePages.size() > 100) {
                m_subtitlePages.pop_front();
            }
        }

        avsubtitle_free(&sub);
        QMutexLocker locker(&m_infoMutex);
        m_info.m_subtitlePages++;
    }

    av_packet_free(&packet);
}

void DTVVideoPlayer::closeCaptions()
{
    avcodec_free_context(&m_captionCtx);
}

// "ReadOrder,Layer,Style,Name,MarginL,MarginR,MarginV,Effect,Text": the text, without override codes ({...}), its
// line breaks (\N) as newlines and hard spaces (\h) as spaces
QString DTVVideoPlayer::assText(const char *ass)
{
    QString event = QString::fromUtf8(ass);
    int commas = 0, start = 0;

    for (int i = 0; (i < event.size()) && (commas < 8); i++)
    {
        if (event[i] == ',')
        {
            commas++;
            start = i + 1;
        }
    }

    QString text = commas == 8 ? event.mid(start) : event;
    QString out;
    bool code = false;

    for (int i = 0; i < text.size(); i++)
    {
        const QChar c = text[i];

        if (c == '{') {
            code = true;
        } else if (c == '}') {
            code = false;
        } else if (code) {
            continue;
        } else if ((c == '\\') && (i + 1 < text.size()) && ((text[i + 1] == 'N') || (text[i + 1] == 'n'))) {
            out += '\n';
            i++;
        } else if ((c == '\\') && (i + 1 < text.size()) && (text[i + 1] == 'h')) {
            out += ' ';
            i++;
        } else {
            out += c;
        }
    }

    return out;
}

// As TVs show CEA-608: white monospaced text on black boxes, the lines left aligned in a block centred at the bottom
// of a 4:3 safe area (the decoder's rows and columns aren't kept, so roll-up and pop-on captions are both there)
GLVideoSubtitlePtr DTVVideoPlayer::makeCaptions(const QString& text)
{
    static constexpr int canvasWidth = 640;
    static constexpr int canvasHeight = 480;
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (QString& line : lines) {
        line = line.trimmed();
    }

    lines.removeAll(QString());

    if (lines.isEmpty()) {
        return nullptr;
    }

    // Fonts need a GUI application: without one (as in a test), just the boxes, at a monospaced font's size
    const bool fonts = qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr;
    QFont font("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(22);
    auto lineWidth = [&](const QString& line) {
        return (fonts ? QFontMetrics(font).horizontalAdvance(line) : 12 * (int) line.size()) + 16;
    };
    const int lineHeight = (fonts ? QFontMetrics(font).height() : 26) + 4;
    int width = 0;

    for (const QString& line : lines) {
        width = std::max(width, lineWidth(line));
    }

    width = std::min(width, canvasWidth);
    const int height = lineHeight * (int) lines.size();
    QImage image(width, height, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    if (fonts) {
        painter.setFont(font);
    }

    for (int i = 0; i < (int) lines.size(); i++)
    {
        const int w = std::min(width, lineWidth(lines[i]));
        const QRect box(0, i * lineHeight, w, lineHeight);   // Left aligned, as the rows of a caption are
        painter.fillRect(box, QColor(0, 0, 0, 220));

        if (fonts)
        {
            painter.setPen(Qt::white);
            painter.drawText(box, Qt::AlignCenter, lines[i]);
        }
    }

    painter.end();
    auto subtitle = std::make_shared<GLVideoSubtitle>();
    subtitle->m_canvasWidth = canvasWidth;
    subtitle->m_canvasHeight = canvasHeight;
    subtitle->m_width = width;
    subtitle->m_height = height;
    subtitle->m_x = (canvasWidth - width) / 2;
    subtitle->m_y = canvasHeight - 32 - height;
    subtitle->m_rgba.resize((size_t) width * height * 4);

    for (int y = 0; y < height; y++) {
        memcpy(subtitle->m_rgba.data() + (size_t) y * width * 4, image.constScanLine(y), (size_t) width * 4);
    }

    return subtitle;
}

// The subtitle due with a video frame (pts seconds), in the video thread
GLVideoSubtitlePtr DTVVideoPlayer::currentSubtitle(double pts)
{
    std::lock_guard<std::mutex> lock(m_subtitleMutex);

    while (!m_subtitlePages.empty())
    {
        const SubtitlePage& page = m_subtitlePages.front();

        if ((page.m_start > pts) && (page.m_start < pts + m_subtitleWindow)) {
            break; // Not due yet
        }

        if (page.m_start <= pts) {
            m_subtitleShown = page;
        } // Else far ahead, after the timestamps jumped

        m_subtitlePages.pop_front();
    }

    if (m_subtitleShown.m_subtitle && ((pts >= m_subtitleShown.m_end) || (pts < m_subtitleShown.m_start - m_subtitleWindow))) {
        m_subtitleShown.m_subtitle.reset();
    }

    return m_subtitleShown.m_subtitle;
}

void DTVVideoPlayer::closeTrackDecoders()
{
    avcodec_free_context(&m_audioCtx);
    avcodec_free_context(&m_adCtx);
    avcodec_free_context(&m_subtitleCtx);
    swr_free(&m_swrCtx);
    swr_free(&m_adSwrCtx);
    m_audioStream = -1;
    m_adStream = -1;
    m_subtitleStream = -1;
    m_captionsEnabled = false;
    m_adPid = -1;
    m_adSamples.clear();
    m_adControl = ADControl{0.0, 0, 0};
    m_fadeGain = 1.0f;
    m_muteGain = 1.0f;
    m_audioNextPts = NAN;
    m_audioPid = -1;
    m_audioPES.clear();
    m_audioCC = -1;
    m_audioErrorPackets = 0;
    m_framePESPts = -1;
    m_audioDamagedLast = false;
    m_audioPeak = 32767;

    {
        std::lock_guard<std::mutex> lock(m_subtitleMutex);
        m_subtitlePages.clear();
        m_subtitleShown = SubtitlePage{0.0, 0.0, nullptr};
    }

    QMutexLocker locker(&m_infoMutex);
    m_info.m_audioCodec.clear();
    m_info.m_audioDescription = false;
    m_info.m_subtitles = false;
    m_info.m_adFade = 0;
}

// The track a stream is, if it's audio or subtitles that can be shown
bool DTVVideoPlayer::getTrack(const AVStream *stream, DTVVideoPlayer::Track& track)
{
    const int disposition = stream->disposition;
    const AVMediaType type = stream->codecpar->codec_type;
    const AVDictionaryEntry *language = av_dict_get(stream->metadata, "language", nullptr, 0);
    track.m_pid = stream->id;
    track.m_language = language ? QString(language->value) : QString();

    if (type == AVMEDIA_TYPE_SUBTITLE)
    {
        if (stream->codecpar->codec_id != AV_CODEC_ID_DVB_SUBTITLE) {
            return false; // Teletext, which isn't used for subtitles in the UK
        }

        track.m_type = (disposition & AV_DISPOSITION_HEARING_IMPAIRED) ? DTVVideoPlayer::Track::SubtitlesHardOfHearing
            : DTVVideoPlayer::Track::Subtitles;
        return true;
    }

    // Audio descriptions' codecs can be unknown, until FFmpeg has seen their packets. Sky marks its (mixed in) only by
    // the language, NAR (narrative), with audio_type 0, and the main audio's AC-3 on its HD services, so NAR was played
    // as the first audio. Likewise qad, ISO 639's code for audio description
    const bool narrative = (track.m_language.compare("nar", Qt::CaseInsensitive) == 0) || (track.m_language.compare("qad", Qt::CaseInsensitive) == 0);
    const bool description = narrative || ((disposition & (AV_DISPOSITION_VISUAL_IMPAIRED | AV_DISPOSITION_DESCRIPTIONS)) != 0);

    if ((type != AVMEDIA_TYPE_AUDIO) && !(description && (type == AVMEDIA_TYPE_UNKNOWN))) {
        return false;
    }

    if (description) {
        track.m_type = (disposition & AV_DISPOSITION_DEPENDENT) ? DTVVideoPlayer::Track::AudioDescription
            : DTVVideoPlayer::Track::AudioDescriptionMixed;
    } else if (disposition & AV_DISPOSITION_HEARING_IMPAIRED) {
        track.m_type = DTVVideoPlayer::Track::CleanAudio;
    } else {
        track.m_type = DTVVideoPlayer::Track::Main;
    }

    return true;
}

// The first track of one of the types, in the language if there is one
int DTVVideoPlayer::findTrack(const QList<DTVVideoPlayer::Track>& tracks, std::initializer_list<DTVVideoPlayer::Track::Type> types,
    const QString& language)
{
    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < tracks.size(); i++)
        {
            if ((std::find(types.begin(), types.end(), tracks[i].m_type) != types.end())
                && ((pass == 1) || language.isEmpty() || (tracks[i].m_language == language))) {
                return i;
            }
        }
    }

    return -1;
}

// The first track of the type in the language (any language if it's empty), -1 if none
int DTVVideoPlayer::findTrackInLanguage(const QList<DTVVideoPlayer::Track>& tracks, DTVVideoPlayer::Track::Type type,
    const QString& language)
{
    for (int i = 0; i < tracks.size(); i++)
    {
        if ((tracks[i].m_type == type) && (language.isEmpty() || (tracks[i].m_language == language))) {
            return i;
        }
    }

    return -1;
}

// The main audio in the language, else the first. Clean audio in its language, if there is some (by language first:
// another service may not have it). With audio description, one to mix in, or else audio with it mixed (the other way
// round, if the broadcaster's mix is preferred). Subtitles of the type in the language, else the other type's, else
// any of the language's, else the first
void DTVVideoPlayer::chooseTracks(const Tracks& tracks, const QString& audioLanguage, Track::Type audioType, bool subtitles,
    const QString& subtitleLanguage, Track::Type subtitleType, int& audio, int& description, int& subtitle)
{
    audio = findTrack(tracks.m_audio, {Track::Main}, audioLanguage);

    if (audio < 0) {
        audio = findTrack(tracks.m_audio, {Track::CleanAudio, Track::AudioDescriptionMixed}, audioLanguage);
    }

    const QString language = audio >= 0 ? tracks.m_audio[audio].m_language : audioLanguage;
    description = -1;

    if (audioType == Track::CleanAudio)
    {
        const int clean = findTrackInLanguage(tracks.m_audio, Track::CleanAudio, language);
        audio = clean >= 0 ? clean : audio;
    }
    else if ((audioType == Track::AudioDescription) || (audioType == Track::AudioDescriptionMixed))
    {
        const int toMix = audio >= 0 ? findTrackInLanguage(tracks.m_audio, Track::AudioDescription, language) : -1;
        const int mixed = findTrackInLanguage(tracks.m_audio, Track::AudioDescriptionMixed, language);

        if ((audioType == Track::AudioDescriptionMixed) && (mixed >= 0)) {
            audio = mixed;
        } else if (toMix >= 0) {
            description = toMix;
        } else if (mixed >= 0) {
            audio = mixed;
        }
    }

    subtitle = -1;

    if (subtitles)
    {
        const Track::Type other = subtitleType == Track::SubtitlesHardOfHearing ? Track::Subtitles : Track::SubtitlesHardOfHearing;
        subtitle = findTrackInLanguage(tracks.m_subtitles, subtitleType, subtitleLanguage);
        subtitle = subtitle >= 0 ? subtitle : findTrackInLanguage(tracks.m_subtitles, other, subtitleLanguage);
        subtitle = subtitle >= 0 ? subtitle : findTrack(tracks.m_subtitles, {subtitleType, other}, subtitleLanguage);
    }
}

// Choose the service's audio, audio description and subtitles, as preferred, and open their decoders
void DTVVideoPlayer::selectTracks()
{
    closeTrackDecoders();
    QString audioLanguage, subtitleLanguage;
    Track::Type audioType, subtitleType;
    bool subtitles;

    {
        QMutexLocker locker(&m_tracksMutex);
        audioLanguage = m_audioLanguage;
        audioType = m_audioType;
        subtitles = m_subtitles;
        subtitleLanguage = m_subtitleLanguage;
        subtitleType = m_subtitleType;
    }

    Tracks tracks;
    std::vector<int> audioStreams, subtitleStreams;

    if (m_programIndex >= 0)
    {
        const AVProgram *program = m_formatCtx->programs[m_programIndex];

        for (unsigned int j = 0; j < program->nb_stream_indexes; j++)
        {
            const int index = program->stream_index[j];
            Track track;

            if (getTrack(m_formatCtx->streams[index], track))
            {
                const bool subtitle = (track.m_type == Track::Subtitles) || (track.m_type == Track::SubtitlesHardOfHearing);
                (subtitle ? tracks.m_subtitles : tracks.m_audio).append(track);
                (subtitle ? subtitleStreams : audioStreams).push_back(index);
            }
        }
    }

    // Closed captions in the video, if they've been seen: a track without a stream
    m_captionsListed = m_captionsSeen;

    if (m_captionsListed)
    {
        tracks.m_subtitles.append(Track{m_captionPid, QString(), Track::Subtitles});
        subtitleStreams.push_back(-1);
    }

    int audio, description, subtitle;
    chooseTracks(tracks, audioLanguage, audioType, subtitles, subtitleLanguage, subtitleType, audio, description, subtitle);

    if (audio >= 0)
    {
        m_audioStream = audioStreams[audio];
        m_audioPid = tracks.m_audio[audio].m_pid;
        m_audioCtx = openDecoder(m_formatCtx->streams[m_audioStream]);
        setupResampler();
        tracks.m_audioPid = tracks.m_audio[audio].m_pid;
    }

    if (description >= 0)
    {
        m_adStream = audioStreams[description];
        m_adPid = tracks.m_audio[description].m_pid;
        tracks.m_adPid = m_adPid;
    }

    if ((subtitle >= 0) && (subtitleStreams[subtitle] < 0))
    {
        m_captionsEnabled = true;
        tracks.m_subtitlePid = m_captionPid;
    }
    else if (subtitle >= 0)
    {
        m_subtitleStream = subtitleStreams[subtitle];
        m_subtitleCtx = openDecoder(m_formatCtx->streams[m_subtitleStream]);
        tracks.m_subtitlePid = tracks.m_subtitles[subtitle].m_pid;
    }

    qDebug() << "DTVVideoPlayer::selectTracks:" << tracks.m_audio.size() << "audio tracks," << tracks.m_subtitles.size()
        << "subtitle tracks. Playing audio" << tracks.m_audioPid << "description" << tracks.m_adPid << "subtitles" << tracks.m_subtitlePid;

    {
        QMutexLocker locker(&m_infoMutex);
        m_info.m_audioCodec = m_audioCtx ? QString(m_audioCtx->codec->long_name) : QString();
        m_info.m_audioDescription = m_adStream >= 0;
        m_info.m_subtitles = (m_subtitleCtx != nullptr) || m_captionsEnabled;
    }

    m_tracks = tracks;
    emit tracksChanged(tracks);
}

// Captions found in the video after the tracks were chosen: listed, and played if preferred (and no subtitles are)
void DTVVideoPlayer::addCaptionTrack()
{
    m_captionsListed = true;
    m_tracks.m_subtitles.append(Track{m_captionPid, QString(), Track::Subtitles});
    QString audioLanguage, subtitleLanguage;
    Track::Type audioType, subtitleType;
    bool subtitles;

    {
        QMutexLocker locker(&m_tracksMutex);
        audioLanguage = m_audioLanguage;
        audioType = m_audioType;
        subtitles = m_subtitles;
        subtitleLanguage = m_subtitleLanguage;
        subtitleType = m_subtitleType;
    }

    int audio, description, subtitle;
    chooseTracks(m_tracks, audioLanguage, audioType, subtitles, subtitleLanguage, subtitleType, audio, description, subtitle);

    if ((subtitle >= 0) && (m_tracks.m_subtitles[subtitle].m_pid == m_captionPid) && !m_subtitleCtx)
    {
        m_captionsEnabled = true;
        m_tracks.m_subtitlePid = m_captionPid;
        QMutexLocker locker(&m_infoMutex);
        m_info.m_subtitles = true;
    }

    emit tracksChanged(m_tracks);
}
