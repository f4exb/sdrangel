///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 SDRangel Contributors                                      //
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

#ifndef PLUGINS_CHANNELTX_MODCW_CWMODGUI_H
#define PLUGINS_CHANNELTX_MODCW_CWMODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "cwmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class CWMod;

/**
 * Placeholder GUI class for the CW modulator channel plugin.
 *
 * A full GUI implementation with waveform display, WPM control,
 * and text input would be built here following the pattern of
 * other modulator plugins (e.g. RttyModGUI).
 */
class CWModGUI : public ChannelGUI
{
    Q_OBJECT

public:
    static CWModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel);
    virtual void destroy() { delete this; }

    void resetToDefaults() final;
    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;
    MessageQueue *getInputMessageQueue() final { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) final { m_settings.m_workspaceIndex = index; }
    int getWorkspaceIndex() const final { return m_settings.m_workspaceIndex; }
    void setGeometryBytes(const QByteArray& blob) final { m_settings.m_geometryBytes = blob; }
    QByteArray getGeometryBytes() const final { return m_settings.m_geometryBytes; }
    QString getTitle() const final { return m_settings.m_title; }
    QColor getTitleColor() const final { return m_settings.m_rgbColor; }
    void zetHidden(bool hidden) final { m_settings.m_hidden = hidden; }
    bool getHidden() const final { return m_settings.m_hidden; }
    ChannelMarker& getChannelMarker() final { return m_channelMarker; }
    int getStreamIndex() const final { return m_settings.m_streamIndex; }
    void setStreamIndex(int streamIndex) final { m_settings.m_streamIndex = streamIndex; }

    bool handleMessage(const Message& message) final;

private:
    explicit CWModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel, QWidget* parent = nullptr);
    ~CWModGUI() override = default;

    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    CWModSettings m_settings;
    MessageQueue m_inputMessageQueue;
    CWMod* m_cwMod;
};

#endif // PLUGINS_CHANNELTX_MODCW_CWMODGUI_H
