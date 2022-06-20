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

#ifndef INCLUDE_BFMDEMODBASEBAND_H
#define INCLUDE_BFMDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "bfmdemodsink.h"

class DownChannelizer;
class SpectrumVis;

class BFMDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureBFMDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const BFMDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureBFMDemodBaseband* create(const BFMDemodSettings& settings, bool force)
        {
            return new MsgConfigureBFMDemodBaseband(settings, force);
        }

    private:
        BFMDemodSettings m_settings;
        bool m_force;

        MsgConfigureBFMDemodBaseband(const BFMDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    BFMDemodBaseband();
    ~BFMDemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void setBasebandSampleRate(int sampleRate);
    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumVis = spectrumSink; m_sink.setSpectrumSink((BasebandSampleSink*) spectrumSink); }
    void setChannel(ChannelAPI *channel);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }

    int getAudioSampleRate() const { return m_sink.getAudioSampleRate(); }
    int getSquelchState() const { return m_sink.getSquelchState(); }
	double getMagSq() const { return m_sink.getMagSq(); }
	bool getPilotLock() const { return m_sink.getPilotLock(); }
	Real getPilotLevel() const { return m_sink.getPilotLevel(); }
	Real getDecoderQua() const { return m_sink.getDecoderQua(); }
	bool getDecoderSynced() const { return m_sink.getDecoderSynced(); }
	Real getDemodAcc() const { return m_sink.getDemodAcc(); }
	Real getDemodQua() const { return m_sink.getDemodQua(); }
	Real getDemodFclk() const { return m_sink.getDemodFclk(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    RDSParser& getRDSParser() { return m_sink.getRDSParser(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    int m_channelSampleRate;
    BFMDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    BFMDemodSettings m_settings;
    QMutex m_mutex;
    MessageQueue *m_messageQueueToGUI;
    SpectrumVis *m_spectrumVis;

    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }

    bool handleMessage(const Message& cmd);
    void applySettings(const BFMDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_BFMDEMODBASEBAND_H
