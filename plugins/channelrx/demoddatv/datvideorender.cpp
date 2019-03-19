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

#include "datvideorender.h"

DATVideoRender::DATVideoRender(QWidget * parent):
    TVScreen(true, parent)
{
    installEventFilter(this);
    m_blnIsFullScreen = false;
    m_blnRunning = false;

    m_blnIsFFMPEGInitialized = false;
    m_blnIsOpen = false;
    m_formatCtx = nullptr;
    m_videoDecoderCtx = nullptr;
    m_objSwsCtx = nullptr;
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
    m_audioFifo = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;

    m_intCurrentRenderWidth=-1;
    m_intCurrentRenderHeight=-1;

    m_frame=nullptr;
    m_frameCount=-1;
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


void DATVideoRender::SetFullScreen(bool blnFullScreen)
{
    if (m_blnIsFullScreen == blnFullScreen) {
        return;
    }

    if (blnFullScreen == true)
    {
        setWindowFlags(Qt::Window);
        setWindowState(Qt::WindowFullScreen);
        show();
        m_blnIsFullScreen=true;
    }
    else
    {
        setWindowFlags(Qt::Widget);
        setWindowState(Qt::WindowNoState);
        show();
        m_blnIsFullScreen=false;
    }
}


static int ReadFunction(void *opaque, uint8_t *buf, int buf_size)
{
    QIODevice* objStream = reinterpret_cast<QIODevice*>(opaque);
    int intNbBytes = objStream->read((char*)buf, buf_size);
    return intNbBytes;
}

static int64_t SeekFunction(void* opaque, int64_t offset, int whence)
{
    QIODevice* objStream = reinterpret_cast<QIODevice*>(opaque);

    if (whence == AVSEEK_SIZE) {
        return -1;
    }

    if (objStream->isSequential()) {
        return -1;
    }

    if (objStream->seek(offset) == false) {
        return -1;
    }

    return objStream->pos();
}

void DATVideoRender::ResetMetaData()
{
    MetaData.CodecID=-1;
    MetaData.PID=-1;
    MetaData.Program="";
    MetaData.Stream="";
    MetaData.Width=-1;
    MetaData.Height=-1;
    MetaData.BitRate=-1;
    MetaData.Channels=-1;
    MetaData.CodecDescription= "";

    MetaData.OK_Decoding=false;
    MetaData.OK_TransportStream=false;
    MetaData.OK_VideoStream=false;

    emit onMetaDataChanged(&MetaData);
}

bool DATVideoRender::InitializeFFMPEG()
{
    ResetMetaData();

    if (m_blnIsFFMPEGInitialized) {
        return false;
    }

    avcodec_register_all();
    av_register_all();
    av_log_set_level(AV_LOG_FATAL);
    //av_log_set_level(AV_LOG_ERROR);

    m_blnIsFFMPEGInitialized=true;

    return true;
}

bool DATVideoRender::PreprocessStream()
{
    AVDictionary *opts = nullptr;
    AVCodec *videoCodec = nullptr;
    AVCodec *audioCodec = nullptr;

    int intRet=-1;
    char *buffer=nullptr;

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

    if (intRet < 0) {
        qDebug() << "DATVideoProcess::PreprocessStream cannot find audio stream";
    }

    m_audioStreamIndex = intRet;

    // Prepare Video Codec and extract meta data

    // FIXME: codec is depreecated but replacement fails
//    AVCodecParameters *parms = m_formatCtx->streams[m_videoStreamIndex]->codecpar;
//    m_videoDecoderCtx = new AVCodecContext();
//    avcodec_parameters_to_context(m_videoDecoderCtx, parms);
    m_videoDecoderCtx = m_formatCtx->streams[m_videoStreamIndex]->codec;

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

        if (buffer != nullptr) {
            MetaData.Program = QString("%1").arg(buffer);
        }
    }

    buffer = nullptr;

    av_dict_get_string(m_formatCtx->streams[m_videoStreamIndex]->metadata, &buffer, ':', '\n');

    if (buffer != nullptr) {
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

    av_dict_set(&opts, "refcounted_frames", "1", 0);

    if (avcodec_open2(m_videoDecoderCtx, videoCodec, &opts) < 0)
    {
        avformat_close_input(&m_formatCtx);
        m_formatCtx=nullptr;

        qDebug() << "DATVideoProcess::PreprocessStream cannot open associated video CODEC";
        return false;
    }

    //Allocate Frame
    m_frame = av_frame_alloc();

    if (!m_frame)
    {
        avformat_close_input(&m_formatCtx);
        m_formatCtx=nullptr;

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
        m_audioDecoderCtx = m_formatCtx->streams[m_audioStreamIndex]->codec;

        audioCodec = avcodec_find_decoder(m_audioDecoderCtx->codec_id);

        if (audioCodec == nullptr)
        {
            qDebug() << "DATVideoProcess::PreprocessStream cannot find associated audio CODEC";
            m_audioStreamIndex = -1; // invalidate audio
        }
        else
        {
            if (avcodec_open2(m_audioDecoderCtx, audioCodec, nullptr) < 0)
            {
                qDebug() << "DATVideoProcess::PreprocessStream cannot open associated audio CODEC";
                m_audioStreamIndex = -1; // invalidate audio
            }
        }
    }

    return true;
}

bool DATVideoRender::OpenStream(DATVideostream *objDevice)
{
    int intIOBufferSize = 32768;
    unsigned char * ptrIOBuffer = nullptr;
    AVIOContext * objIOCtx = nullptr;

    if(m_blnRunning) {
        return false;
    }

    if (objDevice == nullptr)
    {
        qDebug() << "DATVideoProcess::OpenStream QIODevice is nullptr";
        return false;
    }

    if (m_blnIsOpen)
    {
        qDebug() << "DATVideoProcess::OpenStream already open";
        return false;
    }

    if (objDevice->bytesAvailable() <= 0)
    {
        qDebug() << "DATVideoProcess::OpenStream no data available";
        MetaData.OK_Data = false;
        emit onMetaDataChanged(&MetaData);
        return false;
    }

    //Only once execution
    m_blnRunning = true;

    MetaData.OK_Data = true;
    emit onMetaDataChanged(&MetaData);

    InitializeFFMPEG();

    if (!m_blnIsFFMPEGInitialized)
    {
        qDebug() << "DATVideoProcess::OpenStream FFMPEG not initialized";
        m_blnRunning = false;
        return false;
    }

    if (!objDevice->open(QIODevice::ReadOnly))
    {
        qDebug() << "DATVideoProcess::OpenStream cannot open QIODevice";
        m_blnRunning = false;
        return false;
    }

    //Connect QIODevice to FFMPEG Reader

    m_formatCtx = avformat_alloc_context();

    if (m_formatCtx == nullptr)
    {
        qDebug() << "DATVideoProcess::OpenStream cannot alloc format FFMPEG context";
        m_blnRunning = false;
        return false;
    }

    ptrIOBuffer = (unsigned char *)av_malloc(intIOBufferSize+ AV_INPUT_BUFFER_PADDING_SIZE);

    objIOCtx = avio_alloc_context(ptrIOBuffer,
        intIOBufferSize,
        0,
        reinterpret_cast<void *>(objDevice),
        &ReadFunction,
        nullptr,
        &SeekFunction);

    m_formatCtx->pb = objIOCtx;
    m_formatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

    if (avformat_open_input(&m_formatCtx, nullptr  , nullptr, nullptr) < 0)
    {
        qDebug() << "DATVideoProcess::OpenStream cannot open stream";
        m_blnRunning=false;
        return false;
    }

    if (!PreprocessStream())
    {
        m_blnRunning=false;
        return false;
    }

    m_blnIsOpen=true;
    m_blnRunning=false;

    return true;
}

bool DATVideoRender::RenderStream()
{
    AVPacket objPacket;
    int intGotFrame;
    bool blnNeedRenderingSetup;

    if(!m_blnIsOpen)
    {
        qDebug() << "DATVideoProcess::RenderStream Stream not open";
        return false;
    }

    if(m_blnRunning) {
        return false;
    }

    //Only once execution
    m_blnRunning=true;

    //********** Rendering **********

    if (av_read_frame(m_formatCtx, &objPacket) < 0)
    {
        qDebug() << "DATVideoProcess::RenderStream reading packet error";
        m_blnRunning=false;
        return false;
    }

    //Video channel
    if (objPacket.stream_index == m_videoStreamIndex)
    {
        memset(m_frame, 0, sizeof(AVFrame));
        av_frame_unref(m_frame);

        intGotFrame = 0;

        if (new_decode(m_videoDecoderCtx, m_frame, &intGotFrame, &objPacket) < 0)
        {
            qDebug() << "DATVideoProcess::RenderStream decoding video packet error";
            m_blnRunning = false;
            return false;
        }

        if (intGotFrame)
        {
            //Rendering and RGB Converter setup
            blnNeedRenderingSetup=(m_frameCount==0);
            blnNeedRenderingSetup|=(m_objSwsCtx==nullptr);

            if ((m_intCurrentRenderWidth!=m_frame->width) || (m_intCurrentRenderHeight!=m_frame->height)) {
                blnNeedRenderingSetup=true;
            }

            if (blnNeedRenderingSetup)
            {
                if (m_objSwsCtx != nullptr)
                {
                    sws_freeContext(m_objSwsCtx);
                    m_objSwsCtx=nullptr;
                }

                //Convertisseur YUV -> RGB
                m_objSwsCtx = sws_alloc_context();

                av_opt_set_int(m_objSwsCtx,"srcw",m_frame->width,0);
                av_opt_set_int(m_objSwsCtx,"srch",m_frame->height,0);
                av_opt_set_int(m_objSwsCtx,"src_format",m_frame->format,0);

                av_opt_set_int(m_objSwsCtx,"dstw",m_frame->width,0);
                av_opt_set_int(m_objSwsCtx,"dsth",m_frame->height,0);
                av_opt_set_int(m_objSwsCtx,"dst_format",AV_PIX_FMT_RGB24 ,0);

                av_opt_set_int(m_objSwsCtx,"sws_flag", SWS_FAST_BILINEAR  /* SWS_BICUBIC*/,0);

                if (sws_init_context(m_objSwsCtx, nullptr, nullptr) < 0)
                {
                    qDebug() << "DATVideoProcess::RenderStream cannont init video data converter";
                    m_objSwsCtx=nullptr;
                    m_blnRunning=false;
                    return false;
                }

                if ((m_intCurrentRenderHeight>0) && (m_intCurrentRenderWidth>0))
                {
                    //av_freep(&m_pbytDecodedData[0]);
                    //av_freep(&m_pintDecodedLineSize[0]);
                }

                if (av_image_alloc(m_pbytDecodedData, m_pintDecodedLineSize,m_frame->width, m_frame->height, AV_PIX_FMT_RGB24, 1)<0)
                {
                    qDebug() << "DATVideoProcess::RenderStream cannont init video image buffer";
                    sws_freeContext(m_objSwsCtx);
                    m_objSwsCtx=nullptr;
                    m_blnRunning=false;
                    return false;
                }

                //Rendering device setup

                resizeTVScreen(m_frame->width,m_frame->height);
                update();
                resetImage();

                m_intCurrentRenderWidth=m_frame->width;
                m_intCurrentRenderHeight=m_frame->height;

                MetaData.Width = m_frame->width;
                MetaData.Height = m_frame->height;
                MetaData.OK_Decoding = true;
                emit onMetaDataChanged(&MetaData);
            }

            //Frame rendering

            if (sws_scale(m_objSwsCtx, m_frame->data, m_frame->linesize, 0, m_frame->height, m_pbytDecodedData, m_pintDecodedLineSize)<0)
            {
                qDebug() << "DATVideoProcess::RenderStream error converting video frame to RGB";
                m_blnRunning=false;
                return false;
            }

            renderImage(m_pbytDecodedData[0]);
            av_frame_unref(m_frame);
            m_frameCount ++;
        }
    }
    // Audio channel
    else if (objPacket.stream_index == m_audioStreamIndex)
    {
    }

    av_packet_unref(&objPacket);

    //********** Rendering **********

    m_blnRunning=false;

    return true;
}

bool DATVideoRender::CloseStream(QIODevice *objDevice)
{

    if (m_blnRunning) {
        return false;
    }

    if (!objDevice)
    {
        qDebug() << "DATVideoProcess::CloseStream QIODevice is nullptr";
        return false;
    }

    if (!m_blnIsOpen)
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
    m_blnRunning=true;

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

    if (m_objSwsCtx != nullptr)
    {
        sws_freeContext(m_objSwsCtx);
        m_objSwsCtx = nullptr;
    }

    objDevice->close();
    m_blnIsOpen=false;
    m_blnRunning=false;
    m_intCurrentRenderWidth=-1;
    m_intCurrentRenderHeight=-1;

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
        if (ret < 0) {
            return ret == AVERROR_EOF ? 0 : ret;
        }
    }

    ret = avcodec_receive_frame(avctx, frame);

    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return ret;
    }

    if (ret >= 0) {
        *got_frame = 1;
    }

    return 0;
}
