///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTGUI_H_
#define PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTGUI_H_

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "usrpoutput.h"

class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
    class USRPOutputGUI;
}

class USRPOutputGUI : public DeviceGUI {
    Q_OBJECT

public:
    explicit USRPOutputGUI(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~USRPOutputGUI();
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

protected:
    void resizeEvent(QResizeEvent* size);

private:
    Ui::USRPOutputGUI* ui;

    USRPOutput* m_usrpOutput; //!< Same object as above but gives easy access to USRPOutput methods and attributes that are used intensively
    USRPOutputSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_sampleRateMode; //!< true: device, false: base band sample rate update mode
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
    void displaySampleRate();
    void setCenterFrequencyDisplay();
    void setCenterFrequencySetting(uint64_t kHzValue);
    void sendSettings();
    void updateSampleRateAndFrequency();
    void updateSampleRate();
    void updateFrequencyLimits();
    void blockApplySettings(bool block);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_centerFrequency_changed(quint64 value);
    void on_sampleRate_changed(quint64 value);
    void on_swInterp_currentIndexChanged(int index);
    void on_lpf_changed(quint64 value);
    void on_loOffset_changed(qint64 value);
    void on_gain_valueChanged(int value);
    void on_antenna_currentIndexChanged(int index);
    void on_clockSource_currentIndexChanged(int index);
    void on_transverter_clicked();
    void on_sampleRateMode_toggled(bool checked);
    void openDeviceSettingsDialog(const QPoint& p);

    void updateHardware();
    void updateStatus();
};

#endif /* PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTGUI_H_ */
