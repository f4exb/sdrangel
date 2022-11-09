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

#ifndef _TESTMI_TESTMIGUI_H_
#define _TESTMI_TESTMIGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "testmisettings.h"
#include "testmi.h"

class DeviceUISet;

namespace Ui {
	class TestMIGui;
}

class TestMIGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit TestMIGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~TestMIGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::TestMIGui* ui;

	TestMISettings m_settings;
    int m_streamIndex; //!< Current stream index being dealt with
    int m_spectrumStreamIndex; //!< Index of the stream displayed on main spectrum
    QTimer m_updateTimer;
    QTimer m_statusTimer;
	bool m_doApplySettings;
    bool m_forceSettings;
	DeviceSampleMIMO* m_sampleMIMO;
	std::size_t m_tickCount;
    std::vector<int> m_deviceSampleRates;
    std::vector<quint64> m_deviceCenterFrequencies; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void sendSettings();
    void updateSampleRateAndFrequency();
    void displayAmplitude();
    void updateAmpCoarseLimit();
    void updateAmpFineLimit();
    void updateFrequencyShiftLimit();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
    void on_streamIndex_currentIndexChanged(int index);
    void on_spectrumSource_currentIndexChanged(int index);
    void on_streamLock_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_autoCorr_currentIndexChanged(int index);
    void on_frequencyShift_changed(qint64 value);
    void on_decimation_currentIndexChanged(int index);
    void on_fcPos_currentIndexChanged(int index);
    void on_sampleRate_changed(quint64 value);
    void on_sampleSize_currentIndexChanged(int index);
    void on_amplitudeCoarse_valueChanged(int value);
    void on_amplitudeFine_valueChanged(int value);
    void on_modulation_currentIndexChanged(int index);
    void on_modulationFrequency_valueChanged(int value);
    void on_amModulation_valueChanged(int value);
    void on_fmDeviation_valueChanged(int value);
    void on_dcBias_valueChanged(int value);
    void on_iBias_valueChanged(int value);
    void on_qBias_valueChanged(int value);
    void on_phaseImbalance_valueChanged(int value);
    void openDeviceSettingsDialog(const QPoint& p);
    void updateStatus();
    void updateHardware();
};

#endif // _TESTMI_TESTMIGUI_H_
