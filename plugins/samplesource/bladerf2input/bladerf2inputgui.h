///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUTGUI_H_
#define PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUTGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "bladerf2input.h"

class DeviceUISet;

namespace Ui {
    class Bladerf2InputGui;
}

class BladeRF2InputGui : public DeviceGUI {
    Q_OBJECT

public:
    explicit BladeRF2InputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~BladeRF2InputGui();
    virtual void destroy();

    virtual void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    Ui::Bladerf2InputGui* ui;

    bool m_forceSettings;
    bool m_doApplySettings;
    BladeRF2InputSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_sampleRateMode; //!< true: device, false: base band sample rate update mode
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    std::vector<int> m_gains;
    BladeRF2Input* m_sampleSource;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;
    int m_gainMin;
    int m_gainMax;
    int m_gainStep;
    float m_gainScale;

    void displaySettings();
    void displaySampleRate();
    void displayFcTooltip();
    void sendSettings();
    void updateSampleRateAndFrequency();
    void updateFrequencyLimits();
    void setCenterFrequencySetting(uint64_t kHzValue);
    float getGainDB(int gainValue);
    int getGainValue(float gainDB);
    void blockApplySettings(bool block);
    bool handleMessage(const Message& message);
    void makeUIConnections();

protected:
    void resizeEvent(QResizeEvent* size);

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_LOppm_valueChanged(int value);
    void on_sampleRate_changed(quint64 value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_biasTee_toggled(bool checked);
    void on_bandwidth_changed(quint64 value);
    void on_decim_currentIndexChanged(int index);
    void on_fcPos_currentIndexChanged(int index);
    void on_gainMode_currentIndexChanged(int index);
    void on_gain_valueChanged(int value);
    void on_transverter_clicked();
    void on_startStop_toggled(bool checked);
    void on_sampleRateMode_toggled(bool checked);
    void updateHardware();
    void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif /* PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUTGUI_H_ */
