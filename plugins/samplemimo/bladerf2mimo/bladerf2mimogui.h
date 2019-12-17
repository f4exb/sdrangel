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
#include "plugin/plugininstancegui.h"

#include "bladerf2mimosettings.h"

class DeviceUISet;
class BladeRF2MIMO;

namespace Ui {
	class BladeRF2MIMOGui;
}

class BladeRF2MIMOGui : public QWidget, public PluginInstanceGUI {
	Q_OBJECT
public:
	explicit BladeRF2MIMOGui(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
	virtual ~BladeRF2MIMOGui();
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
	Ui::BladeRF2MIMOGui* ui;

	DeviceUISet* m_deviceUISet;
	BladeRF2MIMOSettings m_settings;
    bool m_rxElseTx;   //!< Which side is being dealt with
    int m_streamIndex; //!< Current stream index being dealt with
	bool m_spectrumRxElseTx;
    int m_spectrumStreamIndex; //!< Index of the stream displayed on main spectrum
    QTimer m_updateTimer;
    QTimer m_statusTimer;
	bool m_doApplySettings;
    bool m_forceSettings;
	BladeRF2MIMO* m_sampleMIMO;
	std::size_t m_tickCount;
    int m_deviceSampleRate;
    quint64 m_rxDeviceCenterFrequency; //!< Center frequency in Rx device
    quint64 m_txDeviceCenterFrequency; //!< Center frequency in Tx device
	int m_lastEngineState;
	MessageQueue m_inputMessageQueue;

    bool m_sampleRateMode;
    int m_srMax, m_srMin, m_srStep;
    int m_bwMaxRx, m_bwMinRx, m_bwStepRx;
    int m_bwMaxTx, m_bwMinTx, m_bwStepTx;
    uint64_t m_fMinRx, m_fMaxRx;
    uint64_t m_fMinTx, m_fMaxTx;
    int m_fStepRx, m_fStepTx;

	void blockApplySettings(bool block) { m_doApplySettings = !block; }
	void displaySettings();
    void displaySampleRate();
    void displayFcTooltip();
	void displayGainModes();
	void sendSettings();
    void updateSampleRateAndFrequency();
    void updateFileRecordStatus();
    void updateFrequencyLimits();
    void setCenterFrequencySetting(uint64_t kHzValue);

private slots:
    void handleInputMessages();
	void updateHardware();
    void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
	void on_streamSide_currentIndexChanged(int index);
	void on_streamIndex_currentIndexChanged(int index);
	void on_spectrumSide_currentIndexChanged(int index);
	void on_spectrumIndex_currentIndexChanged(int index);
	void on_startStop_toggled(bool checked);
	void on_record_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
	void on_LOppm_valueChanged(int value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
	void on_bandwidth_changed(quint64 value);
	void on_sampleRate_changed(quint64 value);
	void on_fcPos_currentIndexChanged(int index);
	void on_decim_currentIndexChanged(int index);
	void on_gainMode_currentIndexChanged(int index);
	void on_gain_valueChanged(int value);
	void on_biasTee_toggled(bool checked);
	void on_transverter_clicked();
};

#endif // _BLADERF2MIMO_BLADERF2MIMOGUI_H_
