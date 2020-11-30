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

#ifndef INCLUDE_FEATURE_VORLOCALIZERWORKER_H_
#define INCLUDE_FEATURE_VORLOCALIZERWORKER_H_

#include <QObject>
#include <QTimer>

#include "util/message.h"
#include "util/messagequeue.h"

#include "vorlocalizersettings.h"

class WebAPIAdapterInterface;
class ChannelAPI;

class VorLocalizerWorker : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureVORLocalizerWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const VORLocalizerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureVORLocalizerWorker* create(const VORLocalizerSettings& settings, bool force)
        {
            return new MsgConfigureVORLocalizerWorker(settings, force);
        }

    private:
        VORLocalizerSettings m_settings;
        bool m_force;

        MsgConfigureVORLocalizerWorker(const VORLocalizerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgRefreshChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRefreshChannels* create() {
            return new MsgRefreshChannels();
        }

    protected:
        MsgRefreshChannels() :
            Message()
        { }
    };

    VorLocalizerWorker(WebAPIAdapterInterface *webAPIAdapterInterface);
    ~VorLocalizerWorker();
    void reset();
    bool startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_msgQueueToGUI = messageQueue; }

private:
    struct VORChannel
    {
        int m_subChannelId; //!< Unique VOR identifier (from database)
        int m_frequency;    //!< Frequency the VOR is on
        bool m_audioMute;   //!< Mute the audio from this VOR
    };

    struct AvailableChannel
    {
        int m_deviceSetIndex;
        int m_channelIndex;
        ChannelAPI *m_channelAPI;
    };

    WebAPIAdapterInterface *m_webAPIAdapterInterface;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToGUI; //!< Queue to report state to GUI
    VORLocalizerSettings m_settings;
    QList<VORChannel> m_vorChannels;
    QList<AvailableChannel> m_availableChannels;
    bool m_running;
	QTimer m_updateTimer;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const VORLocalizerSettings& settings, bool force = false);
    void updateChannels();
    void removeVORChannel(int navId);
    void addVORChannel(const VORLocalizerSubChannelSettings *subChannelSettings);

private slots:
    void handleInputMessages();
	void updateHardware();
};

#endif // INCLUDE_FEATURE_VORLOCALIZERWORKER_H_
