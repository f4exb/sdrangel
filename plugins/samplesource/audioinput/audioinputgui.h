///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_AUDIOINPUTGUI_H
#define INCLUDE_AUDIOINPUTGUI_H

#include <device/devicegui.h>
#include <QTimer>
#include <QWidget>

#include "util/messagequeue.h"

#include "audioinput.h"

class QWidget;
class DeviceUISet;

namespace Ui {
    class AudioInputGui;
}

class AudioInputGui : public DeviceGUI {
    Q_OBJECT

public:
    explicit AudioInputGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~AudioInputGui();
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    Ui::AudioInputGui* ui;

    bool m_doApplySettings;
    bool m_forceSettings;
    AudioInputSettings m_settings;
    QList<QString> m_settingsKeys;
    QTimer m_updateTimer;
    DeviceSampleSource* m_sampleSource;
    int m_sampleRate;
    qint64 m_centerFrequency;

    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void refreshDeviceList();
    void refreshSampleRates(QString deviceName);
    void displaySettings();
    void displayFcTooltip();
    void sendSettings();
    void updateSampleRateAndFrequency();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_device_currentIndexChanged(int index);
    void on_sampleRate_currentIndexChanged(int index);
    void on_decim_currentIndexChanged(int index);
    void on_volume_valueChanged(int value);
    void on_channels_currentIndexChanged(int index);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
    void on_fcPos_currentIndexChanged(int index);
    void on_startStop_toggled(bool checked);
    void updateHardware();
    void openDeviceSettingsDialog(const QPoint& p);
    void updateStatus();
};

#endif // INCLUDE_AUDIOINPUTGUI_H
