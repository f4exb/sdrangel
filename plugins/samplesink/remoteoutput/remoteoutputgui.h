///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_REMOTEOUTPUTGUI_H
#define INCLUDE_REMOTEOUTPUTGUI_H

#include <stdint.h>

#include <QTimer>
#include <QElapsedTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"
#include "util/limitedcounter.h"

#include "remoteoutput.h"
#include "remoteoutputsettings.h"

class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
	class RemoteOutputGui;
}

class RemoteOutputExpAvg {
public:
    RemoteOutputExpAvg(float alpha) :
        m_alpha(alpha),
        m_start(true),
        m_s(0)
    {}
    int put(int y)
    {
        if (m_start) {
            m_start = false;
            m_s = y;
        } else {
            m_s = m_alpha*y + (1.0-m_alpha)*m_s;
        }
        return roundf(m_s);
    }
    void reset() {
        m_start = true;
    }

private:
    float m_alpha;
    bool m_start;
    float m_s;
};

class RemoteOutputSinkGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit RemoteOutputSinkGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~RemoteOutputSinkGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::RemoteOutputGui* ui;

	RemoteOutputSettings m_settings;        //!< current settings
    QList<QString> m_settingsKeys;
	RemoteOutputSettings m_controlSettings; //!< settings last sent to device via control port
	QTimer m_updateTimer;
    QTimer m_statusTimer;
	DeviceSampleSink* m_remoteOutput;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
	int m_samplesCount;
	uint32_t m_tickCount;
	std::size_t m_nbSinceLastFlowCheck;
	int m_lastEngineState;
    bool m_doApplySettings;
    bool m_forceSettings;
    bool m_remoteAPIConnected;

    uint32_t m_countUnrecoverable;
    uint32_t m_countRecovered;
    uint32_t m_lastCountUnrecoverable;
    uint32_t m_lastCountRecovered;
	uint32_t m_lastSampleCount;
	uint64_t m_lastTimestampUs;
    QElapsedTimer m_time;

    QPalette m_paletteGreenText;
    QPalette m_paletteRedText;
    QPalette m_paletteWhiteText;

    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block);
	void displaySettings();
	void displayTime();
    void displayRemoteData(const RemoteOutput::MsgReportRemoteData::RemoteData& remoteData);
    void displayRemoteFixedData(const RemoteOutput::MsgReportRemoteFixedData::RemoteData& remoteData);
    void sendControl(bool force = false);
	void sendSettings();
	void updateSampleRate();
	void displayEventCounts();
	void displayEventStatus(int recoverableCount, int unrecoverableCount);
    void displayEventTimer();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_nbFECBlocks_valueChanged(int value);
    void on_deviceIndex_returnPressed();
    void on_channelIndex_returnPressed();
    void on_nbTxBytes_currentIndexChanged(int index);
    void on_apiAddress_returnPressed();
    void on_apiPort_returnPressed();
    void on_dataAddress_returnPressed();
    void on_dataPort_returnPressed();
    void on_apiApplyButton_clicked(bool checked);
    void on_dataApplyButton_clicked(bool checked);
	void on_startStop_toggled(bool checked);
	void on_eventCountsReset_clicked(bool checked);
    void updateHardware();
    void updateStatus();
	void tick();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_REMOTEOUTPUTGUI_H
