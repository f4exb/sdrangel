///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMOGUI_H_
#define PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMOGUI_H_

#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"
#include "device/devicegui.h"

#include "limesdrmimosettings.h"

class DeviceUISet;
class LimeSDRMIMO;

namespace Ui {
	class LimeSDRMIMOGUI;
}

class LimeSDRMIMOGUI : public DeviceGUI {
	Q_OBJECT
public:
	explicit LimeSDRMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~LimeSDRMIMOGUI();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::LimeSDRMIMOGUI* ui;

	LimeSDRMIMOSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_rxElseTx;   //!< Which side is being dealt with
    int m_streamIndex; //!< Current stream index being dealt with
	bool m_spectrumRxElseTx;
    int m_spectrumStreamIndex; //!< Index of the stream displayed on main spectrum
    bool m_gainLock; //!< Lock Rx or Tx channel gains (set channel gains to gain of channel 0 when engaged)
    QTimer m_updateTimer;
    QTimer m_statusTimer;
	bool m_doApplySettings;
    bool m_forceSettings;
	LimeSDRMIMO* m_limeSDRMIMO;
	std::size_t m_tickCount;
    int m_rxBasebandSampleRate;
    int m_txBasebandSampleRate;
    quint64 m_rxDeviceCenterFrequency; //!< Center frequency in Rx device
    quint64 m_txDeviceCenterFrequency; //!< Center frequency in Tx device
	int m_lastRxEngineState;
	int m_lastTxEngineState;
    int m_statusCounter;
    int m_deviceStatusCounter;
	MessageQueue m_inputMessageQueue;

    bool m_sampleRateMode;
    int m_srMaxRx, m_srMinRx, m_srStepRx;
    int m_srMaxTx, m_srMinTx, m_srStepTx;
    int m_bwMaxRx, m_bwMinRx, m_bwStepRx;
    int m_bwMaxTx, m_bwMinTx, m_bwStepTx;
    uint64_t m_fMinRx, m_fMaxRx;
    uint64_t m_fMinTx, m_fMaxTx;
    int m_fStepRx, m_fStepTx;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void setRxCenterFrequencyDisplay();
    void setRxCenterFrequencySetting(uint64_t kHzValue);
    void displayRxSampleRate();
    void updateADCRate();
    void setTxCenterFrequencyDisplay();
    void setTxCenterFrequencySetting(uint64_t kHzValue);
    void displayTxSampleRate();
    void updateDACRate();
    void setNCODisplay();
    void updateFrequencyLimits();
    void updateLPFLimits();
    void updateSampleRateAndFrequency();
    void sendSettings();
	bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void updateHardware();
    void updateStatus();
	void on_streamSide_currentIndexChanged(int index);
	void on_streamIndex_currentIndexChanged(int index);
	void on_spectrumSide_currentIndexChanged(int index);
	void on_spectrumIndex_currentIndexChanged(int index);
	void on_startStopRx_toggled(bool checked);
	void on_startStopTx_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_ncoEnable_toggled(bool checked);
    void on_ncoFrequency_changed(qint64 value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_extClock_clicked();
    void on_hwDecim_currentIndexChanged(int index);
    void on_swDecim_currentIndexChanged(int index);
    void on_sampleRateMode_toggled(bool checked);
    void on_sampleRate_changed(quint64 value);
    void on_lpf_changed(quint64 value);
    void on_lpFIREnable_toggled(bool checked);
    void on_lpFIR_changed(quint64 value);
    void on_transverter_clicked();
    void on_gainMode_currentIndexChanged(int index);
    void on_gain_valueChanged(int value);
    void on_lnaGain_valueChanged(int value);
    void on_tiaGain_currentIndexChanged(int index);
    void on_pgaGain_valueChanged(int value);
    void on_antenna_currentIndexChanged(int index);
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMOGUI_H_
