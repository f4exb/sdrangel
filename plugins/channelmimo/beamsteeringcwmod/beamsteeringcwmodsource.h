///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_BEAMSTEERINGCWMODSOURCE_H
#define INCLUDE_BEAMSTEERINGCWMODSOURCE_H

#include <QObject>
#include <QMutex>

#include "dsp/samplemofifo.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "beamsteeringcwmodstreamsource.h"

class UpSampleChannelizer;

class BeamSteeringCWModSource : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getLog2Interp() const { return m_log2Interp; }
        int getFilterChainHash() const { return m_filterChainHash; }

        static MsgConfigureChannelizer* create(unsigned int log2Interp, unsigned int filterChainHash) {
            return new MsgConfigureChannelizer(log2Interp, filterChainHash);
        }

    private:
        unsigned int m_log2Interp;
        unsigned int m_filterChainHash;

        MsgConfigureChannelizer(unsigned int log2Interp, unsigned int filterChainHash) :
            Message(),
            m_log2Interp(log2Interp),
            m_filterChainHash(filterChainHash)
        { }
    };

    class MsgSignalNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getOutputSampleRate() const { return m_outputSampleRate; }
        qint64 getCenterFrequency() const { return m_centerFrequency; }

        static MsgSignalNotification* create(int outputSampleRate, qint64 centerFrequency) {
            return new MsgSignalNotification(outputSampleRate, centerFrequency);
        }
    private:
        int m_outputSampleRate;
        qint64 m_centerFrequency;

        MsgSignalNotification(int outputSampleRate, qint64 centerFrequency) :
            Message(),
            m_outputSampleRate(outputSampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

    class MsgConfigureSteeringAngle : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getSteeringAngle() const { return m_steeringAngle; }

        static MsgConfigureSteeringAngle* create(float steeringAngle) {
            return new MsgConfigureSteeringAngle(steeringAngle);
        }
    private:
        float m_steeringAngle;

        MsgConfigureSteeringAngle(float steeringAngle) :
            Message(),
            m_steeringAngle(steeringAngle)
        {}
    };

    BeamSteeringCWModSource();
    ~BeamSteeringCWModSource();
    void reset();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication

    void setSteeringDegrees(int steeringDegrees);
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples, unsigned int streamIndex);

private:
    void processFifo(std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend);
    bool handleMessage(const Message& cmd);

    int m_steeringDegrees;
    SampleMOFifo m_sampleMOFifo;
    std::vector<SampleVector::iterator> m_vbegin;
    int m_sizes[2];
    UpSampleChannelizer *m_channelizers[2];
    BeamSteeringCWModStreamSource m_streamSources[2];
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    QMutex m_mutex;
    unsigned int m_lastStream;

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_BEAMSTEERINGCWMODSOURCE_H