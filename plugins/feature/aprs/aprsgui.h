///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_APRSGUI_H_
#define INCLUDE_FEATURE_APRSGUI_H_

#include <QTimer>
#include <QList>
#include <QAbstractListModel>
#include <QMenu>
#include <QtCharts>
#include <QDateTime>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/aprs.h"
#include "settings/rollupstate.h"

#include "aprssettings.h"

class PluginAPI;
class FeatureUISet;
class APRS;

namespace Ui {
    class APRSGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class APRSGUI;

class APRSStation {
public:

    APRSStation(QString& station) :
        m_station(station),
        m_isObject(false),
        m_hasWeather(false),
        m_hasTelemetry(false),
        m_hasCourseAndSpeed(false)
    {
    }

    void addPacket(APRSPacket *packet)
    {
        m_packets.append(packet);
    }

private:
    friend APRSGUI;
    QString m_station;                  // Station callsign
    QList <APRSPacket *> m_packets;     // Packets received for the station
    QString m_symbolImage;
    QString m_latestStatus;
    QString m_latestComment;
    QString m_latestPosition;
    QString m_latestAltitude;
    QString m_latestCourse;
    QString m_latestSpeed;
    QString m_latestPacket;
    QString m_powerWatts;
    QString m_antennaHeightFt;
    QString m_antennaGainDB;
    QString m_antennaDirectivity;
    QString m_radioRange;
    bool m_isObject;                    // Is an object or item rather than a station
    QString m_reportingStation;
    QList <QString> m_telemetryNames;
    QList <QString> m_telemetryLabels;
    double m_telemetryCoefficientsA[5];
    double m_telemetryCoefficientsB[5];
    double m_telemetryCoefficientsC[5];
    int m_hasTelemetryCoefficients;
    int m_telemetryBitSense[8];
    bool m_hasTelemetryBitSense;
    QString m_telemetryProjectName;
    bool m_hasWeather;
    bool m_hasTelemetry;
    bool m_hasCourseAndSpeed;
};

class APRSGUI : public FeatureGUI {
    Q_OBJECT
public:
    static APRSGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

protected:
    void resizeEvent(QResizeEvent* size);

private:
    Ui::APRSGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    APRSSettings m_settings;
    RollupState m_rollupState;
    bool m_doApplySettings;
    QList<APRSSettings::AvailableChannel> m_availableChannels;

    APRS* m_aprs;
    MessageQueue m_inputMessageQueue;
    QTimer m_statusTimer;
    int m_lastFeatureState;

    QHash<QString,APRSStation *> m_stations;    // All stations we've recieved packets for. Hashed on callsign

    QMenu *packetsTableMenu;                    // Column select context menus
    QMenu *weatherTableMenu;
    QMenu *statusTableMenu;
    QMenu *messagesTableMenu;
    QMenu *telemetryTableMenu;
    QMenu *motionTableMenu;

    QChart m_weatherChart;
    QDateTimeAxis m_weatherChartXAxis;
    QValueAxis m_weatherChartYAxis;

    QChart m_telemetryChart;
    QDateTimeAxis m_telemetryChartXAxis;
    QValueAxis m_telemetryChartYAxis;

    QChart m_motionChart;
    QDateTimeAxis m_motionChartXAxis;
    QValueAxis m_motionChartYAxis;

    explicit APRSGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~APRSGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displayTableSettings(QTableWidget *table, QMenu *menu, int *columnIndexes, int *columnSizes, int columns);
    bool filterStation(APRSStation *station);
    void filterStations();
    void displaySettings();
    void updateChannelList();
    bool handleMessage(const Message& message);
    void makeUIConnections();

    void filterMessageRow(int row);
    void filterMessages();
    void resizeTable();
    QAction *packetsTable_createCheckableItem(QString& text, int idx, bool checked);
    QAction *weatherTable_createCheckableItem(QString& text, int idx, bool checked);
    QAction *statusTable_createCheckableItem(QString& text, int idx, bool checked);
    QAction *messagesTable_createCheckableItem(QString& text, int idx, bool checked);
    QAction *telemetryTable_createCheckableItem(QString& text, int idx, bool checked);
    QAction *motionTable_createCheckableItem(QString& text, int idx, bool checked);

    void updateSummary(APRSStation *station);
    void addPacketToGUI(APRSStation *station, APRSPacket *aprs);
    void setUnits();
    QString convertAltitude(const QString& altitude);
    float convertAltitude(float altitude);
    QString convertSpeed(const QString& speed);
    int convertSpeed(int speed);
    int convertTemperature(int temperature);
    int convertRainfall(int rainfall);

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_stationFilter_currentIndexChanged(int index);
    void on_stationSelect_currentIndexChanged(int index);
    void on_filterAddressee_editingFinished();
    void on_deleteMessages_clicked();
    QDateTime calcTimeLimit(int timeSelectIdx);
    void calcTimeAxis(int timeSelectIdx, QDateTimeAxis *axis, QLineSeries *series, int width);
    void calcYAxis(double minValue, double maxValue, QValueAxis *axis, bool binary=false, int precision=1);
    QString formatDate(APRSPacket *aprs);
    QString formatTime(APRSPacket *aprs);
    double applyCoefficients(int idx, int value, APRSStation *station);
    void plotWeather();
    void on_weatherTimeSelect_currentIndexChanged(int index);
    void on_weatherPlotSelect_currentIndexChanged(int index);
    void plotTelemetry();
    void on_telemetryTimeSelect_currentIndexChanged(int index);
    void on_telemetryPlotSelect_currentIndexChanged(int index);
    void plotMotion();
    void on_motionTimeSelect_currentIndexChanged(int index);
    void on_motionPlotSelect_currentIndexChanged(int index);
    void updateStatus();
    void packetsTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void packetsTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void packetsTable_columnSelectMenu(QPoint pos);
    void packetsTable_columnSelectMenuChecked(bool checked = false);
    void weatherTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void weatherTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void weatherTable_columnSelectMenu(QPoint pos);
    void weatherTable_columnSelectMenuChecked(bool checked = false);
    void statusTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void statusTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void statusTable_columnSelectMenu(QPoint pos);
    void statusTable_columnSelectMenuChecked(bool checked = false);
    void messagesTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void messagesTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void messagesTable_columnSelectMenu(QPoint pos);
    void messagesTable_columnSelectMenuChecked(bool checked = false);
    void telemetryTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void telemetryTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void telemetryTable_columnSelectMenu(QPoint pos);
    void telemetryTable_columnSelectMenuChecked(bool checked = false);
    void motionTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void motionTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void motionTable_columnSelectMenu(QPoint pos);
    void motionTable_columnSelectMenuChecked(bool checked = false);
    void on_displaySettings_clicked();
    void on_igate_toggled(bool checked);
    void on_viewOnMap_clicked();
};


#endif // INCLUDE_FEATURE_APRSGUI_H_
