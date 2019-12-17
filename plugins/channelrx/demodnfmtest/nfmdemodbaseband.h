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

#ifndef INCLUDE_NFMDEMODBASEBAND_H
#define INCLUDE_NFMDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "nfmdemodsink.h"

class DownSampleChannelizer;

class NFMDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureNFMDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const NFMDemodTestSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureNFMDemodBaseband* create(const NFMDemodTestSettings& settings, bool force)
        {
            return new MsgConfigureNFMDemodBaseband(settings, force);
        }

    private:
        NFMDemodTestSettings m_settings;
        bool m_force;

        MsgConfigureNFMDemodBaseband(const NFMDemodTestSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    NFMDemodBaseband();
    ~NFMDemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    void setSelectedCtcssIndex(int selectedCtcssIndex) { m_sink.setSelectedCtcssIndex(selectedCtcssIndex); }
    bool getSquelchOpen() const { return m_sink.getSquelchOpen(); }
    const Real *getCtcssToneSet(int& nbTones) const { return m_sink.getCtcssToneSet(nbTones); }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_sink.setMessageQueueToGUI(messageQueue); }
    unsigned int getAudioSampleRate() const { return m_sink.getAudioSampleRate(); }
    void setBasebandSampleRate(int sampleRate);

private:
    SampleSinkFifo m_sampleFifo;
    DownSampleChannelizer *m_channelizer;
    NFMDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    NFMDemodTestSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const NFMDemodTestSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_NFMDEMODBASEBAND_H