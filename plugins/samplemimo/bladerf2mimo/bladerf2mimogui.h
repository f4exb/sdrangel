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

#ifndef _BLADERF2MIMO_BLADERF2MIMOGUI_H_
#define _BLADERF2MIMO_BLADERF2MIMOGUI_H_

#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"
#include "device/devicegui.h"

#include "bladerf2mimosettings.h"

class DeviceUISet;
class BladeRF2MIMO;

namespace Ui {
	class BladeRF2MIMOGui;
}

class BladeRF2MIMOGui : public DeviceGUI {
	Q_OBJECT
public:
	explicit BladeRF2MIMOGui(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~BladeRF2MIMOGui();
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

protected:
    void resizeEvent(QResizeEvent* size);

private:
	Ui::BladeRF2MIMOGui* ui;

	BladeRF2MIMOSettings m_settings;
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
	BladeRF2MIMO* m_sampleMIMO;
	std::size_t m_tickCount;
    int m_rxBasebandSampleRate;
    int m_txBasebandSampleRate;
    quint64 m_rxDeviceCenterFrequency; //!< Center frequency in Rx device
    quint64 m_txDeviceCenterFrequency; //!< Center frequency in Tx device
	int m_lastRxEngineState;
	int m_lastTxEngineState;
	MessageQueue m_inputMessageQueue;

    bool m_sampleRateMode;
    int m_srMax, m_srMin, m_srStep;
    int m_bwMaxRx, m_bwMinRx, m_bwStepRx;
    int m_bwMaxTx, m_bwMinTx, m_bwStepTx;
    uint64_t m_fMinRx, m_fMaxRx;
    uint64_t m_fMinTx, m_fMaxTx;
    int m_fStepRx, m_fStepTx;
	int m_gainMinRx, m_gainMaxRx, m_gainStepRx;
	int m_gainMinTx, m_gainMaxTx, m_gainStepTx;
	float m_fScaleRx, m_fScaleTx;
	float m_srScaleRx, m_srScaleTx;
	float m_bwScaleRx, m_bwScaleTx;
	float m_gainScaleRx, m_gainScaleTx;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
    void displaySampleRate();
    void displayFcTooltip();
	void displayGainModes();
	void displayGain();
	void sendSettings();
    void updateSampleRateAndFrequency();
    void updateFrequencyLimits();
    void setCenterFrequencySetting(uint64_t kHzValue);
	float getGainDB(int gainValue, int gainMin, int gainMax, int gainStep, float gainScale);
    int getGainValue(float gainDB, int gainMin, int gainMax, int gainStep, float gainScale);
	float setGainFromValue(int value);
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
	void on_LOppm_valueChanged(int value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
	void on_bandwidth_changed(quint64 value);
	void on_sampleRate_changed(quint64 value);
	void on_fcPos_currentIndexChanged(int index);
	void on_decim_currentIndexChanged(int index);
    void on_gainLock_toggled(bool checked);
	void on_gainMode_currentIndexChanged(int index);
	void on_gain_valueChanged(int value);
	void on_biasTee_toggled(bool checked);
	void on_transverter_clicked();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // _BLADERF2MIMO_BLADERF2MIMOGUI_H_
