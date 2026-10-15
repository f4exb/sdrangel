///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef _USRPMIMO_USRPMIMOGUI_H_
#define _USRPMIMO_USRPMIMOGUI_H_

#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"
#include "usrp/deviceusrpstreamstats.h"
#include "device/devicegui.h"

#include "usrpmimosettings.h"

class QComboBox;
class DeviceUISet;
class USRPMIMO;

namespace Ui {
    class USRPMIMOGUI;
}

class USRPMIMOGUI : public DeviceGUI {
    Q_OBJECT

public:
    explicit USRPMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
    virtual ~USRPMIMOGUI();
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    struct StreamStatus {
        bool m_success;
        bool m_active;
        quint32 m_overUnderRuns;
        quint32 m_timeoutsDropped;
    };

    Ui::USRPMIMOGUI* ui;

    USRPMIMOSettings m_settings;
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
    USRPMIMO* m_sampleMIMO;
    int m_rxBasebandSampleRate;
    int m_txBasebandSampleRate;
    quint64 m_rxDeviceCenterFrequency; //!< Center frequency in Rx device
    quint64 m_txDeviceCenterFrequency; //!< Center frequency in Tx device
    int m_lastRxEngineState;
    int m_lastTxEngineState;
    MessageQueue m_inputMessageQueue;
    bool m_sampleRateMode;
    int m_statusCounter;
    int m_deviceStatusCounter;
    StreamStatus m_streamStatus[2]; //!< Rx and Tx
    DeviceUSRPStreamStats m_overUnderRunStats[2];   //!< Rx overruns and Tx underruns
    DeviceUSRPStreamStats m_timeoutDroppedStats[2]; //!< Rx timeouts and Tx dropped packets

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void displaySampleRate();
    void displayMasterClockRate();
    void displayGain();
    void displayAntennas();
    void displayStreamStatus();
    void updateStreamIndexItems(QComboBox *comboBox, bool rxElseTx);
    void sendSettings();
    void updateSampleRateAndFrequency();
    void updateFrequencyLimits();
    void setCenterFrequencySetting(uint64_t kHzValue);
    void setGain(int gain);
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
    void on_loOffset_changed(qint64 value);
    void on_clockSource_currentIndexChanged(int index);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_bandwidth_changed(quint64 value);
    void on_transverter_clicked();
    void on_sampleRateMode_toggled(bool checked);
    void on_sampleRate_changed(quint64 value);
    void on_decim_currentIndexChanged(int index);
    void on_gainLock_toggled(bool checked);
    void on_gainMode_currentIndexChanged(int index);
    void on_gain_valueChanged(int value);
    void on_antenna_currentIndexChanged(int index);
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // _USRPMIMO_USRPMIMOGUI_H_
