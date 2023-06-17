///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef _AUDIOCATSISO_AUDIOCATSISOGUI_H_
#define _AUDIOCATSISO_AUDIOCATSISOGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "audiocatsisosettings.h"
#include "audiocatsiso.h"

class DeviceUISet;

namespace Ui {
	class AudioCATSISOGUI;
}

class AudioCATSISOGUI : public DeviceGUI {
	Q_OBJECT

public:
	explicit AudioCATSISOGUI(DeviceUISet *deviceUISet, QWidget* parent = 0);
	virtual ~AudioCATSISOGUI();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::AudioCATSISOGUI* ui;

	AudioCATSISOSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_rxElseTx;   //!< Which side is being dealt with
    int m_rxSampleRate;
    int m_txSampleRate;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
	bool m_doApplySettings;
    bool m_forceSettings;
	AudioCATSISO* m_sampleMIMO;
	std::size_t m_tickCount;
    std::vector<int> m_deviceSampleRates;
    std::vector<quint64> m_deviceCenterFrequencies; //!< Center frequency in device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;
    static const int m_absMaxFreq = 61440; // kHz
    AudioCATSISOSettings::MsgCATReportStatus::Status m_lastCATStatus;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
    void displayFrequency();
    void displaySampleRate();
    void displayDecim();
    void displayFcRxTooltip();
    void displayCatDevice();
    void displayCatType();
    void updateTxEnable();
    void updateSpectrum(bool rxElseTx);
    void updateCATStatus(AudioCATSISOSettings::MsgCATReportStatus::Status status);
	void sendSettings();
	void setCenterFrequency(qint64 centerFrequency);
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_streamSide_currentIndexChanged(int index);
    void on_spectrumSide_currentIndexChanged(int index);
    void on_streamLock_toggled(bool checked);
	void on_startStop_toggled(bool checked);
    void on_ptt_toggled(bool checked);
    void on_pttSpectrumLinkToggled(bool checked);
    void on_catConnect_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_log2Decim_currentIndexChanged(int index);
    void on_dcBlock_toggled(bool checked);
    void on_iqCorrection_toggled(bool checked);
    void on_txEnable_toggled(bool checked);
    void on_transverter_clicked();
    void on_rxDeviceSelect_clicked();
    void on_txDeviceSelect_clicked();
    void on_rxChannels_currentIndexChanged(int index);
    void on_rxVolume_valueChanged(int value);
    void on_txChannels_currentIndexChanged(int index);
    void on_txVolume_valueChanged(int value);
    void on_fcPosRx_currentIndexChanged(int index);
    void on_catDevice_currentIndexChanged(int index);
    void on_catType_currentIndexChanged(int index);
    void on_catSettings_clicked();
    void openDeviceSettingsDialog(const QPoint& p);
    void updateStatus();
    void updateHardware();
};

#endif // _AUDIOCATSISO_AUDIOCATSISOGUI_H_
