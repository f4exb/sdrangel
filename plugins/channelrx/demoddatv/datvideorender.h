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

#ifndef DATVIDEORENDER_H
#define DATVIDEORENDER_H

#include <QEvent>
#include <QIODevice>
#include <QThread>
#include <QWidget>

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
        PID = -1;
        CodecID = -1;
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
};

class DATVideoRender : public TVScreen
{
    Q_OBJECT

  public:
    explicit DATVideoRender(QWidget *parent);
    ~DATVideoRender();

    void SetFullScreen(bool blnFullScreen);

    bool OpenStream(DATVideostream *objDevice);
    bool RenderStream();
    bool CloseStream(QIODevice *objDevice);

    void setAudioFIFO(AudioFifo *fifo) { m_audioFifo = fifo; }
    int getVideoStreamIndex() const { return m_videoStreamIndex; }
    int getAudioStreamIndex() const { return m_audioStreamIndex; }

    void setAudioMute(bool audioMute) { m_audioMute = audioMute; }
    void setAudioVolume(int audioVolume);

    struct DataTSMetaData2 MetaData;

  private:
    bool m_running;
    bool m_isFullScreen;

    bool m_isFFMPEGInitialized;
    bool m_isOpen;

    SwsContext *m_swsCtx;
    AVFormatContext *m_formatCtx;
    AVCodecContext *m_videoDecoderCtx;
    AVCodecContext *m_audioDecoderCtx;
    AVFrame *m_frame;
    AudioVector m_audioBuffer;
    uint32_t m_audioBufferFill;
    AudioFifo *m_audioFifo;
    struct SwrContext* m_audioSWR;
    int m_audioSampleRate;
    static const int m_audioFifoBufferSize = 16000;
    uint16_t m_audioFifoBuffer[m_audioFifoBufferSize*2]; // 2 channels
    int m_audioFifoBufferIndex;
    bool m_audioMute;
    int m_audioVolume;
    bool m_updateAudioResampler;

    uint8_t *m_pbytDecodedData[4];
    int m_pintDecodedLineSize[4];

    int m_frameCount;
    int m_videoStreamIndex;
    int m_audioStreamIndex;

    int m_currentRenderWidth;
    int m_currentRenderHeight;

    bool InitializeFFMPEG();
    bool PreprocessStream();
    void ResetMetaData();

    int new_decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt);
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
        m_renderer = renderer;
        m_stream = stream;
        m_renderingVideo = false;
    }

    void run()
    {
        if (m_renderingVideo)
        {
            return;
        }

        if ((m_renderer == nullptr) || (m_stream == nullptr))
        {
            return;
        }

        m_renderingVideo = m_renderer->OpenStream(m_stream);

        if (!m_renderingVideo)
        {
            return;
        }

        while ((m_renderer->RenderStream()) && (m_renderingVideo == true))
        {
        }

        m_renderer->CloseStream(m_stream);
        m_renderingVideo = false;
    }

    void stopRendering()
    {
        m_renderingVideo = false;
    }

  private:
    DATVideoRender *m_renderer;
    DATVideostream *m_stream;
    bool m_renderingVideo;
};

#endif // DATVIDEORENDER_H
