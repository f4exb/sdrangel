///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_REMOTETCPINPUTGUI_H
#define INCLUDE_REMOTETCPINPUTGUI_H

#include <QTimer>
#include <QElapsedTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "remotetcpinput.h"
#include "../../channelrx/remotetcpsink/remotetcpprotocol.h"

class DeviceUISet;
class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;

namespace Ui {
    class RemoteTCPInputGui;
}

class RemoteTCPInputGui : public DeviceGUI {
    Q_OBJECT

    struct DeviceGains {
        struct GainRange {
            QString m_name;
            int m_min;
            int m_max;
            int m_step;                 // In dB
            QVector<int> m_gains;       // In 10ths of dB
            QString m_units;            // Units label for display in the GUI

            GainRange(const QString& name, int min, int max, int step, const QString& units = "dB") :
                m_name(name),
                m_min(min),
                m_max(max),
                m_step(step),
                m_units(units)
            {
            }

            GainRange(const QString& name, QVector<int> gains, const QString& units = "dB") :
                m_name(name),
                m_min(0),
                m_max(0),
                m_step(0),
                m_gains(gains),
                m_units(units)
            {
            }
        };

        DeviceGains()
        {
        }

        DeviceGains(QList<GainRange> gains, bool agc, bool biasTee) :
            m_gains(gains),
            m_agc(agc),
            m_biasTee(biasTee)
        {
        }

        QList<GainRange> m_gains;
        bool m_agc;
        bool m_biasTee;
    };

    struct SampleRateRange {
        int m_min;
        int m_max;
    };

public:
    explicit RemoteTCPInputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~RemoteTCPInputGui();
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    Ui::RemoteTCPInputGui* ui;

    RemoteTCPInputSettings m_settings;        //!< current settings
    QList<QString> m_settingsKeys;
    RemoteTCPInput* m_sampleSource;
    QTimer m_updateTimer;
    MessageQueue m_inputMessageQueue;

    int m_sampleRate;
    quint64 m_centerFrequency;
    uint32_t m_tickCount;

    bool m_doApplySettings;
    bool m_forceSettings;

    const DeviceGains *m_deviceGains;
    RemoteTCPProtocol::Device m_remoteDevice;     // Remote device reported when connecting
    bool m_connectionError;

    DeviceGains::GainRange m_spyServerGainRange;
    DeviceGains m_spyServerGains;

    bool m_sdra;
    bool m_spyServer;
    bool m_remoteControl;
    bool m_iqOnly;

    static const DeviceGains::GainRange m_rtlSDR34kGainRange;
    static const DeviceGains m_rtlSDRe4kGains;
    static const DeviceGains::GainRange m_rtlSDRR820GainRange;
    static const DeviceGains m_rtlSDRR820Gains;
    static const DeviceGains::GainRange m_airspyLNAGainRange;
    static const DeviceGains::GainRange m_airspyMixerGainRange;
    static const DeviceGains::GainRange m_airspyVGAGainRange;
    static const DeviceGains m_airspyGains;
    static const DeviceGains::GainRange m_airspyHFAttRange;
    static const DeviceGains m_airspyHFGains;
    static const DeviceGains::GainRange m_bladeRF1LNARange;
    static const DeviceGains::GainRange m_bladeRF1VGA1Range;
    static const DeviceGains::GainRange m_bladeRF1VGA2Range;
    static const DeviceGains m_baldeRF1Gains;
    static const DeviceGains::GainRange m_funCubeProPlusRange;
    static const DeviceGains m_funCubeProPlusGains;
    static const DeviceGains::GainRange m_hackRFLNAGainRange;
    static const DeviceGains::GainRange m_hackRFVGAGainRange;
    static const DeviceGains m_hackRFGains;
    static const DeviceGains::GainRange m_kiwiGainRange;
    static const DeviceGains m_kiwiGains;
    static const DeviceGains::GainRange m_limeRange;
    static const DeviceGains m_limeGains;
    static const DeviceGains::GainRange m_sdrplayV3LNAGainRange;
    static const DeviceGains::GainRange m_sdrplayV3IFGainRange;
    static const DeviceGains m_sdrplayV3Gains;
    static const DeviceGains::GainRange m_plutoGainRange;
    static const DeviceGains m_plutoGains;
    static const DeviceGains::GainRange m_usrpGainRange;
    static const DeviceGains m_usrpGains;
    static const DeviceGains::GainRange m_xtrxGainRange;
    static const DeviceGains m_xtrxGains;
    static const QHash<RemoteTCPProtocol::Device, const DeviceGains *> m_gains;

    static const SampleRateRange m_rtlSDRSampleRateRange;
    static const SampleRateRange m_sdrPlaySampleRateRange;
    static const SampleRateRange m_bladeRF1SampleRateRange;
    static const SampleRateRange m_hackRFSampleRateRange;
    static const SampleRateRange m_limeSampleRateRange;
    static const SampleRateRange m_plutoSampleRateRange;
    static const SampleRateRange m_usrpSampleRateRange;
    static const QHash<RemoteTCPProtocol::Device, const SampleRateRange *> m_sampleRateRanges;

    static const QList<int> m_airspySampleRateList;
    static const QList<int> m_airspyHFSampleRateList;
    static const QHash<RemoteTCPProtocol::Device, const QList<int> *> m_sampleRateLists;

    void blockApplySettings(bool block);
    void displaySettings();
    QString gainText(int stage);
    void displayEnabled();
    void displayGains();
    void displayRemoteSettings();
    void displayRemoteShift();
	void displayReplayLength();
	void displayReplayOffset();
	void displayReplayStep();
    void sendSettings();
    void updateSampleRateAndFrequency();
    void applyDecimation();
    void applyPosition();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_ppm_valueChanged(int value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_biasTee_toggled(bool checked);
    void on_directSampling_toggled(bool checked);
    void on_devSampleRate_changed(quint64 value);
    void on_decim_currentIndexChanged(int index);
    void on_gain1_valueChanged(int value);
    void on_gain2_valueChanged(int value);
    void on_gain3_valueChanged(int value);
    void on_agc_toggled(bool checked);
    void on_rfBW_changed(int value);
    void on_deltaFrequency_changed(int value);
    void on_channelGain_valueChanged(int value);
    void on_channelSampleRate_changed(quint64 value);
    void on_decimation_toggled(bool checked);
    void on_sampleBits_currentIndexChanged(int index);
    void on_squelchEnabled_toggled(bool checked);
    void on_squelch_valueChanged(int value);
    void on_squelchGate_valueChanged(double value);
    void on_dataAddress_editingFinished();
    void on_dataAddress_currentIndexChanged(int index);
    void on_dataPort_valueChanged(int value);
    void on_overrideRemoteSettings_toggled(bool checked);
    void on_preFill_valueChanged(int value);
    void on_protocol_currentIndexChanged(int index);
    void on_replayOffset_valueChanged(int value);
    void on_replayNow_clicked();
    void on_replayPlus_clicked();
    void on_replayMinus_clicked();
    void on_replaySave_clicked();
    void on_replayLoop_toggled(bool checked);
    void on_sendMessage_clicked();
    void on_txMessage_returnPressed();
    void updateHardware();
    void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
    void tick();
};

#endif // INCLUDE_REMOTETCPINPUTGUI_H
