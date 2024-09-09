///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_CHANNELPOWERBASEBAND_H
#define INCLUDE_CHANNELPOWERBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "channelpowersink.h"

class DownChannelizer;
class ChannelAPI;
class ChannelPower;

class ChannelPowerBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureChannelPowerBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChannelPowerSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureChannelPowerBaseband* create(const ChannelPowerSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureChannelPowerBaseband(settings, settingsKeys, force);
        }

    private:
        ChannelPowerSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureChannelPowerBaseband(const ChannelPowerSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    ChannelPowerBaseband();
    ~ChannelPowerBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagLevels(double& avg, double& pulseAvg, double &maxPeak, double &minPeak) {
        m_sink.getMagLevels(avg, pulseAvg, maxPeak, minPeak);
    }
    void resetMagLevels() {
        m_sink.resetMagLevels();
    }
    void setBasebandSampleRate(int sampleRate);
    int getChannelSampleRate() const;
    void setChannel(ChannelAPI *channel);
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    ChannelPowerSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    ChannelPowerSettings m_settings;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const ChannelPowerSettings& settings, const QStringList& settingsKeys, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_CHANNELPOWERBASEBAND_H

