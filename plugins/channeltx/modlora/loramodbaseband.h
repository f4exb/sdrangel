///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_LORAMODBASEBAND_H
#define INCLUDE_LORAMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "loramodsource.h"

class UpChannelizer;

class LoRaModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureLoRaModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LoRaModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLoRaModBaseband* create(const LoRaModSettings& settings, bool force)
        {
            return new MsgConfigureLoRaModBaseband(settings, force);
        }

    private:
        LoRaModSettings m_settings;
        bool m_force;

        MsgConfigureLoRaModBaseband(const LoRaModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureLoRaModPayload : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<unsigned short>& getPayload() const { return m_payload; }

        static MsgConfigureLoRaModPayload* create() {
            return new MsgConfigureLoRaModPayload();
        }
        static MsgConfigureLoRaModPayload* create(const std::vector<unsigned short>& payload) {
            return new MsgConfigureLoRaModPayload(payload);
        }

    private:
        std::vector<unsigned short> m_payload;

        MsgConfigureLoRaModPayload() : // This is empty payload notification
            Message()
        {}
        MsgConfigureLoRaModPayload(const std::vector<unsigned short>& payload) :
            Message()
        { m_payload = payload; }
    };

    LoRaModBaseband();
    ~LoRaModBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    bool getActive() const { return m_source.getActive(); }

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
    LoRaModSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    LoRaModSettings m_settings;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const LoRaModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_NFMMODBASEBAND_H
