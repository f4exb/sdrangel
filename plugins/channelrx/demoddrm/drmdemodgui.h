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

#ifndef INCLUDE_DRMDEMODGUI_H
#define INCLUDE_DRMDEMODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "settings/rollupstate.h"
#include "util/messagequeue.h"

#include "drmdemod.h"
#include "drmdemodsettings.h"

#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

class DeviceUISet;
class PluginAPI;

namespace Ui { class DRMDemodGUI; }

class DRMDemodGUI : public ChannelGUI
{
    Q_OBJECT
public:
    static DRMDemodGUI* create(
        PluginAPI* pluginAPI,
        DeviceUISet* deviceUISet,
        BasebandSampleSink* rxChannel
    );
    void resetToDefaults();
    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;
    MessageQueue* getInputMessageQueue() override { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) override { m_settings.m_workspaceIndex = index; }
    int getWorkspaceIndex() const override { return m_settings.m_workspaceIndex; }
    void setGeometryBytes(const QByteArray& blob) override { m_settings.m_geometryBytes = blob; }
    QByteArray getGeometryBytes() const override { return m_settings.m_geometryBytes; }
    QString getTitle() const override { return m_settings.m_title; }
    QColor getTitleColor() const override { return QColor(m_settings.m_rgbColor); }
    void zetHidden(bool hidden) override { m_settings.m_hidden = hidden; }
    bool getHidden() const override { return m_settings.m_hidden; }
    ChannelMarker& getChannelMarker() override { return m_channelMarker; }
    int getStreamIndex() const override { return m_settings.m_streamIndex; }
    void setStreamIndex(int index) override { m_settings.m_streamIndex = index; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private slots:
    void handleInputMessages();
    void tick();
    void on_frequencyOffset_changed(qint64 value);
    void on_searchRange_valueChanged(int value);
    void on_forcedMode_currentIndexChanged(int index);
    void on_forcedOccupancy_currentIndexChanged(int index);
    void on_service_currentIndexChanged(int index);
    void on_volume_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void audioSelect(const QPoint& point);
    void on_reacquire_clicked();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void journalineLinkClicked(const QUrl& link);
    void journalineHome();
    void journalineBack();
    void journalineServiceChanged(int index);
    void onMenuDialogCalled(const QPoint& point);

private:
    friend class DRMDemodGUITest;
    explicit DRMDemodGUI(
        PluginAPI* pluginAPI,
        DeviceUISet* deviceUISet,
        BasebandSampleSink* rxChannel,
        QWidget* parent = nullptr
    );
    ~DRMDemodGUI() override;

    bool handleMessage(const Message& message);
    void applySettings(const QStringList& keys, bool force = false);
    void displaySettings();
    void applySanitizedSettings(QStringList keys);
    // Rows of the receiver status table.
    enum StatusRow
    {
        StatusSync,
        StatusMode,
        StatusOccupancy,
        StatusSpectrum,
        StatusCarrierOffset,
        StatusFAC,
        StatusSDC,
        StatusMSC,
        StatusAudio,
        StatusAudioFormat,
        StatusBroadcastTime,
        StatusSampleClock,
        StatusMSCMER,
        StatusMSCFrames,
        StatusAudioFrames,
        StatusRowCount
    };
    static QString audioFormatText(const DRMServiceStatus& service);
    static QString afsTypeName(int type);
    static QString afsFrequencies(const DRMAFSStatus& afs);
    static QString afsDetails(const DRMAFSStatus& afs);
    void setupStatusTable();
    void setStatus(StatusRow row, const QString& value);
    void setStatusToolTip(StatusRow row, const QString& toolTip);
    void displayStatus(const DRMDemodStatus& status);
    void updateSlideshow(const DRMDemodStatus& status);
    void showSlide();
    void resizeEvent(QResizeEvent* event) override;
    void updateServices(const DRMDemodStatus& status);
    void updateJournaline(const DRMDemodStatus& status);
    void updateWebsite(const DRMDemodStatus& status);
    void showJournaline();
    static QString journalineHtml(const DRMDemodStatus::JournalineObjects& objects, int objectId);
    void updateAlternativeFrequencies(const DRMDemodStatus& status);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void updateChannelMarker(const DRMDemodStatus& status);
    void blockApplySettings(bool block) { m_doApplySettings = !block; }

    Ui::DRMDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    DRMDemod* m_drmDemod;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    DRMDemodSettings m_settings;
    MessageQueue m_inputMessageQueue;
    qint64 m_deviceCenterFrequency = 0;
    int m_basebandSampleRate = 1;
    bool m_doApplySettings = true;
    bool m_audioPlaying = false;
    // The master timer ticks every 50 ms; the status refreshes every 200 ms.
    static constexpr int StatusRefreshTicks = 4;
    int m_statusTicks = StatusRefreshTicks;
    std::vector<std::uint32_t> m_displayedServices;
    QStringList m_displayedServiceLabels;
    qint64 m_displayedSelection = -2;
    QString m_displayedText;
    quint64 m_displayedSlideshowUpdates = 0;
    bool m_displayedSlideshowPresent = false;
    QPixmap m_slide;
    // Journaline browsing: the services' objects, the component shown (by
    // one of its services' short Ids), the object shown and the way back.
    std::map<int, std::shared_ptr<const DRMDemodStatus::JournalineObjects>> m_journaline;
    quint64 m_displayedJournalineUpdates = ~0ULL;
    std::vector<int> m_journalineServices;  // the selector's entries, a short Id each
    int m_journalineShortId = -1;
    int m_journalineObject = 0;
    std::vector<int> m_journalineHistory;
    QStringList m_journalineNames;    // the selector's labels
    QStringList m_journalineLabels;   // each Journaline service's label
    QString m_journalineShownHtml;    // the page shown
    quint64 m_displayedWebsiteUpdates = ~0ULL;
    QStringList m_displayedAlternativeFrequencies;
};

#endif
