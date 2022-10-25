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

#ifndef INCLUDE_REMOTEINPUTGUI_H
#define INCLUDE_REMOTEINPUTGUI_H

#include <QTimer>
#include <QElapsedTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "remoteinput.h"

class DeviceUISet;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;

namespace Ui {
	class RemoteInputGui;
}

class RemoteInputGui : public DeviceGUI {
	Q_OBJECT

public:
	explicit RemoteInputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~RemoteInputGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

protected:
    void resizeEvent(QResizeEvent* size);

private:
	Ui::RemoteInputGui* ui;

    RemoteInputSettings m_settings;        //!< current settings
    QList<QString> m_settingsKeys;
	RemoteInput::RemoteChannelSettings m_remoteChannelSettings;
	double m_remoteShiftFrequencyFactor;  //!< Remote channel frequency shift factor
	RemoteInput* m_sampleSource;
    bool m_acquisition;
    int m_streamSampleRate;          //!< Sample rate of received stream
    quint64 m_streamCenterFrequency; //!< Center frequency of received stream
	QTimer m_updateTimer;
	QTimer m_remoteUpdateTimer;
	QTimer m_statusTimer;
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;

    //	int m_sampleRate;
    //	quint64 m_centerFrequency;
	uint64_t m_startingTimeStampms;
	int m_framesDecodingStatus;
	bool m_allBlocksReceived;
	float m_bufferLengthInSecs;
    int32_t m_bufferGauge;
    int m_minNbBlocks;
    int m_minNbOriginalBlocks;
    int m_maxNbRecovery;
    float m_avgNbBlocks;
    float m_avgNbOriginalBlocks;
    float m_avgNbRecovery;
    int m_nbOriginalBlocks;
    int m_nbFECBlocks;
    int m_sampleBits;
    int m_sampleBytes;

	int m_samplesCount;
	std::size_t m_tickCount;

	bool m_addressEdited;
	bool m_dataPortEdited;

    uint32_t m_countUnrecoverable;
    uint32_t m_countRecovered;
    QElapsedTimer m_eventsTime;

	bool m_doApplySettings;
    bool m_forceSettings;

    QPalette m_paletteGreenText;
    QPalette m_paletteWhiteText;

    void blockApplySettings(bool block);
	void displaySettings();
	void displayRemoteSettings();
	void displayRemoteShift();
	void displayTime();
	void displayRemoteFixedData(const RemoteInput::MsgReportRemoteFixedData::RemoteData& remoteData);
    void sendSettings();
	void updateWithAcquisition();
	void updateWithStreamTime();
	void updateSampleRateAndFrequency();
	void displayEventCounts();
    void displayEventTimer();
	void applyDecimation();
	void applyPosition();
	void applyRemoteSettings();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void on_remoteDeviceFrequency_changed(quint64 value);
    void on_decimationFactor_currentIndexChanged(int index);
    void on_position_valueChanged(int value);
	void on_apiApplyButton_clicked(bool checked);
    void on_dataApplyButton_clicked(bool checked);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_apiAddress_editingFinished();
	void on_apiPort_editingFinished();
    void on_dataAddress_editingFinished();
	void on_dataPort_editingFinished();
    void on_multicastAddress_editingFinished();
	void on_multicastJoin_toggled(bool checked);
	void on_startStop_toggled(bool checked);
    void on_eventCountsReset_clicked(bool checked);
    void updateHardware();
	void updateRemote();
	void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_REMOTEINPUTGUI_H
