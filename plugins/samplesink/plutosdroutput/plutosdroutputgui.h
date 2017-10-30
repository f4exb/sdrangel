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

#ifndef PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDROUTPUTGUI_H_
#define PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDROUTPUTGUI_H_

#include <plugin/plugininstancegui.h>
#include <QObject>
#include <QWidget>
#include <QTimer>

#include "util/messagequeue.h"

#include "plutosdroutputsettings.h"

class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
    class PlutoSDROutputGUI;
}

class PlutoSDROutputGUI : public QWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    explicit PlutoSDROutputGUI(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~PlutoSDROutputGUI();

    virtual void destroy();
    virtual void setName(const QString& name);
    virtual QString getName() const;
    virtual void resetToDefaults();
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

private:
    Ui::PlutoSDROutputGUI* ui;
    DeviceUISet* m_deviceUISet;
    PlutoSDROutputSettings m_settings;
    bool m_forceSettings;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    DeviceSampleSink* m_sampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;
    bool m_doApplySettings;
    uint32_t m_statusCounter;
    MessageQueue m_inputMessageQueue;

    void displaySettings();
    void sendSettings(bool forceSettings = false);
    void blockApplySettings(bool block);
    void updateSampleRateAndFrequency();
    void setFIRBWLimits();
    void setSampleRateLimits();
    void updateFrequencyLimits();

private slots:
    void on_startStop_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_loPPM_valueChanged(int value);
    void on_swInterp_currentIndexChanged(int index);
    void on_sampleRate_changed(quint64 value);
    void on_lpf_changed(quint64 value);
    void on_lpFIREnable_toggled(bool checked);
    void on_lpFIR_changed(quint64 value);
    void on_lpFIRInterpolation_currentIndexChanged(int index);
    void on_lpFIRGain_currentIndexChanged(int index);
    void on_att_valueChanged(int value);
    void on_antenna_currentIndexChanged(int index);
    void on_transverter_clicked();
    void updateHardware();
    void updateStatus();
    void handleInputMessages();
};

#endif /* PLUGINS_SAMPLESOURCE_PLUTOSDROUTPUT_PLUTOSDROUTPUTGUI_H_ */
