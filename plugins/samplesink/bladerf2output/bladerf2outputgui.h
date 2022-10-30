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

#ifndef PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTGUI_H_
#define PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTGUI_H_

#include <QTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "bladerf2output.h"

class DeviceSampleSink;
class DeviceUISet;

namespace Ui {
    class BladeRF2OutputGui;
}

class BladeRF2OutputGui : public DeviceGUI {
    Q_OBJECT

public:
    explicit BladeRF2OutputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~BladeRF2OutputGui();
    virtual void destroy();

    void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

protected:
    void resizeEvent(QResizeEvent* size);

private:
    Ui::BladeRF2OutputGui* ui;

    bool m_doApplySettings;
    bool m_forceSettings;
    BladeRF2OutputSettings m_settings;
    QList<QString> m_settingsKeys;
    bool m_sampleRateMode; //!< true: device, false: base band sample rate update mode
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    BladeRF2Output* m_sampleSink;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;
    int m_gainMin;
    int m_gainMax;
    int m_gainStep;
    float m_gainScale;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void displaySampleRate();
    void sendSettings();
    void updateSampleRateAndFrequency();
    void updateFrequencyLimits();
    void setCenterFrequencySetting(uint64_t kHzValue);
    float getGainDB(int gainValue);
    int getGainValue(float gainDB);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_LOppm_valueChanged(int value);
    void on_biasTee_toggled(bool checked);
    void on_sampleRate_changed(quint64 value);
    void on_bandwidth_changed(quint64 value);
    void on_interp_currentIndexChanged(int index);
    void on_gain_valueChanged(int value);
    void on_startStop_toggled(bool checked);
    void on_transverter_clicked();
    void on_sampleRateMode_toggled(bool checked);
    void updateHardware();
    void updateStatus();
    void openDeviceSettingsDialog(const QPoint& p);
};



#endif /* PLUGINS_SAMPLESINK_BLADERF2OUTPUT_BLADERF2OUTPUTGUI_H_ */
