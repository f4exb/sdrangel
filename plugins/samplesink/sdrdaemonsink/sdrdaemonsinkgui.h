///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SDRDAEMONSINKGUI_H
#define INCLUDE_SDRDAEMONSINKGUI_H

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QTime>
#include <QWidget>

#include "util/messagequeue.h"

#include "sdrdaemonsinksettings.h"
#include "sdrdaemonsinkoutput.h"


class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
	class SDRdaemonSinkGui;
}

class SDRdaemonSinkGui : public QWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	explicit SDRdaemonSinkGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~SDRdaemonSinkGui();
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
	Ui::SDRdaemonSinkGui* ui;

	DeviceUISet* m_deviceUISet;
	SDRdaemonSinkSettings m_settings;        //!< current settings
	SDRdaemonSinkSettings m_controlSettings; //!< settings last sent to device via control port
	QTimer m_updateTimer;
    QTimer m_statusTimer;
	DeviceSampleSink* m_deviceSampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_samplesCount;
	std::size_t m_tickCount;
	std::size_t m_nbSinceLastFlowCheck;
	int m_lastEngineState;
    bool m_doApplySettings;
    bool m_forceSettings;

    int m_nnSender;

    uint32_t m_countUnrecoverable;
    uint32_t m_countRecovered;
    QTime m_time;

    QPalette m_paletteGreenText;
    QPalette m_paletteRedText;
    QPalette m_paletteWhiteText;

    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block);
	void displaySettings();
	void displayTime();
    void sendControl(bool force = false);
	void sendSettings();
	void updateWithStreamTime();
	void updateSampleRateAndFrequency();
	void updateTxDelayTooltip();
	void displayEventCounts();
    void displayEventTimer();

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
    void on_interp_currentIndexChanged(int index);
    void on_txDelay_valueChanged(int value);
    void on_nbFECBlocks_valueChanged(int value);
    void on_address_returnPressed();
    void on_dataPort_returnPressed();
    void on_controlPort_returnPressed();
    void on_specificParms_returnPressed();
    void on_applyButton_clicked(bool checked);
    void on_sendButton_clicked(bool checked);
	void on_startStop_toggled(bool checked);
	void on_eventCountsReset_clicked(bool checked);
    void updateHardware();
    void updateStatus();
	void tick();
};

#endif // INCLUDE_FILESINKGUI_H
