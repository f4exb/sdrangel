///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_AIRSPYHFGUI_H
#define INCLUDE_AIRSPYHFGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "airspyhfinput.h"

class DeviceUISet;

namespace Ui {
	class AirspyHFGui;
	class AirspyHFSampleRates;
}

class AirspyHFGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit AirspyHFGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~AirspyHFGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	uint32_t getDevSampleRate(unsigned int index);
	int getDevSampleRateIndex(uint32_t sampleRate);

private:
	Ui::AirspyHFGui* ui;

	bool m_doApplySettings;
	bool m_forceSettings;
	AirspyHFSettings m_settings;
    QList<QString> m_settingsKeys;
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	std::vector<uint32_t> m_rates;
	DeviceSampleSource* m_sampleSource;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void displaySampleRates();
	void displayAGC();
	void sendSettings();
    void updateSampleRateAndFrequency();
    void updateFrequencyLimits();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
	void on_centerFrequency_changed(quint64 value);
    void on_LOppm_valueChanged(int value);
    void on_resetLOppm_clicked();
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_sampleRate_currentIndexChanged(int index);
	void on_decim_currentIndexChanged(int index);
	void on_startStop_toggled(bool checked);
    void on_transverter_clicked();
    void on_band_currentIndexChanged(int index);
	void on_dsp_toggled(bool checked);
	void on_lna_toggled(bool checked);
	void on_agc_currentIndexChanged(int index);
	void on_att_currentIndexChanged(int index);
	void updateHardware();
    void updateStatus();
	void handleInputMessages();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_AIRSPYHFGUI_H
