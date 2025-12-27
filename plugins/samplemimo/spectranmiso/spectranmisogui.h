///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef _SPECTRANMISO_SPECTRANMISOGUI_H_
#define _SPECTRANMISO_SPECTRANMISOGUI_H_

#include <device/devicegui.h>
#include <QWidget>
#include <QTimer>

#include "util/messagequeue.h"
#include "spectranmisosettings.h"
#include "spectranmiso.h"

class DeviceUISet;
namespace Ui {
    class SpectranMISOGui;
}

class SpectranMISOGui : public DeviceGUI
{
    Q_OBJECT
public:
    explicit SpectranMISOGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~SpectranMISOGui();
    virtual void destroy();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void sendSettings();
private:
    Ui::SpectranMISOGui* ui;
    MessageQueue m_inputMessageQueue;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    SpectranMISOSettings m_settings;
    QList<QString> m_settingsKeys;
	bool m_doApplySettings;
    bool m_forceSettings;
    SpectranMISO* m_sampleMIMO;
    int m_lastEngineState;
    bool handleMessage(const Message& message);
    bool isSpectrumRx() const;
    void on_startStop_toggled(bool checked);
    void on_spectrumSource_currentIndexChanged(int index);
    void on_mode_currentIndexChanged(int index);
    void on_rxChannel_currentIndexChanged(int index);
    void on_sampleRate_valueChanged(qint64 value);
    void on_clockRate_currentIndexChanged(int index);
    void on_log2Decimation_currentIndexChanged(int index);
    void on_rxCenterFrequency_valueChanged(qint64 value);
    void on_txCenterFrequency_valueChanged(qint64 value);
    void on_txDrive_valueChanged(int value);
    void makeUIConnections();

private slots:
    void updateStatus();
    void updateHardware();
    void handleInputMessages();
};

#endif // _SPECTRANMISO_SPECTRANMISOGUI_H_
