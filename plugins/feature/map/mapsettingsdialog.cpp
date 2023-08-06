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

#include <QDebug>
#include <QColorDialog>
#include <QColor>
#include <QToolButton>
#include <QFileDialog>

#if (QT_VERSION < QT_VERSION_CHECK(6, 6, 0))
#include <QtGui/private/qzipreader_p.h>
#else
#include <QtCore/private/qzipreader_p.h>
#endif

#include "util/units.h"

#include "mapsettingsdialog.h"
#include "maplocationdialog.h"
#include "mapcolordialog.h"

static QString rgbToColor(quint32 rgb)
{
    QColor color = QColor::fromRgba(rgb);
    return QString("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
}

static QString backgroundCSS(quint32 rgb)
{
    // Must specify a border, otherwise we end up with a gradient instead of solid background
    return QString("QToolButton { background-color: rgb(%1); border: none; }").arg(rgbToColor(rgb));
}

static QString noColorCSS()
{
    return "QToolButton { background-color: black; border: none; }";
}

MapColorGUI::MapColorGUI(QTableWidget *table, int row, int col, bool noColor, quint32 color) :
    m_noColor(noColor),
    m_color(color)
{
    m_colorButton = new QToolButton(table);
    m_colorButton->setFixedSize(22, 22);
    if (!m_noColor)
    {
        m_colorButton->setStyleSheet(backgroundCSS(m_color));
    }
    else
    {
        m_colorButton->setStyleSheet(noColorCSS());
        m_colorButton->setText("-");
    }
    table->setCellWidget(row, col, m_colorButton);
    connect(m_colorButton, &QToolButton::clicked, this, &MapColorGUI::on_color_clicked);
}

void MapColorGUI::on_color_clicked()
{
    MapColorDialog dialog(QColor::fromRgba(m_color), m_colorButton);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_noColor = dialog.noColorSelected();
        if (!m_noColor)
        {
            m_colorButton->setText("");
            m_color = dialog.selectedColor().rgba();
            m_colorButton->setStyleSheet(backgroundCSS(m_color));
        }
        else
        {
            m_colorButton->setText("-");
            m_colorButton->setStyleSheet(noColorCSS());
        }
    }
}

MapItemSettingsGUI::MapItemSettingsGUI(QTableWidget *table, int row, MapSettings::MapItemSettings *settings) :
    m_track2D(table, row, MapSettingsDialog::COL_2D_TRACK, !settings->m_display2DTrack, settings->m_2DTrackColor),
    m_point3D(table, row, MapSettingsDialog::COL_3D_POINT, !settings->m_display3DPoint, settings->m_3DPointColor),
    m_track3D(table, row, MapSettingsDialog::COL_3D_TRACK, !settings->m_display3DTrack, settings->m_3DTrackColor)
{
    m_minZoom = new QSpinBox(table);
    m_minZoom->setRange(0, 15);
    m_minZoom->setValue(settings->m_2DMinZoom);
    m_minPixels = new QSpinBox(table);
    m_minPixels->setRange(0, 200);
    m_minPixels->setValue(settings->m_3DModelMinPixelSize);
    m_labelScale = new QDoubleSpinBox(table);
    m_labelScale->setDecimals(2);
    m_labelScale->setRange(0.01, 10.0);
    m_labelScale->setValue(settings->m_3DLabelScale);
    table->setCellWidget(row, MapSettingsDialog::COL_2D_MIN_ZOOM, m_minZoom);
    table->setCellWidget(row, MapSettingsDialog::COL_3D_MIN_PIXELS, m_minPixels);
    table->setCellWidget(row, MapSettingsDialog::COL_3D_LABEL_SCALE, m_labelScale);
}

MapSettingsDialog::MapSettingsDialog(MapSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_settings(settings),
    m_downloadDialog(this),
    m_progressDialog(nullptr),
    ui(new Ui::MapSettingsDialog)
{
    ui->setupUi(this);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    ui->mapProvider->clear();
    ui->mapProvider->addItem("OpenStreetMap");
#endif
    ui->mapProvider->setCurrentIndex(MapSettings::m_mapProviders.indexOf(settings->m_mapProvider));
    ui->thunderforestAPIKey->setText(settings->m_thunderforestAPIKey);
    ui->maptilerAPIKey->setText(settings->m_maptilerAPIKey);
    ui->mapBoxAPIKey->setText(settings->m_mapBoxAPIKey);
    ui->cesiumIonAPIKey->setText(settings->m_cesiumIonAPIKey);
    ui->checkWXAPIKey->setText(settings->m_checkWXAPIKey);
    ui->osmURL->setText(settings->m_osmURL);
    ui->mapBoxStyles->setText(settings->m_mapBoxStyles);
    ui->map2DEnabled->setChecked(m_settings->m_map2DEnabled);
    ui->map3DEnabled->setChecked(m_settings->m_map3DEnabled);
    ui->terrain->setCurrentIndex(ui->terrain->findText(m_settings->m_terrain));
    ui->buildings->setCurrentIndex(ui->buildings->findText(m_settings->m_buildings));
    ui->sunLightEnabled->setCurrentIndex((int)m_settings->m_sunLightEnabled);
    ui->eciCamera->setCurrentIndex((int)m_settings->m_eciCamera);
    ui->antiAliasing->setCurrentIndex(ui->antiAliasing->findText(m_settings->m_antiAliasing));

    // Sort groups in table alphabetically
    QList<MapSettings::MapItemSettings *> itemSettings = m_settings->m_itemSettings.values();
    std::sort(itemSettings.begin(), itemSettings.end(),
        [](const MapSettings::MapItemSettings* a, const MapSettings::MapItemSettings* b) -> bool {
            return a->m_group < b->m_group;
        });
    QListIterator<MapSettings::MapItemSettings *> itr(itemSettings);
    while (itr.hasNext())
    {
        MapSettings::MapItemSettings *itemSettings = itr.next();

        // Add row to table with header
        int row = ui->mapItemSettings->rowCount();
        ui->mapItemSettings->setRowCount(row + 1);
        QTableWidgetItem *header = new QTableWidgetItem(itemSettings->m_group);
        ui->mapItemSettings->setVerticalHeaderItem(row, header);

        QTableWidgetItem *item;
        item = new QTableWidgetItem();
        item->setCheckState(itemSettings->m_enabled ? Qt::Checked : Qt::Unchecked);
        ui->mapItemSettings->setItem(row, COL_ENABLED, item);

        item = new QTableWidgetItem();
        item->setCheckState(itemSettings->m_display2DIcon ? Qt::Checked : Qt::Unchecked);
        ui->mapItemSettings->setItem(row, COL_2D_ICON, item);
        item = new QTableWidgetItem();
        item->setCheckState(itemSettings->m_display2DLabel ? Qt::Checked : Qt::Unchecked);
        ui->mapItemSettings->setItem(row, COL_2D_LABEL, item);

        item = new QTableWidgetItem();
        item->setCheckState(itemSettings->m_display3DModel ? Qt::Checked : Qt::Unchecked);
        ui->mapItemSettings->setItem(row, COL_3D_MODEL, item);
        item = new QTableWidgetItem();
        item->setCheckState(itemSettings->m_display3DLabel ? Qt::Checked : Qt::Unchecked);
        ui->mapItemSettings->setItem(row, COL_3D_LABEL, item);

        item = new QTableWidgetItem(itemSettings->m_filterName);
        ui->mapItemSettings->setItem(row, COL_FILTER_NAME, item);
        item = new QTableWidgetItem();
        if (itemSettings->m_filterDistance > 0) {
            item->setText(QString::number(itemSettings->m_filterDistance / 1000));
        }
        ui->mapItemSettings->setItem(row, COL_FILTER_DISTANCE, item);

        MapItemSettingsGUI *gui = new MapItemSettingsGUI(ui->mapItemSettings, row, itemSettings);
        m_mapItemSettingsGUIs.append(gui);
    }
    ui->mapItemSettings->resizeColumnsToContents();

    on_map2DEnabled_clicked(m_settings->m_map2DEnabled);
    on_map3DEnabled_clicked(m_settings->m_map3DEnabled);

    connect(&m_dlm, &HttpDownloadManagerGUI::downloadComplete, this, &MapSettingsDialog::downloadComplete);
    connect(&m_openAIP, &OpenAIP::downloadingURL, this, &MapSettingsDialog::downloadingURL);
    connect(&m_openAIP, &OpenAIP::downloadError, this, &MapSettingsDialog::downloadError);
    connect(&m_openAIP, &OpenAIP::downloadAirspaceFinished, this, &MapSettingsDialog::downloadAirspaceFinished);
    connect(&m_openAIP, &OpenAIP::downloadNavAidsFinished, this, &MapSettingsDialog::downloadNavAidsFinished);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadingURL, this, &MapSettingsDialog::downloadingURL);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadProgress, this, &MapSettingsDialog::downloadProgress);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadError, this, &MapSettingsDialog::downloadError);
    connect(&m_ourAirportsDB, &OurAirportsDB::downloadAirportInformationFinished, this, &MapSettingsDialog::downloadAirportInformationFinished);

#ifndef QT_WEBENGINE_FOUND
    ui->map3DSettings->setVisible(false);
    ui->downloadModels->setVisible(false);
    ui->mapItemSettings->hideColumn(COL_3D_MODEL);
    ui->mapItemSettings->hideColumn(COL_3D_MIN_PIXELS);
    ui->mapItemSettings->hideColumn(COL_3D_LABEL);
    ui->mapItemSettings->hideColumn(COL_3D_POINT);
    ui->mapItemSettings->hideColumn(COL_3D_TRACK);
    ui->mapItemSettings->hideColumn(COL_3D_LABEL_SCALE);
#endif
}

MapSettingsDialog::~MapSettingsDialog()
{
    delete ui;
    qDeleteAll(m_mapItemSettingsGUIs);
}

void MapSettingsDialog::accept()
{
    QString mapProvider = MapSettings::m_mapProviders[ui->mapProvider->currentIndex()];
    QString osmURL = ui->osmURL->text();
    QString mapBoxStyles = ui->mapBoxStyles->text();
    QString mapBoxAPIKey = ui->mapBoxAPIKey->text();
    QString thunderforestAPIKey = ui->thunderforestAPIKey->text();
    QString maptilerAPIKey = ui->maptilerAPIKey->text();
    QString cesiumIonAPIKey = ui->cesiumIonAPIKey->text();
    m_osmURLChanged = osmURL != m_settings->m_osmURL;
    if ((mapProvider != m_settings->m_mapProvider)
        || (thunderforestAPIKey != m_settings->m_thunderforestAPIKey)
        || (maptilerAPIKey != m_settings->m_maptilerAPIKey)
        || (mapBoxAPIKey != m_settings->m_mapBoxAPIKey)
        || (mapBoxStyles != m_settings->m_mapBoxStyles)
        || (osmURL != m_settings->m_osmURL))
    {
        m_settings->m_mapProvider = mapProvider;
        m_settings->m_thunderforestAPIKey = thunderforestAPIKey;
        m_settings->m_maptilerAPIKey = maptilerAPIKey;
        m_settings->m_mapBoxAPIKey = mapBoxAPIKey;
        m_settings->m_osmURL = osmURL;
        m_settings->m_mapBoxStyles = mapBoxStyles;
        m_settings->m_cesiumIonAPIKey = cesiumIonAPIKey;
        m_map2DSettingsChanged = true;
    }
    else
    {
        m_map2DSettingsChanged = false;
    }
    if (cesiumIonAPIKey != m_settings->m_cesiumIonAPIKey)
    {
        m_settings->m_cesiumIonAPIKey = cesiumIonAPIKey;
        m_map3DSettingsChanged = true;
    }
    else
    {
        m_map3DSettingsChanged = false;
    }

    if (m_settings->m_map2DEnabled != ui->map2DEnabled->isChecked())
    {
        m_settings->m_map2DEnabled = ui->map2DEnabled->isChecked();
        m_settingsKeysChanged.append("map2DEnabled");
    }
    if (m_settings->m_map3DEnabled != ui->map3DEnabled->isChecked())
    {
        m_settings->m_map3DEnabled = ui->map3DEnabled->isChecked();
        m_settingsKeysChanged.append("map3DEnabled");
    }
    if (m_settings->m_terrain != ui->terrain->currentText())
    {
        m_settings->m_terrain = ui->terrain->currentText();
        m_settingsKeysChanged.append("terrain");
    }
    if (m_settings->m_buildings != ui->buildings->currentText())
    {
        m_settings->m_buildings = ui->buildings->currentText();
        m_settingsKeysChanged.append("buildings");
    }
    if (m_settings->m_sunLightEnabled != (ui->sunLightEnabled->currentIndex() == 1))
    {
        m_settings->m_sunLightEnabled = ui->sunLightEnabled->currentIndex() == 1;
        m_settingsKeysChanged.append("sunLightEnabled");
    }
    if (m_settings->m_eciCamera != (ui->eciCamera->currentIndex() == 1))
    {
        m_settings->m_eciCamera = ui->eciCamera->currentIndex() == 1;
        m_settingsKeysChanged.append("eciCamera");
    }
    if (m_settings->m_antiAliasing != ui->antiAliasing->currentText())
    {
        m_settings->m_antiAliasing = ui->antiAliasing->currentText();
        m_settingsKeysChanged.append("antiAliasing");
    }

    for (int row = 0; row < ui->mapItemSettings->rowCount(); row++)
    {
        MapSettings::MapItemSettings *itemSettings = m_settings->m_itemSettings[ui->mapItemSettings->verticalHeaderItem(row)->text()];
        MapItemSettingsGUI *gui = m_mapItemSettingsGUIs[row];

        itemSettings->m_enabled = ui->mapItemSettings->item(row, COL_ENABLED)->checkState() == Qt::Checked;
        itemSettings->m_display2DIcon = ui->mapItemSettings->item(row, COL_2D_ICON)->checkState() == Qt::Checked;
        itemSettings->m_display2DLabel = ui->mapItemSettings->item(row, COL_2D_LABEL)->checkState() == Qt::Checked;
        itemSettings->m_display2DTrack = !gui->m_track2D.m_noColor;
        itemSettings->m_2DTrackColor = gui->m_track2D.m_color;
        itemSettings->m_2DMinZoom = gui->m_minZoom->value();
        itemSettings->m_display3DModel = ui->mapItemSettings->item(row, COL_3D_MODEL)->checkState() == Qt::Checked;
        itemSettings->m_display3DLabel = ui->mapItemSettings->item(row, COL_3D_LABEL)->checkState() == Qt::Checked;
        itemSettings->m_display3DPoint = !gui->m_point3D.m_noColor;
        itemSettings->m_3DPointColor = gui->m_point3D.m_color;
        itemSettings->m_display3DTrack = !gui->m_track3D.m_noColor;
        itemSettings->m_3DTrackColor = gui->m_track3D.m_color;
        itemSettings->m_3DModelMinPixelSize = gui->m_minPixels->value();
        itemSettings->m_3DLabelScale = gui->m_labelScale->value();
        itemSettings->m_filterName = ui->mapItemSettings->item(row, COL_FILTER_NAME)->text();
        itemSettings->m_filterNameRE.setPattern(itemSettings->m_filterName);
        itemSettings->m_filterNameRE.optimize();
        bool ok;
        int filterDistance = ui->mapItemSettings->item(row, COL_FILTER_DISTANCE)->text().toInt(&ok);
        if (ok && filterDistance > 0) {
            itemSettings->m_filterDistance = filterDistance * 1000;
        } else {
            itemSettings->m_filterDistance = 0;
        }
    }

    QDialog::accept();
}

void MapSettingsDialog::on_map2DEnabled_clicked(bool checked)
{
    if (checked)
    {
        ui->mapItemSettings->showColumn(COL_2D_ICON);
        ui->mapItemSettings->showColumn(COL_2D_LABEL);
        ui->mapItemSettings->showColumn(COL_2D_MIN_ZOOM);
        ui->mapItemSettings->showColumn(COL_2D_TRACK);
    }
    else
    {
        ui->mapItemSettings->hideColumn(COL_2D_ICON);
        ui->mapItemSettings->hideColumn(COL_2D_LABEL);
        ui->mapItemSettings->hideColumn(COL_2D_MIN_ZOOM);
        ui->mapItemSettings->hideColumn(COL_2D_TRACK);
    }
    ui->mapProvider->setEnabled(checked);
    ui->osmURL->setEnabled(checked);
    ui->mapBoxStyles->setEnabled(checked);
}

void MapSettingsDialog::on_map3DEnabled_clicked(bool checked)
{
    if (checked)
    {
        ui->mapItemSettings->showColumn(COL_3D_MODEL);
        ui->mapItemSettings->showColumn(COL_3D_MIN_PIXELS);
        ui->mapItemSettings->showColumn(COL_3D_LABEL);
        ui->mapItemSettings->showColumn(COL_3D_POINT);
        ui->mapItemSettings->showColumn(COL_3D_TRACK);
        ui->mapItemSettings->showColumn(COL_3D_LABEL_SCALE);
    }
    else
    {
        ui->mapItemSettings->hideColumn(COL_3D_MODEL);
        ui->mapItemSettings->hideColumn(COL_3D_MIN_PIXELS);
        ui->mapItemSettings->hideColumn(COL_3D_LABEL);
        ui->mapItemSettings->hideColumn(COL_3D_POINT);
        ui->mapItemSettings->hideColumn(COL_3D_TRACK);
        ui->mapItemSettings->hideColumn(COL_3D_LABEL_SCALE);
    }
    ui->terrain->setEnabled(checked);
    ui->buildings->setEnabled(checked);
    ui->sunLightEnabled->setEnabled(checked);
    ui->eciCamera->setEnabled(checked);
    ui->antiAliasing->setEnabled(checked);
}

// Models have individual licensing. See LICENSE on github
#define SDRANGEL_3D_MODELS "https://github.com/srcejon/sdrangel-3d-models/releases/latest/download/sdrangel3dmodels.zip"
// Textures from Bluebell CSL - https://github.com/oktal3700/bluebell
// These are Copyrighted by their authors and shouldn't be uploaded to any other sites
#define BB_AIRBUS_PNG   "https://drive.google.com/uc?export=download&id=10fFhflgWXCu7hmd8wqNdXw1qHJ6ecz9Z"
#define BB_BOEING_PNG   "https://drive.google.com/uc?export=download&id=1OA3pmAp5jqrjP7kRS1z_zNNyi_iLu9z_"
#define BB_GA_PNG       "https://drive.google.com/uc?export=download&id=1TZsvlLqT5x3KLkiqtN8LzAzoLxeYTA-1"
#define BB_HELI_PNG     "https://drive.google.com/uc?export=download&id=1qB2xDVHdooLeLKCPyVnVDDHRlhPVpUYs"
#define BB_JETS_PNG     "https://drive.google.com/uc?export=download&id=1v1fzTpyjjfcXyoT7vHjnyvuwqrSQzPrg"
#define BB_MIL_PNG      "https://drive.google.com/uc?export=download&id=1lI-2bAVVxhKvel7_suGVdkky4BQDQE9n"
#define BB_PROPS_PNG    "https://drive.google.com/uc?export=download&id=1fD8YxKsa9P_z2gL1aM97ZEN-HoI28SLE"

static QStringList urls = {
    SDRANGEL_3D_MODELS,
    BB_AIRBUS_PNG,
    BB_BOEING_PNG,
    BB_GA_PNG,
    BB_HELI_PNG,
    BB_JETS_PNG,
    BB_MIL_PNG,
    BB_PROPS_PNG
};

static QStringList files = {
    "sdrangel3dmodels.zip",
    "bb_airbus_png.zip",
    "bb_boeing_png.zip",
    "bb_ga_png.zip",
    "bb_heli_png.zip",
    "bb_jets_png.zip",
    "bb_mil_png.zip",
    "bb_props_png.zip"
};

void MapSettingsDialog::unzip(const QString &filename)
{
    QZipReader reader(filename);
    if (!reader.extractAll(m_settings->m_modelDir)) {
        qWarning() << "MapSettingsDialog::unzip - Failed to extract files from " << filename << " to " << m_settings->m_modelDir;
    } else {
        qDebug() << "MapSettingsDialog::unzip - Unzipped " << filename << " to " << m_settings->m_modelDir;
    }
}

void MapSettingsDialog::on_downloadModels_clicked()
{
    m_downloadDialog.setText("Downloading 3D models");
    m_downloadDialog.setStandardButtons(QMessageBox::NoButton);
    Qt::WindowFlags flags = m_downloadDialog.windowFlags();
    flags |= Qt::CustomizeWindowHint;
    flags &= ~Qt::WindowCloseButtonHint;
    m_downloadDialog.setWindowFlags(flags);
    m_downloadDialog.open();
    m_fileIdx = 0;

    QUrl url(urls[m_fileIdx]);
    QString filename = HttpDownloadManager::downloadDir() + "/" + files[m_fileIdx];

    m_dlm.download(url, filename, this);
}

void MapSettingsDialog::downloadComplete(const QString &filename, bool success, const QString &url, const QString &errorMessage)
{
    if (success)
    {
        // Unzip
        if (filename.endsWith(".zip"))
        {
            unzip(filename);

            if (filename.endsWith("bb_boeing_png.zip"))
            {
                // Copy missing textures
                // These are wrong, but prevents cesium from stopping rendering
                // Waiting on: https://github.com/oktal3700/bluebell/issues/63
                if (!QFile::copy(m_settings->m_modelDir + "/BB_Boeing_png/B77L/B77L_LIT.png", m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png")) {
                    qDebug() << "Failed to copy " + m_settings->m_modelDir + "/BB_Boeing_png/B77L/B77L_LIT.png" + " to " + m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png";
                }
                if (!QFile::copy(m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png", m_settings->m_modelDir + "/BB_Boeing_png/B77W/B773_LIT.png")) {
                    qDebug() << "Failed to copy " + m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png" + " to " + m_settings->m_modelDir + "/BB_Boeing_png/B77W/B773_LIT.png";
                }
                if (!QFile::copy(m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png", m_settings->m_modelDir + "/BB_Boeing_png/B773/B773_LIT.png")) {
                    qDebug() << "Failed to copy " + m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png" + " to " + m_settings->m_modelDir + "/BB_Boeing_png/B773/B773_LIT.png";
                }
                if (!QFile::copy(m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png", m_settings->m_modelDir + "/BB_Boeing_png/B788/B788_LIT.png")) {
                    qDebug() << "Failed to copy " + m_settings->m_modelDir + "/BB_Boeing_png/B772/B772_LIT.png" + " to " + m_settings->m_modelDir + "/BB_Boeing_png/B788/B788_LIT.png";
                }
                if (!QFile::copy(m_settings->m_modelDir + "/BB_Boeing_png/B752/B75F_LIT.png", m_settings->m_modelDir + "/BB_Boeing_png/B752/B752_LIT.png")) {
                    qDebug() << "Failed to copy " + m_settings->m_modelDir + "/BB_Boeing_png/B752/B75F_LIT.png" + " to " + m_settings->m_modelDir + "/BB_Boeing_png/B752/B752_LIT.png";
                }
            }
        }

        m_fileIdx++;

        // Download next file
        if (m_fileIdx < urls.size())
        {
            QUrl url(urls[m_fileIdx]);
            QString filename = HttpDownloadManager::downloadDir() + "/" + files[m_fileIdx];

            m_dlm.download(url, filename, this);
        }
        else
        {
            m_downloadDialog.reject();
        }
    }
    else
    {
        m_downloadDialog.reject();
        QMessageBox::warning(this, "Download failed", QString("Failed to download %1 to %2\n%3").arg(url).arg(filename).arg(errorMessage));
    }
}

void MapSettingsDialog::on_getAirportDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_ourAirportsDB.downloadAirportInformation();
    }
}

void MapSettingsDialog::on_getAirspacesDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setMaximum(OpenAIP::m_countryCodes.size());
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_openAIP.downloadAirspaces();
    }
}

void MapSettingsDialog::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void MapSettingsDialog::downloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void MapSettingsDialog::downloadError(const QString& error)
{
    QMessageBox::critical(this, "Map", error);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void MapSettingsDialog::downloadAirspaceFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading airspaces.");
    }
    emit airspacesUpdated();
    m_openAIP.downloadNavAids();
}

void MapSettingsDialog::downloadNavAidsFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading NAVAIDs.");
    }
    emit navAidsUpdated();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void MapSettingsDialog::downloadAirportInformationFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading airports.");
    }
    emit airportsUpdated();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

