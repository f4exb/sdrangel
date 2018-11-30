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
    m_blnIsFullScreen=false;
    m_blnRunning=false;

    m_blnIsFFMPEGInitialized=false;
    m_blnIsOpen=false;
    m_objFormatCtx=NULL;
    m_objDecoderCtx=NULL;
    m_objSwsCtx=NULL;
    m_intVideoStreamIndex=-1;

    m_intCurrentRenderWidth=-1;
    m_intCurrentRenderHeight=-1;

    m_objFrame=NULL;
    m_intFrameCount=-1;
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
    if(m_blnIsFullScreen==blnFullScreen)
    {
        return;
    }

    if(blnFullScreen==true)
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

    if (whence == AVSEEK_SIZE)
    {
        return -1;
    }

    if (objStream->isSequential())
    {
        return -1;
    }

    if (objStream->seek(offset)==false)
    {
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

    if(m_blnIsFFMPEGInitialized)
    {
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
    AVDictionary *objOpts = NULL;
    AVCodec *objCodec = NULL;

    int intRet=-1;
    char *objBuffer=NULL;

    //Identify stream

    if (avformat_find_stream_info(m_objFormatCtx, NULL) < 0)
    {
        avformat_close_input(&m_objFormatCtx);
        m_objFormatCtx=NULL;

        qDebug() << "DATVideoProcess::PreprocessStream cannot find stream info";
        return false;
    }

    //Find video stream
    intRet = av_find_best_stream(m_objFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (intRet < 0)
    {
        avformat_close_input(&m_objFormatCtx);
        qDebug() << "DATVideoProcess::PreprocessStream cannot find video stream";
        return false;
    }

    m_intVideoStreamIndex = intRet;

    //Prepare Codec and extract meta data

    // FIXME: codec is depreecated but replacement fails
//    AVCodecParameters *parms = m_objFormatCtx->streams[m_intVideoStreamIndex]->codecpar;
//    m_objDecoderCtx = new AVCodecContext();
//    avcodec_parameters_to_context(m_objDecoderCtx, parms);
    m_objDecoderCtx = m_objFormatCtx->streams[m_intVideoStreamIndex]->codec;

    //Meta Data

    MetaData.PID = m_objFormatCtx->streams[m_intVideoStreamIndex]->id;
    MetaData.CodecID = m_objDecoderCtx->codec_id;
    MetaData.OK_TransportStream = true;


    MetaData.Program="";
    MetaData.Stream="";

    if(m_objFormatCtx->programs)
    {
        objBuffer=NULL;

        av_dict_get_string(m_objFormatCtx->programs[m_intVideoStreamIndex]->metadata,&objBuffer,':','\n');
        if(objBuffer!=NULL)
        {
            MetaData.Program = QString("%1").arg(objBuffer);
        }
    }

    objBuffer=NULL;

    av_dict_get_string(m_objFormatCtx->streams[m_intVideoStreamIndex]->metadata,&objBuffer,':','\n');

    if(objBuffer!=NULL)
    {
        MetaData.Stream = QString("%1").arg(objBuffer);
    }

    emit onMetaDataChanged(&MetaData);

    //Decoder
    objCodec = avcodec_find_decoder(m_objDecoderCtx->codec_id);
    if(objCodec==NULL)
    {
        avformat_close_input(&m_objFormatCtx);
        m_objFormatCtx=NULL;

        qDebug() << "DATVideoProcess::PreprocessStream cannot find associated CODEC";
        return false;
    }

    av_dict_set(&objOpts, "refcounted_frames", "1", 0);

    if (avcodec_open2(m_objDecoderCtx, objCodec, &objOpts) < 0)
    {
        avformat_close_input(&m_objFormatCtx);
        m_objFormatCtx=NULL;

        qDebug() << "DATVideoProcess::PreprocessStream cannot open associated CODEC";
        return false;
    }

    //Allocate Frame
    m_objFrame = av_frame_alloc();

    if (!m_objFrame)
    {
        avformat_close_input(&m_objFormatCtx);
        m_objFormatCtx=NULL;

        qDebug() << "DATVideoProcess::PreprocessStream cannot allocate frame";
        return false;
    }

    m_intFrameCount=0;


    MetaData.Width=m_objDecoderCtx->width;
    MetaData.Height=m_objDecoderCtx->height;
    MetaData.BitRate= m_objDecoderCtx->bit_rate;
    MetaData.Channels=m_objDecoderCtx->channels;
    MetaData.CodecDescription= QString("%1").arg(objCodec->long_name);

    MetaData.OK_VideoStream = true;

    emit onMetaDataChanged(&MetaData);

    return true;
}

bool DATVideoRender::OpenStream(DATVideostream *objDevice)
{
    int intIOBufferSize = 32768;
    unsigned char * ptrIOBuffer = NULL;
    AVIOContext * objIOCtx = NULL;

    if(m_blnRunning)
    {
        return false;
    }


    if(objDevice==NULL)
    {
        qDebug() << "DATVideoProcess::OpenStream QIODevice is NULL";

        return false;
    }


    if(m_blnIsOpen)
    {
        qDebug() << "DATVideoProcess::OpenStream already open";

        return false;
    }

    if(objDevice->bytesAvailable()<=0)
    {

        qDebug() << "DATVideoProcess::OpenStream no data available";

        MetaData.OK_Data=false;
        emit onMetaDataChanged(&MetaData);

        return false;
    }

    //Only once execution
    m_blnRunning=true;

    MetaData.OK_Data=true;
    emit onMetaDataChanged(&MetaData);


    InitializeFFMPEG();


    if(!m_blnIsFFMPEGInitialized)
    {
        qDebug() << "DATVideoProcess::OpenStream FFMPEG not initialized";

        m_blnRunning=false;
        return false;
    }

    if(!objDevice->open(QIODevice::ReadOnly))
    {
        qDebug() << "DATVideoProcess::OpenStream cannot open QIODevice";

        m_blnRunning=false;
        return false;
    }


    //Connect QIODevice to FFMPEG Reader

    m_objFormatCtx = avformat_alloc_context();

    if(m_objFormatCtx==NULL)
    {
        qDebug() << "DATVideoProcess::OpenStream cannot alloc format FFMPEG context";

        m_blnRunning=false;
        return false;
    }

    ptrIOBuffer = (unsigned char *)av_malloc(intIOBufferSize+ AV_INPUT_BUFFER_PADDING_SIZE);

    objIOCtx = avio_alloc_context(  ptrIOBuffer,
                                    intIOBufferSize,
                                    0,
                                    reinterpret_cast<void *>(objDevice),
                                    &ReadFunction,
                                    NULL,
                                    &SeekFunction);

    m_objFormatCtx->pb = objIOCtx;
    m_objFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;


    if (avformat_open_input(&m_objFormatCtx, NULL  , NULL, NULL) < 0)
    {
        qDebug() << "DATVideoProcess::OpenStream cannot open stream";

        m_blnRunning=false;
        return false;
    }

    if(!PreprocessStream())
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

    if(m_blnRunning)
    {
        return false;
    }

    //Only once execution
    m_blnRunning=true;

    //********** Rendering **********

    if (av_read_frame(m_objFormatCtx, &objPacket) < 0)
    {
        qDebug() << "DATVideoProcess::RenderStream reading packet error";

        m_blnRunning=false;
        return false;
    }

    //Video channel
    if (objPacket.stream_index == m_intVideoStreamIndex)
    {
        memset(m_objFrame, 0, sizeof(AVFrame));
        av_frame_unref(m_objFrame);

        intGotFrame=0;

        if(new_decode( m_objDecoderCtx, m_objFrame, &intGotFrame, &objPacket)<0)
        {
            qDebug() << "DATVideoProcess::RenderStream decoding packet error";

            m_blnRunning=false;
            return false;
        }

        if(intGotFrame)
        {
            //Rendering and RGB Converter setup

            blnNeedRenderingSetup=(m_intFrameCount==0);
            blnNeedRenderingSetup|=(m_objSwsCtx==NULL);

            if((m_intCurrentRenderWidth!=m_objFrame->width) || (m_intCurrentRenderHeight!=m_objFrame->height))
            {
                blnNeedRenderingSetup=true;
            }

            if(blnNeedRenderingSetup)
            {
                if(m_objSwsCtx!=NULL)
                {
                    sws_freeContext(m_objSwsCtx);
                    m_objSwsCtx=NULL;
                }

                //Convertisseur YUV -> RGB
                m_objSwsCtx = sws_alloc_context();

                av_opt_set_int(m_objSwsCtx,"srcw",m_objFrame->width,0);
                av_opt_set_int(m_objSwsCtx,"srch",m_objFrame->height,0);
                av_opt_set_int(m_objSwsCtx,"src_format",m_objFrame->format,0);

                av_opt_set_int(m_objSwsCtx,"dstw",m_objFrame->width,0);
                av_opt_set_int(m_objSwsCtx,"dsth",m_objFrame->height,0);
                av_opt_set_int(m_objSwsCtx,"dst_format",AV_PIX_FMT_RGB24 ,0);

                av_opt_set_int(m_objSwsCtx,"sws_flag", SWS_FAST_BILINEAR  /* SWS_BICUBIC*/,0);

                if(sws_init_context(m_objSwsCtx, NULL, NULL)<0)
                {
                    qDebug() << "DATVideoProcess::RenderStream cannont init video data converter";

                    m_objSwsCtx=NULL;

                    m_blnRunning=false;
                    return false;

                }

                if((m_intCurrentRenderHeight>0) && (m_intCurrentRenderWidth>0))
                {
                    //av_freep(&m_pbytDecodedData[0]);
                    //av_freep(&m_pintDecodedLineSize[0]);
                }

                if(av_image_alloc(m_pbytDecodedData, m_pintDecodedLineSize,m_objFrame->width, m_objFrame->height, AV_PIX_FMT_RGB24, 1)<0)
                {
                    qDebug() << "DATVideoProcess::RenderStream cannont init video image buffer";

                    sws_freeContext(m_objSwsCtx);
                    m_objSwsCtx=NULL;

                    m_blnRunning=false;
                    return false;

                }

                //Rendering device setup

                 resizeTVScreen(m_objFrame->width,m_objFrame->height);
                 update();
                 resetImage();

                 m_intCurrentRenderWidth=m_objFrame->width;
                 m_intCurrentRenderHeight=m_objFrame->height;

                 MetaData.Width = m_objFrame->width;
                 MetaData.Height = m_objFrame->height;
                 MetaData.OK_Decoding = true;
                 emit onMetaDataChanged(&MetaData);
            }

            //Frame rendering

            if(sws_scale(m_objSwsCtx, m_objFrame->data, m_objFrame->linesize, 0, m_objFrame->height, m_pbytDecodedData, m_pintDecodedLineSize)<0)
            {
                qDebug() << "DATVideoProcess::RenderStream error converting video frame to RGB";

                m_blnRunning=false;
                return false;
            }

            renderImage(m_pbytDecodedData[0]);

            av_frame_unref(m_objFrame);

            m_intFrameCount ++;
        }

    }

    av_packet_unref(&objPacket);

    //********** Rendering **********

    m_blnRunning=false;

    return true;
}

bool DATVideoRender::CloseStream(QIODevice *objDevice)
{

    if(m_blnRunning)
    {
        return false;
    }

    if(!objDevice)
    {
        qDebug() << "DATVideoProcess::CloseStream QIODevice is NULL";

        return false;
    }

    if(!m_blnIsOpen)
    {
        qDebug() << "DATVideoProcess::CloseStream Stream not open";

        return false;
    }

    if(!m_objFormatCtx)
    {
        qDebug() << "DATVideoProcess::CloseStream FFMEG Context is not initialized";

        return false;
    }

    //Only once execution
    m_blnRunning=true;

    // maybe done in the avcodec_close
//    avformat_close_input(&m_objFormatCtx);
//    m_objFormatCtx=NULL;

    if(m_objDecoderCtx)
    {
        avcodec_close(m_objDecoderCtx);
        m_objDecoderCtx=NULL;
    }

    if(m_objFrame)
    {
        av_frame_unref(m_objFrame);
        av_frame_free(&m_objFrame);
    }

    if(m_objSwsCtx!=NULL)
    {
        sws_freeContext(m_objSwsCtx);
        m_objSwsCtx=NULL;
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

    if (pkt) {
        ret = avcodec_send_packet(avctx, pkt);
        // In particular, we don't expect AVERROR(EAGAIN), because we read all
        // decoded frames with avcodec_receive_frame() until done.
        if (ret < 0)
            return ret == AVERROR_EOF ? 0 : ret;
    }

    ret = avcodec_receive_frame(avctx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return ret;
    if (ret >= 0)
        *got_frame = 1;

    return 0;
}
