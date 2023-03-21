///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_FEATURE_MAPSETTINGSDIALOG_H
#define INCLUDE_FEATURE_MAPSETTINGSDIALOG_H

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QProgressDialog>

#include "gui/httpdownloadmanagergui.h"
#include "util/openaip.h"
#include "util/ourairportsdb.h"

#include "ui_mapsettingsdialog.h"
#include "mapsettings.h"

class MapColorGUI : public QObject {
    Q_OBJECT
public:

    MapColorGUI(QTableWidget *table, int row, int col, bool noColor, quint32 color);

public slots:
    void on_color_clicked();

private:
    QToolButton *m_colorButton;

public:
    // Have copies of settings, so we don't change unless main dialog is accepted
    bool m_noColor;
    quint32 m_color;

};

class MapItemSettingsGUI : public QObject {
    Q_OBJECT
public:

    MapItemSettingsGUI(QTableWidget *table, int row, MapSettings::MapItemSettings *settings);

    MapColorGUI m_track2D;
    MapColorGUI m_point3D;
    MapColorGUI m_track3D;
    QSpinBox *m_minZoom;
    QSpinBox *m_minPixels;
    QDoubleSpinBox *m_labelScale;
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
        COL_3D_MODEL,
        COL_3D_MIN_PIXELS,
        COL_3D_LABEL,
        COL_3D_POINT,
        COL_3D_TRACK,
        COL_3D_LABEL_SCALE,
        COL_FILTER_NAME,
        COL_FILTER_DISTANCE
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

    void unzip(const QString &filename);

private slots:
    void accept();
    void on_map2DEnabled_clicked(bool checked=false);
    void on_map3DEnabled_clicked(bool checked=false);
    void on_downloadModels_clicked();
    void on_getAirportDB_clicked();
    void on_getAirspacesDB_clicked();
    void downloadComplete(const QString &filename, bool success, const QString &url, const QString &errorMessage);
    void downloadingURL(const QString& url);
    void downloadProgress(qint64 bytesRead, qint64 totalBytes);
    void downloadError(const QString& error);
    void downloadAirspaceFinished();
    void downloadNavAidsFinished();
    void downloadAirportInformationFinished();

signals:
    void navAidsUpdated();
    void airspacesUpdated();
    void airportsUpdated();

private:
    Ui::MapSettingsDialog* ui;

};

#endif // INCLUDE_FEATURE_MAPSETTINGSDIALOG_H
