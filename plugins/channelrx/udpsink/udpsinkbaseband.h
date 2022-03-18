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

#ifndef INCLUDE_UDPSINKBASEBAND_H
#define INCLUDE_UDPSINKBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "udpsinksink.h"

class DownChannelizer;

class UDPSinkBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureUDPSinkBaseband : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const UDPSinkSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSinkBaseband* create(const UDPSinkSettings& settings, bool force)
        {
            return new MsgConfigureUDPSinkBaseband(settings, force);
        }

    private:
        UDPSinkSettings m_settings;
        bool m_force;

        MsgConfigureUDPSinkBaseband(const UDPSinkSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        {
        }
    };

    class MsgEnableSpectrum : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        bool getEnabled() { return m_enable; }

        static MsgEnableSpectrum* create(bool enable) {
            return new MsgEnableSpectrum(enable);
        }

    private:
        bool m_enable;

        MsgEnableSpectrum(bool enable) :
            Message(),
            m_enable(enable)
        {}
    };

    UDPSinkBaseband();
    ~UDPSinkBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void setBasebandSampleRate(int sampleRate);

	void setSpectrum(BasebandSampleSink* spectrum) { m_sink.setSpectrum(spectrum); }
    void enableSpectrum(bool enable) { m_sink.enableSpectrum(enable); }
    void setSpectrumPositiveOnly(bool positiveOnly) { m_sink.setSpectrumPositiveOnly(positiveOnly); }
	double getMagSq() const { return m_sink.getMagSq(); }
	double getInMagSq() const { return m_sink.getInMagSq(); }
	bool getSquelchOpen() const { return m_sink.getSquelchOpen(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    UDPSinkSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    UDPSinkSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const UDPSinkSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_UDPSINKBASEBAND_H
