///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3GUI_H_
#define PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3GUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "sdrplayv3input.h"
#include "sdrplayv3settings.h"

class DeviceUISet;

namespace Ui {
    class SDRPlayV3Gui;
}

class SDRPlayV3Gui : public DeviceGUI {
    Q_OBJECT

public:
    explicit SDRPlayV3Gui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~SDRPlayV3Gui();
    virtual void destroy();

    virtual void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

protected:
    void resizeEvent(QResizeEvent* size);

private:
    Ui::SDRPlayV3Gui* ui;

    bool m_doApplySettings;
    bool m_forceSettings;
    SDRPlayV3Settings m_settings;
    QList<QString> m_settingsKeys;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    SDRPlayV3Input* m_sdrPlayV3Input;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency;
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void updateLNAValues();
    void sendSettings();
    void updateSampleRateAndFrequency();
    void updateFrequencyLimits();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void updateHardware();
    void updateStatus();
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_ppm_valueChanged(int value);
    void on_tuner_currentIndexChanged(int index);
    void on_antenna_currentIndexChanged(int index);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_extRef_toggled(bool checked);
    void on_biasTee_toggled(bool checked);
    void on_amNotch_toggled(bool checked);
    void on_fmNotch_toggled(bool checked);
    void on_dabNotch_toggled(bool checked);
    void on_bandwidth_currentIndexChanged(int index);
    void on_samplerate_changed(quint64 value);
    void on_ifFrequency_currentIndexChanged(int index);
    void on_decim_currentIndexChanged(int index);
    void on_fcPos_currentIndexChanged(int index);
    void on_gainLNA_currentIndexChanged(int index);
    void on_gainIFAGC_toggled(bool checked);
    void on_gainIF_valueChanged(int value);
    void on_startStop_toggled(bool checked);
    void on_transverter_clicked();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3GUI_H_ */
