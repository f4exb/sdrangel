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

#ifndef INCLUDE_FEATURE_SATELLITETRACKERGUI_H_
#define INCLUDE_FEATURE_SATELLITETRACKERGUI_H_

#include <QTimer>
#include <QMenu>
#include <QtCharts>
#include <QTextToSpeech>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "satellitetrackersettings.h"
#include "satnogs.h"

class PluginAPI;
class FeatureUISet;
class SatelliteTracker;
struct SatelliteState;

namespace Ui {
    class SatelliteTrackerGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class SatelliteTrackerGUI : public FeatureGUI {
    Q_OBJECT
public:
    static SatelliteTrackerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

private:
    Ui::SatelliteTrackerGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    SatelliteTrackerSettings m_settings;
    RollupState m_rollupState;
    bool m_doApplySettings;

    SatelliteTracker* m_satelliteTracker;
    MessageQueue m_inputMessageQueue;
    QTimer m_statusTimer;
    int m_lastFeatureState;
    bool m_lastUpdatingSatData;

    QHash<QString, SatNogsSatellite *> m_satellites;
    SatelliteState *m_targetSatState;

    int m_plotPass;

    QChart m_emptyChart;
    QChart *m_lineChart;
    QPolarChart *m_polarChart;

    QDateTime m_nextTargetAOS;
    QDateTime m_nextTargetLOS;
    bool m_geostationarySatVisible;

    QTextToSpeech *m_speech;
    QMenu *menu;                        // Column select context menu

    enum SatCol {
        SAT_COL_NAME,
        SAT_COL_AZ,
        SAT_COL_EL,
        SAT_COL_TNE,
        SAT_COL_DUR,
        SAT_COL_AOS,
        SAT_COL_LOS,
        SAT_COL_MAX_EL,
        SAT_COL_DIR,
        SAT_COL_LATITUDE,
        SAT_COL_LONGITUDE,
        SAT_COL_ALT,
        SAT_COL_RANGE,
        SAT_COL_RANGE_RATE,
        SAT_COL_DOPPLER,
        SAT_COL_PATH_LOSS,
        SAT_COL_DELAY,
        SAT_COL_NORAD_ID
    };

    explicit SatelliteTrackerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~SatelliteTrackerGUI();

    void aos(const QString &speech);
    void los(const QString &speech);

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void setTarget(const QString& target);
    QString convertDegreesToText(double degrees);
    bool handleMessage(const Message& message);
    void plotChart();
    void plotAzElChart();
    void plotPolarChart();
    void resizeTable();
    void updateTable(SatelliteState *satState);
    void updateSelectedSats();
    QAction *createCheckableItem(QString& text, int idx, bool checked);
    void updateTimeToAOS();
    QString formatDaysTime(qint64 days, QDateTime dateTime);
    QString formatSecondsAsHHMMSS(qint64 seconds);
    void updateDeviceFeatureCombo();
    void updateDeviceFeatureCombo(const QStringList &items, const QString &selected);
    void updateFileInputList();
    void updateMapList();
    void makeUIConnections();

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_useMyPosition_clicked(bool checked=false);
    void on_latitude_valueChanged(double value);
    void on_longitude_valueChanged(double value);
    void on_target_currentTextChanged(const QString &text);
    void on_displaySettings_clicked();
    void on_radioControl_clicked();
    void on_dateTimeSelect_currentIndexChanged(int index);
    void on_dateTime_dateTimeChanged(const QDateTime &datetime);
    void updateStatus();
    void on_viewOnMap_clicked();
    void on_updateSatData_clicked();
    void on_selectSats_clicked();
    void on_autoTarget_clicked(bool checked);
    void on_chartSelect_currentIndexChanged(int index);
    void on_nextPass_clicked();
    void on_prevPass_clicked();
    void on_darkTheme_clicked(bool checked);
    void on_satTable_cellDoubleClicked(int row, int column);
    void satTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void satTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void on_satTableHeader_sortIndicatorChanged(int logicalIndex, Qt::SortOrder order);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void on_deviceFeatureSelect_currentIndexChanged(int index);
};


#endif // INCLUDE_FEATURE_SATELLITETRACKERGUI_H_
