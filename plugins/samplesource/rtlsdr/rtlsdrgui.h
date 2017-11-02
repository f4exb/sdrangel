///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "rtlsdrinput.h"

class DeviceUISet;

namespace Ui {
	class RTLSDRGui;
	class RTLSDRSampleRates;
}

class RTLSDRGui : public QWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	explicit RTLSDRGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~RTLSDRGui();
	virtual void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
	virtual bool handleMessage(const Message& message);

private:
	Ui::RTLSDRGui* ui;

	DeviceUISet* m_deviceUISet;
	bool m_forceSettings;
	RTLSDRSettings m_settings;
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	std::vector<int> m_gains;
	DeviceSampleSource* m_sampleSource;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void displayGains();
	void displaySettings();
	void sendSettings();
	void updateSampleRateAndFrequency();
	void updateFrequencyLimits();

private slots:
    void handleInputMessages();
	void on_centerFrequency_changed(quint64 value);
	void on_sampleRate_changed(quint64 value);
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
    void on_record_toggled(bool checked);
    void on_transverter_clicked();
	void updateHardware();
	void updateStatus();
};

#endif // INCLUDE_RTLSDRGUI_H
