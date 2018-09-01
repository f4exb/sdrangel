///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_DAEMONSRC_DAEMONSRCGUI_H_
#define PLUGINS_CHANNELTX_DAEMONSRC_DAEMONSRCGUI_H_

#include "plugin/plugininstancegui.h"
#include "gui/rollupwidget.h"
#include "util/messagequeue.h"
#include "dsp/channelmarker.h"

#include "daemonsrcsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class DaemonSrc;

namespace Ui {
    class DaemonSrcGUI;
}

class DaemonSrcGUI : public RollupWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    static DaemonSrcGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

private:
    Ui::DaemonSrcGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    DaemonSrcSettings m_settings;
    bool m_doApplySettings;

    DaemonSrc* m_daemonSrc;
    MessageQueue m_inputMessageQueue;

    explicit DaemonSrcGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~DaemonSrcGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void handleSourceMessages();
    void on_dataAddress_returnPressed();
    void on_dataPort_returnPressed();
    void on_dataApplyButton_clicked(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
};


#endif /* PLUGINS_CHANNELTX_DAEMONSRC_DAEMONSRCGUI_H_ */
