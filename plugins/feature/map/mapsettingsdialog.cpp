///////////////////////////////////////////////////////////////////////////////////
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

#include <QDebug>
#include <QColorDialog>
#include <QColor>
#include <QToolButton>
#include <QFileDialog>
#include <QDir>
#include <QPointer>
#include <QStandardItemModel>
#include <QThread>

#include "util/flightpathpalette.h"
#include "maincore.h"
#include "channel/channelapi.h"

#include "mapsettingsdialog.h"
#include "gui/messagedialog.h"
#include "msfsassetconverter/batchconverter.h"

// Shown in the palette column when the track is drawn in a single colour
const QString NO_PALETTE = "None";

#if (QT_VERSION < QT_VERSION_CHECK(6, 6, 0))
#include <QtGui/private/qzipreader_p.h>
#else
#include <QtCore/private/qzipreader_p.h>
#endif

MapItemSettingsGUI::MapItemSettingsGUI(QTableWidget *table, int row, MapSettings::MapItemSettings *settings) :
    m_track2D(table, row, MapSettingsDialog::COL_2D_TRACK, !settings->m_display2DTrack, settings->m_2DTrackColor),
    m_point3D(table, row, MapSettingsDialog::COL_3D_POINT, !settings->m_display3DPoint, settings->m_3DPointColor),
    m_track3D(table, row, MapSettingsDialog::COL_3D_TRACK, !settings->m_display3DTrack, settings->m_3DTrackColor)
{
    m_trackPalette = new QComboBox(table);
    m_trackPalette->addItem(NO_PALETTE);
    m_trackPalette->addItems(FlightPathPalette::getPaletteNames());
    m_trackPalette->setCurrentText(settings->m_2DTrackPalette.isEmpty() ? NO_PALETTE : settings->m_2DTrackPalette);
    m_minZoom = new QSpinBox(table);
    m_minZoom->setRange(0, 15);
    m_minZoom->setValue(settings->m_2DMinZoom);
    m_minZoom->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_minPixels = new QSpinBox(table);
    m_minPixels->setRange(0, 200);
    m_minPixels->setValue(settings->m_3DModelMinPixelSize);
    m_minPixels->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_labelScale = new QDoubleSpinBox(table);
    m_labelScale->setDecimals(2);
    m_labelScale->setRange(0.01, 10.0);
    m_labelScale->setValue(settings->m_3DLabelScale);
    m_labelScale->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_filterDistance = new QSpinBox(table);
    m_filterDistance->setRange(0, MAX_FILTER_DISTANCE_KM);
    m_filterDistance->setValue(settings->m_filterDistance / 1000);
    m_filterDistance->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_filterDistance->setSpecialValueText(" ");
    m_filterDistance->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
    m_smoothingWindow = new QSpinBox(table);
    m_smoothingWindow->setRange(0, 1000);
    m_smoothingWindow->setValue(settings->m_smoothingWindow);
    m_smoothingWindow->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_smoothingLambda = new QDoubleSpinBox(table);
    m_smoothingLambda->setRange(0, 1e9);
    m_smoothingLambda->setValue(settings->m_smoothingLambda);
    m_smoothingLambda->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setCellWidget(row, MapSettingsDialog::COL_2D_TRACK_PALETTE, m_trackPalette);
    table->setCellWidget(row, MapSettingsDialog::COL_2D_MIN_ZOOM, m_minZoom);
    table->setCellWidget(row, MapSettingsDialog::COL_3D_MIN_PIXELS, m_minPixels);
    table->setCellWidget(row, MapSettingsDialog::COL_3D_LABEL_SCALE, m_labelScale);
    table->setCellWidget(row, MapSettingsDialog::COL_FILTER_DISTANCE, m_filterDistance);
    table->setCellWidget(row, MapSettingsDialog::COL_SMOOTHING_WINDOW, m_smoothingWindow);
    table->setCellWidget(row, MapSettingsDialog::COL_SMOOTHING_LAMBDA, m_smoothingLambda);
}

MapSettingsDialog::MapSettingsDialog(MapSettings *settings, QWidget* parent) :
    QDialog(parent),
    m_map2DSettingsChanged(false),
    m_map3DSettingsChanged(false),
    m_osmURLChanged(false),
    m_settings(settings),
    m_fileIdx(0),
    m_downloadDialog(this),
    m_progressDialog(nullptr),
    m_msfsWorker(nullptr),
    ui(new Ui::MapSettingsDialog)
{
    ui->setupUi(this);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    ui->mapProvider->clear();
    ui->mapProvider->addItem("OpenStreetMap");
#else
#ifdef WIN32
    ui->mapProvider->removeItem(ui->mapProvider->findText("MapboxGL")); // Not supported on Windows
#endif
    ui->mapProvider->removeItem(ui->mapProvider->findText("ESRI")); // Currently broken https://bugreports.qt.io/browse/QTBUG-121228
#endif
    const QString mapProviderName = MapSettings::m_mapProviderNames[MapSettings::m_mapProviders.indexOf(settings->m_mapProvider)];
    ui->mapProvider->setCurrentIndex(ui->mapProvider->findText(mapProviderName));
    ui->thunderforestAPIKey->setText(settings->m_thunderforestAPIKey);
    ui->maptilerAPIKey->setText(settings->m_maptilerAPIKey);
    ui->cartoDBAPIKey->setText(settings->m_cartoDBAPIKey);
    ui->mapBoxAPIKey->setText(settings->m_mapBoxAPIKey);
    ui->cesiumIonAPIKey->setText(settings->m_cesiumIonAPIKey);
    ui->checkWXAPIKey->setText(settings->m_checkWXAPIKey);
    ui->arcGISAPIKey->setText(settings->m_arcGISAPIKey);
    ui->osmURL->setText(settings->m_osmURL);
    ui->mapBoxStyles->setText(settings->m_mapBoxStyles);
    ui->map2DEnabled->setChecked(m_settings->m_map2DEnabled);
    ui->useLiveryIcons->setChecked(m_settings->m_useLiveryIcons);
    ui->map3DEnabled->setChecked(m_settings->m_map3DEnabled);
    ui->defaultImagery->setCurrentIndex(ui->defaultImagery->findText(m_settings->m_defaultImagery));
    ui->terrain->setCurrentIndex(ui->terrain->findText(m_settings->m_terrain));
    ui->buildings->setCurrentIndex(ui->buildings->findText(m_settings->m_buildings));
    ui->sunLightEnabled->setCurrentIndex((int)m_settings->m_sunLightEnabled);
    ui->lightIntensity->setValue(m_settings->m_lightIntensity);
    // Unlike the camera light, this applies whichever light source is in use
    ui->distantLightIntensity->setValue(m_settings->m_distantLightIntensity);
    ui->lowDetailModels->setChecked(m_settings->m_lowDetailModels);
    ui->lightIntensity->setEnabled(!m_settings->m_sunLightEnabled);
    ui->eciCamera->setCurrentIndex((int)m_settings->m_eciCamera);
    ui->fxaa->setChecked(m_settings->m_fxaa);
    ui->msaa->setCurrentText(msaaToString(m_settings->m_msaa));
    ui->terrainLighting->setChecked(m_settings->m_terrainLighting);
    ui->water->setChecked(m_settings->m_water);
    ui->googlePhotorealistic3DTiles->setChecked(m_settings->m_googlePhotorealistic3DTiles);
    ui->hdr->setChecked(m_settings->m_hdr);
    ui->fog->setChecked(m_settings->m_fog);
    ui->fps->setChecked(m_settings->m_fps);
    ui->frameRate->setValue(m_settings->m_frameRate);
    ui->mapItemSoundVolume->setValue(m_settings->m_mapItemSoundVolume);
    ui->cockpitSoundVolume->setValue(m_settings->m_cockpitSoundVolume);
    ui->msfsNavigraphDirectory->setText(QDir::toNativeSeparators(m_settings->m_msfsNavigraphDir));
    ui->msfsAircraftDirectory->setText(QDir::toNativeSeparators(m_settings->m_msfsAircraftDir));
    ui->msfsOutputDirectory->setText(QDir::toNativeSeparators(QDir(m_settings->m_modelDir).filePath("MSFS")));
    ui->msfsMaxTextureSize->setCurrentText(QString::number(m_settings->m_msfsMaxTextureSize));
    ui->msfsLowDetailMaxTextureSize->setCurrentText(QString::number(m_settings->m_msfsLowDetailMaxTextureSize));
    ui->cancelMSFS->setEnabled(false);

    // The AM Demods the RMP's VHF channels can be linked to. The dialog is rebuilt each
    // time it is opened, so the list is simply what exists now
    std::vector<ChannelAPI*> amDemods = MainCore::instance()->getChannels("sdrangel.channel.amdemod");
    for (QComboBox *combo : {ui->vhf1Channel, ui->vhf2Channel})
    {
        combo->addItem("None", "");
        for (const auto channel : amDemods)
        {
            const QString id = QString("R%1:%2").arg(channel->getDeviceSetIndex()).arg(channel->getIndexInDeviceSet());
            combo->addItem(id, id);
        }
    }
    // A linked channel that no longer exists is kept, as it may be created later on
    for (const auto& pair : {std::make_pair(ui->vhf1Channel, m_settings->m_vhf1Channel), std::make_pair(ui->vhf2Channel, m_settings->m_vhf2Channel)})
    {
        QComboBox *combo = pair.first;
        const QString& id = pair.second;
        if (!id.isEmpty() && (combo->findData(id) < 0)) {
            combo->addItem(id, id);
        }
        combo->setCurrentIndex(combo->findData(id));
    }

    // Sort groups in table alphabetically
    QList<MapSettings::MapItemSettings *> itemSettings = m_settings->m_itemSettings.values();
    std::sort(itemSettings.begin(), itemSettings.end(),
        [](const MapSettings::MapItemSettings* a, const MapSettings::MapItemSettings* b) -> bool {
            return a->m_group.compare(b->m_group, Qt::CaseInsensitive) < 0;
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
        ui->mapItemSettings->setItem(row, COL_SMOOTHING_WINDOW, item);

        item = new QTableWidgetItem();
        ui->mapItemSettings->setItem(row, COL_SMOOTHING_LAMBDA, item);

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
    connect(&m_waypoints, &Waypoints::downloadingURL, this, &MapSettingsDialog::downloadingURL);
    connect(&m_waypoints, &Waypoints::downloadError, this, &MapSettingsDialog::downloadError);
    connect(&m_waypoints, &Waypoints::downloadWaypointsFinished, this, &MapSettingsDialog::downloadWaypointsFinished);
    connect(&m_sidStar, &SidStar::downloadingURL, this, &MapSettingsDialog::downloadingURL);
    connect(&m_sidStar, &SidStar::downloadError, this, &MapSettingsDialog::downloadError);
    connect(&m_sidStar, &SidStar::downloadSidStarFinished, this, &MapSettingsDialog::downloadSidStarFinished);

#ifndef QT_WEBENGINE_FOUND
    ui->map3DSettings->setVisible(false);
    ui->downloadModels->setVisible(false);
    ui->mapItemSettings->hideColumn(COL_3D_MODEL);
    ui->mapItemSettings->hideColumn(COL_3D_MIN_PIXELS);
    ui->mapItemSettings->hideColumn(COL_3D_LABEL);
    ui->mapItemSettings->hideColumn(COL_3D_POINT);
    ui->mapItemSettings->hideColumn(COL_3D_TRACK);
    ui->mapItemSettings->hideColumn(COL_3D_LABEL_SCALE);
    ui->mapItemSettings->hideColumn(COL_SMOOTHING_WINDOW);
    ui->mapItemSettings->hideColumn(COL_SMOOTHING_LAMBDA);
#endif
}

MapSettingsDialog::~MapSettingsDialog()
{
    if (m_msfsCancelRequested) {
        m_msfsCancelRequested->store(true);
    }
    if (m_msfsWorker) {
        m_msfsWorker->wait();
    }
    delete ui;
    qDeleteAll(m_mapItemSettingsGUIs);
}

void MapSettingsDialog::accept()
{
    QString mapProvider = MapSettings::m_mapProviders[MapSettings::m_mapProviderNames.indexOf(ui->mapProvider->currentText())];
    QString osmURL = ui->osmURL->text();
    QString mapBoxStyles = ui->mapBoxStyles->text();
    QString mapBoxAPIKey = ui->mapBoxAPIKey->text();
    QString thunderforestAPIKey = ui->thunderforestAPIKey->text();
    QString maptilerAPIKey = ui->maptilerAPIKey->text();
    QString cartoDBAPIKey = ui->cartoDBAPIKey->text();
    QString cesiumIonAPIKey = ui->cesiumIonAPIKey->text();
    QString arcGISAPIKey = ui->arcGISAPIKey->text();
    QString msfsNavigraphDir = QDir::fromNativeSeparators(ui->msfsNavigraphDirectory->text().trimmed());
    if (!msfsNavigraphDir.isEmpty()) {
        msfsNavigraphDir = QDir::cleanPath(msfsNavigraphDir);
    }
    QString msfsAircraftDir = QDir::cleanPath(QDir::fromNativeSeparators(ui->msfsAircraftDirectory->text().trimmed()));
    m_osmURLChanged = osmURL != m_settings->m_osmURL;
    if ((mapProvider != m_settings->m_mapProvider)
        || (thunderforestAPIKey != m_settings->m_thunderforestAPIKey)
        || (maptilerAPIKey != m_settings->m_maptilerAPIKey)
        || (cartoDBAPIKey != m_settings->m_cartoDBAPIKey)
        || (mapBoxAPIKey != m_settings->m_mapBoxAPIKey)
        || (mapBoxStyles != m_settings->m_mapBoxStyles)
        || (osmURL != m_settings->m_osmURL))
    {
        m_settings->m_mapProvider = mapProvider;
        m_settings->m_thunderforestAPIKey = thunderforestAPIKey;
        m_settings->m_maptilerAPIKey = maptilerAPIKey;
        m_settings->m_cartoDBAPIKey = cartoDBAPIKey;
        m_settings->m_mapBoxAPIKey = mapBoxAPIKey;
        m_settings->m_osmURL = osmURL;
        m_settings->m_mapBoxStyles = mapBoxStyles;
        m_map2DSettingsChanged = true;
    }
    else
    {
        m_map2DSettingsChanged = false;
    }
    if (   (cesiumIonAPIKey != m_settings->m_cesiumIonAPIKey)
        || (arcGISAPIKey != m_settings->m_arcGISAPIKey))
    {
        m_settings->m_cesiumIonAPIKey = cesiumIonAPIKey;
        m_settings->m_arcGISAPIKey = arcGISAPIKey;
        m_map3DSettingsChanged = true;
        m_settingsKeysChanged.append("cesiumIonAPIKey");
        m_settingsKeysChanged.append("arcGISAPIKey");
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
    if (m_settings->m_useLiveryIcons != ui->useLiveryIcons->isChecked())
    {
        m_settings->m_useLiveryIcons = ui->useLiveryIcons->isChecked();
        m_settingsKeysChanged.append("useLiveryIcons");
    }
    if (m_settings->m_map3DEnabled != ui->map3DEnabled->isChecked())
    {
        m_settings->m_map3DEnabled = ui->map3DEnabled->isChecked();
        m_settingsKeysChanged.append("map3DEnabled");
    }
    if (m_settings->m_msfsNavigraphDir != msfsNavigraphDir)
    {
        m_settings->m_msfsNavigraphDir = msfsNavigraphDir;
        m_settingsKeysChanged.append("msfsNavigraphDir");
    }
    if (m_settings->m_msfsAircraftDir != msfsAircraftDir)
    {
        m_settings->m_msfsAircraftDir = msfsAircraftDir;
        m_settingsKeysChanged.append("msfsAircraftDir");
    }
    const int msfsMaxTextureSize = ui->msfsMaxTextureSize->currentText().toInt();
    if (m_settings->m_msfsMaxTextureSize != msfsMaxTextureSize)
    {
        m_settings->m_msfsMaxTextureSize = msfsMaxTextureSize;
        m_settingsKeysChanged.append("msfsMaxTextureSize");
    }
    const int msfsLowDetailMaxTextureSize = ui->msfsLowDetailMaxTextureSize->currentText().toInt();
    if (m_settings->m_msfsLowDetailMaxTextureSize != msfsLowDetailMaxTextureSize)
    {
        m_settings->m_msfsLowDetailMaxTextureSize = msfsLowDetailMaxTextureSize;
        m_settingsKeysChanged.append("msfsLowDetailMaxTextureSize");
    }
    if (m_settings->m_defaultImagery != ui->defaultImagery->currentText())
    {
        m_settings->m_defaultImagery = ui->defaultImagery->currentText();
        m_settingsKeysChanged.append("defaultImagery");
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
    if (m_settings->m_lightIntensity != ui->lightIntensity->value())
    {
        m_settings->m_lightIntensity = ui->lightIntensity->value();
        m_settingsKeysChanged.append("lightIntensity");
    }
    if (m_settings->m_distantLightIntensity != ui->distantLightIntensity->value())
    {
        m_settings->m_distantLightIntensity = ui->distantLightIntensity->value();
        m_settingsKeysChanged.append("distantLightIntensity");
    }
    if (m_settings->m_lowDetailModels != ui->lowDetailModels->isChecked())
    {
        m_settings->m_lowDetailModels = ui->lowDetailModels->isChecked();
        m_settingsKeysChanged.append("lowDetailModels");
    }
    if (m_settings->m_eciCamera != (ui->eciCamera->currentIndex() == 1))
    {
        m_settings->m_eciCamera = ui->eciCamera->currentIndex() == 1;
        m_settingsKeysChanged.append("eciCamera");
    }
    if (m_settings->m_fxaa != ui->fxaa->isChecked())
    {
        m_settings->m_fxaa = ui->fxaa->isChecked();
        m_settingsKeysChanged.append("fxaa");
    }
    if (m_settings->m_msaa != stringToMSAA(ui->msaa->currentText()))
    {
        m_settings->m_msaa = stringToMSAA(ui->msaa->currentText());
        m_settingsKeysChanged.append("msaa");
    }
    if (m_settings->m_terrainLighting != ui->terrainLighting->isChecked())
    {
        m_settings->m_terrainLighting = ui->terrainLighting->isChecked();
        m_settingsKeysChanged.append("terrainLighting");
    }
    if (m_settings->m_water != ui->water->isChecked())
    {
        m_settings->m_water = ui->water->isChecked();
        m_settingsKeysChanged.append("water");
    }
    if (m_settings->m_googlePhotorealistic3DTiles != ui->googlePhotorealistic3DTiles->isChecked())
    {
        m_settings->m_googlePhotorealistic3DTiles = ui->googlePhotorealistic3DTiles->isChecked();
        m_settingsKeysChanged.append("googlePhotorealistic3DTiles");
    }
    if (m_settings->m_hdr != ui->hdr->isChecked())
    {
        m_settings->m_hdr = ui->hdr->isChecked();
        m_settingsKeysChanged.append("hdr");
    }
    if (m_settings->m_fog != ui->fog->isChecked())
    {
        m_settings->m_fog = ui->fog->isChecked();
        m_settingsKeysChanged.append("fog");
    }
    if (m_settings->m_fps != ui->fps->isChecked())
    {
        m_settings->m_fps = ui->fps->isChecked();
        m_settingsKeysChanged.append("fps");
    }
    if (m_settings->m_frameRate != ui->frameRate->value())
    {
        m_settings->m_frameRate = ui->frameRate->value();
        m_settingsKeysChanged.append("frameRate");
    }
    if (m_settings->m_mapItemSoundVolume != ui->mapItemSoundVolume->value())
    {
        m_settings->m_mapItemSoundVolume = ui->mapItemSoundVolume->value();
        m_settingsKeysChanged.append("mapItemSoundVolume");
    }
    if (m_settings->m_cockpitSoundVolume != ui->cockpitSoundVolume->value())
    {
        m_settings->m_cockpitSoundVolume = ui->cockpitSoundVolume->value();
        m_settingsKeysChanged.append("cockpitSoundVolume");
    }
    // The 3D map need not be reloaded for these: the RMP is told its new channels
    if (m_settings->m_vhf1Channel != ui->vhf1Channel->currentData().toString())
    {
        m_settings->m_vhf1Channel = ui->vhf1Channel->currentData().toString();
        m_settingsKeysChanged.append("vhf1Channel");
    }
    if (m_settings->m_vhf2Channel != ui->vhf2Channel->currentData().toString())
    {
        m_settings->m_vhf2Channel = ui->vhf2Channel->currentData().toString();
        m_settingsKeysChanged.append("vhf2Channel");
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
        itemSettings->m_2DTrackPalette = gui->m_trackPalette->currentText() == NO_PALETTE ? "" : gui->m_trackPalette->currentText();
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
        itemSettings->m_filterDistance = gui->m_filterDistance->value() * 1000;
        itemSettings->m_smoothingWindow = gui->m_smoothingWindow->value();
        itemSettings->m_smoothingLambda = gui->m_smoothingLambda->value();
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
        ui->mapItemSettings->showColumn(COL_2D_TRACK_PALETTE);
    }
    else
    {
        ui->mapItemSettings->hideColumn(COL_2D_ICON);
        ui->mapItemSettings->hideColumn(COL_2D_LABEL);
        ui->mapItemSettings->hideColumn(COL_2D_MIN_ZOOM);
        ui->mapItemSettings->hideColumn(COL_2D_TRACK);
        ui->mapItemSettings->hideColumn(COL_2D_TRACK_PALETTE);
    }
    ui->mapProvider->setEnabled(checked);
    ui->osmURL->setEnabled(checked);
    ui->mapBoxStyles->setEnabled(checked);
    ui->useLiveryIcons->setEnabled(checked);
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
        ui->mapItemSettings->showColumn(COL_SMOOTHING_WINDOW);
        ui->mapItemSettings->showColumn(COL_SMOOTHING_LAMBDA);
    }
    else
    {
        ui->mapItemSettings->hideColumn(COL_3D_MODEL);
        ui->mapItemSettings->hideColumn(COL_3D_MIN_PIXELS);
        ui->mapItemSettings->hideColumn(COL_3D_LABEL);
        ui->mapItemSettings->hideColumn(COL_3D_POINT);
        ui->mapItemSettings->hideColumn(COL_3D_TRACK);
        ui->mapItemSettings->hideColumn(COL_3D_LABEL_SCALE);
        ui->mapItemSettings->hideColumn(COL_SMOOTHING_WINDOW);
        ui->mapItemSettings->hideColumn(COL_SMOOTHING_LAMBDA);
    }
    ui->sunLightEnabled->setEnabled(checked);
    ui->eciCamera->setEnabled(checked);
    ui->fxaa->setEnabled(checked);
    ui->msaa->setEnabled(checked);
    // Enables imagery, terrain and buildings, subject to Cesium Ion and Google 3D tiles
    updateCesiumIonItems();
}

void MapSettingsDialog::on_terrain_currentIndexChanged(int index)
{
    (void) index;

    bool ellipsoid = ui->terrain->currentText() == "Ellipsoid";
    bool googleTiles = ui->googlePhotorealistic3DTiles->isChecked();

    ui->terrainLighting->setEnabled(!ellipsoid && !googleTiles);
    ui->water->setEnabled(!ellipsoid && !googleTiles);
}

void MapSettingsDialog::on_cesiumIonAPIKey_textChanged(const QString& text)
{
    (void) text;

    updateCesiumIonItems();
}

void MapSettingsDialog::on_googlePhotorealistic3DTiles_toggled(bool checked)
{
    (void) checked;

    updateCesiumIonItems();
}

// Grey out options that would use our Cesium Ion quota, unless the user has entered
// their own Cesium Ion API key, and those that Google's 3D tiles override
void MapSettingsDialog::updateCesiumIonItems()
{
    bool hasKey = !ui->cesiumIonAPIKey->text().trimmed().isEmpty();

    ui->googlePhotorealistic3DTiles->setEnabled(hasKey && ui->map3DEnabled->isChecked());
    ui->googlePhotorealistic3DTiles->setToolTip(hasKey ?
          "Display Google Photorealistic 3D Tiles. This replaces the imagery, terrain and buildings settings and hides map layers drawn on the globe, such as rain and clouds"
        : "Requires a Cesium Ion API key");
    if (!hasKey) {
        ui->googlePhotorealistic3DTiles->setChecked(false);
    }

    updateComboItems(ui->defaultImagery, MapSettings::m_cesiumIonImagery, hasKey, "ArcGIS world imagery");
    updateComboItems(ui->terrain, MapSettings::m_cesiumIonTerrain, hasKey, "Ellipsoid");
    updateComboItems(ui->buildings, MapSettings::m_cesiumIonBuildings, hasKey, "None");

    // Google's 3D tiles include imagery, terrain and buildings, so those settings are unused
    bool googleTiles = ui->googlePhotorealistic3DTiles->isChecked();
    ui->defaultImagery->setEnabled(!googleTiles && ui->map3DEnabled->isChecked());
    ui->terrain->setEnabled(!googleTiles && ui->map3DEnabled->isChecked());
    ui->buildings->setEnabled(!googleTiles && ui->map3DEnabled->isChecked());
    on_terrain_currentIndexChanged(ui->terrain->currentIndex());
}

void MapSettingsDialog::updateComboItems(QComboBox *combo, const QStringList& ionItems, bool enable, const QString& fallback)
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(combo->model());

    if (!model)
    {
        qDebug() << "MapSettingsDialog::updateComboItems: Can't disable items in" << combo->objectName();
        return;
    }
    for (const auto& ionItem : ionItems)
    {
        int idx = combo->findText(ionItem);

        if (idx >= 0)
        {
            QStandardItem *item = model->item(idx);

            item->setEnabled(enable);
            item->setToolTip(enable ? "" : "Requires a Cesium Ion API key");
            // Don't leave a disabled item selected
            if (!enable && (combo->currentIndex() == idx))
            {
                int fallbackIdx = combo->findText(fallback);

                if (fallbackIdx >= 0) {
                    combo->setCurrentIndex(fallbackIdx);
                } else {
                    qDebug() << "MapSettingsDialog::updateComboItems: No" << fallback << "in" << combo->objectName();
                }
            }
        }
    }
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

void MapSettingsDialog::on_msfsBrowse_clicked()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        "Select the parent directory containing MSFS aircraft",
        ui->msfsAircraftDirectory->text());
    if (!directory.isEmpty()) {
        ui->msfsAircraftDirectory->setText(QDir::toNativeSeparators(directory));
    }
}

void MapSettingsDialog::on_msfsNavigraphBrowse_clicked()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        "Select the Navigraph BGL directory",
        ui->msfsNavigraphDirectory->text());
    if (!directory.isEmpty()) {
        ui->msfsNavigraphDirectory->setText(QDir::toNativeSeparators(directory));
    }
}

void MapSettingsDialog::on_convertMSFS_clicked()
{
    if (m_msfsWorker) {
        return;
    }

    const QString aircraftDirectory = QFileInfo(
        QDir::fromNativeSeparators(ui->msfsAircraftDirectory->text().trimmed())).absoluteFilePath();
    if (!QFileInfo(aircraftDirectory).isDir())
    {
        QMessageBox::critical(this, "MSFS aircraft conversion",
            QString("Aircraft directory does not exist: %1").arg(QDir::toNativeSeparators(aircraftDirectory)));
        return;
    }

    const QString outputDirectory = QDir(m_settings->m_modelDir).filePath("MSFS");
    // Read from the box rather than the settings: Convert can be pressed before OK
    const int maximumTextureSize = ui->msfsMaxTextureSize->currentText().toInt();
    const int lowDetailMaximumTextureSize = ui->msfsLowDetailMaxTextureSize->currentText().toInt();
    ui->msfsOutputDirectory->setText(QDir::toNativeSeparators(outputDirectory));
    ui->msfsLog->clear();
    ui->msfsProgress->setRange(0, 0);
    ui->convertMSFS->setEnabled(false);
    ui->cancelMSFS->setEnabled(true);
    ui->msfsBrowse->setEnabled(false);
    ui->msfsAircraftDirectory->setEnabled(false);
    ui->msfsMaxTextureSize->setEnabled(false);
    ui->msfsLowDetailMaxTextureSize->setEnabled(false);
    ui->buttonBox->setEnabled(false);

    m_msfsCancelRequested = std::make_shared<std::atomic_bool>(false);
    const auto cancelRequested = m_msfsCancelRequested;
    const QPointer<MapSettingsDialog> dialog(this);
    m_msfsWorker = QThread::create([dialog, aircraftDirectory, outputDirectory, cancelRequested,
                                    maximumTextureSize, lowDetailMaximumTextureSize] {
        MSFSBatchConversionSummary summary;
        QString error;
        const bool success = convertMSFSAircraftDirectory(
            aircraftDirectory,
            outputDirectory,
            &summary,
            &error,
            [dialog](int current, int total, const QString& message) {
                if (dialog) {
                    QMetaObject::invokeMethod(dialog.data(), [dialog, current, total, message] {
                        if (dialog) {
                            dialog->updateMSFSProgress(current, total, message);
                        }
                    }, Qt::QueuedConnection);
                }
            },
            [cancelRequested] { return cancelRequested->load(); },
            0,
            maximumTextureSize,
            lowDetailMaximumTextureSize);

        if (dialog) {
            QMetaObject::invokeMethod(dialog.data(), [dialog, success, summary, error] {
                if (dialog) {
                    dialog->msfsConversionFinished(success, summary, error);
                }
            }, Qt::QueuedConnection);
        }
    });
    connect(m_msfsWorker, &QThread::finished, this, [this] { m_msfsWorker = nullptr; });
    connect(m_msfsWorker, &QThread::finished, m_msfsWorker, &QObject::deleteLater);
    m_msfsWorker->start();
}

void MapSettingsDialog::on_cancelMSFS_clicked()
{
    if (m_msfsCancelRequested)
    {
        m_msfsCancelRequested->store(true);
        ui->cancelMSFS->setEnabled(false);
        ui->msfsLog->appendPlainText("Cancelling after the current conversion step…");
    }
}

void MapSettingsDialog::updateMSFSProgress(int current, int total, const QString& message)
{
    if (total <= 0) {
        ui->msfsProgress->setRange(0, 0);
    } else {
        ui->msfsProgress->setRange(0, total);
        ui->msfsProgress->setValue(current);
    }
    ui->msfsLog->appendPlainText(message);
}

void MapSettingsDialog::msfsConversionFinished(
    bool success, const MSFSBatchConversionSummary& summary, const QString& error)
{
    m_msfsCancelRequested.reset();
    ui->convertMSFS->setEnabled(true);
    ui->cancelMSFS->setEnabled(false);
    ui->msfsBrowse->setEnabled(true);
    ui->msfsAircraftDirectory->setEnabled(true);
    ui->msfsMaxTextureSize->setEnabled(true);
    ui->msfsLowDetailMaxTextureSize->setEnabled(true);
    ui->buttonBox->setEnabled(true);

    if (!success)
    {
        ui->msfsProgress->setRange(0, 1);
        ui->msfsProgress->setValue(0);
        ui->msfsLog->appendPlainText(QString("ERROR: %1").arg(error));
        QMessageBox::critical(this, "MSFS aircraft conversion", error);
        return;
    }

    ui->msfsProgress->setRange(0, qMax(1, summary.m_discovered));
    ui->msfsProgress->setValue(summary.m_converted + summary.m_failed);
    const QString result = summary.m_cancelled
        ? QString("Conversion cancelled. %1 of %2 aircraft variants converted; %3 failed.")
            .arg(summary.m_converted).arg(summary.m_discovered).arg(summary.m_failed)
        : QString("Conversion complete. %1 of %2 aircraft variants converted; %3 failed. Restart SDRangel to make the new models available.")
            .arg(summary.m_converted).arg(summary.m_discovered).arg(summary.m_failed);
    const QString lowDetail = QString(" %1 also got a low detail model.").arg(summary.m_lowDetailModels);
    const QString performance = QString(" Used %1 worker%2; %3 cached texture%4 reused; "
                                        "%5 missing optional texture%6 replaced; "
                                        "%7 missing fallback director%8 skipped; "
                                        "%9 unused cached texture%10 removed (%11 GB freed); "
                                        "%12 texture%13 limited to %14.")
        .arg(summary.m_workerCount)
        .arg(summary.m_workerCount == 1 ? QString() : QStringLiteral("s"))
        .arg(summary.m_reusedTextures)
        .arg(summary.m_reusedTextures == 1 ? QString() : QStringLiteral("s"))
        .arg(summary.m_synthesizedTextures)
        .arg(summary.m_synthesizedTextures == 1 ? QString() : QStringLiteral("s"))
        .arg(summary.m_skippedFallbackDirectories)
        .arg(summary.m_skippedFallbackDirectories == 1
            ? QStringLiteral("y") : QStringLiteral("ies"))
        .arg(summary.m_removedTextures)
        .arg(summary.m_removedTextures == 1 ? QString() : QStringLiteral("s"))
        .arg(summary.m_removedTextureBytes / 1e9, 0, 'f', 1)
        .arg(summary.m_downscaledTextures)
        .arg(summary.m_downscaledTextures == 1 ? QString() : QStringLiteral("s"))
        .arg(QStringLiteral("%1/%2").arg(ui->msfsMaxTextureSize->currentText(),
                                         ui->msfsLowDetailMaxTextureSize->currentText()));
    const QString resultWithPerformance = result + lowDetail + performance;
    ui->msfsLog->appendPlainText(resultWithPerformance);

    if (summary.m_cancelled) {
        QMessageBox::information(this, "MSFS aircraft conversion", resultWithPerformance);
    } else if (summary.m_failed > 0) {
        QMessageBox::warning(this, "MSFS aircraft conversion", resultWithPerformance + " See the log for details.");
    } else {
        QMessageBox::information(this, "MSFS aircraft conversion", resultWithPerformance);
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
        MessageDialog::warning(this, "Download failed", QString("Failed to download %1 to %2\n%3").arg(url).arg(filename).arg(errorMessage));
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

void MapSettingsDialog::on_getWaypoints_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        // Two steps, not one: downloadingURL() advances the bar by one, and a
        // QProgressDialog defaults to autoReset and autoClose, so a maximum of 1 would
        // have it hide itself the moment the download started. Neither of these emits a
        // byte level progress signal, so the second step is the download completing.
        m_progressDialog->setMaximum(2);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_waypoints.downloadWaypoints();
    }
}

void MapSettingsDialog::on_getSidStar_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        // Two steps, not one: downloadingURL() advances the bar by one, and a
        // QProgressDialog defaults to autoReset and autoClose, so a maximum of 1 would
        // have it hide itself the moment the download started. Neither of these emits a
        // byte level progress signal, so the second step is the download completing.
        m_progressDialog->setMaximum(2);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_sidStar.downloadSidStar();
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
    MessageDialog::critical(this, "Map", error);
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

void MapSettingsDialog::downloadWaypointsFinished()
{
    if (m_progressDialog) {
        m_progressDialog->setLabelText("Reading waypoints.");
    }
    emit waypointsUpdated();
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void MapSettingsDialog::downloadSidStarFinished()
{
    // No sidStarUpdated() to go with this, unlike the databases above: the Map draws none
    // of these itself. SIDs and STARs are read by the Aircraft feature when it expands a
    // flight plan, and SidStar::getWaypoints() re-reads the file whenever its modification
    // time has moved - so a fresh download is picked up on the next route drawn, with
    // nothing here needing to say so.
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void MapSettingsDialog::on_sunLightEnabled_currentIndexChanged(int index)
{
    ui->lightIntensity->setEnabled(index == 0);
}

QString MapSettingsDialog::msaaToString(int msaa) const
{
    if (msaa <= 1) {
        return "Off";
    } else {
        return QString::number(msaa);
    }
}

int MapSettingsDialog::stringToMSAA(const QString& string) const
{
    if (string == "Off") {
        return 1;
    } else {
        return string.toInt();
    }
}
