///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_WEFAXDEMODGUI_H
#define INCLUDE_WEFAXDEMODGUI_H

#include <QImage>
#include <QPoint>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "settings/rollupstate.h"
#include "util/messagequeue.h"
#include "wefaxdecoder.h"
#include "wefaxdemodsettings.h"

class BasebandSampleSink;
class DeviceUISet;
class PluginAPI;
class WefaxDemod;
namespace Ui {
    class WefaxDemodGUI;
}

class WefaxDemodGUI : public ChannelGUI
{
    Q_OBJECT

public:
    static WefaxDemodGUI *create(
        PluginAPI *pluginAPI,
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel);

    void destroy() { delete this; }
    void resetToDefaults() override;
    QByteArray serialize() const override { return m_settings.serialize(); }
    bool deserialize(const QByteArray& data) override;
    MessageQueue *getInputMessageQueue() override { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) override { m_settings.m_workspaceIndex = index; }
    int getWorkspaceIndex() const override { return m_settings.m_workspaceIndex; }
    void setGeometryBytes(const QByteArray& blob) override { m_settings.m_geometryBytes = blob; }
    QByteArray getGeometryBytes() const override { return m_settings.m_geometryBytes; }
    QString getTitle() const override { return m_settings.m_title; }
    QColor getTitleColor() const override { return m_settings.m_rgbColor; }
    void zetHidden(bool hidden) override { m_settings.m_hidden = hidden; }
    bool getHidden() const override { return m_settings.m_hidden; }
    ChannelMarker& getChannelMarker() override { return m_channelMarker; }
    int getStreamIndex() const override { return m_settings.m_streamIndex; }
    void setStreamIndex(int streamIndex) override { m_settings.m_streamIndex = streamIndex; }

private:
    Ui::WefaxDemodGUI *ui;
    PluginAPI *m_pluginAPI;
    DeviceUISet *m_deviceUISet;
    WefaxDemod *m_wefaxDemod;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    WefaxDemodSettings m_settings;
    MessageQueue m_inputMessageQueue;
    bool m_doApplySettings;
    int m_basebandSampleRate;
    qint64 m_deviceCenterFrequency;
    uint32_t m_tickCount;
    WefaxDecoder::State m_decoderState;
    QImage m_image;
    bool m_panning;
    QPoint m_panStartPosition;
    int m_panStartHorizontal;
    int m_panStartVertical;

    explicit WefaxDemodGUI(
        PluginAPI *pluginAPI,
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel,
        QWidget *parent = nullptr);
    ~WefaxDemodGUI() override;

    void makeUIConnections();
    void displaySettings();
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void sendAction(int action);
    bool handleMessage(const Message& message);
    void updateStartStopControl();
    void updateAbsoluteCenterFrequency();
    // newRows: the decoder delivered more of the image, so Follow may scroll
    // to it. Redrawing for an adjustment keeps the current view.
    void updateImage(bool newRows = false);
    void updatePanCursor();
    int effectiveDisplayZoomPercent() const;
    void setDisplayZoomPercent(int percent);
    void stepDisplayZoom(int direction);
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void handleInputMessages();
    void tick();
    void onWidgetRolled(QWidget *widget, bool rollDown);
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();
    void chooseSaveFile();
    void chooseAutoSaveDirectory();
    void onMenuDialogCalled(const QPoint& position);
};

#endif // INCLUDE_WEFAXDEMODGUI_H
