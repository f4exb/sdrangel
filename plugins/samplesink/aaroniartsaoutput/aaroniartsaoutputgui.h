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

#ifndef INCLUDE_AARONIARTSAOUTPUTGUI_H
#define INCLUDE_AARONIARTSAOUTPUTGUI_H

#include <QTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "aaroniartsaoutput.h"

class DeviceUISet;
class QNetworkReply;
class QJsonObject;

namespace Ui {
	class AaroniaRTSAOutputGui;
}

class AaroniaRTSAOutputGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit AaroniaRTSAOutputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~AaroniaRTSAOutputGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::AaroniaRTSAOutputGui* ui;

    AaroniaRTSAOutputSettings m_settings;        //!< current settings
    QList<QString> m_settingsKeys;
	AaroniaRTSAOutput* m_sampleSink;
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

	std::vector<QString> m_statusColors;
	std::vector<QString> m_statusTooltips;

    void blockApplySettings(bool block);
	void displaySettings();
    void sendSettings();
	void updateSampleRateAndFrequency();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
	void on_serverAddress_returnPressed();
	void on_serverAddressApplyButton_clicked();
    void updateHardware();
	void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_AARONIARTSAOUTPUTGUI_H
