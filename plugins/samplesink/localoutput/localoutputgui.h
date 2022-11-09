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

#ifndef INCLUDE_LOCALOUTPUTGUI_H
#define INCLUDE_LOCALOUTPUTGUI_H

#include <QTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "localoutput.h"

class DeviceUISet;
class QNetworkReply;
class QJsonObject;

namespace Ui {
	class LocalOutputGui;
}

class LocalOutputGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit LocalOutputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~LocalOutputGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::LocalOutputGui* ui;

    LocalOutputSettings m_settings;        //!< current settings
    QList<QString> m_settingsKeys;
	LocalOutput* m_sampleSink;
    bool m_acquisition;
    int m_streamSampleRate;          //!< Sample rate of received stream
    quint64 m_streamCenterFrequency; //!< Center frequency of received stream
	QTimer m_updateTimer;
	QTimer m_statusTimer;
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;

	int m_samplesCount;
	std::size_t m_tickCount;

	bool m_doApplySettings;
    bool m_forceSettings;

    QPalette m_paletteGreenText;
    QPalette m_paletteWhiteText;

    void blockApplySettings(bool block);
	void displaySettings();
    void sendSettings();
	void updateSampleRateAndFrequency();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
    void updateHardware();
	void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_LOCALOUTPUTGUI_H
