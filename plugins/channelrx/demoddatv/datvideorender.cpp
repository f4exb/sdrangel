///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <algorithm>

extern "C"
{
#include <libswresample/swresample.h>
}

#include "audio/audiofifo.h"
#include "datvideorender.h"

DATVideoRender::DATVideoRender(QWidget *parent) : TVScreen(true, parent)
{
    installEventFilter(this);
    m_isFullScreen = false;
    m_running = false;

    m_isFFMPEGInitialized = false;
    m_isOpen = false;
    m_formatCtx = nullptr;
    m_videoDecoderCtx = nullptr;
    m_swsCtx = nullptr;
    m_audioBuffer.resize(1 << 14);
    m_audioBufferFill = 0;
    m_audioFifo = nullptr;
    m_audioSWR = nullptr;
    m_audioSampleRate = 48000;
    m_audioFifoBufferIndex = 0;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;

    m_currentRenderWidth = -1;
    m_currentRenderHeight = -1;

    m_frame = nullptr;
    m_frameCount = -1;

    // for (int i = 0; i < m_audioFifoBufferSize; i++)
    // {
    //     m_audioFifoBuffer[2*i]   = 8192.0f * sin((M_PI * i)/(m_audioFifoBufferSize/1000.0f));
    //     m_audioFifoBuffer[2*i+1] = m_audioFifoBuffer[2*i];
    // }
}

bool DATVideoRender::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        SetFullScreen(false);
        return true;
    }
    else
    {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}

void DATVideoRender::SetFullScreen(bool fullScreen)
{
    if (m_isFullScreen == fullScreen)
    {
        return;
    }

    if (fullScreen == true)
    {
        setWindowFlags(Qt::Window);
        setWindowState(Qt::WindowFullScreen);
        show();
        m_isFullScreen = true;
    }
    else
    {
        setWindowFlags(Qt::Widget);
        setWindowState(Qt::WindowNoState);
        show();
        m_isFullScreen = false;
    }
}

static int ReadFunction(void *opaque, uint8_t *buf, int buf_size)
{
    QIODevice *stream = reinterpret_cast<QIODevice *>(opaque);
    int nbBytes = stream->read((char *)buf, buf_size);
    return nbBytes;
}

static int64_t SeekFunction(void *opaque, int64_t offset, int whence)
{
    QIODevice *stream = reinterpret_cast<QIODevice *>(opaque);

    if (whence == AVSEEK_SIZE)
    {
        return -1;
    }

    if (stream->isSequential())
    {
        return -1;
    }

    if (stream->seek(offset) == false)
    {
        return -1;
    }

    return stream->pos();
}

void DATVideoRender::ResetMetaData()
{
    MetaData.CodecID = -1;
    MetaData.PID = -1;
    MetaData.Program = "";
    MetaData.Stream = "";
    MetaData.Width = -1;
    MetaData.Height = -1;
    MetaData.BitRate = -1;
    MetaData.Channels = -1;
    MetaData.CodecDescription = "";

    MetaData.OK_Decoding = false;
    MetaData.OK_TransportStream = false;
    MetaData.OK_VideoStream = false;

    emit onMetaDataChanged(&MetaData);
}

bool DATVideoRender::InitializeFFMPEG()
{
    ResetMetaData();

    if (m_isFFMPEGInitialized)
    {
        return false;
    }

    avcodec_register_all();
    av_register_all();
    av_log_set_level(AV_LOG_FATAL);
    //av_log_set_level(AV_LOG_ERROR);

    m_isFFMPEGInitialized = true;

    return true;
}

bool DATVideoRender::PreprocessStream()
{
    AVDictionary *opts = nullptr;
    AVCodec *videoCodec = nullptr;
    AVCodec *audioCodec = nullptr;

    int intRet = -1;
    char *buffer = nullptr;

    //Identify stream

    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0)
    {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        qDebug() << "DATVideoProcess::PreprocessStream cannot find stream info";
        return false;
    }

    //Find video stream
    intRet = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (intRet < 0)
    {
        avformat_close_input(&m_formatCtx);
        qDebug() << "DATVideoProcess::PreprocessStream cannot find video stream";
        return false;
    }

    m_videoStreamIndex = intRet;

    //Find audio stream
    intRet = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (intRet < 0)
    {
        qDebug() << "DATVideoProcess::PreprocessStream cannot find audio stream";
    }

    m_audioStreamIndex = intRet;

    // Prepare Video Codec and extract meta data

    AVCodecParameters *parms = m_formatCtx->streams[m_videoStreamIndex]->codecpar;

    if (m_videoDecoderCtx) {
        avcodec_free_context(&m_videoDecoderCtx);
    }

    m_videoDecoderCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(m_videoDecoderCtx, parms);

    // m_videoDecoderCtx = m_formatCtx->streams[m_videoStreamIndex]->codec; // old style

    //Meta Data

    MetaData.PID = m_formatCtx->streams[m_videoStreamIndex]->id;
    MetaData.CodecID = m_videoDecoderCtx->codec_id;
    MetaData.OK_TransportStream = true;
    MetaData.Program = "";
    MetaData.Stream = "";

    if (m_formatCtx->programs)
    {
        buffer = nullptr;
        av_dict_get_string(m_formatCtx->programs[m_videoStreamIndex]->metadata, &buffer, ':', '\n');

        if (buffer != nullptr)
        {
            MetaData.Program = QString("%1").arg(buffer);
        }
    }

    buffer = nullptr;

    av_dict_get_string(m_formatCtx->streams[m_videoStreamIndex]->metadata, &buffer, ':', '\n');

    if (buffer != nullptr)
    {
        MetaData.Stream = QString("%1").arg(buffer);
    }

    emit onMetaDataChanged(&MetaData);

    //Decoder
    videoCodec = avcodec_find_decoder(m_videoDecoderCtx->codec_id);

    if (videoCodec == nullptr)
    {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;

        qDebug() << "DATVideoProcess::PreprocessStream cannot find associated video CODEC";
        return false;
    }
    else
    {
        qDebug() << "DATVideoProcess::PreprocessStream: video CODEC found: " << videoCodec->name;
    }

    av_dict_set(&opts, "refcounted_frames", "1", 0);

    if (avcodec_open2(m_videoDecoderCtx, videoCodec, &opts) < 0)
    {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;

        qDebug() << "DATVideoProcess::PreprocessStream cannot open associated video CODEC";
        return false;
    }

    //Allocate Frame
    m_frame = av_frame_alloc();

    if (!m_frame)
    {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;

        qDebug() << "DATVideoProcess::PreprocessStream cannot allocate frame";
        return false;
    }

    m_frameCount = 0;
    MetaData.Width = m_videoDecoderCtx->width;
    MetaData.Height = m_videoDecoderCtx->height;
    MetaData.BitRate = m_videoDecoderCtx->bit_rate;
    MetaData.Channels = m_videoDecoderCtx->channels;
    MetaData.CodecDescription = QString("%1").arg(videoCodec->long_name);
    MetaData.OK_VideoStream = true;

    emit onMetaDataChanged(&MetaData);

    // Prepare Audio Codec

    if (m_audioStreamIndex >= 0)
    {
        AVCodecParameters *parms = m_formatCtx->streams[m_audioStreamIndex]->codecpar;

        if (m_audioDecoderCtx) {
            avcodec_free_context(&m_audioDecoderCtx);
        }

        m_audioDecoderCtx = avcodec_alloc_context3(NULL);
        avcodec_parameters_to_context(m_audioDecoderCtx, parms);

        //m_audioDecoderCtx = m_formatCtx->streams[m_audioStreamIndex]->codec; // old style

        qDebug() << "DATVideoProcess::PreprocessStream: audio: "
        << " channels: " << m_audioDecoderCtx->channels
        << " channel_layout: " << m_audioDecoderCtx->channel_layout
        << " sample_rate: " << m_audioDecoderCtx->sample_rate
        << " sample_fmt: " << m_audioDecoderCtx->sample_fmt
        << " codec_id: "<< m_audioDecoderCtx->codec_id;

        audioCodec = avcodec_find_decoder(m_audioDecoderCtx->codec_id);

        if (audioCodec == nullptr)
        {
            qDebug() << "DATVideoProcess::PreprocessStream cannot find associated audio CODEC";
            m_audioStreamIndex = -1; // invalidate audio
        }
        else
        {
            qDebug() << "DATVideoProcess::PreprocessStream: audio CODEC found: " << audioCodec->name;

            if (avcodec_open2(m_audioDecoderCtx, audioCodec, nullptr) < 0)
            {
                qDebug() << "DATVideoProcess::PreprocessStream cannot open associated audio CODEC";
                m_audioStreamIndex = -1; // invalidate audio
            }
            else
            {
                setResampler();
            }

        }
    }

    return true;
}

bool DATVideoRender::OpenStream(DATVideostream *device)
{
    int ioBufferSize = 32768;
    unsigned char *ptrIOBuffer = nullptr;
    AVIOContext *ioCtx = nullptr;

    if (m_running)
    {
        return false;
    }

    if (device == nullptr)
    {
        qDebug() << "DATVideoProcess::OpenStream QIODevice is nullptr";
        return false;
    }

    if (m_isOpen)
    {
        qDebug() << "DATVideoProcess::OpenStream already open";
        return false;
    }

    if (device->bytesAvailable() <= 0)
    {
        qDebug() << "DATVideoProcess::OpenStream no data available";
        MetaData.OK_Data = false;
        emit onMetaDataChanged(&MetaData);
        return false;
    }

    //Only once execution
    m_running = true;

    MetaData.OK_Data = true;
    emit onMetaDataChanged(&MetaData);

    InitializeFFMPEG();

    if (!m_isFFMPEGInitialized)
    {
        qDebug() << "DATVideoProcess::OpenStream FFMPEG not initialized";
        m_running = false;
        return false;
    }

    if (!device->open(QIODevice::ReadOnly))
    {
        qDebug() << "DATVideoProcess::OpenStream cannot open QIODevice";
        m_running = false;
        return false;
    }

    //Connect QIODevice to FFMPEG Reader

    m_formatCtx = avformat_alloc_context();

    if (m_formatCtx == nullptr)
    {
        qDebug() << "DATVideoProcess::OpenStream cannot alloc format FFMPEG context";
        m_running = false;
        return false;
    }

    ptrIOBuffer = (unsigned char *)av_malloc(ioBufferSize + AV_INPUT_BUFFER_PADDING_SIZE);

    ioCtx = avio_alloc_context(ptrIOBuffer,
                                  ioBufferSize,
                                  0,
                                  reinterpret_cast<void *>(device),
                                  &ReadFunction,
                                  nullptr,
                                  &SeekFunction);

    m_formatCtx->pb = ioCtx;
    m_formatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

    if (avformat_open_input(&m_formatCtx, nullptr, nullptr, nullptr) < 0)
    {
        qDebug() << "DATVideoProcess::OpenStream cannot open stream";
        m_running = false;
        return false;
    }

    if (!PreprocessStream())
    {
        m_running = false;
        return false;
    }

    m_isOpen = true;
    m_running = false;

    return true;
}

bool DATVideoRender::RenderStream()
{
    AVPacket packet;
    int gotFrame;
    bool needRenderingSetup;

    if (!m_isOpen)
    {
        qDebug() << "DATVideoProcess::RenderStream Stream not open";
        return false;
    }

    if (m_running)
    {
        return false;
    }

    //Only once execution
    m_running = true;

    //********** Rendering **********

    if (av_read_frame(m_formatCtx, &packet) < 0)
    {
        qDebug() << "DATVideoProcess::RenderStream reading packet error";
        m_running = false;
        return false;
    }

    //Video channel
    if (packet.stream_index == m_videoStreamIndex)
    {
        memset(m_frame, 0, sizeof(AVFrame));
        av_frame_unref(m_frame);

        gotFrame = 0;

        if (new_decode(m_videoDecoderCtx, m_frame, &gotFrame, &packet) < 0)
        {
            qDebug() << "DATVideoProcess::RenderStream decoding video packet error";
            m_running = false;
            return false;
        }

        if (gotFrame)
        {
            //Rendering and RGB Converter setup
            needRenderingSetup = (m_frameCount == 0);
            needRenderingSetup |= (m_swsCtx == nullptr);

            if ((m_currentRenderWidth != m_frame->width) || (m_currentRenderHeight != m_frame->height))
            {
                needRenderingSetup = true;
            }

            if (needRenderingSetup)
            {
                if (m_swsCtx != nullptr)
                {
                    sws_freeContext(m_swsCtx);
                    m_swsCtx = nullptr;
                }

                //Convertisseur YUV -> RGB
                m_swsCtx = sws_alloc_context();

                av_opt_set_int(m_swsCtx, "srcw", m_frame->width, 0);
                av_opt_set_int(m_swsCtx, "srch", m_frame->height, 0);
                av_opt_set_int(m_swsCtx, "src_format", m_frame->format, 0);

                av_opt_set_int(m_swsCtx, "dstw", m_frame->width, 0);
                av_opt_set_int(m_swsCtx, "dsth", m_frame->height, 0);
                av_opt_set_int(m_swsCtx, "dst_format", AV_PIX_FMT_RGB24, 0);

                av_opt_set_int(m_swsCtx, "sws_flag", SWS_FAST_BILINEAR /* SWS_BICUBIC*/, 0);

                if (sws_init_context(m_swsCtx, nullptr, nullptr) < 0)
                {
                    qDebug() << "DATVideoProcess::RenderStream cannont init video data converter";
                    m_swsCtx = nullptr;
                    m_running = false;
                    return false;
                }

                if ((m_currentRenderHeight > 0) && (m_currentRenderWidth > 0))
                {
                    //av_freep(&m_pbytDecodedData[0]);
                    //av_freep(&m_pintDecodedLineSize[0]);
                }

                if (av_image_alloc(m_pbytDecodedData, m_pintDecodedLineSize, m_frame->width, m_frame->height, AV_PIX_FMT_RGB24, 1) < 0)
                {
                    qDebug() << "DATVideoProcess::RenderStream cannont init video image buffer";
                    sws_freeContext(m_swsCtx);
                    m_swsCtx = nullptr;
                    m_running = false;
                    return false;
                }

                //Rendering device setup

                resizeTVScreen(m_frame->width, m_frame->height);
                update();
                resetImage();

                m_currentRenderWidth = m_frame->width;
                m_currentRenderHeight = m_frame->height;

                MetaData.Width = m_frame->width;
                MetaData.Height = m_frame->height;
                MetaData.OK_Decoding = true;
                emit onMetaDataChanged(&MetaData);
            }

            //Frame rendering

            if (sws_scale(m_swsCtx, m_frame->data, m_frame->linesize, 0, m_frame->height, m_pbytDecodedData, m_pintDecodedLineSize) < 0)
            {
                qDebug() << "DATVideoProcess::RenderStream error converting video frame to RGB";
                m_running = false;
                return false;
            }

            renderImage(m_pbytDecodedData[0]);
            av_frame_unref(m_frame);
            m_frameCount++;
        }
    }
    // Audio channel
    else if ((packet.stream_index == m_audioStreamIndex) && (m_audioFifo) && (swr_is_initialized(m_audioSWR)))
    {
        memset(m_frame, 0, sizeof(AVFrame));
        av_frame_unref(m_frame);
        gotFrame = 0;

        if (new_decode(m_audioDecoderCtx, m_frame, &gotFrame, &packet) >= 0)
        //if (avcodec_decode_audio4(m_audioDecoderCtx, m_frame, &gotFrame, &packet) >= 0) // old style
        {
            if (gotFrame)
            {
                uint16_t *audioBuffer;
                av_samples_alloc((uint8_t**) &audioBuffer, nullptr, 2, m_frame->nb_samples, AV_SAMPLE_FMT_S16, 0);
                int frame_count = swr_convert(m_audioSWR, (uint8_t**) &audioBuffer, m_frame->nb_samples, (const uint8_t**) m_frame->data, m_frame->nb_samples);

                // int ret = m_audioFifo->write((const quint8*) &audioBuffer[0], frame_count);

                // if (ret < frame_count) {
                //     qDebug("DATVideoRender::RenderStream: audio frames missing %d vs %d", ret, frame_count);
                // }

                if (m_audioFifoBufferIndex + frame_count < m_audioFifoBufferSize)
                {
                    std::copy(&audioBuffer[0], &audioBuffer[2*frame_count], &m_audioFifoBuffer[2*m_audioFifoBufferIndex]);
                    m_audioFifoBufferIndex += frame_count;
                }
                else
                {
                    int remainder = m_audioFifoBufferSize - m_audioFifoBufferIndex;
                    std::copy(&audioBuffer[0], &audioBuffer[2*remainder], &m_audioFifoBuffer[2*m_audioFifoBufferIndex]);
                    m_audioFifo->write((const quint8*) &m_audioFifoBuffer[0], m_audioFifoBufferSize);
                    std::copy(&audioBuffer[2*remainder], &audioBuffer[2*frame_count], &m_audioFifoBuffer[0]);
                    m_audioFifoBufferIndex = frame_count - remainder;
                }

                // m_audioFifoBufferIndex += frame_count;

                // if (m_audioFifoBufferIndex >= m_audioFifoBufferSize)
                // {
                //     m_audioFifo->write((const quint8*)&m_audioFifoBuffer[0], m_audioFifoBufferSize);
                //     m_audioFifoBufferIndex -= m_audioFifoBufferSize;
                // }
            }
        }
        else
        {
            qDebug("DATVideoRender::RenderStream: audio decode error");
        }
    }

    av_packet_unref(&packet);

    //********** Rendering **********

    m_running = false;

    return true;
}

void DATVideoRender::setResampler()
{
    m_audioSWR = swr_alloc();
    av_opt_set_int(m_audioSWR, "in_channel_count",  m_audioDecoderCtx->channels, 0);
    av_opt_set_int(m_audioSWR, "out_channel_count", 2, 0);
    av_opt_set_int(m_audioSWR, "in_channel_layout",  m_audioDecoderCtx->channel_layout, 0);
    av_opt_set_int(m_audioSWR, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(m_audioSWR, "in_sample_rate", m_audioDecoderCtx->sample_rate, 0);
    av_opt_set_int(m_audioSWR, "out_sample_rate", m_audioSampleRate, 0);
    av_opt_set_sample_fmt(m_audioSWR, "in_sample_fmt",  m_audioDecoderCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(m_audioSWR, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
    swr_init(m_audioSWR);
}

bool DATVideoRender::CloseStream(QIODevice *device)
{

    if (m_running)
    {
        return false;
    }

    if (!device)
    {
        qDebug() << "DATVideoProcess::CloseStream QIODevice is nullptr";
        return false;
    }

    if (!m_isOpen)
    {
        qDebug() << "DATVideoProcess::CloseStream Stream not open";
        return false;
    }

    if (!m_formatCtx)
    {
        qDebug() << "DATVideoProcess::CloseStream FFMEG Context is not initialized";
        return false;
    }

    //Only once execution
    m_running = true;

    // maybe done in the avcodec_close
    //    avformat_close_input(&m_formatCtx);
    //    m_formatCtx=nullptr;

    if (m_videoDecoderCtx)
    {
        avcodec_close(m_videoDecoderCtx);
        m_videoDecoderCtx = nullptr;
    }

    if (m_frame)
    {
        av_frame_unref(m_frame);
        av_frame_free(&m_frame);
    }

    if (m_swsCtx != nullptr)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    device->close();
    m_isOpen = false;
    m_running = false;
    m_currentRenderWidth = -1;
    m_currentRenderHeight = -1;

    ResetMetaData();
    emit onMetaDataChanged(&MetaData);

    return true;
}

/**
 * Replacement of deprecated avcodec_decode_video2 with the same signature
 * https://blogs.gentoo.org/lu_zero/2016/03/29/new-avcodec-api/
 */
int DATVideoRender::new_decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt)
{
    int ret;

    *got_frame = 0;

    if (pkt)
    {
        ret = avcodec_send_packet(avctx, pkt);
        // In particular, we don't expect AVERROR(EAGAIN), because we read all
        // decoded frames with avcodec_receive_frame() until done.
        if (ret < 0)
        {
            return ret == AVERROR_EOF ? 0 : ret;
        }
    }

    ret = avcodec_receive_frame(avctx, frame);

    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
        return ret;
    }

    if (ret >= 0)
    {
        *got_frame = 1;
    }

    return 0;
}
