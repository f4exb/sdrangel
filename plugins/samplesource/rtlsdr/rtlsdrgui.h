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

#ifndef INCLUDE_RTLSDRGUI_H
#define INCLUDE_RTLSDRGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "rtlsdrsettings.h"
#include "rtlsdrinput.h"

class DeviceUISet;

namespace Ui {
	class RTLSDRGui;
	class RTLSDRSampleRates;
}

class RTLSDRGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit RTLSDRGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~RTLSDRGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::RTLSDRGui* ui;

    bool m_doApplySettings;
	bool m_forceSettings;
	RTLSDRSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_sampleRateMode; //!< true: device, false: base band sample rate update mode
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	RTLSDRInput* m_sampleSource;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	MessageQueue m_inputMessageQueue;

	void displayGains();
    void displaySampleRate();
    void displayFcTooltip();
	void displaySettings();
	void sendSettings();
	void updateSampleRateAndFrequency();
	void updateFrequencyLimits();
    void blockApplySettings(bool block);
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_centerFrequency_changed(quint64 value);
	void on_sampleRate_changed(quint64 value);
	void on_offsetTuning_toggled(bool checked);
    void on_rfBW_changed(quint64 value);
	void on_lowSampleRate_toggled(bool checked);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_decim_currentIndexChanged(int index);
	void on_fcPos_currentIndexChanged(int index);
	void on_ppm_valueChanged(int value);
	void on_gain_valueChanged(int value);
	void on_checkBox_stateChanged(int state);
    void on_agc_stateChanged(int state);
	void on_startStop_toggled(bool checked);
    void on_transverter_clicked();
    void on_sampleRateMode_toggled(bool checked);
    void on_biasT_stateChanged(int state);
    void openDeviceSettingsDialog(const QPoint& p);
	void updateHardware();
	void updateStatus();
};

#endif // INCLUDE_RTLSDRGUI_H
