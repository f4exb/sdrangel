///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_FEATURE_MAPSETTINGSDIALOG_H
#define INCLUDE_FEATURE_MAPSETTINGSDIALOG_H

#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QThread>

#include <atomic>
#include <memory>

#include "gui/httpdownloadmanagergui.h"
#include "gui/tablecolorchooser.h"
#include "util/openaip.h"
#include "util/ourairportsdb.h"
#include "util/waypoints.h"
#include "util/sidstar.h"

#include "ui_mapsettingsdialog.h"
#include "mapsettings.h"

struct MSFSBatchConversionSummary;

class MapItemSettingsGUI : public QObject {
    Q_OBJECT
public:

    MapItemSettingsGUI(QTableWidget *table, int row, MapSettings::MapItemSettings *settings);

    TableColorChooser m_track2D;
    TableColorChooser m_point3D;
    TableColorChooser m_track3D;
    QComboBox *m_trackPalette;
    QSpinBox *m_minZoom;
    QSpinBox *m_minPixels;
    QDoubleSpinBox *m_labelScale;
    QSpinBox *m_filterDistance;
    QSpinBox *m_smoothingWindow;
    QDoubleSpinBox *m_smoothingLambda;
};

class MapSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit MapSettingsDialog(MapSettings *settings, QWidget* parent = 0);
    ~MapSettingsDialog();

    enum Columns {
        COL_ENABLED,
        COL_2D_ICON,
        COL_2D_LABEL,
        COL_2D_MIN_ZOOM,
        COL_2D_TRACK,
        COL_2D_TRACK_PALETTE,
        COL_3D_MODEL,
        COL_3D_MIN_PIXELS,
        COL_3D_LABEL,
        COL_3D_POINT,
        COL_3D_TRACK,
        COL_3D_LABEL_SCALE,
        COL_FILTER_NAME,
        COL_FILTER_DISTANCE,
        COL_SMOOTHING_WINDOW,
        COL_SMOOTHING_LAMBDA
    };

public:
    bool m_map2DSettingsChanged;    // 2D map needs to be reloaded
    bool m_map3DSettingsChanged;    // 3D map needs to be reloaded
    bool m_osmURLChanged;
    QStringList m_settingsKeysChanged; // List of setting keys that have been changed

private:
    MapSettings *m_settings;
    QList<MapItemSettingsGUI *> m_mapItemSettingsGUIs;
    HttpDownloadManagerGUI m_dlm;
    int m_fileIdx;
    QMessageBox m_downloadDialog;
    QProgressDialog *m_progressDialog;
    OpenAIP m_openAIP;
    OurAirportsDB m_ourAirportsDB;
    Waypoints m_waypoints;
    SidStar m_sidStar;
    QPointer<QThread> m_msfsWorker;
    std::shared_ptr<std::atomic_bool> m_msfsCancelRequested;

    void unzip(const QString &filename);
    QString msaaToString(int msaa) const;
    int stringToMSAA(const QString& string) const;
    void updateCesiumIonItems();
    void updateComboItems(QComboBox *combo, const QStringList& ionItems, bool enable, const QString& fallback);
    void updateMSFSProgress(int current, int total, const QString& message);
    void msfsConversionFinished(bool success, const MSFSBatchConversionSummary& summary, const QString& error);

private slots:
    void accept();
    void on_map2DEnabled_clicked(bool checked=false);
    void on_map3DEnabled_clicked(bool checked=false);
    void on_terrain_currentIndexChanged(int index);
    void on_cesiumIonAPIKey_textChanged(const QString& text);
    void on_googlePhotorealistic3DTiles_toggled(bool checked);
    void on_downloadModels_clicked();
    void on_msfsNavigraphBrowse_clicked();
    void on_msfsBrowse_clicked();
    void on_convertMSFS_clicked();
    void on_cancelMSFS_clicked();
    void on_getAirportDB_clicked();
    void on_getAirspacesDB_clicked();
    void on_getWaypoints_clicked();
    void on_getSidStar_clicked();
    void on_sunLightEnabled_currentIndexChanged(int index);
    void downloadComplete(const QString &filename, bool success, const QString &url, const QString &errorMessage);
    void downloadingURL(const QString& url);
    void downloadProgress(qint64 bytesRead, qint64 totalBytes);
    void downloadError(const QString& error);
    void downloadAirspaceFinished();
    void downloadNavAidsFinished();
    void downloadAirportInformationFinished();
    void downloadWaypointsFinished();
    void downloadSidStarFinished();

signals:
    void navAidsUpdated();
    void airspacesUpdated();
    void airportsUpdated();
    void waypointsUpdated();

private:
    Ui::MapSettingsDialog* ui;

};

#endif // INCLUDE_FEATURE_MAPSETTINGSDIALOG_H
