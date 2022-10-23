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

#ifndef INCLUDE_FCDPROGUI_H
#define INCLUDE_FCDPROGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "fcdproinput.h"

class QWidget;
class DeviceUISet;

namespace Ui {
	class FCDProGui;
}

class FCDProGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit FCDProGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~FCDProGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::FCDProGui* ui;

	bool m_doApplySettings;
	bool m_forceSettings;
	FCDProSettings m_settings;
    QList<QString> m_settingsKeys;
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	std::vector<int> m_gains;
	DeviceSampleSource* m_sampleSource;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void sendSettings();
	void updateSampleRateAndFrequency();
    void updateFrequencyLimits();
	bool handleMessage(const Message& message);
    void makeUIConnections();

protected:
    void resizeEvent(QResizeEvent* size);

private slots:
    void handleInputMessages();
	void on_centerFrequency_changed(quint64 value);
	void on_ppm_valueChanged(int value);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	// TOOD: defaults push button
	void on_lnaGain_currentIndexChanged(int index);
	void on_rfFilter_currentIndexChanged(int index);
	void on_lnaEnhance_currentIndexChanged(int index);
	void on_band_currentIndexChanged(int index);
	void on_mixGain_currentIndexChanged(int index);
	void on_mixFilter_currentIndexChanged(int index);
	void on_bias_currentIndexChanged(int index);
	void on_mode_currentIndexChanged(int index);
	void on_gain1_currentIndexChanged(int index);
	void on_rcFilter_currentIndexChanged(int index);
	void on_gain2_currentIndexChanged(int index);
	void on_gain3_currentIndexChanged(int index);
	void on_gain4_currentIndexChanged(int index);
	void on_ifFilter_currentIndexChanged(int index);
	void on_gain5_currentIndexChanged(int index);
	void on_gain6_currentIndexChanged(int index);
	void on_decim_currentIndexChanged(int index);
	void on_fcPos_currentIndexChanged(int index);
	void on_setDefaults_clicked(bool checked);
	void on_startStop_toggled(bool checked);
    void on_transverter_clicked();
	void updateHardware();
	void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_FCDPROGUI_H
