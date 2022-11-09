///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_FILEINPUTGUI_H
#define INCLUDE_FILEINPUTGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "fileinputsettings.h"
#include "fileinput.h"

class DeviceUISet;

namespace Ui {
	class FileInputGUI;
}

class FileInputGUI : public DeviceGUI {
	Q_OBJECT

public:
	explicit FileInputGUI(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~FileInputGUI();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
	virtual bool handleMessage(const Message& message);

private:
	Ui::FileInputGUI* ui;

	FileInputSettings m_settings;
    QList<QString> m_settingsKeys;
	bool m_doApplySettings;
	QTimer m_statusTimer;
	std::vector<int> m_gains;
	DeviceSampleSource* m_sampleSource;
    bool m_acquisition;
	int m_sampleRate;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
    quint64 m_recordLengthMuSec;
    quint64 m_startingTimeStamp;
    quint64 m_samplesCount;
	std::size_t m_tickCount;
	bool m_enableNavTime;
    int m_deviceSampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void displayTime();
	void sendSettings();
    void updateSampleRateAndFrequency();
    void checkLPF();
	void configureFileName();
	void updateWithAcquisition();
	void updateWithStreamData();
	void updateWithStreamTime();
    void setAccelerationCombo();
    void setNumberStr(int n, QString& s);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_playLoop_toggled(bool checked);
	void on_play_toggled(bool checked);
	void on_navTimeSlider_valueChanged(int value);
	void on_showFileDialog_clicked(bool checked);
	void on_acceleration_currentIndexChanged(int index);
    void updateStatus();
	void tick();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_FILEINPUTGUI_H
