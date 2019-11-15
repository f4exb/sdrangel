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

#ifndef INCLUDE_UDPSOURCEBASEBAND_H
#define INCLUDE_UDPSOURCEBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "udpsourcesource.h"

class UpChannelizer;

class UDPSourceBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureUDPSourceBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSourceBaseband* create(const UDPSourceSettings& settings, bool force)
        {
            return new MsgConfigureUDPSourceBaseband(settings, force);
        }

    private:
        UDPSourceSettings m_settings;
        bool m_force;

        MsgConfigureUDPSourceBaseband(const UDPSourceSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSourceSampleRate() const { return m_sourceSampleRate; }
        int getSourceCenterFrequency() const { return m_sourceCenterFrequency; }

        static MsgConfigureChannelizer* create(int sourceSampleRate, int sourceCenterFrequency)
        {
            return new MsgConfigureChannelizer(sourceSampleRate, sourceCenterFrequency);
        }

    private:
        int m_sourceSampleRate;
        int m_sourceCenterFrequency;

        MsgConfigureChannelizer(int sourceSampleRate, int sourceCenterFrequency) :
            Message(),
            m_sourceSampleRate(sourceSampleRate),
            m_sourceCenterFrequency(sourceCenterFrequency)
        { }
    };

    class MsgUDPSourceSpectrum : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getEnabled() const { return m_enabled; }

        static MsgUDPSourceSpectrum* create(bool enabled)
        {
            return new MsgUDPSourceSpectrum(enabled);
        }

    private:
        bool m_enabled;

        MsgUDPSourceSpectrum(bool enabled) :
            Message(),
            m_enabled(enabled)
        { }
    };

    class MsgResetReadIndex : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgResetReadIndex* create()
        {
            return new MsgResetReadIndex();
        }

    private:

        MsgResetReadIndex() :
            Message()
        { }
    };

    UDPSourceBaseband();
    ~UDPSourceBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    double getMagSq() const { return m_source.getMagSq(); }
    double getInMagSq() const { return m_source.getInMagSq(); }
    int32_t getBufferGauge() const { return m_source.getBufferGauge(); }
    bool getSquelchOpen() const { return m_source.getSquelchOpen(); }
    int getChannelSampleRate() const;
    bool isSquelchOpen() const;
    void setSpectrumSink(BasebandSampleSink *spectrumSink) { m_source.setSpectrumSink(spectrumSink); }

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of audio samples analyzed
	 */
	void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    UDPSourceSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    UDPSourceSettings m_settings;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const UDPSourceSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_UDPSOURCEBASEBAND_H
