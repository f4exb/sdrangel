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

#ifndef INCLUDE_TESTMOSYNCGUI_H
#define INCLUDE_TESTMOSYNCGUI_H

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "testmosyncsettings.h"


class DeviceUISet;
class TestMOSync;

namespace Ui {
	class TestMOSyncGui;
}

class TestMOSyncGui : public QWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	explicit TestMOSyncGui(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~TestMOSyncGui();
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
	Ui::TestMOSyncGui* ui;

	DeviceUISet* m_deviceUISet;
	bool m_doApplySettings;
	bool m_forceSettings;
	TestMOSyncSettings m_settings;
	QTimer m_updateTimer;
    QTimer m_statusTimer;
	TestMOSync* m_sampleMIMO;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    bool m_generation;
	int m_samplesCount;
	std::size_t m_tickCount;
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
	void sendSettings();
	void updateSampleRateAndFrequency();

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
	void on_startStop_toggled(bool checked);
	void on_interp_currentIndexChanged(int index);
    void updateHardware();
    void updateStatus();
	void tick();
};

#endif // INCLUDE_TESTMOSYNCGUI_H
