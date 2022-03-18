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

#ifndef INCLUDE_FREEDVDEMODBASEBAND_H
#define INCLUDE_FREEDVDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "freedvdemodsink.h"

class DownChannelizer;

class FreeDVDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureFreeDVDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreeDVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreeDVDemodBaseband* create(const FreeDVDemodSettings& settings, bool force)
        {
            return new MsgConfigureFreeDVDemodBaseband(settings, force);
        }

    private:
        FreeDVDemodSettings m_settings;
        bool m_force;

        MsgConfigureFreeDVDemodBaseband(const FreeDVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgResyncFreeDVDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        static MsgResyncFreeDVDemod* create() {
            return new MsgResyncFreeDVDemod();
        }

	private:
		MsgResyncFreeDVDemod()
		{}
	};

    FreeDVDemodBaseband();
    ~FreeDVDemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    int getAudioSampleRate() const { return m_sink.getAudioSampleRate(); }
    double getMagSq() { return m_sink.getMagSq(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    void setBasebandSampleRate(int sampleRate);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }

	void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_sink.setSpectrumSink(spectrumSink); }
    uint32_t getModemSampleRate() const { return m_sink.getModemSampleRate(); }
    double getMagSq() const { return m_sink.getMagSq(); }
	bool getAudioActive() const { return m_sink.getAudioActive(); }
	void getSNRLevels(double& avg, double& peak, int& nbSamples) { m_sink.getSNRLevels(avg, peak, nbSamples); }
	int getBER() const { return m_sink.getBER(); }
	float getFrequencyOffset() const { return m_sink.getFrequencyOffset(); }
	bool isSync() const { return m_sink.isSync(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of samples analyzed
	 */
	void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    FreeDVDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    FreeDVDemodSettings m_settings;
    QMutex m_mutex;
    MessageQueue *m_messageQueueToGUI;

    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }
    bool handleMessage(const Message& cmd);
    void applySettings(const FreeDVDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_DSDDEMODBASEBAND_H
