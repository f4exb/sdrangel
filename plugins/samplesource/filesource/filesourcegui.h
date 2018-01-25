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

#ifndef INCLUDE_FILESOURCEGUI_H
#define INCLUDE_FILESOURCEGUI_H

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "filesourcesettings.h"
#include "filesourceinput.h"

class DeviceUISet;

namespace Ui {
	class FileSourceGui;
}

class FileSourceGui : public QWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	explicit FileSourceGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~FileSourceGui();
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
	Ui::FileSourceGui* ui;

	DeviceUISet* m_deviceUISet;
	FileSourceSettings m_settings;
	bool m_doApplySettings;
	QTimer m_statusTimer;
	std::vector<int> m_gains;
	DeviceSampleSource* m_sampleSource;
    bool m_acquisition;
    QString m_fileName;
	int m_sampleRate;
	quint32 m_sampleSize;
	quint64 m_centerFrequency;
	quint32 m_recordLength;
	std::time_t m_startingTimeStamp;
	int m_samplesCount;
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
	void configureFileName();
	void updateWithAcquisition();
	void updateWithStreamData();
	void updateWithStreamTime();

private slots:
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_playLoop_toggled(bool checked);
	void on_play_toggled(bool checked);
	void on_navTimeSlider_valueChanged(int value);
	void on_showFileDialog_clicked(bool checked);
    void updateStatus();
	void tick();
};

#endif // INCLUDE_FILESOURCEGUI_H
