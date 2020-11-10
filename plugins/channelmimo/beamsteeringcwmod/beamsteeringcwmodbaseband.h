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

#ifndef INCLUDE_BEAMSTEERINGCWMODBASEBAND_H
#define INCLUDE_BEAMSTEERINGCWMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplemofifo.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "beamsteeringcwmodstreamsource.h"
#include "beamsteeringcwmodsettings.h"

class UpChannelizer;

class BeamSteeringCWModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureBeamSteeringCWModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const BeamSteeringCWModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureBeamSteeringCWModBaseband* create(const BeamSteeringCWModSettings& settings, bool force)
        {
            return new MsgConfigureBeamSteeringCWModBaseband(settings, force);
        }

    private:
        BeamSteeringCWModSettings m_settings;
        bool m_force;

        MsgConfigureBeamSteeringCWModBaseband(const BeamSteeringCWModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };
    class MsgSignalNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getBasebandSampleRate() const { return m_basebandSampleRate; }

        static MsgSignalNotification* create(int basebandSampleRate) {
            return new MsgSignalNotification(basebandSampleRate);
        }
    private:
        int m_basebandSampleRate;

        MsgSignalNotification(int basebandSampleRate) :
            Message(),
            m_basebandSampleRate(basebandSampleRate)
        { }
    };

    BeamSteeringCWModBaseband();
    ~BeamSteeringCWModBaseband();
    void reset();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication

	void pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int streamIndex);

private:
    void processFifo(std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend);
    bool handleMessage(const Message& cmd);
    void applySettings(const BeamSteeringCWModSettings& settings, bool force = false);

    BeamSteeringCWModSettings m_settings;
    SampleMOFifo m_sampleMOFifo;
    std::vector<SampleVector::iterator> m_vbegin;
    int m_sizes[2];
    UpChannelizer *m_channelizers[2];
    BeamSteeringCWModStreamSource m_streamSources[2];
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    QMutex m_mutex;
    unsigned int m_lastStream;

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_BEAMSTEERINGCWMODBASEBAND_H