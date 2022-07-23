///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_SIFMFFILESINKBASEBAND_H_
#define INCLUDE_SIFMFFILESINKBASEBAND_H_

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/downchannelizer.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "sigmffilesinksink.h"
#include "sigmffilesinksettings.h"

class QTimer;
class SpectrumVis;

class SigMFFileSinkBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureSigMFFileSinkBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SigMFFileSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSigMFFileSinkBaseband* create(const SigMFFileSinkSettings& settings, bool force)
        {
            return new MsgConfigureSigMFFileSinkBaseband(settings, force);
        }

    private:
        SigMFFileSinkSettings m_settings;
        bool m_force;

        MsgConfigureSigMFFileSinkBaseband(const SigMFFileSinkSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgConfigureSigMFFileSinkWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureSigMFFileSinkWork* create(bool working)
		{
			return new MsgConfigureSigMFFileSinkWork(working);
		}

	private:
		bool m_working;

		MsgConfigureSigMFFileSinkWork(bool working) :
			Message(),
			m_working(working)
		{ }
	};

    SigMFFileSinkBaseband();
    ~SigMFFileSinkBaseband();

    void reset();
    void startWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void setBasebandSampleRate(int sampleRate);
    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; m_sink.setSpectrumSink(spectrumSink); }
    uint64_t getMsCount() const { return m_sink.getMsCount(); }
    uint64_t getByteCount() const { return m_sink.getByteCount(); }
    unsigned int getNbTracks() const { return m_sink.getNbTracks(); }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; m_sink.setMessageQueueToGUI(messageQueue); }
    void setDeviceHwId(const QString& hwId) { m_sink.setDeviceHwId(hwId); }
    void setDeviceUId(int uid) { m_sink.setDeviceUId(uid); }
    bool isSquelchOpen() const { return m_squelchOpen; }
    bool isRecording() const { return m_sink.isRecording(); }
    float getSpecMax() const { return m_specMax; }
    int getSinkSampleRate() const { return m_sink.getSampleRate(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer m_channelizer;
    SigMFFileSinkSink m_sink;
    SpectrumVis *m_spectrumSink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_messageQueueToGUI;
    SigMFFileSinkSettings m_settings;
    float m_specMax; //!< Last max used for comparison
    float m_squelchLevel;
    bool m_squelchOpen;
    int64_t m_centerFrequency;
    QMutex m_mutex;
    QTimer *m_timer;

    void stopWork();
    bool handleMessage(const Message& cmd);
    void applySettings(const SigMFFileSinkSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
    void tick();
};


#endif // INCLUDE_SIFMFFILESINKBASEBAND_H_
