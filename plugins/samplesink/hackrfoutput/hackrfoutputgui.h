///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_HACKRFOUTPUTGUI_H
#define INCLUDE_HACKRFOUTPUTGUI_H

#include <QTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "hackrfoutput.h"

#define HACKRF_MAX_DEVICE (32)

class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
	class HackRFOutputGui;
}

class HackRFOutputGui : public DeviceGUI {
	Q_OBJECT

public:
	typedef enum
	{
		HACKRF_IMGREJ_BYPASS = 0,
		HACKRF_IMGREJ_LOWPASS,
		HACKRF_IMGREJ_HIGHPASS,
		HACKRF_IMGREJ_NB
	} HackRFImgRejValue;

	explicit HackRFOutputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~HackRFOutputGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::HackRFOutputGui* ui;

	bool m_forceSettings;
	HackRFOutputSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_sampleRateMode; //!< true: device, false: base band sample rate update mode
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	DeviceSampleSink* m_deviceSampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;
    bool m_doApplySettings;

	void displaySettings();
    void displaySampleRate();
    void displayFcTooltip();
	void displayBandwidths();
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
	void on_LOppm_valueChanged(int value);
	void on_biasT_stateChanged(int state);
	void on_interp_currentIndexChanged(int index);
	void on_fcPos_currentIndexChanged(int index);
	void on_lnaExt_stateChanged(int state);
	void on_bbFilter_currentIndexChanged(int index);
	void on_txvga_valueChanged(int value);
	void on_startStop_toggled(bool checked);
    void on_sampleRateMode_toggled(bool checked);
    void on_transverter_clicked();
	void updateHardware();
	void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_HACKRFOUTPUTGUI_H
