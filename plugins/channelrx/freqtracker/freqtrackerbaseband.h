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

#ifndef INCLUDE_FREQTRACKERBASEBAND_H
#define INCLUDE_FREQTRACKERBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "freqtrackersink.h"

class DownChannelizer;
class SpectrumVis;

class FreqTrackerBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureFreqTrackerBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreqTrackerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreqTrackerBaseband* create(const FreqTrackerSettings& settings, bool force)
        {
            return new MsgConfigureFreqTrackerBaseband(settings, force);
        }

    private:
        FreqTrackerSettings m_settings;
        bool m_force;

        MsgConfigureFreqTrackerBaseband(const FreqTrackerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    FreqTrackerBaseband();
    ~FreqTrackerBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumVis = spectrumSink; m_sink.setSpectrumSink(spectrumSink); }
    int getChannelSampleRate() const;
    void setBasebandSampleRate(int sampleRate);
    void setMessageQueueToInput(MessageQueue *messageQueue) { m_sink.setMessageQueueToInput(messageQueue); }

	double getMagSq() const { return m_sink.getMagSq(); }
    bool getSquelchOpen() const { return m_sink.getSquelchOpen(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    uint32_t getSampleRate() const { return m_sink.getSampleRate(); }
	bool getPllLocked() const { return m_sink.getPllLocked(); }
	Real getFrequency() const { return m_sink.getFrequency(); };
    Real getAvgDeltaFreq() const { return m_sink.getAvgDeltaFreq(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    FreqTrackerSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    FreqTrackerSettings m_settings;
    unsigned int m_basebandSampleRate;
    SpectrumVis *m_spectrumVis;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const FreqTrackerSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_FREQTRACKERBASEBAND_H
