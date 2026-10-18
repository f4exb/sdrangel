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
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <limits>
#include <mutex>
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

#include <QElapsedTimer>

#include "dtv/dtvtsfifo.h"
#include "dtv/dtvstreamremuxer.h"

namespace {

constexpr int m_fifoHigh = 3 * 1024 * 1024;     //!< Bytes in the FIFO above which writing waits
// Seconds ahead of real time packets are written: the player decodes what it's given straight away (as a broadcast
// comes in real time), and keeps its audio FIFO (a second) to 3/4 full by dropping the oldest, so a stream given faster
// than it's played sounds too fast, and the video, timed by the audio, runs ahead. Enough for the decoders' delays
constexpr double m_lead = 0.3;
constexpr int m_ioBufferSize = 188 * 512;
// Read ahead of what's written, by another thread: reading waits while a segment's fetched (as FFmpeg's DASH and HLS
// demuxers do), which would otherwise hold up what's due, as the player plays it (video late, then audio dropped as
// what was held up comes all at once)
constexpr double m_readAhead = 10.0;                    //!< Seconds of the stream read ahead, at most
constexpr size_t m_readAheadBytes = 64 * 1024 * 1024;   //!< And bytes
// What's written late by more than this (the stream's fetched more slowly than it's played, for longer than what's
// read ahead) is played on from then, rather than what's held up written all at once
constexpr double m_resync = 1.0;

QString errorText(int error)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(error, text, sizeof(text));
    return QString::fromUtf8(text);
}

// The URL an HTTP one's redirected to, as FFmpeg's asked for it (its options given): else as it is. A DASH manifest's
// segments are found relative to where it is, which FFmpeg's demuxer takes to be the URL it's given, not where that
// redirects to: as TVP's, a token's URL redirected to a cache, whose segments from the first host fail (412)
QString redirected(const QString& url, const AVDictionary *options, AVIOInterruptCB *interrupt)
{
    if (!url.startsWith("http:", Qt::CaseInsensitive) && !url.startsWith("https:", Qt::CaseInsensitive)) {
        return url;
    }

    AVDictionary *copy = nullptr;
    av_dict_copy(&copy, options, 0);
    AVIOContext *io = nullptr;
    QString result = url;

    if (avio_open2(&io, url.toUtf8().constData(), AVIO_FLAG_READ, interrupt, &copy) >= 0)
    {
        uint8_t *location = nullptr;

        if ((av_opt_get(io, "location", AV_OPT_SEARCH_CHILDREN, &location) >= 0) && location && *location) {
            result = QString::fromUtf8((const char *) location);
        }

        av_free(location);
        avio_closep(&io);
    }

    av_dict_free(&copy);
    return result;
}

} // namespace

DTVStreamRemuxer::DTVStreamRemuxer(DTVTSFifo *fifo, const QString& url, const QMap<QString, QString>& options, double start, QObject *parent) :
    QThread(parent),
    m_fifo(fifo),
    m_url(url),
    m_options(options),
    m_start(start),
    m_stop(false),
    m_duration(std::nan("")),
    m_offset(std::nan("")),
    m_finished(false)
{
}

DTVStreamRemuxer::~DTVStreamRemuxer()
{
    stop();
}

void DTVStreamRemuxer::stop()
{
    m_stop = true;
    wait();
}

int DTVStreamRemuxer::interrupt(void *opaque)
{
    return static_cast<DTVStreamRemuxer *>(opaque)->m_stop ? 1 : 0;
}

// The muxer's output, into the FIFO: as fast as it's read from it
int DTVStreamRemuxer::writePacket(void *opaque, const uint8_t *buf, int size)
{
    DTVStreamRemuxer *remuxer = static_cast<DTVStreamRemuxer *>(opaque);

    while (!remuxer->m_stop && (remuxer->m_fifo->fill() > m_fifoHigh)) {
        msleep(20);
    }

    if (remuxer->m_stop) {
        return AVERROR_EXIT;
    }

    remuxer->m_fifo->write(buf, size);
    return size;
}

void DTVStreamRemuxer::run()
{
    AVFormatContext *in = avformat_alloc_context();
    AVFormatContext *out = nullptr;
    AVPacket *packet = av_packet_alloc();
    in->interrupt_callback.callback = &DTVStreamRemuxer::interrupt;
    in->interrupt_callback.opaque = this;

    AVDictionary *options = nullptr;

    for (auto it = m_options.begin(); it != m_options.end(); ++it) {
        av_dict_set(&options, it.key().toUtf8().constData(), it.value().toUtf8().constData(), 0);
    }

    av_dict_set(&options, "reconnect", "1", 0);
    const QString url = m_resolveRedirects ? redirected(m_url, options, &in->interrupt_callback) : m_url;

    if (url != m_url)
    {
        qDebug("DTVStreamRemuxer::run: %s redirected to %s", qPrintable(m_url.left(80)), qPrintable(url.left(120)));
        emit resolved(url);
    }

    int ret = avformat_open_input(&in, url.toUtf8().constData(), nullptr, &options);
    av_dict_free(&options);

    if (ret < 0)
    {
        if (!m_stop) {
            emit failed(QString("Can't open %1: %2").arg(m_url).arg(errorText(ret)));
        }

        av_packet_free(&packet);
        return;
    }

    ret = avformat_find_stream_info(in, nullptr);

    if ((ret < 0) && !m_stop) {
        qDebug("DTVStreamRemuxer::run: avformat_find_stream_info: %s", qPrintable(errorText(ret)));
    }

    // The best video (the largest picture, up to 1080 lines, and of those the highest bit rate) and audio (the
    // highest bit rate): the others discarded, so their segments aren't fetched
    int video = -1, audio = -1;

    for (unsigned int i = 0; i < in->nb_streams; i++)
    {
        const AVCodecParameters *p = in->streams[i]->codecpar;

        if (p->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            const AVCodecParameters *best = video >= 0 ? in->streams[video]->codecpar : nullptr;
            const int64_t pixels = (int64_t) p->width * p->height;
            const int64_t bestPixels = best ? (int64_t) best->width * best->height : -1;

            if ((p->height <= 1088) && (!best || (pixels > bestPixels) || ((pixels == bestPixels) && (p->bit_rate > best->bit_rate)))) {
                video = (int) i;
            }
        }
        else if (p->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if ((audio < 0) || (p->bit_rate > in->streams[audio]->codecpar->bit_rate)) {
                audio = (int) i;
            }
        }
    }

    for (unsigned int i = 0; i < in->nb_streams; i++) {
        in->streams[i]->discard = ((int) i == video) || ((int) i == audio) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    }

    if ((video < 0) && (audio < 0))
    {
        emit failed(QString("%1 has no video or audio").arg(m_url));
        avformat_close_input(&in);
        av_packet_free(&packet);
        return;
    }

    // Live streams have no duration
    const double duration = in->duration != AV_NOPTS_VALUE ? in->duration / (double) AV_TIME_BASE : std::numeric_limits<double>::infinity();
    m_duration = duration;
    emit opened(duration, video >= 0 ? in->streams[video]->codecpar->width : 0, video >= 0 ? in->streams[video]->codecpar->height : 0);

    if (m_start > 0.0)
    {
        const int64_t target = (int64_t) (m_start * AV_TIME_BASE) + (in->start_time != AV_NOPTS_VALUE ? in->start_time : 0);
        ret = avformat_seek_file(in, -1, INT64_MIN, target, target, 0);

        if (ret < 0) {
            qDebug("DTVStreamRemuxer::run: seek to %.1f s: %s", m_start, qPrintable(errorText(ret)));
        }
    }

    // A transport stream, as a broadcast's: its streams' timestamps from m_timestampStart
    avformat_alloc_output_context2(&out, nullptr, "mpegts", nullptr);
    int outIndex[64];
    std::fill(std::begin(outIndex), std::end(outIndex), -1);

    for (int s : {video, audio})
    {
        if ((s < 0) || (s >= 64)) {
            continue;
        }

        AVStream *os = avformat_new_stream(out, nullptr);
        avcodec_parameters_copy(os->codecpar, in->streams[s]->codecpar);
        os->codecpar->codec_tag = 0;
        os->time_base = AVRational{1, 90000};
        outIndex[s] = os->index;
    }

    uint8_t *ioBuffer = (uint8_t *) av_malloc(m_ioBufferSize);
    // FFmpeg 7 (libavformat 61) made the buffer const
#if LIBAVFORMAT_VERSION_MAJOR >= 61
    int (*write)(void *, const uint8_t *, int) = &DTVStreamRemuxer::writePacket;
#else
    int (*write)(void *, uint8_t *, int) = [](void *opaque, uint8_t *buf, int size) { return writePacket(opaque, buf, size); };
#endif
    out->pb = avio_alloc_context(ioBuffer, m_ioBufferSize, 1, this, nullptr, write, nullptr);
    out->flags |= AVFMT_FLAG_CUSTOM_IO;
    AVDictionary *muxOptions = nullptr;
    av_dict_set(&muxOptions, "mpegts_flags", "resend_headers", 0);
    ret = avformat_write_header(out, &muxOptions);
    av_dict_free(&muxOptions);

    if (ret < 0)
    {
        emit failed(QString("Can't remux %1: %2").arg(m_url).arg(errorText(ret)));
    }
    else
    {
        // Packets read ahead, with their times (seconds, in the stream), and how much
        std::mutex mutex;
        std::condition_variable changed;
        std::deque<std::pair<AVPacket *, double>> queue;
        size_t queuedBytes = 0;
        double lastWritten = -1e9;          // Time of the last packet written
        bool readDone = false;              // The end of the stream (or an error) read
        bool endOfStream = false;           // The end of the stream

        std::thread reader([&]() {
            AVPacket *read = av_packet_alloc();

            while (!m_stop)
            {
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    changed.wait(lock, [&]() {
                        return m_stop || queue.empty()
                            || ((queuedBytes < m_readAheadBytes) && (queue.back().second - lastWritten < m_readAhead));
                    });
                }

                if (m_stop) {
                    break;
                }

                const int result = av_read_frame(in, read);

                if (result < 0)
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    endOfStream = result != AVERROR_EXIT;
                    break;
                }

                const int s = read->stream_index;

                if ((s < 0) || (s >= 64) || (outIndex[s] < 0) || ((read->dts == AV_NOPTS_VALUE) && (read->pts == AV_NOPTS_VALUE)))
                {
                    av_packet_unref(read);
                    continue;
                }

                const int64_t first = read->dts != AV_NOPTS_VALUE ? read->dts : read->pts;
                const double time = av_rescale_q(first, in->streams[s]->time_base, AVRational{1, AV_TIME_BASE}) / (double) AV_TIME_BASE;
                std::lock_guard<std::mutex> lock(mutex);
                queuedBytes += read->size;
                queue.emplace_back(av_packet_clone(read), time);
                av_packet_unref(read);
                changed.notify_all();
            }

            av_packet_free(&read);
            std::lock_guard<std::mutex> lock(mutex);
            readDone = true;
            changed.notify_all();
        });

        int64_t base = AV_NOPTS_VALUE;      // The first packet's time (in AV_TIME_BASE)
        // Seconds it's been played since its first packet was written: standing still while it isn't played
        QElapsedTimer clock;
        bool clockRunning = false;
        double played = 0.0;                // Before the clock was last stopped
        auto now = [&]() {
            const bool playing = m_playing;

            if (playing && !clockRunning)
            {
                clock.start();
                clockRunning = true;
            }
            else if (!playing && clockRunning)
            {
                played += clock.nsecsElapsed() * 1e-9;
                clockRunning = false;
            }

            return played + (clockRunning ? clock.nsecsElapsed() * 1e-9 : 0.0);
        };
        double slip = 0.0;                  // Seconds it's been held up for, beyond what was read ahead

        while (!m_stop)
        {
            AVPacket *next = nullptr;

            {
                std::unique_lock<std::mutex> lock(mutex);

                while (!m_stop && queue.empty() && !readDone) {
                    changed.wait_for(lock, std::chrono::milliseconds(20));
                }

                if (m_stop || queue.empty())
                {
                    // All of it written
                    m_finished = !m_stop && endOfStream;
                    break;
                }

                next = queue.front().first;
                queuedBytes -= next->size;
                lastWritten = queue.front().second;
                queue.pop_front();
                changed.notify_all();
            }

            av_packet_free(&packet);
            packet = next;
            const int s = packet->stream_index;
            const AVRational inBase = in->streams[s]->time_base;
            const int64_t first = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;

            if (base == AV_NOPTS_VALUE)
            {
                // Positions are from the stream's start, as a media element's currentTime is
                base = av_rescale_q(first, inBase, AVRational{1, AV_TIME_BASE});
                m_offset = (base - (in->start_time != AV_NOPTS_VALUE ? in->start_time : 0)) / (double) AV_TIME_BASE - m_timestampStart;
                qDebug("DTVStreamRemuxer::run: start_time %.3f, first packet (stream %d) %.3f, pts %.3f", in->start_time / (double) AV_TIME_BASE, s,
                    base / (double) AV_TIME_BASE, packet->pts != AV_NOPTS_VALUE ? packet->pts * av_q2d(inBase) : -1.0);
            }

            // Played in real time: a packet's written no sooner than its time, less the lead
            double due = (av_rescale_q(first, inBase, AVRational{1, AV_TIME_BASE}) - base) / (double) AV_TIME_BASE - m_lead + slip;
            const double late = now() - due;

            if (late > m_resync)
            {
                qDebug("DTVStreamRemuxer::run: %.1f s late (the stream's fetched more slowly than it's played): played on from now", late);
                slip += late;
                due += late;
            }

            while (!m_stop && (now() < due)) {
                msleep((unsigned long) std::max(1.0, std::min(20.0, (due - now()) * 1000.0)));
            }

            // Rebased, to start at m_timestampStart
            const int64_t shift = av_rescale_q((int64_t) (m_timestampStart * AV_TIME_BASE) - base, AVRational{1, AV_TIME_BASE}, inBase);

            if (packet->pts != AV_NOPTS_VALUE) {
                packet->pts += shift;
            }

            if (packet->dts != AV_NOPTS_VALUE) {
                packet->dts += shift;
            }

            AVStream *os = out->streams[outIndex[s]];
            av_packet_rescale_ts(packet, inBase, os->time_base);
            packet->stream_index = os->index;
            packet->pos = -1;
            ret = av_interleaved_write_frame(out, packet);

            if ((ret < 0) && (ret != AVERROR_EXIT)) {
                qDebug("DTVStreamRemuxer::run: write: %s", qPrintable(errorText(ret)));
            }
        }

        // The reader stopped (the loop only ends early when it's stopped, which interrupts its reading), and what it
        // read and wasn't written freed
        {
            std::lock_guard<std::mutex> lock(mutex);
            changed.notify_all();
        }

        reader.join();

        for (auto& queued : queue) {
            av_packet_free(&queued.first);
        }

        if (!m_stop) {
            av_write_trailer(out);
        }
    }

    if (out->pb)
    {
        avio_flush(out->pb);
        av_freep(&out->pb->buffer);
        avio_context_free(&out->pb);
    }

    avformat_free_context(out);
    avformat_close_input(&in);
    av_packet_free(&packet);
}
