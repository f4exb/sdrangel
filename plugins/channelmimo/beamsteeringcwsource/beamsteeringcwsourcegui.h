///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_BEAMSTEERINGCWSOURCEGUI_H_
#define INCLUDE_BEAMSTEERINGCWSOURCEGUI_H_

#include <stdint.h>

#include <QObject>
#include <QTime>

#include "plugin/plugininstancegui.h"
#include "dsp/channelmarker.h"
#include "gui/rollupwidget.h"
#include "util/messagequeue.h"

#include "beamsteeringcwsourcesettings.h"

class PluginAPI;
class DeviceUISet;
class BeamSteeringCWSource;
class MIMOChannel;

namespace Ui {
    class BeamSteeringCWSourceGUI;
}

class BeamSteeringCWSourceGUI : public RollupWidget, public PluginInstanceGUI {
    Q_OBJECT
public:
    static BeamSteeringCWSourceGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *mimoChannel);

    virtual void destroy();
    void setName(const QString& name);
    QString getName() const;
    virtual void resetToDefaults();
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

private:
    Ui::BeamSteeringCWSourceGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    BeamSteeringCWSourceSettings m_settings;
    int m_sampleRate;
    qint64 m_centerFrequency;
    double m_shiftFrequencyFactor; //!< Channel frequency shift factor
    bool m_doApplySettings;

    BeamSteeringCWSource* m_bsCWSource;
    MessageQueue m_inputMessageQueue;

    QTime m_time;
    uint32_t m_tickCount;

    explicit BeamSteeringCWSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, MIMOChannel *mimoChannel, QWidget* parent = nullptr);
    virtual ~BeamSteeringCWSourceGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayRateAndShift();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

    void applyInterpolation();
    void applyPosition();

private slots:
    void handleSourceMessages();
    void on_interpolationFactor_currentIndexChanged(int index);
    void on_position_valueChanged(int value);
    void on_localDevice_currentIndexChanged(int index);
    void on_localDevicesRefresh_clicked(bool checked);
    void on_steeringDegrees_valueChanged(int value);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};



#endif /* INCLUDE_BEAMSTEERINGCWSOURCEGUI_H_ */
