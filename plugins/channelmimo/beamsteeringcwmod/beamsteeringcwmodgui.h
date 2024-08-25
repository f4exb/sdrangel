///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_BEAMSTEERINGCWMODGUI_H_
#define INCLUDE_BEAMSTEERINGCWMODGUI_H_

#include <stdint.h>

#include <QObject>

#include "dsp/channelmarker.h"
#include "channel/channelgui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "beamsteeringcwmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BeamSteeringCWMod;
class MIMOChannel;

namespace Ui {
    class BeamSteeringCWModGUI;
}

class BeamSteeringCWModGUI : public ChannelGUI {
    Q_OBJECT
public:
    static BeamSteeringCWModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *mimoChannel);

    void resetToDefaults() final;
    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;
    MessageQueue *getInputMessageQueue() final { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) final { m_settings.m_workspaceIndex = index; };
    int getWorkspaceIndex() const final { return m_settings.m_workspaceIndex; };
    void setGeometryBytes(const QByteArray& blob) final { m_settings.m_geometryBytes = blob; };
    QByteArray getGeometryBytes() const final { return m_settings.m_geometryBytes; };
    QString getTitle() const final { return m_settings.m_title; };
    QColor getTitleColor() const final  { return m_settings.m_rgbColor; };
    void zetHidden(bool hidden) final { m_settings.m_hidden = hidden; }
    bool getHidden() const final { return m_settings.m_hidden; }
    ChannelMarker& getChannelMarker() final { return m_channelMarker; }
    int getStreamIndex() const final { return -1; }
    void setStreamIndex(int streamIndex) final { (void) streamIndex; }

private:
    Ui::BeamSteeringCWModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    BeamSteeringCWModSettings m_settings;
    int m_basebandSampleRate;
    qint64 m_centerFrequency;
    double m_shiftFrequencyFactor; //!< Channel frequency shift factor
    bool m_doApplySettings;

    BeamSteeringCWMod* m_bsCWSource;
    MessageQueue m_inputMessageQueue;

    uint32_t m_tickCount;

    explicit BeamSteeringCWModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *mimoChannel, QWidget* parent = nullptr);
    ~BeamSteeringCWModGUI() final;

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayRateAndShift();
    bool handleMessage(const Message& message);
    void makeUIConnections() const;
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*) final;
    void enterEvent(EnterEventType*) final;

    void applyInterpolation();
    void applyPosition();

private slots:
    void handleSourceMessages();
    void on_channelOutput_currentIndexChanged(int index);
    void on_interpolationFactor_currentIndexChanged(int index);
    void on_position_valueChanged(int value);
    void on_steeringDegrees_valueChanged(int value);
    void onWidgetRolled(const QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};



#endif /* INCLUDE_BEAMSTEERINGCWSOURCEGUI_H_ */
