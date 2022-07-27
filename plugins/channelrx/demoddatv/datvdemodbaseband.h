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

#ifndef INCLUDE_DATVDEMODBASEBAND_H
#define INCLUDE_DATVDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "datvdemodsettings.h"
#include "datvdemodsink.h"

class DownChannelizer;

class DATVDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureDATVDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DATVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDATVDemodBaseband* create(const DATVDemodSettings& settings, bool force)
        {
            return new MsgConfigureDATVDemodBaseband(settings, force);
        }

    private:
        DATVDemodSettings m_settings;
        bool m_force;

        MsgConfigureDATVDemodBaseband(const DATVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    DATVDemodBaseband();
    ~DATVDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    double getMagSq() const { return m_sink->getMagSq(); }
    void setTVScreen(TVScreen *tvScreen) { m_sink->setTVScreen(tvScreen); }
    float getMERAvg() const { return m_sink->getMERAvg(); }
    float getMERRMS() const { return m_sink->getMERRMS(); }
    float getMERPeak() const { return m_sink->getMERPeak(); }
    int getMERNbAvg() const { return m_sink->getMERNbAvg(); }
    float getCNRAvg() const { return m_sink->getCNRAvg(); }
    float getCNRRMS() const { return m_sink->getCNRRMS(); }
    float getCNRPeak() const { return m_sink->getCNRPeak(); }
    int getCNRNbAvg() const { return m_sink->getCNRNbAvg(); }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_sink->setMessageQueueToGUI(messageQueue); }
    void setBasebandSampleRate(int sampleRate); //!< To be used when supporting thread is stopped
    void SetVideoRender(DATVideoRender *objScreen) { m_sink->SetVideoRender(objScreen); }
    DATVideostream *getVideoStream() { return m_sink->getVideoStream(); }
    DATVUDPStream *getUDPStream() { return m_sink->getUDPStream(); }
    bool audioActive() { return m_sink->audioActive(); }
    bool audioDecodeOK() { return m_sink->audioDecodeOK(); }
    bool videoActive() { return m_sink->videoActive(); }
    bool videoDecodeOK() { return m_sink->videoDecodeOK(); }
    bool udpRunning() { return m_sink->udpRunning(); }
    bool playVideo() { return m_sink->playVideo(); }

    int getModcodModulation() const { return m_sink->getModcodModulation(); }
    int getModcodCodeRate() const { return m_sink->getModcodCodeRate(); }
    bool isCstlnSetByModcod() const { return m_sink->isCstlnSetByModcod(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink->setAudioFifoLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    DATVDemodSink *m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    DATVDemodSettings m_settings;
    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const DATVDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_CHANNELANALYZERBASEBAND_H
