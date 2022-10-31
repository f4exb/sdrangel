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

#ifndef INCLUDE_TESTSINKGUI_H
#define INCLUDE_TESTSINKGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "testsinkoutput.h"
#include "testsinksettings.h"


class TestSinkOutput;
class DeviceUISet;
class SpectrumVis;

namespace Ui {
	class TestSinkGui;
}

class TestSinkGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit TestSinkGui(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~TestSinkGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::TestSinkGui* ui;

	bool m_doApplySettings;
	bool m_forceSettings;
	TestSinkSettings m_settings;
    QList<QString> m_settingsKeys;
	QTimer m_updateTimer;
    QTimer m_statusTimer;
	TestSinkOutput* m_sampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    bool m_generation;
	int m_samplesCount;
	std::size_t m_tickCount;
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;
    SpectrumVis* m_spectrumVis;
    static const int m_MinimumWidth = 360;
    static const int m_MinimumHeight = 200 + 20 + 10 + 4*22 + 5;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void sendSettings();
	void updateSampleRateAndFrequency();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
	void on_startStop_toggled(bool checked);
	void on_interp_currentIndexChanged(int index);
    void openDeviceSettingsDialog(const QPoint& p);
    void updateHardware();
    void updateStatus();
	void tick();
};

#endif // INCLUDE_TESTSINKGUI_H
