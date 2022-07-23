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

#ifndef INCLUDE_DSDDEMODBASEBAND_H
#define INCLUDE_DSDDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/downchannelizer.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "dsddemodsink.h"

class ChannelAPI;
class Feature;

class DSDDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureDSDDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DSDDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDSDDemodBaseband* create(const DSDDemodSettings& settings, bool force)
        {
            return new MsgConfigureDSDDemodBaseband(settings, force);
        }

    private:
        DSDDemodSettings m_settings;
        bool m_force;

        MsgConfigureDSDDemodBaseband(const DSDDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    DSDDemodBaseband();
    ~DSDDemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    int getAudioSampleRate() const { return m_sink.getAudioSampleRate(); }
    double getMagSq() { return m_sink.getMagSq(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    bool getSquelchOpen() const { return m_sink.getSquelchOpen(); }
    void setBasebandSampleRate(int sampleRate);
	void setScopeXYSink(BasebandSampleSink* scopeSink) { m_sink.setScopeXYSink(scopeSink); }
	void configureMyPosition(float myLatitude, float myLongitude) { m_sink.configureMyPosition(myLatitude, myLongitude); }
   	const DSDDecoder& getDecoder() const { return m_sink.getDecoder(); }
    const char *updateAndGetStatusText() { return m_sink.updateAndGetStatusText(); }
    void setChannel(ChannelAPI *channel);
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }
    void setAMBEFeature(Feature *ambeFeature) { m_sink.setAmbeFeature(ambeFeature); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer m_channelizer;
    int m_channelSampleRate;
    DSDDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    DSDDemodSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const DSDDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_DSDDEMODBASEBAND_H
