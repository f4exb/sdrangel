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

#ifndef INCLUDE_BLADERFOUTPUTGUI_H
#define INCLUDE_BLADERFOUTPUTGUI_H

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "bladerfoutput.h"

class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
	class BladerfOutputGui;
}

class BladerfOutputGui : public QWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	explicit BladerfOutputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~BladerfOutputGui();
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
	Ui::BladerfOutputGui* ui;

	DeviceUISet* m_deviceUISet;
	bool m_forceSettings;
	BladeRFOutputSettings m_settings;
	QTimer m_updateTimer;
	QTimer m_statusTimer;
	DeviceSampleSink* m_deviceSampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

	void displaySettings();
	void sendSettings();
	unsigned int getXb200Index(bool xb_200, bladerf_xb200_path xb200Path, bladerf_xb200_filter xb200Filter);
	void updateSampleRateAndFrequency();

private slots:
    void handleInputMessages();
	void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
	void on_bandwidth_currentIndexChanged(int index);
	void on_interp_currentIndexChanged(int index);
	void on_vga1_valueChanged(int value);
	void on_vga2_valueChanged(int value);
	void on_xb200_currentIndexChanged(int index);
	void on_startStop_toggled(bool checked);
	void updateHardware();
	void updateStatus();
};

#endif // INCLUDE_BLADERFOUTPUTGUI_H
