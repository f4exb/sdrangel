///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_M17DEMODBASEBAND_H
#define INCLUDE_M17DEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "m17demodsink.h"

class DownChannelizer;
class ChannelAPI;

class M17DemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureM17DemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const M17DemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureM17DemodBaseband* create(const M17DemodSettings& settings, bool force)
        {
            return new MsgConfigureM17DemodBaseband(settings, force);
        }

    private:
        M17DemodSettings m_settings;
        bool m_force;

        MsgConfigureM17DemodBaseband(const M17DemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    M17DemodBaseband();
    ~M17DemodBaseband();
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
    void setChannel(ChannelAPI *channel);
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }

    void getDiagnostics(
        bool& dcd,
        float& evm,
        float& deviation,
        float& offset,
        int& status,
        float& clock,
        int& sampleIndex,
        int& syncIndex,
        int& clockIndex,
        int& viterbiCost
    ) const
    {
        m_sink.getDiagnostics(dcd, evm, deviation, offset, status, clock, sampleIndex, syncIndex, clockIndex, viterbiCost);
    }

    uint32_t getLSFCount() const { return m_sink.getLSFCount(); }
    const QString& getSrcCall() const { return m_sink.getSrcCall(); }
    const QString& getDestcCall() const { return m_sink.getDestcCall(); }
    const QString& getTypeInfo() const { return m_sink.getTypeInfo(); }
    uint16_t getCRC() const { return m_sink.getCRC(); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    int m_channelSampleRate;
    M17DemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    M17DemodSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const M17DemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_M17DEMODBASEBAND_H
