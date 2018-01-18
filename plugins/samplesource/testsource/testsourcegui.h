///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef _TESTSOURCE_TESTSOURCEGUI_H_
#define _TESTSOURCE_TESTSOURCEGUI_H_

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "testsourcesettings.h"
#include "testsourceinput.h"

class DeviceUISet;

namespace Ui {
	class TestSourceGui;
}

class TestSourceGui : public QWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	explicit TestSourceGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~TestSourceGui();
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
	Ui::TestSourceGui* ui;

	DeviceUISet* m_deviceUISet;
	TestSourceSettings m_settings;
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

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void sendSettings();
    void updateSampleRateAndFrequency();
    void displayAmplitude();
    void updateAmpCoarseLimit();
    void updateAmpFineLimit();
    void updateFrequencyShiftLimit();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_frequencyShift_changed(qint64 value);
    void on_decimation_currentIndexChanged(int index);
    void on_fcPos_currentIndexChanged(int index);
    void on_sampleRate_changed(quint64 value);
    void on_sampleSize_currentIndexChanged(int index);
    void on_amplitudeCoarse_valueChanged(int value);
    void on_amplitudeFine_valueChanged(int value);
    void on_dcBias_valueChanged(int value);
    void on_iBias_valueChanged(int value);
    void on_qBias_valueChanged(int value);
    void on_record_toggled(bool checked);
    void updateStatus();
    void updateHardware();
};

#endif // _TESTSOURCE_TESTSOURCEGUI_H_
