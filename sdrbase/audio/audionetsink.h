///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_AUDIO_AUDIONETSINK_H_
#define SDRBASE_AUDIO_AUDIONETSINK_H_

#include "dsp/dsptypes.h"
#include "audiofilter.h"
#include "audiocompressor.h"
#include "audiog722.h"
#include "audioopus.h"
#include "export.h"

#include <QObject>
#include <QHostAddress>
#include <stdint.h>

class QUdpSocket;
class RTPSink;
class QThread;

class SDRBASE_API AudioNetSink {
public:
    typedef enum
    {
        SinkUDP,
        SinkRTP
    } SinkType;

    typedef enum
    {
        CodecL16,  //!< Linear 16 bit samples (no formatting)
        CodecL8,   //!< Linear 8 bit samples
        CodecPCMA, //!< PCM A-law 8 bit samples
        CodecPCMU, //!< PCM Mu-law 8 bit samples
        CodecG722, //!< G722 compressed 8 bit samples 16kS/s in 8kS/s out
        CodecOpus  //!< Opus compressed 8 bit samples at 64kbits/s (8kS/s out). Various input sample rates
    } Codec;

    AudioNetSink(QObject *parent); //!< without RTP
    AudioNetSink(QObject *parent, int sampleRate, bool stereo); //!< with RTP
    ~AudioNetSink();

    void setDestination(const QString& address, uint16_t port);
    void addDestination(const QString& address, uint16_t port);
    void deleteDestination(const QString& address, uint16_t port);
    void setParameters(Codec codec, bool stereo, int sampleRate);
    void setDecimation(uint32_t decimation);

    void write(qint16 sample);
    void write(qint16 lSample, qint16 rSample);

    bool isRTPCapable() const;
    bool selectType(SinkType type);

    void moveToThread(QThread *thread);

    static const int m_udpBlockSize;
    static const int m_dataBlockSize = 65536; // room for G722 conversion (64000 = 12800*5 largest to date)
    static const int m_g722BlockSize = 12800; // number of resulting G722 bytes (80*20ms frames)
    static const int m_opusBlockSize = 960*2; // provision for 20ms of 2 int16 channels at 48 kS/s
    static const int m_opusOutputSize = 160;  // output frame: 20ms of 8 bit data @ 64 kbits/s = 160 bytes

protected:
    void setNewCodecData();       // actions to take when changes affecting codec dependent data occurs
    void setDecimationFilters();  // set decimation filters limits depending on effective sample rate and codec

    SinkType m_type;
    Codec m_codec;
    QUdpSocket *m_udpSocket;
    RTPSink *m_rtpBufferAudio;
    AudioCompressor m_audioCompressor;
    AudioG722 m_g722;
    AudioOpus m_opus;
    AudioFilter m_audioFilter;
    int m_sampleRate;
    bool m_stereo;
    uint32_t m_decimation;
    uint32_t m_decimationCount;
    char m_data[m_dataBlockSize];
    int16_t m_opusIn[m_opusBlockSize];
    int m_codecInputSize;  // codec input block size - for codecs with actual encoding (Opus only for now)
    int m_codecInputIndex; // codec input block fill index
    int m_codecRatio;      // codec compression ratio
    unsigned int m_bufferIndex;
    QHostAddress m_address;
    unsigned int m_port;
};

#endif /* SDRBASE_AUDIO_AUDIONETSINK_H_ */
