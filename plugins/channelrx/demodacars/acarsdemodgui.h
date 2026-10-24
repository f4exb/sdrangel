///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ACARSDEMODGUI_H
#define INCLUDE_ACARSDEMODGUI_H

#include <QIcon>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QProgressDialog>
#include <QGeoCoordinate>
#include <QtCharts>
#include <QDialog>
#include <QTableWidget>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "util/planespotters.h"
#include "settings/rollupstate.h"
#include "util/acarsmessage.h"
#include "gui/acarsmessageview.h"
#include "acarsdemodsettings.h"
#include "acarsdemodworker.h"
#include "acarsmessagemodel.h"
#include "acarsvdl2.h"
#include "acarshfdl.h"
#include "util/osndb.h"
#include "util/ourairportsdb.h"
#include "util/aircraftreport.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class AcarsDemod;
class AcarsDemodGUI;
class ScopeVis;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;   // Qt6 moved the chart classes out of this namespace
#endif

namespace Ui {
    class AcarsDemodGUI;
}
class AcarsDemodGUI;

class AcarsDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static AcarsDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::AcarsDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    AcarsDemodSettings m_settings;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;

    AcarsDemod* m_acarsDemod;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    PlaneSpotters m_planeSpotters;
    QString m_photoLink;
    QSharedPointer<const QHash<QString, AircraftInformation *>> m_aircraftInfo;
    OsnDB m_osnDB;
    QProgressDialog *m_progressDialog;

    QMenu *menu;                                // Column select context menu

    AcarsMessageModel *m_messageModel;
    AcarsMessageFilter *m_messageFilter;

    // Scrolling to the newest message is coalesced on to this timer rather than done per
    // message: measured at 41 messages a second it was 393 us each, because
    // scrollToBottom forces a synchronous relayout, and one relayout serves every
    // message that arrived in the tick
    QTimer *m_tableTimer;
    bool m_scrollPending;

    // Maps a row of the table as displayed to the model row behind it
    int sourceRow(int viewRow) const;
    QString udpToolTip() const;

    // Multipart assembly states from the worker, for the decode view
    QHash<quint64, AcarsAssemblyEvent> m_assemblies;

    // Frames received per second chart
    QChart *m_chart;
    // One series per row type, indexed by AcarsRowEvent::m_frameType, so a protocol
    // cannot be added without a series appearing for it. There used to be two series -
    // "ACARS" and "VDL-2 link" - with everything that was not an ACARS message counted
    // into the second one, so in HFDL or Aero mode the legend named the wrong protocol.
    static const int CHART_FRAME_SERIES = 4;
    static const char *m_frameSeriesNames[CHART_FRAME_SERIES];
    QLineSeries *m_frameRateSeries[CHART_FRAME_SERIES];
    QLineSeries *m_aircraftSeries;

    bool anyFrameSeriesVisible() const;
    QDateTimeAxis *m_xAxis;
    QValueAxis *m_fpsYAxis;
    QValueAxis *m_aircraftYAxis;
    int m_frameRateCount[CHART_FRAME_SERIES];
    QDateTime m_frameRateTime;
    QHash<QString, QDateTime> m_aircraftLastSeen;   // For the aircraft-seen series
    QDateTime m_averageTime;                        // Up to when old chart data has been averaged
    QPoint m_chartPanPos;
    bool m_chartPanning;

    // HFDL ground stations and the frequencies they announced in squitters
    struct GsHeard
    {
        QSet<int> m_frequencies;    // kHz
        QDateTime m_lastHeard;
    };
    QHash<int, GsHeard> m_gsHeard;

    QDialog *m_gsDialog = nullptr;
    QTableWidget *m_gsTableWidget = nullptr;

    explicit AcarsDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~AcarsDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updateModeDependentWidgets();
    void applyModeBandwidth();
    void updateChannelMarker();
    void updateGsTable();
    void sendGroundStationToMap(const QString& name, float latitude, float longitude, const QString& text, QDateTime dateTime);
    bool handleMessage(const Message& message);
    void plotChart();
    void resetChartAxes();
    static void averageSeries(QLineSeries *series, const QDateTime& startTime, const QDateTime& endTime);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void updatePhotoText(const QString& registration);
    void hideAircraftDetails();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);
    virtual bool eventFilter(QObject *obj, QEvent *event) override;

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_mode_currentIndexChanged(int value);
    void on_aeroChannel_currentIndexChanged(int value);
    void on_iridiumBandwidth_currentIndexChanged(int value);
    void on_gsTable_clicked();
    void gsTableContextMenu(QPoint pos);
    void on_rfBW_valueChanged(int index);
    void on_threshold_valueChanged(int value);
    void on_filter_currentIndexChanged(int index);
    void on_filterPattern_editingFinished();
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void udpSettings(const QPoint& p);
    void on_getOSNDB_clicked();
    void applyFilter();
    void restoreSplitter();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_displayChart_clicked(bool checked);
    void on_noInfo_clicked(bool checked);
    void updateMessagesTable();
    void setShowDate(bool showDate);
    void clearChart(const QPoint& p);
    void legendMarkerClicked();
    void on_channel1_currentIndexChanged(int index);
    void on_channel2_currentIndexChanged(int index);
    void messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void messages_sectionResized(int logicalIndex, int oldSize, int newSize);
    void messagesSelectionChanged();
    void customContextMenuRequested(QPoint point);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadProgress(qint64 bytesRead, qint64 totalBytes);
    void downloadAircraftInformationFinished();
    void aircraftPhoto(const PlaneSpottersPhoto *photo);
    void photoClicked();

    // Display events from the channel's worker, which does all the decode
    void rowReady(const AcarsRowEvent& event);
    void assemblyUpdated(const AcarsAssemblyEvent& event);
};

#endif // INCLUDE_ACARSDEMODGUI_H
