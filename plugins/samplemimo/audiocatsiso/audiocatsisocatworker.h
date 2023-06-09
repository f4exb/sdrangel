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

#ifndef INCLUDE_AUDIOCATSISOCATWORKER_H
#define INCLUDE_AUDIOCATSISOCATWORKER_H

#include <hamlib/rig.h>

#include <QObject>
#include "util/message.h"
#include "util/messagequeue.h"
#include "audiocatsisosettings.h"

class AudioCATSISOCATWorker : public QObject {
    Q_OBJECT

public:
	class MsgConfigureAudioCATSISOCATWorker : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const AudioCATSISOSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureAudioCATSISOCATWorker* create(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureAudioCATSISOCATWorker(settings, settingsKeys, force);
		}

	private:
		AudioCATSISOSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureAudioCATSISOCATWorker(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force) :
			Message(),
			m_settings(settings),
            m_settingsKeys(settingsKeys),
			m_force(force)
		{ }
	};

    AudioCATSISOCATWorker(QObject* parent = nullptr);
    ~AudioCATSISOCATWorker();

    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_inputMessageQueueToGUI = queue; }

private:
    void applySettings(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force);
    bool handleMessage(const Message& message);
    void catConnect();
    void catDisconnect();

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_inputMessageQueueToGUI;
    bool m_running;
    bool m_connected;
    AudioCATSISOSettings m_settings;
    RIG *m_rig;

private slots:
    void handleInputMessages();
};


#endif // INCLUDE_AUDIOCATSISOCATWORKER_H
