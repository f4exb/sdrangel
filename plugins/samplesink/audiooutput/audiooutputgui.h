///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_AUDIOOUTPUTGUI_H
#define INCLUDE_AUDIOOUTPUTGUI_H

#include <QTimer>
#include <QWidget>

#include "device/devicegui.h"
#include "util/messagequeue.h"

#include "audiooutput.h"

class QWidget;
class DeviceUISet;

namespace Ui {
    class AudioOutputGui;
}

class AudioOutputGui : public DeviceGUI {
    Q_OBJECT

public:
    explicit AudioOutputGui(DeviceUISet *deviceUISet, QWidget* parent = nullptr);
    virtual ~AudioOutputGui();
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    Ui::AudioOutputGui* ui;

    AudioOutput* m_audioOutput;
    bool m_doApplySettings;
    bool m_forceSettings;
    AudioOutputSettings m_settings;
    QList<QString> m_settingsKeys;
    QTimer m_updateTimer;
    int m_sampleRate;
    qint64 m_centerFrequency;

    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void sendSettings();
    void updateSampleRateAndFrequency();
    bool handleMessage(const Message& message);
    void makeUIConnections();

private slots:
    void handleInputMessages();
    void on_deviceSelect_clicked();
    void on_volume_valueChanged(int value);
    void on_channels_currentIndexChanged(int index);
    void on_startStop_toggled(bool checked);
    void updateHardware();
    void openDeviceSettingsDialog(const QPoint& p);
};

#endif // INCLUDE_AUDIOINPUTGUI_H
