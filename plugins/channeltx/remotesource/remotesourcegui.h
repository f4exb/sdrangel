///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCGUI_H_
#define PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCGUI_H_

#include <QElapsedTimer>

#include "dsp/channelmarker.h"
#include "channel/channelgui.h"
#include "util/messagequeue.h"

#include "../remotesource/remotesourcesettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class RemoteSource;

namespace Ui {
    class RemoteSourceGUI;
}

class RemoteSourceGUI : public ChannelGUI {
    Q_OBJECT

public:
    static RemoteSourceGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::RemoteSourceGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RemoteSourceSettings m_settings;
    int m_remoteSampleRate;
    bool m_doApplySettings;

    RemoteSource* m_remoteSrc;
    MessageQueue m_inputMessageQueue;

    uint32_t m_countUnrecoverable;
    uint32_t m_countRecovered;
    uint32_t m_lastCountUnrecoverable;
    uint32_t m_lastCountRecovered;
    uint32_t m_lastSampleCount;
    uint64_t m_lastTimestampUs;
    bool m_resetCounts;
    QElapsedTimer m_time;
    uint32_t m_tickCount;

    explicit RemoteSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~RemoteSourceGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayStreamIndex();
    bool handleMessage(const Message& message);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

    void displayEventCounts();
    void displayEventStatus(int recoverableCount, int unrecoverableCount);
    void displayEventTimer();

private slots:
    void handleSourceMessages();
    void on_dataAddress_returnPressed();
    void on_dataPort_returnPressed();
    void on_dataApplyButton_clicked(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void on_eventCountsReset_clicked(bool checked);
    void tick();
};


#endif /* PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCGUI_H_ */
