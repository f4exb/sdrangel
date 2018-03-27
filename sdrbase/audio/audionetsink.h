///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_AUDIO_AUDIONETSINK_H_
#define SDRBASE_AUDIO_AUDIONETSINK_H_

#include "dsp/dsptypes.h"
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

    AudioNetSink(QObject *parent); //!< without RTP
    AudioNetSink(QObject *parent, int sampleRate, bool stereo); //!< with RTP
    ~AudioNetSink();

    void setDestination(const QString& address, uint16_t port);
    void addDestination(const QString& address, uint16_t port);
    void deleteDestination(const QString& address, uint16_t port);
    void setParameters(bool stereo, int sampleRate);

    void write(qint16 sample);
    void write(qint16 lSample, qint16 rSample);
    void write(AudioSample* samples, uint32_t numSamples);

    bool isRTPCapable() const;
    bool selectType(SinkType type);

    void moveToThread(QThread *thread);

    static const int m_udpBlockSize;

protected:
    SinkType m_type;
    QUdpSocket *m_udpSocket;
    RTPSink *m_rtpBufferAudio;
    char m_data[65536];
    unsigned int m_bufferIndex;
    QHostAddress m_address;
    unsigned int m_port;
};




#endif /* SDRBASE_AUDIO_AUDIONETSINK_H_ */
