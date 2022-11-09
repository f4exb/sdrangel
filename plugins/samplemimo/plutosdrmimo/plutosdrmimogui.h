///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#ifndef _PLUTOSDRMIMO_PLUTOSDRMIMOGUI_H_
#define _PLUTOSDRMIMO_PLUTOSDRMIMOGUI_H_

#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"
#include "device/devicegui.h"

#include "plutosdrmimosettings.h"

class DeviceUISet;
class PlutoSDRMIMO;

namespace Ui {
	class PlutoSDRMIMOGUI;
}

class PlutoSDRMIMOGUI : public DeviceGUI {
	Q_OBJECT
public:
	explicit PlutoSDRMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~PlutoSDRMIMOGUI();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
	Ui::PlutoSDRMIMOGUI* ui;

	PlutoSDRMIMOSettings m_settings;
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
	PlutoSDRMIMO* m_sampleMIMO;
	std::size_t m_tickCount;
    int m_rxBasebandSampleRate;
    int m_txBasebandSampleRate;
    quint64 m_rxDeviceCenterFrequency; //!< Center frequency in Rx device
    quint64 m_txDeviceCenterFrequency; //!< Center frequency in Tx device
	int m_lastRxEngineState;
	int m_lastTxEngineState;
	uint32_t m_statusCounter;
	MessageQueue m_inputMessageQueue;
	bool m_sampleRateMode;

    void displaySettings();
    void displaySampleRate();
    void displayFcTooltip();
    void sendSettings(bool forceSettings = false);
    void blockApplySettings(bool block);
    void updateSampleRateAndFrequency();
    void setFIRBWLimits();
    void setSampleRateLimits();
    void updateFrequencyLimits();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
	void updateHardware();
    void updateStatus();
	void updateGainCombo();
	void on_streamSide_currentIndexChanged(int index);
	void on_streamIndex_currentIndexChanged(int index);
	void on_spectrumSide_currentIndexChanged(int index);
	void on_spectrumIndex_currentIndexChanged(int index);
	void on_startStopRx_toggled(bool checked);
	void on_startStopTx_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_loPPM_valueChanged(int value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
	void on_sampleRate_changed(quint64 value);
    void on_sampleRateMode_toggled(bool checked);
    void on_fcPos_currentIndexChanged(int index);
    void on_swDecim_currentIndexChanged(int index);
    void on_gainLock_toggled(bool checked);
	void on_gainMode_currentIndexChanged(int index);
	void on_gain_valueChanged(int value);
	void on_att_valueChanged(int value);
	void on_transverter_clicked();
    void on_rfDCOffset_toggled(bool checked);
    void on_bbDCOffset_toggled(bool checked);
    void on_hwIQImbalance_toggled(bool checked);
    void on_lpf_changed(quint64 value);
    void on_lpFIREnable_toggled(bool checked);
    void on_lpFIR_changed(quint64 value);
    void on_lpFIRDecimation_currentIndexChanged(int index);
    void on_lpFIRGain_currentIndexChanged(int index);
    void on_antenna_currentIndexChanged(int index);

    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // _PLUTOSDRMIMO_PLUTOSDRMIMOGUI_H_
