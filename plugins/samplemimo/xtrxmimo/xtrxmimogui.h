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

#ifndef PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMOGUI_H_
#define PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMOGUI_H_

#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"
#include "device/devicegui.h"

#include "xtrxmimosettings.h"

class DeviceUISet;
class XTRXMIMO;

namespace Ui {
	class XTRXMIMOGUI;
}

class XTRXMIMOGUI : public DeviceGUI {
	Q_OBJECT
public:
	explicit XTRXMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~XTRXMIMOGUI();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::XTRXMIMOGUI* ui;

	XTRXMIMOSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_rxElseTx;   //!< Which side is being dealt with
    int m_streamIndex; //!< Current stream index being dealt with
	bool m_spectrumRxElseTx;
    int m_spectrumStreamIndex; //!< Index of the stream displayed on main spectrum
    QTimer m_updateTimer;
    QTimer m_statusTimer;
	bool m_doApplySettings;
    bool m_forceSettings;
	XTRXMIMO* m_xtrxMIMO;
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

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void displaySampleRate();
    void setNCODisplay();
    void setRxCenterFrequencyDisplay();
    void setRxCenterFrequencySetting(uint64_t kHzValue);
    void setTxCenterFrequencyDisplay();
    void setTxCenterFrequencySetting(uint64_t kHzValue);
    void sendSettings();
    void updateSampleRateAndFrequency();
    void updateADCRate();
    void updateDACRate();
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
    void on_pwrmode_currentIndexChanged(int index);
    void on_gainMode_currentIndexChanged(int index);
    void on_gain_valueChanged(int value);
    void on_lnaGain_valueChanged(int value);
    void on_tiaGain_currentIndexChanged(int index);
    void on_pgaGain_valueChanged(int value);
    void on_antenna_currentIndexChanged(int index);
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMOGUI_H_
