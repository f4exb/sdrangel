///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUTGUI_H_
#define PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUTGUI_H_

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "limesdrinput.h"

class DeviceUISet;

namespace Ui {
    class LimeSDRInputGUI;
}

class LimeSDRInputGUI : public QWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    explicit LimeSDRInputGUI(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~LimeSDRInputGUI();
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
    Ui::LimeSDRInputGUI* ui;

    DeviceUISet* m_deviceUISet;
    LimeSDRInput* m_limeSDRInput; //!< Same object as above but gives easy access to LimeSDRInput methods and attributes that are used intensively
    LimeSDRInputSettings m_settings;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;
    bool m_doApplySettings;
    bool m_forceSettings;
    int m_statusCounter;
    int m_deviceStatusCounter;
    MessageQueue m_inputMessageQueue;

    void displaySettings();
    void setNCODisplay();
    void sendSettings();
    void updateSampleRateAndFrequency();
    void updateADCRate();
    void blockApplySettings(bool block);

private slots:
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_record_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_ncoFrequency_changed(quint64 value);
    void on_ncoEnable_toggled(bool checked);
    void on_ncoReset_clicked(bool checked);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_sampleRate_changed(quint64 value);
    void on_hwDecim_currentIndexChanged(int index);
    void on_swDecim_currentIndexChanged(int index);
    void on_lpf_changed(quint64 value);
    void on_lpFIREnable_toggled(bool checked);
    void on_lpFIR_changed(quint64 value);
    void on_gainMode_currentIndexChanged(int index);
    void on_gain_valueChanged(int value);
    void on_lnaGain_valueChanged(int value);
    void on_tiaGain_currentIndexChanged(int index);
    void on_pgaGain_valueChanged(int value);
    void on_antenna_currentIndexChanged(int index);
    void on_extClock_clicked();

    void updateHardware();
    void updateStatus();
};

#endif /* PLUGINS_SAMPLESOURCE_LIMESDRINPUT_LIMESDRINPUTGUI_H_ */
