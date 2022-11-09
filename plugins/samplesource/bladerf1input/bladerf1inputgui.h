///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_BLADERFINPUTGUI_H
#define INCLUDE_BLADERFINPUTGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "bladerf1input.h"

class DeviceUISet;

namespace Ui {
	class Bladerf1InputGui;
}

class Bladerf1InputGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit Bladerf1InputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~Bladerf1InputGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::Bladerf1InputGui* ui;

	bool m_forceSettings;
	bool m_doApplySettings;
	BladeRF1InputSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_sampleRateMode; //!< true: device, false: base band sample rate update mode
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	std::vector<int> m_gains;
	DeviceSampleSource* m_sampleSource;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void displaySettings();
    void displaySampleRate();
    void displayFcTooltip();
	void sendSettings();
	unsigned int getXb200Index(bool xb_200, bladerf_xb200_path xb200Path, bladerf_xb200_filter xb200Filter);
	void updateSampleRateAndFrequency();
	void blockApplySettings(bool block);
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_bandwidth_currentIndexChanged(int index);
	void on_decim_currentIndexChanged(int index);
	void on_lna_currentIndexChanged(int index);
	void on_vga1_valueChanged(int value);
	void on_vga2_valueChanged(int value);
	void on_xb200_currentIndexChanged(int index);
	void on_fcPos_currentIndexChanged(int index);
	void on_startStop_toggled(bool checked);
    void on_sampleRateMode_toggled(bool checked);
	void updateHardware();
	void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_BLADERFINPUTGUI_H
