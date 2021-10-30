///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#ifndef DATVIDEORENDER_H
#define DATVIDEORENDER_H

#include <QEvent>
#include <QIODevice>
#include <QThread>
#include <QWidget>
#include <QTextStream>
#include <QDebug>

#include "datvideostream.h"
#include "gui/tvscreen.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>

#include "libswscale/swscale.h"
}

class AudioFifo;

struct DataTSMetaData2
{
    int PID;
    int CodecID;

    bool OK_Data;
    bool OK_Decoding;
    bool OK_TransportStream;
    bool OK_VideoStream;

    QString Program;
    QString Stream;

    int Width;
    int Height;
    int BitRate;
    int Channels;

    QString CodecDescription;

    DataTSMetaData2()
    {
        reset();
    }

    void reset()
    {
        CodecID = -1;
        PID = -1;
        Program = "";
        Stream = "";
        Width = -1;
        Height = -1;
        BitRate = -1;
        Channels = -1;
        CodecDescription = "";

        OK_Data = false;
        OK_Decoding = false;
        OK_TransportStream = false;
        OK_VideoStream = false;
    }

    void formatString(QString &s)
    {
        QTextStream out(&s);
        out << " CodecID:" << CodecID
            << " PID:" << PID
            << " Program:" << Program
            << " Stream:" << Stream
            << " Width:" << Width
            << " Height:" << Height
            << " BitRate:" << BitRate
            << " Channels:" << Channels
            << " CodecDescription:" << CodecDescription;
    }
};

class DATVideoRender : public TVScreen
{
    Q_OBJECT

  public:
    explicit DATVideoRender(QWidget *parent);
    ~DATVideoRender();

    void setFullScreen(bool blnFullScreen);

    void setAudioFIFO(AudioFifo *fifo) { m_audioFifo = fifo; }
    bool openStream(DATVideostream *objDevice);
    bool renderStream();
    bool closeStream(QIODevice *objDevice);

    int getVideoStreamIndex() const { return m_videoStreamIndex; }
    int getAudioStreamIndex() const { return m_audioStreamIndex; }

    void setAudioMute(bool audioMute) { m_audioMute = audioMute; }
    void setVideoMute(bool videoMute) { m_videoMute = videoMute; }
    void setAudioVolume(int audioVolume);

    bool getAudioDecodeOK() const { return m_audioDecodeOK; }
    bool getVideoDecodeOK() const { return m_videoDecodeOK; }

  private:
    struct DataTSMetaData2 m_metaData;
    QWidget *m_parentWidget;
    Qt::WindowFlags m_originalWindowFlags;
    QSize m_originalSize;
    bool m_isFullScreen;
    bool m_isOpen;
    SwsContext *m_swsCtx;
    AVFormatContext *m_formatCtx;
    AVCodecContext *m_videoDecoderCtx;
    AVCodecContext *m_audioDecoderCtx;
    AVFrame *m_frame;
    AudioFifo *m_audioFifo;
    struct SwrContext* m_audioSWR;
    int m_audioSampleRate;
    static const int m_audioFifoBufferSize = 16000;
    int16_t m_audioFifoBuffer[m_audioFifoBufferSize*2]; // 2 channels
    int m_audioFifoBufferIndex;
    bool m_audioMute;
    bool m_videoMute;
    float m_audioVolume;

    uint8_t *m_decodedData[4];
    int m_decodedLineSize[4];

    int m_frameCount;
    int m_videoStreamIndex;
    int m_audioStreamIndex;

    int m_currentRenderWidth;
    int m_currentRenderHeight;

    bool m_audioDecodeOK;
    bool m_videoDecodeOK;

    static int ReadFunction(void *opaque, uint8_t *buf, int buf_size);
    static int64_t SeekFunction(void *opaque, int64_t offset, int whence);

    bool preprocessStream();
    void resetMetaData();

    int newDecode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt);
    void setResampler();

  protected:
    virtual bool eventFilter(QObject *obj, QEvent *event);

  signals:
    void onMetaDataChanged(DataTSMetaData2 *metaData);
};

//To run Video Rendering with a dedicated thread
class DATVideoRenderThread : public QThread
{
  public:
    DATVideoRenderThread()
    {
        m_renderer = nullptr;
        m_stream = nullptr;
        m_renderingVideo = false;
    }

    DATVideoRenderThread(DATVideoRender *renderer, DATVideostream *stream)
    {
        m_renderer = renderer;
        m_stream = stream;
        m_renderingVideo = false;
    }

    void setStreamAndRenderer(DATVideoRender *renderer, DATVideostream *stream)
    {
        m_renderingVideo = false;
        m_renderer = renderer;
        m_stream = stream;
    }

    void run()
    {
        if (m_renderingVideo) {
            return;
        }

        if ((m_renderer == nullptr) || (m_stream == nullptr)) {
            return;
        }

        m_renderingVideo = m_renderer->openStream(m_stream);

        if (!m_renderingVideo) {
            return;
        }

        while ((m_renderingVideo == true) && (m_renderer))
        {
            if (!m_renderer->renderStream()) {
                break;
            }
        }

        m_renderer->closeStream(m_stream);
        m_renderingVideo = false;
    }

    void stopRendering() {
        m_renderingVideo = false;
    }

    static const int videoThreadTimeoutMs = 1000;

  private:
    DATVideoRender *m_renderer;
    DATVideostream *m_stream;
    bool m_renderingVideo;
};

#endif // DATVIDEORENDER_H
