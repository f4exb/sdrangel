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
#include <QTimer>

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

    class MsgReportFrequency : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint64_t getFrequency() const { return m_frequency; }

        static MsgReportFrequency* create(uint64_t frequency) {
            return new MsgReportFrequency(frequency);
        }

    protected:
        uint64_t m_frequency; //!< Frequency in Hz

        MsgReportFrequency(uint64_t frequency) :
            Message(),
            m_frequency(frequency)
        { }
    };

    AudioCATSISOCATWorker(QObject* parent = nullptr);
    ~AudioCATSISOCATWorker();

    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_inputMessageQueueToGUI = queue; }
    void setMessageQueueToSISO(MessageQueue *queue) { m_inputMessageQueueToSISO = queue; }

private:
    void applySettings(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force);
    bool handleMessage(const Message& message);
    void catConnect();
    void catDisconnect();
    void catPTT(bool ptt);
    void catSetFrequency(uint64_t frequency);

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_inputMessageQueueToGUI;
    MessageQueue *m_inputMessageQueueToSISO;
    bool m_running;
    bool m_connected;
    AudioCATSISOSettings m_settings;
    RIG *m_rig;
    QTimer m_pollTimer;
    bool m_ptt;
    uint64_t m_frequency;

private slots:
    void handleInputMessages();
    void pollingTick();
};


#endif // INCLUDE_AUDIOCATSISOCATWORKER_H
