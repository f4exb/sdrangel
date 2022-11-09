///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Vort                                                       //
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

#ifndef _KIWISDR_KIWISDRGUI_H_
#define _KIWISDR_KIWISDRGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "kiwisdrsettings.h"
#include "kiwisdrinput.h"

class DeviceUISet;

namespace Ui {
	class KiwiSDRGui;
}

class KiwiSDRGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit KiwiSDRGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~KiwiSDRGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::KiwiSDRGui* ui;

	KiwiSDRSettings m_settings;
    QList<QString> m_settingsKeys;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
	bool m_doApplySettings;
    bool m_forceSettings;
	DeviceSampleSource* m_sampleSource;
	std::size_t m_tickCount;
    int m_deviceSampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;
	std::vector<QString> m_statusColors;
	std::vector<QString> m_statusTooltips;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void sendSettings();
    void updateSampleRateAndFrequency();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
	void on_gain_valueChanged(int value);
	void on_agc_toggled(bool checked);
	void on_serverAddress_returnPressed();
	void on_serverAddressApplyButton_clicked();
  	void on_dcBlock_toggled(bool checked);
	void openDeviceSettingsDialog(const QPoint& p);
    void updateStatus();
    void updateHardware();
};

#endif // _KIWISDR_KIWISDRGUI_H_
