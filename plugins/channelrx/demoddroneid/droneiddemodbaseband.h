///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DRONEIDDEMODBASEBAND_H
#define INCLUDE_DRONEIDDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>
#include <QThread>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "droneiddemodsink.h"

class DownChannelizer;
class ChannelAPI;
class DroneIDDemod;
class DroneIDDemodWorker;

class DroneIDDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureDroneIDDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DroneIDDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDroneIDDemodBaseband* create(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force)
        {
            return new MsgConfigureDroneIDDemodBaseband(settingsKeys, settings, force);
        }

    private:
        DroneIDDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDroneIDDemodBaseband(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    DroneIDDemodBaseband(DroneIDDemod *droneIDDemod);
    ~DroneIDDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_sink.getMagSqLevels(avg, peak, nbSamples);
    }
    int getChannelSampleRate() const;
    void setMessageQueueToChannel(MessageQueue *messageQueue);
    void setBasebandSampleRate(int sampleRate);
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    int getBurstsDetected() const { return m_sink.getBurstsDetected(); }
    int getBurstsDropped() const { return m_sink.getBurstsDropped(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    DroneIDDemodSink m_sink;
    DroneIDDemodWorker *m_worker;
    QThread m_workerThread;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    DroneIDDemodSettings m_settings;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_DRONEIDDEMODBASEBAND_H
