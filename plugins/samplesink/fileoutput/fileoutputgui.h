///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FILEOUTPUTGUI_H
#define INCLUDE_FILEOUTPUTGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "fileoutput.h"
#include "fileoutputsettings.h"


class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
	class FileOutputGui;
}

class FileOutputGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit FileOutputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~FileOutputGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

protected:
    void resizeEvent(QResizeEvent* size);

private:
	Ui::FileOutputGui* ui;

	bool m_doApplySettings;
	bool m_forceSettings;
	FileOutputSettings m_settings;
    QList<QString> m_settingsKeys;
	QTimer m_updateTimer;
    QTimer m_statusTimer;
	DeviceSampleSink* m_deviceSampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    bool m_generation;
	std::time_t m_startingTimeStamp;
	int m_samplesCount;
	std::size_t m_tickCount;
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void displayTime();
	void sendSettings();
	void configureFileName();
	void updateWithGeneration();
	void updateWithStreamTime();
	void updateSampleRateAndFrequency();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
	void on_startStop_toggled(bool checked);
	void on_showFileDialog_clicked(bool checked);
	void on_interp_currentIndexChanged(int index);
    void openDeviceSettingsDialog(const QPoint& p);
    void updateHardware();
    void updateStatus();
	void tick();
};

#endif // INCLUDE_FILEOUTPUTGUI_H
