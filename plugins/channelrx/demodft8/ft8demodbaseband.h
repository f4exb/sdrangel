///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FT8DEMODBASEBAND_H
#define INCLUDE_FT8DEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>
#include <QDateTime>

#include "dsp/samplesinkfifo.h"
#include "dsp/downchannelizer.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "ft8demodsettings.h"
#include "ft8demodsink.h"
#include "ft8buffer.h"

class ChannelAPI;
class SpectrumVis;
class QThread;
class FT8DemodWorker;

class FT8DemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureFT8DemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FT8DemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFT8DemodBaseband* create(const FT8DemodSettings& settings, bool force)
        {
            return new MsgConfigureFT8DemodBaseband(settings, force);
        }

    private:
        FT8DemodSettings m_settings;
        bool m_force;

        MsgConfigureFT8DemodBaseband(const FT8DemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    FT8DemodBaseband();
    ~FT8DemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
	void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumVis = spectrumSink; m_sink.setSpectrumSink(spectrumSink); }
    double getMagSq() const { return m_sink.getMagSq(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    bool getAudioActive() const { return m_sink.getAudioActive(); }
    void setBasebandSampleRate(int sampleRate);
    void setMessageQueueToGUI(MessageQueue *messageQueue);
    void setChannel(ChannelAPI *channel);
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of samples analyzed
	 */
	void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);
    void bufferReady(int16_t *buffer, QDateTime periodTS);

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer m_channelizer;
    FT8DemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    FT8DemodSettings m_settings;
    int m_channelSampleRate;
    MessageQueue *m_messageQueueToGUI;
    SpectrumVis *m_spectrumVis;
    FT8Buffer m_ft8Buffer;
    int m_tickCount;
    QThread *m_workerThread;
    FT8DemodWorker *m_ft8DemodWorker;
    int16_t *m_ft8WorkerBuffer;
    qint64 m_deviceCenterFrequency;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const FT8DemodSettings& settings, bool force = false);
    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
    void tick();
};

#endif // INCLUDE_SSBDEMODBASEBAND_H
