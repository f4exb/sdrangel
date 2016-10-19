///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODAM_AMMODGUI_H_
#define PLUGINS_CHANNELTX_MODAM_AMMODGUI_H_

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

class PluginAPI;
class DeviceSinkAPI;

class ThreadedBasebandSampleSource;
class UpChannelizer;
class AMMod;

namespace Ui {
    class AMModGUI;
}

class AMModGUI : public RollupWidget, public PluginGUI {
    Q_OBJECT

public:
    static AMModGUI* create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI);
    void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    virtual bool handleMessage(const Message& message);

    static const QString m_channelID;

private slots:
    void viewChanged();
    void on_deltaFrequency_changed(quint64 value);
    void on_deltaMinus_toggled(bool minus);
    void on_rfBW_valueChanged(int value);
    void on_afBW_valueChanged(int value);
    void on_modPercent_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDoubleClicked();
    void tick();

private:
    Ui::AMModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceSinkAPI* m_deviceAPI;
    ChannelMarker m_channelMarker;
    bool m_basicSettingsShown;
    bool m_doApplySettings;

    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;
    AMMod* m_amDemod;
    MovingAverage<Real> m_channelPowerDbAvg;

    static const int m_rfBW[];

    explicit AMModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent = NULL);
    virtual ~AMModGUI();

    void blockApplySettings(bool block);
    void applySettings();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);
};

#endif /* PLUGINS_CHANNELTX_MODAM_AMMODGUI_H_ */
