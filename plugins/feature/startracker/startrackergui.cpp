///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <cmath>
#include <algorithm>
#include <QMessageBox>
#include <QLineEdit>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QGraphicsScene>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#include "SWGStarTrackerDisplaySettings.h"
#include "SWGStarTrackerDisplayLoSSettings.h"

#include "feature/featureset.h"
#include "feature/featureuiset.h"
#include "feature/featureutils.h"
#include "feature/featurewebapiutils.h"
#include "channel/channelwebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dmsspinbox.h"
#include "gui/graphicsviewzoom.h"
#include "gui/dialogpositioner.h"
#include "util/units.h"
#include "util/astronomy.h"
#include "util/interpolation.h"
#include "util/png.h"
#include "util/profiler.h"
#include "maincore.h"

#include "ui_startrackergui.h"
#include "startracker.h"
#include "startrackergui.h"
#include "startrackersettingsdialog.h"

StarTrackerGUI* StarTrackerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    StarTrackerGUI* gui = new StarTrackerGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void StarTrackerGUI::destroy()
{
    qDeleteAll(m_lineOfSightMarkers);
    delete this;
}

void StarTrackerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray StarTrackerGUI::serialize() const
{
    return m_settings.serialize();
}

bool StarTrackerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool StarTrackerGUI::handleMessage(const Message& message)
{
    if (StarTracker::MsgConfigureStarTracker::match(message))
    {
        qDebug("StarTrackerGUI::handleMessage: StarTracker::MsgConfigureStarTracker");
        const StarTracker::MsgConfigureStarTracker& cfg = (StarTracker::MsgConfigureStarTracker&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();

        return true;
    }
    else if (StarTrackerReport::MsgReportAzAl::match(message))
    {
        StarTrackerReport::MsgReportAzAl& azAl = (StarTrackerReport::MsgReportAzAl&) message;
        blockApplySettings(true);
        blockPlotChart();
        ui->azimuth->setValue(azAl.getAzimuth());
        ui->elevation->setValue(azAl.getElevation());
        unblockPlotChartAndPlot();
        blockApplySettings(false);
        return true;
    }
    else if (StarTrackerReport::MsgReportRADec::match(message))
    {
        StarTrackerReport::MsgReportRADec& raDec = (StarTrackerReport::MsgReportRADec&) message;
        QString target = raDec.getTarget();
        if (target == "target")
        {
            m_settings.m_ra = Units::decimalHoursToHoursMinutesAndSeconds(raDec.getRA());
            m_settings.m_dec = Units::decimalDegreesToDegreeMinutesAndSeconds(raDec.getDec());
            blockPlotChart();
            ui->rightAscension->setText(m_settings.m_ra);
            ui->declination->setText(m_settings.m_dec);
            unblockPlotChartAndPlot();
        }
        else if (target == "sun")
        {
            m_sunRA = raDec.getRA();
            m_sunDec = raDec.getDec();
        }
        else if (target == "moon")
        {
            m_moonRA = raDec.getRA();
            m_moonDec = raDec.getDec();
        }
        raDecChanged();
        return true;
    }
    else if (StarTrackerReport::MsgReportAzElVsTime::match(message))
    {
        StarTrackerReport::MsgReportAzElVsTime& azElVsTime = (StarTrackerReport::MsgReportAzElVsTime&) message;
        m_azElVsTimeTarget = azElVsTime.getTarget();
        m_azimuths = azElVsTime.getAzimuths();
        m_elevations = azElVsTime.getElevations();
        m_dateTimes = azElVsTime.getDateTimes();
        plotChart();
        return true;
    }
    else if (StarTrackerReport::MsgReportGalactic::match(message))
    {
        StarTrackerReport::MsgReportGalactic& galactic = (StarTrackerReport::MsgReportGalactic&) message;
        blockApplySettings(true);
        blockPlotChart();
        ui->galacticLongitude->setValue(galactic.getL());
        ui->galacticLatitude->setValue(galactic.getB());
        unblockPlotChartAndPlot();
        blockApplySettings(false);
        return true;
    }
    else if (StarTrackerReport::MsgReportSolarSystemPositions::match(message))
    {
        StarTrackerReport::MsgReportSolarSystemPositions& report = (StarTrackerReport::MsgReportSolarSystemPositions&) message;
        updateSolarSystemPositions(report.getNames(), report.getPositions(), report.getOrbit());
        return true;
    }
    else if (StarTrackerReport::MsgReportJupiter::match(message))
    {
        StarTrackerReport::MsgReportJupiter& report = (StarTrackerReport::MsgReportJupiter&) message;
        ui->jupiterElevation->setText(QString("%1%2").arg((int) std::round(report.getElevation())).arg(QChar(0xb0)));
        ui->cml->setText(QString("%1%2").arg((int) std::round(report.getCML())).arg(QChar(0xb0)));
        ui->ioPhase->setText(QString("%1%2").arg((int) std::round(report.getIoPhase())).arg(QChar(0xb0)));
        ui->ganymedePhase->setText(QString("%1%2").arg((int) std::round(report.getGanymedePhase())).arg(QChar(0xb0)));
        updateJupiterMoonPosition(report.getCML(), report.getIoPhase(), report.getGanymedePhase());
        return true;
    }
    else if (StarTrackerReport::MsgReportJupiterData::match(message))
    {
        StarTrackerReport::MsgReportJupiterData& report = (StarTrackerReport::MsgReportJupiterData&) message;
        updateJupiterMoonPositions(report);
        return true;
    }
    else if (MainCore::MsgStarTrackerDisplaySettings::match(message))
    {
        if (m_settings.m_link)
        {
            MainCore::MsgStarTrackerDisplaySettings& settings = (MainCore::MsgStarTrackerDisplaySettings&) message;
            SWGSDRangel::SWGStarTrackerDisplaySettings *swgSettings = settings.getSWGStarTrackerDisplaySettings();
            ui->dateTimeSelect->setCurrentText("Custom");
            QDateTime dt = QDateTime::fromString(*swgSettings->getDateTime(), Qt::ISODateWithMs);
            ui->dateTime->setDateTime(dt);
            ui->target->setCurrentText("Custom Az/El");
            blockPlotChart();
            ui->azimuth->setValue(swgSettings->getAzimuth());
            ui->elevation->setValue(swgSettings->getElevation());
            unblockPlotChartAndPlot();
        }
        return true;
    }
    else if (MainCore::MsgStarTrackerDisplayLoSSettings::match(message))
    {
        MainCore::MsgStarTrackerDisplayLoSSettings& settings = (MainCore::MsgStarTrackerDisplayLoSSettings&) message;
        SWGSDRangel::SWGStarTrackerDisplayLoSSettings *swgSettings = settings.getSWGStarTrackerDisplayLoSSettings();
        bool found = false;
        for (int i = 0; i < m_lineOfSightMarkers.size(); i++)
        {
            if (m_lineOfSightMarkers[i]->m_name == *swgSettings->getName())
            {
                if (swgSettings->getD() == 0.0)
                {
                    // Delete
                    ui->image->scene()->removeItem(m_lineOfSightMarkers[i]->m_text);
                    delete m_lineOfSightMarkers[i]->m_text;
                    delete m_lineOfSightMarkers[i];
                    m_lineOfSightMarkers.removeAt(i);
                }
                else
                {
                    // Update
                    m_lineOfSightMarkers[i]->m_l = swgSettings->getL();
                    m_lineOfSightMarkers[i]->m_b = swgSettings->getB();
                    m_lineOfSightMarkers[i]->m_d = swgSettings->getD();
                    plotGalacticMarker(m_lineOfSightMarkers[i]);
                }
                found = true;
                break;
            }
        }
        if (!found && (swgSettings->getD() != 0.0))
        {
            // Create new
            LoSMarker* marker = new LoSMarker();
            marker->m_name = *swgSettings->getName();
            marker->m_l = swgSettings->getL();
            marker->m_b = swgSettings->getB();
            marker->m_d = swgSettings->getD();
            marker->m_text = ui->image->scene()->addText(marker->m_name);
            m_lineOfSightMarkers.append(marker);
            plotGalacticMarker(marker);
        }
        return true;
    }
    else if (StarTracker::MsgReportAvailableFeatures::match(message))
    {
        StarTracker::MsgReportAvailableFeatures& report = (StarTracker::MsgReportAvailableFeatures&) message;
        updateFeatureList(report.getFeatures());
        return true;
    }

    return false;
}

void StarTrackerGUI::updateFeatureList(const AvailableChannelOrFeatureList& features)
{
    // Update list of plugins we can get target from
    ui->target->blockSignals(true);

    if (m_settings.m_targetSource == "SDRangel")
    {
        // Remove targets no longer available
        for (int i = 0; i < ui->target->count(); )
        {
            QString text = ui->target->itemText(i);
            bool found = false;
            if (text.contains("SatelliteTracker") || text.contains("SkyMap"))
            {
                for (const auto& feature : features)
                {
                    if (feature.getLongId() == text)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    ui->target->removeItem(i);
                } else {
                    i++;
                }
            }
            else
            {
                i++;
            }
        }

        // Add new targets
        for (const auto& feature : features)
        {
            QString name = feature.getLongId();
            if (ui->target->findText(name) == -1) {
                ui->target->addItem(name);
            }
        }

        // Features can be created after this plugin, so select it
        // if the chosen tracker appears
        int index = ui->target->findText(m_settings.m_target);
        if (index >= 0) {
            ui->target->setCurrentIndex(index);
        }
    }
    else
    {
        // Save feature list for use in updateTargetList
        m_availableFeatures.clear();
        for (const auto& feature : features) {
            m_availableFeatures.append(feature.getLongId());
        }
    }

    ui->target->blockSignals(false);
}

void StarTrackerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void StarTrackerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

StarTrackerGUI::StarTrackerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::StarTrackerGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_doPlotChart(true),
    m_lastFeatureState(0),
    m_azElLineChart(nullptr),
    m_azElPolarChart(nullptr),
    m_solarFluxChart(nullptr),
    m_networkManager(nullptr),
    m_startAfterDownload(false),
    m_jplHorizons(nullptr),
    m_spiceEphemerides(this),
    m_solarFlux(0.0),
    m_solarFluxesValid(false),
    m_images{QImage(":/startracker/startracker/150mhz_ra_dec.png"),
        QImage(":/startracker/startracker/150mhz_galactic.png"),
        QImage(":/startracker/startracker/408mhz_ra_dec.png"),
        QImage(":/startracker/startracker/408mhz_galactic.png"),
        QImage(":/startracker/startracker/1420mhz_ra_dec.png"),
        QImage(":/startracker/startracker/1420mhz_galactic.png")},
    m_zoom(nullptr),
    m_milkyWayImages{QPixmap(":/startracker/startracker/milkyway.png"),
        QPixmap(":/startracker/startracker/milkywayannotated.png")},
    m_solarSystemLabelFontMetrics(font()),
    m_planetImages{
        {"callisto", QPixmap(":/startracker/startracker/callisto-250.png")},
        {"deimos", QPixmap(":/startracker/startracker/deimos-250.png")},
        {"earth", QPixmap(":/startracker/startracker/earth-250.png")},
        {"ganymede", QPixmap(":/startracker/startracker/ganymede-250.png")},
        {"io", QPixmap(":/startracker/startracker/io-250.png")},
        {"jupiter", QPixmap(":/startracker/startracker/jupiter-250.png")},
        {"mars", QPixmap(":/startracker/startracker/mars-250.png")},
        {"mercury", QPixmap(":/startracker/startracker/mercury-250.png")},
        {"moon", QPixmap(":/startracker/startracker/moon-250.png")},
        {"neptune", QPixmap(":/startracker/startracker/neptune-250.png")},
        {"phobos", QPixmap(":/startracker/startracker/phobos-250.png")},
        {"pluto", QPixmap(":/startracker/startracker/pluto-250.png")},
        {"saturn", QPixmap(":/startracker/startracker/saturn-250.png")},
        {"sun", QPixmap(":/startracker/startracker/sun-250.png")},
        {"uranus", QPixmap(":/startracker/startracker/uranus-250.png")},
        {"venus", QPixmap(":/startracker/startracker/venus-250.png")}
    },
    m_jupiterImages{QPixmap(":/startracker/startracker/io-phase-vs-cml.png"),
        QPixmap(":/startracker/startracker/ganymede-phase-vs-cml.png"),
        QPixmap(":/startracker/startracker/phase-cml-legend.png")
    },
    m_sunRA(0.0),
    m_sunDec(0.0),
    m_moonRA(0.0),
    m_moonDec(0.0)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/startracker/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_starTracker = reinterpret_cast<StarTracker*>(feature);
    m_starTracker->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &StarTrackerGUI::downloadFinished);

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(1000);

    connect(&m_redrawTimer, &QTimer::timeout, this, &StarTrackerGUI::plotChart);

    connect(ui->azimuth, SIGNAL(valueChanged(double)), this, SLOT(on_azimuth_valueChanged(double)));
    ui->azimuth->setRange(0, 360.0);
    ui->elevation->setRange(-90.0, 90.0);
    ui->galacticLongitude->setRange(0, 360.0);
    ui->galacticLatitude->setRange(-90.0, 90.0);
    ui->galacticLatitude->setText("");
    ui->galacticLongitude->setText("");

    // Initialise chart
    m_chart.legend()->hide();
    ui->chart->setChart(&m_chart);
    ui->chart->setRenderHint(QPainter::Antialiasing);
    m_chart.addAxis(&m_chartXAxis, Qt::AlignBottom);
    m_chart.addAxis(&m_chartYAxis, Qt::AlignLeft);
    m_chart.layout()->setContentsMargins(0, 0, 0, 0);
    m_chart.setMargins(QMargins(1, 1, 1, 1));

    // Create axes that are static

    m_skyTempGalacticLXAxis.setTitleText(QString("Galactic longitude (%1)").arg(QChar(0xb0)));
    m_skyTempGalacticLXAxis.setMin(0);
    m_skyTempGalacticLXAxis.setMax(360);
    m_skyTempGalacticLXAxis.append("180", 0);
    m_skyTempGalacticLXAxis.append("90", 90);
    m_skyTempGalacticLXAxis.append("0/360", 180);
    m_skyTempGalacticLXAxis.append("270", 270);
    //m_skyTempGalacticLXAxis.append("180", 360); // Note - labels need to be unique, so can't have 180 at start and end
    m_skyTempGalacticLXAxis.setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    m_skyTempGalacticLXAxis.setGridLineVisible(false);

    m_skyTempRAXAxis.setTitleText(QString("Right ascension (hours)"));
    m_skyTempRAXAxis.setMin(0);
    m_skyTempRAXAxis.setMax(24);
    m_skyTempRAXAxis.append("12", 0);
    m_skyTempRAXAxis.append("9", 3);
    m_skyTempRAXAxis.append("6", 6);
    m_skyTempRAXAxis.append("3", 9);
    m_skyTempRAXAxis.append("0", 12);
    m_skyTempRAXAxis.append("21", 15);
    m_skyTempRAXAxis.append("18", 18);
    m_skyTempRAXAxis.append("15", 21);
    //m_skyTempRAXAxis.append("12", 24); // Note - labels need to be unique, so can't have 12 at start and end
    m_skyTempRAXAxis.setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
    m_skyTempRAXAxis.setGridLineVisible(false);

    m_skyTempYAxis.setGridLineVisible(false);
    m_skyTempYAxis.setRange(-90.0, 90.0);
    m_skyTempYAxis.setGridLineVisible(false);

    ui->dateTime->setDateTime(QDateTime::currentDateTime());
    displaySettings();
    applySettings(true);
    disconnect(ui->azimuth, SIGNAL(valueChanged(double)), this, SLOT(on_azimuth_valueChanged(double)));
    makeUIConnections();
    m_resizer.enableChildMouseTracking();

    // Populate subchart menu
    on_chartSelect_currentIndexChanged(0);

    connect(&m_chart, SIGNAL(plotAreaChanged(QRectF)), this, SLOT(plotAreaChanged(QRectF)));

    // Use My Position from preferences, if none set
    if ((m_settings.m_latitude == 0.0) && (m_settings.m_longitude == 0.0)) {
        on_useMyPosition_clicked();
    }

/*
    printf("saemundsson=[");
    for (int i = 0; i <= 90; i+= 5)
        printf("%f ", Astronomy::refractionSaemundsson(i, m_settings.m_pressure, m_settings.m_temperature));
    printf("];\n");
    printf("palRadio=[");
    for (int i = 0; i <= 90; i+= 5)
        printf("%f ", Astronomy::refractionPAL(i, m_settings.m_pressure, m_settings.m_temperature, m_settings.m_humidity,
                                                100000000, m_settings.m_latitude, m_settings.m_heightAboveSeaLevel,
                                                m_settings.m_temperatureLapseRate));
    printf("];\n");
    printf("palLight=[");
    for (int i = 0; i <= 90; i+= 5)
        printf("%f ",Astronomy::refractionPAL(i, m_settings.m_pressure, m_settings.m_temperature, m_settings.m_humidity,
                                                7.5e14, m_settings.m_latitude, m_settings.m_heightAboveSeaLevel,
                                                m_settings.m_temperatureLapseRate));
    printf("];\n");
*/

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &StarTrackerGUI::networkManagerFinished
    );

    readSolarFlux();
    connect(&m_solarFluxTimer, SIGNAL(timeout()), this, SLOT(autoUpdateSolarFlux()));
    m_solarFluxTimer.start(1000*60*60*24); // Update every 24hours
    autoUpdateSolarFlux();

    createGalacticLineOfSightScene();
    createSolarSystemScene();
    createJupiterScene();
    plotChart();

    StarTracker::MsgRequestAvailableFeatures *message = StarTracker::MsgRequestAvailableFeatures::create();
    m_starTracker->getInputMessageQueue()->push(message);

    connect(&m_spiceEphemerides, &SpiceEphemerides::allDownloadsComplete, this, &StarTrackerGUI::spiceDownloadsComplete);

    m_jplHorizons = JPLHorizons::create();
    if (m_jplHorizons)
    {
        connect(m_jplHorizons, &JPLHorizons::majorBodiesUpdated, this, &StarTrackerGUI::majorBodiesUpdated);
        m_jplHorizons->getMajorBodiesList();
    }
}

StarTrackerGUI::~StarTrackerGUI()
{
    if (m_jplHorizons)
    {
        disconnect(m_jplHorizons, &JPLHorizons::majorBodiesUpdated, this, &StarTrackerGUI::majorBodiesUpdated);
        delete m_jplHorizons;
    }
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &StarTrackerGUI::networkManagerFinished
    );
    delete m_networkManager;
    delete ui;
}

void StarTrackerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_settingsKeys.append("workspaceIndex");
    m_feature->setWorkspaceIndex(index);
}

void StarTrackerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void StarTrackerGUI::blockPlotChart()
{
    m_doPlotChart = false;
}

void StarTrackerGUI::unblockPlotChartAndPlot()
{
    m_doPlotChart = true;
    plotChart();
}

void StarTrackerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    blockPlotChart();
    ui->darkTheme->setChecked(m_settings.m_chartsDarkTheme);

    if (m_solarFluxChart) {
        m_solarFluxChart->setTheme(m_settings.m_chartsDarkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    }

    m_chart.setTheme(m_settings.m_chartsDarkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    ui->night->setChecked(m_settings.m_night);
    ui->logScale->setChecked(m_settings.m_logScale);
    ui->drawSun->setChecked(m_settings.m_drawSunOnSkyTempChart);
    ui->drawMoon->setChecked(m_settings.m_drawMoonOnSkyTempChart);
    ui->link->setChecked(m_settings.m_link);
    ui->latitude->setValue(m_settings.m_latitude);
    ui->longitude->setValue(m_settings.m_longitude);
    ui->targetSource->setCurrentIndex(ui->targetSource->findText(m_settings.m_targetSource));
    ui->target->setEditable(m_settings.m_targetSource == "Horizons");
    if (ui->target->lineEdit()) {
        QObject::connect(ui->target->lineEdit(), &QLineEdit::editingFinished, this, &StarTrackerGUI::on_target_editingFinished, Qt::UniqueConnection);
    }
    ui->target->setCurrentIndex(ui->target->findText(m_settings.m_target));
    ui->azimuth->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
    ui->elevation->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
    ui->galacticLatitude->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
    ui->galacticLongitude->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
    ui->azimuthOffset->setValue(m_settings.m_azimuthOffset);
    ui->elevationOffset->setValue(m_settings.m_elevationOffset);

    if (m_settings.m_target == "Custom RA/Dec")
    {
        ui->rightAscension->setText(m_settings.m_ra);
        ui->declination->setText(m_settings.m_dec);
    }
    else if (m_settings.m_target == "Custom Az/El")
    {
        ui->azimuth->setValue(m_settings.m_az);
        ui->elevation->setValue(m_settings.m_el);
    }
    else if ((m_settings.m_target == "Custom l/b")
        || (m_settings.m_target == "S7")
        || (m_settings.m_target == "S8")
        || (m_settings.m_target == "S9")
    )
    {
        ui->galacticLatitude->setValue(m_settings.m_b);
        ui->galacticLongitude->setValue(m_settings.m_l);
    }
    if (m_settings.m_dateTime == "")
    {
        ui->dateTimeSelect->setCurrentIndex(0);
        ui->dateTime->setVisible(false);
        ui->utc->setVisible(false);
        ui->setTimeToNow->setVisible(false);
    }
    else
    {
        ui->dateTime->setDateTime(QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs));
        ui->dateTime->setVisible(true);
        ui->dateTimeSelect->setCurrentIndex(1);
        ui->utc->setVisible(true);
        ui->setTimeToNow->setVisible(true);
    }
    ui->utc->setChecked(m_settings.m_utc);

    if ((m_settings.m_solarFluxData != StarTrackerSettings::DRAO_2800) && !m_solarFluxesValid) {
        autoUpdateSolarFlux();
    }

    ui->chartSelect->setCurrentIndex((int) m_settings.m_chartSelect);
    ui->chartSubSelect->setCurrentIndex(m_settings.m_chartSubSelect);

    ui->frequency->setValue(m_settings.m_frequency/1000000.0);
    ui->beamwidth->setValue(m_settings.m_beamwidth);
    updateForTarget();
    getRollupContents()->restoreState(m_rollupState);
    unblockPlotChartAndPlot();
    blockApplySettings(false);
}

void StarTrackerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void StarTrackerGUI::downloadSPICEEphemerides()
{
    m_spiceEphemerides.download(m_settings.m_spiceEphemerides);
}

bool StarTrackerGUI::checkSPICEEphemerides()
{
    return m_spiceEphemerides.checkDownloaded(m_settings.m_spiceEphemerides);
}

void StarTrackerGUI::spiceDownloadsComplete()
{
    updateTargetList();
    on_chartSelect_currentIndexChanged(ui->chartSelect->currentIndex());

    if (m_startAfterDownload)
    {
        if (checkSPICEEphemerides())
        {
            m_startAfterDownload = false;
            StarTracker::MsgStartStop *message = StarTracker::MsgStartStop::create(true);
            m_starTracker->getInputMessageQueue()->push(message);
        }
    }
}

void StarTrackerGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        if (checked)
        {
            if (checkSPICEEphemerides())
            {
                StarTracker::MsgStartStop *message = StarTracker::MsgStartStop::create(checked);
                m_starTracker->getInputMessageQueue()->push(message);
            }
            else
            {
                m_startAfterDownload = true;
                downloadSPICEEphemerides();
            }
        }
        else
        {
            StarTracker::MsgStartStop *message = StarTracker::MsgStartStop::create(checked);
            m_starTracker->getInputMessageQueue()->push(message);
        }
    }
}

void StarTrackerGUI::on_latitude_valueChanged(double value)
{
    m_settings.m_latitude = value;
    m_settingsKeys.append("latitude");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_longitude_valueChanged(double value)
{
    m_settings.m_longitude = value;
    m_settingsKeys.append("longitude");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_rightAscension_editingFinished()
{
    m_settings.m_ra = ui->rightAscension->text();
    m_settingsKeys.append("ra");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_declination_editingFinished()
{
    m_settings.m_dec = ui->declination->text();
    m_settingsKeys.append("dec");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_azimuth_valueChanged(double value)
{
    m_settings.m_az = value;
    m_settingsKeys.append("azimuth");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_elevation_valueChanged(double value)
{
    m_settings.m_el = value;
    m_settingsKeys.append("elevation");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_azimuthOffset_valueChanged(double value)
{
    m_settings.m_azimuthOffset = value;
    m_settingsKeys.append("azimuthOffset");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_elevationOffset_valueChanged(double value)
{
    m_settings.m_elevationOffset = value;
    m_settingsKeys.append("elevationOffset");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_galacticLatitude_valueChanged(double value)
{
    m_settings.m_b = value;
    m_settingsKeys.append("b");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_galacticLongitude_valueChanged(double value)
{
    m_settings.m_l = value;
    m_settingsKeys.append("l");
    applySettings();
    plotChart();
}

void StarTrackerGUI::updateForTarget()
{
    const QStringList raDecTargets = {
        "PSR B0329+54", "PSR B0833-45", "Sagittarius A", "Cassiopeia A", "Cygnus A", "Taurus A (M1)", "Virgo A (M87)"
    };
    const QStringList lbTargets = {
        "S7", "S8", "S9"
    };

    if ((m_settings.m_target == "Sun") || (m_settings.m_target == "Moon"))
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->azimuth->setReadOnly(true);
        ui->elevation->setReadOnly(true);
        ui->galacticLatitude->setReadOnly(true);
        ui->galacticLongitude->setReadOnly(true);
        ui->rightAscension->setText("");
        ui->declination->setText("");
    }
    else if (raDecTargets.contains(m_settings.m_target))
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->azimuth->setReadOnly(true);
        ui->elevation->setReadOnly(true);
        ui->galacticLatitude->setReadOnly(true);
        ui->galacticLongitude->setReadOnly(true);
        if (m_settings.m_target == "PSR B0329+54")
        {
            ui->rightAscension->setText("03h32m59.35s");
            ui->declination->setText(QString("54%0134'45.05\"").arg(QChar(0xb0)));
        }
        else if (m_settings.m_target == "PSR B0833-45")
        {
            ui->rightAscension->setText("08h35m20.66s");
            ui->declination->setText(QString("-45%0110'35.15\"").arg(QChar(0xb0)));
        }
        else if (m_settings.m_target == "Sagittarius A")
        {
            ui->rightAscension->setText("17h45m40.04s");
            ui->declination->setText(QString("-29%0100'28.17\"").arg(QChar(0xb0)));
        }
        else if (m_settings.m_target == "Cassiopeia A")
        {
            ui->rightAscension->setText("23h23m24s");
            ui->declination->setText(QString("58%0148'54\"").arg(QChar(0xb0)));
        }
        else if (m_settings.m_target == "Cygnus A")
        {
            ui->rightAscension->setText("19h59m28.36s");
            ui->declination->setText(QString("40%0144'02.1\"").arg(QChar(0xb0)));
        }
        else if (m_settings.m_target == "Taurus A (M1)")
        {
            ui->rightAscension->setText("05h34m31.94s");
            ui->declination->setText(QString("22%0100'52.2\"").arg(QChar(0xb0)));
        }
        else if (m_settings.m_target == "Virgo A (M87)")
        {
            ui->rightAscension->setText("12h30m49.42s");
            ui->declination->setText(QString("12%0123'28.04\"").arg(QChar(0xb0)));
        }
        on_rightAscension_editingFinished();
        on_declination_editingFinished();
    }
    else if (lbTargets.contains(m_settings.m_target))
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->azimuth->setReadOnly(true);
        ui->elevation->setReadOnly(true);
        ui->galacticLatitude->setReadOnly(true);
        ui->galacticLongitude->setReadOnly(true);
        ui->rightAscension->setText("");
        ui->declination->setText("");
        if (m_settings.m_target == "S7")
        {
            ui->galacticLatitude->setValue(-1.0);
            ui->galacticLongitude->setValue(132.0);
        }
        else if (m_settings.m_target == "S8")
        {
            ui->galacticLatitude->setValue(-15.0);
            ui->galacticLongitude->setValue(207.0);
        }
        else if (m_settings.m_target == "S9")
        {
            ui->galacticLatitude->setValue(-4.0);
            ui->galacticLongitude->setValue(356.0);
        }
    }
    else if (m_settings.m_target == "Custom RA/Dec")
    {
        ui->rightAscension->setReadOnly(false);
        ui->declination->setReadOnly(false);
        ui->azimuth->setReadOnly(true);
        ui->elevation->setReadOnly(true);
        ui->galacticLatitude->setReadOnly(true);
        ui->galacticLongitude->setReadOnly(true);
        m_settingsKeys.append("ra");
        m_settingsKeys.append("dec");
        applySettings();
    }
    else if (m_settings.m_target == "Custom Az/El")
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->azimuth->setReadOnly(false);
        ui->elevation->setReadOnly(false);
        ui->galacticLatitude->setReadOnly(true);
        ui->galacticLongitude->setReadOnly(true);
        ui->rightAscension->setText("");
        ui->declination->setText("");
        m_settingsKeys.append("azimuth");
        m_settingsKeys.append("elevation");
        applySettings();
    }
    else if (m_settings.m_target == "Custom l/b")
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->azimuth->setReadOnly(true);
        ui->elevation->setReadOnly(true);
        ui->galacticLatitude->setReadOnly(false);
        ui->galacticLongitude->setReadOnly(false);
        ui->rightAscension->setText("");
        ui->declination->setText("");
        m_settingsKeys.append("l");
        m_settingsKeys.append("b");
        applySettings();
    }
    else if (m_settings.m_target.contains("SatelliteTracker"))
    {
        ui->azimuth->setReadOnly(true);
        ui->elevation->setReadOnly(true);
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->galacticLatitude->setReadOnly(true);
        ui->galacticLongitude->setReadOnly(true);
        ui->rightAscension->setText("");
        ui->declination->setText("");
    }
}

void StarTrackerGUI::on_target_currentIndexChanged(int index)
{
    (void) index;

    QString text = ui->target->currentText();
    if (!text.isEmpty())
    {
        m_settings.m_target = text;
        m_settingsKeys.append("target");
        applySettings();
        updateForTarget();
        plotChart();
    }
}

void StarTrackerGUI::on_target_editingFinished()
{
    QString text = ui->target->currentText();
    if (!text.isEmpty())
    {
        m_settings.m_target = text;
        m_settingsKeys.append("target");
        applySettings();
        updateForTarget();
        plotChart();
    }
}

void StarTrackerGUI::on_targetSource_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_targetSource = ui->targetSource->currentText();
    m_settingsKeys.append("targetSource");
    applySettings();

    if (m_settings.m_targetSource == "SPICE")
    {
        if (!checkSPICEEphemerides()) {
            downloadSPICEEphemerides();
        }
    }

    ui->target->setEditable(m_settings.m_targetSource == "Horizons");
    if (ui->target->lineEdit()) {
        QObject::connect(ui->target->lineEdit(), &QLineEdit::editingFinished, this, &StarTrackerGUI::on_target_editingFinished, Qt::UniqueConnection);
    }
    updateTargetList();
    on_target_currentIndexChanged(ui->target->currentIndex()); // updateTargetList blocks signals, so update target manually
}

void StarTrackerGUI::updateLST()
{
    QDateTime dt;

    if (m_settings.m_dateTime.isEmpty()) {
        dt = QDateTime::currentDateTime();
    }
    else
    {
        dt = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs);
        if (m_settings.m_utc) {
            dt.setTimeZone(QTimeZone::utc());
        }
    }

    double lst = Astronomy::localSiderealTime(dt, m_settings.m_longitude);
    ui->lst->setText(Units::decimalHoursToHoursMinutesAndSeconds(lst/15.0, 0));
}

void StarTrackerGUI::updateStatus()
{
    int state = m_starTracker->getState();

    if (m_lastFeatureState != state)
    {
        // We set checked state of start/stop button, in case it was changed via API
        bool oldState;
        switch (state)
        {
            case Feature::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StIdle:
                oldState = ui->startStop->blockSignals(true);
                ui->startStop->setChecked(false);
                ui->startStop->blockSignals(oldState);
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case Feature::StRunning:
                oldState = ui->startStop->blockSignals(true);
                ui->startStop->setChecked(true);
                ui->startStop->blockSignals(oldState);
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case Feature::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_starTracker->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }

    updateLST();
}

void StarTrackerGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        StarTracker::MsgConfigureStarTracker* message = StarTracker::MsgConfigureStarTracker::create(m_settings, m_settingsKeys, force);
        m_starTracker->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

void StarTrackerGUI::on_link_clicked(bool checked)
{
    m_settings.m_link = checked;
    m_settingsKeys.append("link");
    applySettings();
}

void StarTrackerGUI::on_useMyPosition_clicked(bool checked)
{
    (void) checked;
    double stationLatitude = MainCore::instance()->getSettings().getLatitude();
    double stationLongitude = MainCore::instance()->getSettings().getLongitude();
    double stationAltitude = MainCore::instance()->getSettings().getAltitude();

    ui->latitude->setValue(stationLatitude);
    ui->longitude->setValue(stationLongitude);
    m_settings.m_heightAboveSeaLevel = stationAltitude;
    m_settingsKeys.append("heightAboveSeaLevel");
    applySettings();
    plotChart();
}

// Show settings dialog
void StarTrackerGUI::on_displaySettings_clicked()
{
    StarTrackerSettingsDialog dialog(&m_settings, m_settingsKeys);

    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings();
        ui->elevation->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
        ui->azimuth->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
        ui->galacticLatitude->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
        ui->galacticLongitude->setUnits((DMSSpinBox::DisplayUnits)m_settings.m_azElUnits);
        displaySolarFlux();

        if (ui->chartSelect->currentIndex() <= StarTrackerSettings::CHART_SOLAR_FLUX_VS_FREQUENCY) {
            plotChart();
        }
        if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SOLAR_SYSTEM) {
            on_chartSelect_currentIndexChanged(ui->chartSelect->currentIndex()); // Update list of bodies
        }

        checkSPICEEphemerides();
    }
}

void StarTrackerGUI::on_dateTimeSelect_currentTextChanged(const QString &text)
{
    if (text == "Now")
    {
        m_settings.m_dateTime = "";
        ui->dateTime->setVisible(false);
        ui->utc->setVisible(false);
        ui->setTimeToNow->setVisible(false);
    }
    else
    {
        m_settings.m_dateTime = ui->dateTime->dateTime().toString(Qt::ISODateWithMs);
        ui->dateTime->setVisible(true);
        ui->utc->setVisible(true);
        ui->setTimeToNow->setVisible(true);
    }

    m_settingsKeys.append("dateTime");
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_dateTime_dateTimeChanged(const QDateTime &datetime)
{
    (void) datetime;

    if (ui->dateTimeSelect->currentIndex() == 1)
    {
        m_settings.m_dateTime = ui->dateTime->dateTime().toString(Qt::ISODateWithMs);
        m_settingsKeys.append("dateTime");
        applySettings();
        plotChart();
    }
}

void StarTrackerGUI::on_utc_clicked(bool checked)
{
    m_settings.m_utc = checked;
    m_settingsKeys.append("utc");
    applySettings();
}

void StarTrackerGUI::on_setTimeToNow_clicked(bool checked)
{
    (void) checked;

    ui->dateTime->setDateTime(QDateTime::currentDateTime());
}

void StarTrackerGUI::plotChart()
{
    if (!m_doPlotChart) {
        return;
    }
    if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_ELEVATION_VS_TIME)
    {
        if (ui->chartSubSelect->currentIndex() == 0) {
            plotElevationLineChart();
        } else {
            plotElevationPolarChart();
        }
    }
    else if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SOLAR_FLUX_VS_FREQUENCY)
    {
        plotSolarFluxChart();
    }
    else if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SKY_TEMPERATURE)
    {
        plotSkyTemperatureChart();
    }
    else if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_GALACTIC_LINE_OF_SIGHT)
    {
        plotGalacticLineOfSight();
    }
    else if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SOLAR_SYSTEM)
    {
        plotSolarSystem();
    }
    else if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_JUPITER)
    {
        plotJupiter();
    }
}

void StarTrackerGUI::raDecChanged()
{
    if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SKY_TEMPERATURE) {
        plotSkyTemperatureChart();
    } else if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_GALACTIC_LINE_OF_SIGHT) {
        plotGalacticLineOfSight();
    }
}

void StarTrackerGUI::on_frequency_valueChanged(int value)
{
    m_settings.m_frequency = value*1000000.0;
    m_settingsKeys.append("frequency");
    applySettings();

    if (ui->chartSelect->currentIndex() != 0)
    {
        updateChartSubSelect();
        plotChart();
    }

    displaySolarFlux();
}

void StarTrackerGUI::on_beamwidth_valueChanged(double value)
{
    m_settings.m_beamwidth = value;
    m_settingsKeys.append("beamwidth");
    applySettings();
    updateChartSubSelect();

    if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SKY_TEMPERATURE) {
        plotChart();
    }
}

void StarTrackerGUI::plotSolarFluxChart()
{
    ui->chart->setVisible(true);
    ui->image->setVisible(false);
    ui->night->setVisible(false);
    ui->logScale->setVisible(false);
    ui->drawSun->setVisible(false);
    ui->drawMoon->setVisible(false);
    ui->darkTheme->setVisible(true);
    ui->zoomIn->setVisible(false);
    ui->zoomOut->setVisible(false);
    ui->addAnimationFrame->setVisible(false);
    ui->clearAnimation->setVisible(false);
    ui->saveAnimation->setVisible(false);
    ui->jupiterElevationLabel->setVisible(false);
    ui->jupiterElevation->setVisible(false);
    ui->cmlLabel->setVisible(false);
    ui->cml->setVisible(false);
    ui->ioPhaseLabel->setVisible(false);
    ui->ioPhase->setVisible(false);
    ui->ganymedePhaseLabel->setVisible(false);
    ui->ganymedePhase->setVisible(false);

    QChart *oldChart = m_solarFluxChart;

    m_solarFluxChart = new QChart();

    if (m_solarFluxesValid)
    {

        m_solarFluxChart->setTitle("");
        m_solarFluxChart->legend()->hide();
        m_solarFluxChart->layout()->setContentsMargins(0, 0, 0, 0);
        m_solarFluxChart->setMargins(QMargins(1, 1, 1, 1));
        m_solarFluxChart->setTheme(m_settings.m_chartsDarkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);

        double maxValue = -std::numeric_limits<double>::infinity();
        double minValue = std::numeric_limits<double>::infinity();
        QLineSeries *series = new QLineSeries();
        for (int i = 0; i < 8; i++)
        {
            double value = convertSolarFluxUnits(m_solarFluxes[i]);
            series->append(m_solarFluxFrequencies[i], value);
            maxValue = std::max(value, maxValue);
            minValue = std::min(value, minValue);
        }
        series->setPointLabelsVisible(true);
        series->setPointLabelsFormat("@yPoint");
        series->setPointLabelsClipping(false);
        m_solarFluxChart->addSeries(series);

        QLogValueAxis *chartSolarFluxXAxis = new QLogValueAxis();
        QValueAxis *chartSolarFluxYAxis = new QValueAxis();

        chartSolarFluxXAxis->setTitleText(QString("Frequency (MHz)"));
        chartSolarFluxXAxis->setMinorTickCount(-1);

        chartSolarFluxYAxis->setTitleText(QString("Solar flux density (%1)").arg(solarFluxUnit()));
        chartSolarFluxYAxis->setMinorTickCount(-1);
        if (m_settings.m_solarFluxUnits == StarTrackerSettings::SFU)
        {
            chartSolarFluxYAxis->setLabelFormat("%d");
            chartSolarFluxYAxis->setRange(0.0, ((((int)maxValue)+99)/100)*100);
        }
        else if (m_settings.m_solarFluxUnits == StarTrackerSettings::JANSKY)
        {
            chartSolarFluxYAxis->setLabelFormat("%.2g");
            chartSolarFluxYAxis->setRange(0, ((((int)maxValue)+999999)/100000)*100000);
        }
        else
        {
            chartSolarFluxYAxis->setLabelFormat("%.2g");
            // Bug in QtCharts for values < ~1e-12  https://bugreports.qt.io/browse/QTBUG-95304
            // Set range to 0-1 here, then real range after axis have been attached
            chartSolarFluxYAxis->setRange(0.0, 1.0);
        }

        m_solarFluxChart->addAxis(chartSolarFluxXAxis, Qt::AlignBottom);
        m_solarFluxChart->addAxis(chartSolarFluxYAxis, Qt::AlignLeft);
        series->attachAxis(chartSolarFluxXAxis);
        series->attachAxis(chartSolarFluxYAxis);

        if (m_settings.m_solarFluxUnits == StarTrackerSettings::WATTS_M_HZ) {
            chartSolarFluxYAxis->setRange(minValue, maxValue);
        }

    }
    else
        m_solarFluxChart->setTitle("Press download Solar flux density data to view");

    ui->chart->setChart(m_solarFluxChart);

    delete oldChart;
}

QList<QLineSeries*> StarTrackerGUI::createDriftScan(bool galactic)
{
    QList<QLineSeries *>list;
    QLineSeries *series = new QLineSeries();
    list.append(series);

    QDateTime dt;

    // Get date and time to calculate position at
    dt = m_settings.getDateTime();

    // Create a list of RA/Dec points of drift scan path
    AzAlt aa;
    aa.alt = m_settings.m_el;
    aa.az = m_settings.m_az;
    double prevX;
    // Plot every 30min over a day
    for (int i = 0; i <= 24*2; i++)
    {
        dt = dt.addSecs(30*60);
        RADec rd = Astronomy::azAltToRaDec(aa, m_settings.m_latitude, m_settings.m_longitude, dt);
        double x, y;
        mapRaDec(rd.ra, rd.dec, galactic, x, y);
        if (i == 0)
        {
            series->append(x, y);
        }
        else
        {
            // Check for crossing edge of chart
            if (galactic)
            {
                if (((prevX < 90.0) && (x > 270.0)) || ((prevX > 270.0) && (x < 90.0)))
                {
                    // Start new series, so we don't have lines crossing across the chart
                    series = new QLineSeries();
                    list.append(series);
                }
            }
            series->append(x, y);
        }
        prevX = x;
    }

    return list;
}

void StarTrackerGUI::mapRaDec(double ra, double dec, bool galactic, double& x, double& y)
{
    if (galactic)
    {
        // Convert to category coordinates
        double l, b;
        Astronomy::equatorialToGalactic(ra, dec, l, b);
        // Map to linear axis
        double lAxis;
        if (l < 180.0) {
            lAxis = 180.0 - l;
        } else {
            lAxis = 360.0 - l + 180.0;
        }

        x = lAxis;
        y = b;
    }
    else
    {
        // Map to category axis
        double raAxis;
        if (ra <= 12.0) {
            raAxis = 12.0 - ra;
        } else {
            raAxis = 24 - ra + 12;
        }

        x = raAxis;
        y = dec;
    }
}

// Is there a way to get this from the theme? Got these values from the source
QColor StarTrackerGUI::getSeriesColor(int series)
{
    if (m_settings.m_chartsDarkTheme)
    {
        if (series == 0) {
            return QColor(0x38ad6b);
        } else if (series == 1) {
            return QColor(0x3c84a7);
        } else {
            return QColor(0xeb8817);
        }
    }
    else
    {
        if (series == 0) {
            return QColor(0x209fdf);
        } else if (series == 1) {
            return QColor(0x99ca53);
        } else {
            return QColor(0xf6a625);
        }
    }
}

void StarTrackerGUI::createGalacticLineOfSightScene()
{
    m_galacticLineOfSightScene = new QGraphicsScene(ui->image);
    m_galacticLineOfSightScene->setBackgroundBrush(QBrush(Qt::black));

    // Milkyway images
    for (int i = 0; i < m_milkyWayImages.size(); i++)
    {
        m_milkyWayItems.append(m_galacticLineOfSightScene->addPixmap(m_milkyWayImages[i]));
        m_milkyWayItems[i]->setPos(0, 0);
        m_milkyWayItems[i]->setVisible(i == 0);
    }

    // Line of sight
    QPen pen(QColor(255, 0, 0), 4, Qt::SolidLine);
    m_lineOfSight = m_galacticLineOfSightScene->addLine(511, 708, 511, 708, pen);
}

// Draw top-down image of Milky Way
void StarTrackerGUI::plotGalacticLineOfSight()
{
    ui->chart->setVisible(false);
    ui->image->setVisible(true);
    ui->night->setVisible(false);
    ui->logScale->setVisible(false);
    ui->drawSun->setVisible(false);
    ui->drawMoon->setVisible(false);
    ui->darkTheme->setVisible(false);
    ui->zoomIn->setVisible(true);
    ui->zoomOut->setVisible(true);
    ui->addAnimationFrame->setVisible(true);
    ui->clearAnimation->setVisible(true);
    ui->saveAnimation->setVisible(true);
    ui->jupiterElevationLabel->setVisible(false);
    ui->jupiterElevation->setVisible(false);
    ui->cmlLabel->setVisible(false);
    ui->cml->setVisible(false);
    ui->ioPhaseLabel->setVisible(false);
    ui->ioPhase->setVisible(false);
    ui->ganymedePhaseLabel->setVisible(false);
    ui->ganymedePhase->setVisible(false);

    if (ui->image->scene() != m_galacticLineOfSightScene)
    {
        ui->image->setScene(m_galacticLineOfSightScene);
        ui->image->resetTransform();
        // Start zoomed out
        ui->image->fitInView(m_milkyWayItems[0], Qt::KeepAspectRatio);
        ui->image->setDragMode(QGraphicsView::ScrollHandDrag);
    }

    if (!m_zoom)
    {
        m_zoom = new GraphicsViewZoom(ui->image); // Deleted automatically when view is deleted
        connect(m_zoom, &GraphicsViewZoom::zoomed, this, &StarTrackerGUI::zoomed);
    }

    // Select which Milky Way image to show
    int imageIdx = ui->chartSubSelect->currentIndex();
    for (int i = 0; i < m_milkyWayItems.size(); i++) {
        m_milkyWayItems[i]->setVisible(i == imageIdx);
    }

    // Calculate Galactic longitude we're observing
    float ra = Units::raToDecimal(m_settings.m_ra);
    float dec = Units::decToDecimal(m_settings.m_dec);
    double l, b;
    Astronomy::equatorialToGalactic(ra, dec, l, b);

    //l = ui->azimuth->value(); // For testing, just use azimuth

    // Calculate length of line, as Sun is not at centre
    // we assume end point lies on an ellipse, with Sun at foci
    // See https://en.wikipedia.org/wiki/Ellipse Polar form relative to focus
    QPointF sun(511, 708); // Location of Sun on Milky Way image
    float a = sun.x() - 112; // semi-major axis
    float c = sun.y() - sun.x(); // linear eccentricity
    float e = c / a; // eccentricity
    float r = a * (1.0f - e*e) / (1.0f - e * cos(Units::degreesToRadians(l)));

    // Draw line from Sun along observation galactic longitude
    QTransform rotation = QTransform().translate(sun.x(), -sun.y()).rotate(l).translate(-sun.x(), sun.y());  // Flip Y
    QPointF point = rotation.map(QPointF(511, -sun.y() + r));
    m_lineOfSight->setLine(sun.x(), sun.y(), point.x(), -point.y());
}

void StarTrackerGUI::plotGalacticMarker(LoSMarker* marker)
{
    QPointF sun(511, 708); // Location of Sun on Milky Way image
    double pixelsPerKPC = 564.0/22.995;  // 75,000ly = 23kpc
    QTransform rotation = QTransform().translate(sun.x(), -sun.y()).rotate(marker->m_l).translate(-sun.x(), sun.y());  // Flip Y
    QPointF point = rotation.map(QPointF(511, -sun.y() + pixelsPerKPC*marker->m_d));
    marker->m_text->setPos(point.x(), -point.y());
}

void StarTrackerGUI::on_zoomIn_clicked()
{
    m_zoom->gentleZoom(1.25);
}

void StarTrackerGUI::on_zoomOut_clicked()
{
    m_zoom->gentleZoom(0.75);
}

void StarTrackerGUI::zoomed()
{
    if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SOLAR_SYSTEM) {
        scaleSolarSystemItems();
    }
}

void StarTrackerGUI::on_addAnimationFrame_clicked()
{
    QImage image(ui->image->size(), QImage::Format_ARGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    ui->image->render(&painter);
    m_animationImages.append(image);

    ui->saveAnimation->setEnabled(true);
    ui->clearAnimation->setEnabled(true);
}

void StarTrackerGUI::on_clearAnimation_clicked()
{
    m_animationImages.clear();
    ui->saveAnimation->setEnabled(false);
    ui->clearAnimation->setEnabled(false);
}

void StarTrackerGUI::on_saveAnimation_clicked()
{
    // Get filename of animation file
    QFileDialog fileDialog(nullptr, "Select file to save animation to", "", "*.png");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            APNG apng(m_animationImages.size());
            for (int i = 0; i < m_animationImages.size(); i++) {
                apng.addImage(m_animationImages[i]);
            }
            if (!apng.save(fileNames[0])) {
                QMessageBox::critical(this, "Star Tracker", QString("Failed to write to file %1").arg(fileNames[0]));
            }
        }
    }
}

void StarTrackerGUI::createSolarSystemScene()
{
    m_solarSystemScene = new QGraphicsScene(ui->image);
    m_solarSystemScene->setBackgroundBrush(QBrush(Qt::black));

    m_solarSystemLabelFont = m_solarSystemScene->font();
    m_solarSystemLabelFont.setPointSize(6);
    m_solarSystemLabelFontMetrics = QFontMetrics(m_solarSystemLabelFont);

    // Pluto is ~56 from Sun on log scale
    const double r = 57;
    m_solarSystemScene->setSceneRect(-r, -r, r * 2, r * 2);
}

QPixmap *StarTrackerGUI::getPlanetPixmap(const QString& name)
{
    QString nameLower = name.toLower();
    if (m_planetImages.contains(nameLower)) {
        return &m_planetImages[nameLower];
    } else {
        return &m_planetImages["mercury"]; // Use mercury as generic grey moon
    }
}

static float getScale(const QString& name)
{
    if (name == "mercury") {
        return 0.5f;
    } else if (name == "venus") {
        return 0.7f;
    } else if (name == "earth") {
        return 0.7f;
    } else if (name == "moon") {
        return 0.5f;
    } else if (name == "mars") {
        return 0.6f;
    } else if (name == "jupiter") {
        return 1.0f;
    } else if (name == "saturn") {
        return 0.9f;
    } else if (name == "uranus") {
        return 0.8f;
    } else if (name == "neptune") {
        return 0.8f;
    } else if (name == "pluto") {
        return 0.4f;
    } else {
        return 1.0f;
    }
}

static void logScale(QVector3D &p)
{
    // Rectangular to spherical
    float r = p.length();
    float az = std::atan2(p.y(), p.x());
    float el = r > 0.0f ? std::acos(p.z() / r) : 0.0f;

    // Log scale radius (log10(mercuryRadiusKM) = 7.7, log10(neptuneRadiusKM) = 9.6)
    if (r >= 1e7) {
        r = (std::log10(r) - 7.0f) * 20.0f;
    } else {
        r = 0.0f;
    }

    // Spherical to rectangular
    p.setX(r * std::sin(el) * std::cos(az));
    p.setY(r * std::sin(el) * std::sin(az));
    p.setZ(r * std::cos(el));
}

void StarTrackerGUI::updateSolarSystemPositions(const QStringList &names, const QList<QVector3D> &positions, const QList<QList<QPointF>> &orbits)
{
    const double kmToAU = 6.68459e-9;   // Convert position from kM to AU
    const double pixelScale = 20.0;     // Mercury is 0.4AU. Neptune is 30 AU
    bool scale = false;
    QPen orbitPen(Qt::gray);
    orbitPen.setWidth(0);

    for (int i = 0; i < names.size(); i++)
    {
        SolarSystemItem *item;

        if (m_solarSystemItems.contains(names[i]))
        {
            item = m_solarSystemItems.value(names[i]);
        }
        else
        {
            QString name = names[i];
            name = name.replace(" BARYCENTER", "");

            QPixmap* pixmap = getPlanetPixmap(name);

            QBrush brush(QColor(200, 200, 200));
            item = new SolarSystemItem();
            item->m_textItem = m_solarSystemScene->addText(name, m_solarSystemLabelFont);
            item->m_textItem->setZValue(2);
            item->m_scale = getScale(name.toLower());
            item->m_pixmapItem = m_solarSystemScene->addPixmap(*pixmap);
            item->m_pixmapItem->setOffset(-pixmap->width() / 2, -pixmap->height() / 2);
            item->m_pixmapItem->setZValue(1);

            if (orbits[i].size() > 0) {
                item->m_orbitItem = m_solarSystemScene->addPolygon(QPolygonF(), orbitPen);
            } else {
                item->m_orbitItem  = nullptr;
            }

            m_solarSystemItems.insert(names[i], item);
            scale = true;
        }

        // Optional log scaling of orbital radius, so outer planets have similar separation to inner planets
        QVector3D scaledPos = positions[i];
        if (m_settings.m_logScale) {
            logScale(scaledPos);
        } else {
            scaledPos = pixelScale * kmToAU * scaledPos;
        }

        // Draw an ellipse for planet's orbit
        if (item->m_orbitItem)
        {
            QPolygonF polygon;

            for (int j = 0; j < orbits[i].size(); j++)
            {
                QPointF point = orbits[i][j];

                if (m_settings.m_logScale)
                {
                    QVector3D v = {(float) point.x(), (float) point.y(), 0.0f};
                    logScale(v);
                    point.setX(v.x());
                    point.setY(v.y());
                }
                else
                {
                    point = pixelScale * kmToAU * point;
                }

                polygon << point;
            }

            item->m_orbitItem->setPolygon(polygon);
        }

        // Position label to right
        const int textYOffset = m_solarSystemLabelFontMetrics.height();
        item->m_pixmapItem->setPos(scaledPos.x(), scaledPos.y());
        item->m_textItem->setPos(scaledPos.x() - 1.1 * item->m_pixmapItem->offset().x() * item->m_pixmapItem->scale(), scaledPos.y() - textYOffset * item->m_textItem->scale());
    }

    if (scale) {
        scaleSolarSystemItems();
    }

    // Remove no longer needed items
    QMutableHashIterator itr(m_solarSystemItems);

    while (itr.hasNext())
    {
        itr.next();
        if (!names.contains(itr.key()))
        {
            SolarSystemItem* item = itr.value();
            m_solarSystemScene->removeItem(item->m_pixmapItem);
            m_solarSystemScene->removeItem(item->m_textItem);
            m_solarSystemScene->removeItem(item->m_orbitItem);
            delete item;
            itr.remove();
        }
    }
}

void StarTrackerGUI::scaleSolarSystemItems()
{
    float pixmapScale;
    float textScale;
    QTransform tf = ui->image->transform();

    if (tf.m11() <= 1)
    {
        pixmapScale = 0.01 / tf.m11(); // Keep fixed size as we zoom out
        textScale = 1.0 / tf.m11();
    }
    else if (tf.m11() >= 15)
    {
        pixmapScale = 0.01 * 15 / tf.m11(); // Keep fixed size as we zoom in to planetary scale
        textScale = 1.0 / tf.m11();
    }
    else
    {
        pixmapScale = 0.01; // Get larger as we zoom in
        textScale = 1.0 / tf.m11(); // Keep fixed size
    }

    for (auto& item : m_solarSystemItems)
    {
        item->m_textItem->setScale(textScale);

        if (item->m_pixmapItem)
        {
            item->m_pixmapItem->setScale(pixmapScale * item->m_scale);
            QPointF pos = item->m_pixmapItem->pos();
            int textYOffset = m_solarSystemLabelFontMetrics.height();
            item->m_textItem->setPos(pos.x() - 1.1 * item->m_pixmapItem->offset().x() * item->m_pixmapItem->scale(), pos.y() - textYOffset * item->m_textItem->scale());
        }
    }

    centerOnSolarSystemBody();
}

// Draw top-down image of Solar System
void StarTrackerGUI::plotSolarSystem()
{
    ui->chart->setVisible(false);
    ui->image->setVisible(true);
    ui->night->setVisible(false);
    ui->logScale->setVisible(true);
    ui->drawSun->setVisible(false);
    ui->drawMoon->setVisible(false);
    ui->darkTheme->setVisible(false);
    ui->zoomIn->setVisible(true);
    ui->zoomOut->setVisible(true);
    ui->addAnimationFrame->setVisible(false);
    ui->clearAnimation->setVisible(false);
    ui->saveAnimation->setVisible(false);
    ui->jupiterElevationLabel->setVisible(false);
    ui->jupiterElevation->setVisible(false);
    ui->jupiterElevationLabel->setVisible(false);
    ui->jupiterElevation->setVisible(false);
    ui->cmlLabel->setVisible(false);
    ui->cml->setVisible(false);
    ui->ioPhaseLabel->setVisible(false);
    ui->ioPhase->setVisible(false);
    ui->ganymedePhaseLabel->setVisible(false);
    ui->ganymedePhase->setVisible(false);

    if (ui->image->scene() != m_solarSystemScene)
    {
        ui->image->setScene(m_solarSystemScene);
        ui->image->resetTransform();
        ui->image->fitInView(m_solarSystemScene->sceneRect(), Qt::KeepAspectRatio);
        scaleSolarSystemItems();
    }

    if (!m_zoom)
    {
        m_zoom = new GraphicsViewZoom(ui->image); // Deleted automatically when view is deleted
        connect(m_zoom, &GraphicsViewZoom::zoomed, this, &StarTrackerGUI::zoomed);
    }

    centerOnSolarSystemBody();
}

void StarTrackerGUI::centerOnSolarSystemBody()
{
    // Centre on selected body
    QString selectedBody = ui->chartSubSelect->currentText();
    if (m_solarSystemItems.contains(selectedBody))
    {
        SolarSystemItem *item = m_solarSystemItems.value(selectedBody);
        ui->image->centerOn(item->m_pixmapItem);
        ui->image->setDragMode(QGraphicsView::NoDrag);
    }
    else
    {
        ui->image->setDragMode(QGraphicsView::ScrollHandDrag);
    }
}

QList<QGraphicsTextItem *> StarTrackerGUI::createJupiterLegendLabels(int legendXRight, int legendYBottom, int legendStep, int legendMax)
{
    QList<QGraphicsTextItem *> list;

    int legendSteps = legendMax / legendStep;
    int legendYStep = m_jupiterImages[2].height() / legendSteps;
    int maxLegendYLabelWidth = 0;
    for (int i = 0; i <= legendSteps; i++)
    {
        QString legendStr = QString::number(i * legendStep);
        QGraphicsTextItem *legendLabel = m_jupiterScene->addText(legendStr);
        int yw = legendLabel->boundingRect().width();
        maxLegendYLabelWidth = std::max(maxLegendYLabelWidth, yw);
        legendLabel->setPos(legendXRight, legendYBottom - i * legendYStep - legendLabel->boundingRect().height() / 2);
        list.append(legendLabel);
    }

    return list;
}

void StarTrackerGUI::createJupiterScene()
{
    m_jupiterScene = new QGraphicsScene(ui->image);
    m_jupiterScene->setBackgroundBrush(QBrush(Qt::black));

    // Phase vs CML images
    for (int i = 0; i < m_jupiterImages.size(); i++) {
        m_jupiterItems.append(m_jupiterScene->addPixmap(m_jupiterImages[i]));
    }
    m_jupiterItems[0]->setPos(0, 0);
    m_jupiterItems[1]->setPos(0, 0);
    m_jupiterItems[1]->setVisible(false);

    // Legend

    int legendSpacing = 10;
    int legendYTop = (m_jupiterImages[0].height() - m_jupiterImages[2].height()) / 2;
    int legendYBottom = legendYTop + m_jupiterImages[2].height();
    int legendXLeft = m_jupiterImages[0].width() + legendSpacing;
    int legendXRight = legendXLeft + m_jupiterImages[2].width();
    m_jupiterItems[2]->setPos(legendXLeft, legendYTop);
    m_ioLegendLabels = createJupiterLegendLabels(legendXRight, legendYBottom, 10, 60);
    m_ganymedeLegendLabels = createJupiterLegendLabels(legendXRight, legendYBottom, 5, 15);

    QGraphicsTextItem *legendLabel = m_jupiterScene->addText("Probability (%)");
    legendLabel->setTransformOriginPoint(legendLabel->boundingRect().center());
    legendLabel->setRotation(-90);
    legendLabel->setPos(legendXRight, m_jupiterImages[0].height() / 2);

    // Y axis labels

    int phaseMax = 360;
    int phaseStep = 45;
    int phaseSteps = phaseMax / phaseStep;
    int yStep = m_jupiterImages[0].height() / phaseSteps;
    int maxYLabelWidth = 0;

    for (int i = 0; i <= phaseSteps; i++)
    {
        QString phaseStr = QString::number(i * phaseStep);
        QGraphicsTextItem *yLabel = m_jupiterScene->addText(phaseStr);
        int yw = yLabel->boundingRect().width();
        maxYLabelWidth = std::max(maxYLabelWidth, yw);
        yLabel->setPos(-yw, m_jupiterImages[0].height() - i * yStep - yLabel->boundingRect().height() / 2);
    }

    QGraphicsTextItem *phaseLabel = m_jupiterScene->addText(QString("Phase (%1)").arg(QChar(0xb0)));
    phaseLabel->setTransformOriginPoint(phaseLabel->boundingRect().center());
    phaseLabel->setRotation(-90);
    phaseLabel->setPos(-2 * phaseLabel->boundingRect().height() - maxYLabelWidth, m_jupiterImages[0].height() / 2);

    // X axis labels

    QGraphicsTextItem *cmlLabel = m_jupiterScene->addText(QString("CML (%1)").arg(QChar(0xb0)));
    int cmlLabelWidth = cmlLabel->boundingRect().width();
    cmlLabel->setPos(m_jupiterImages[0].width() / 2 - cmlLabelWidth / 2, m_jupiterImages[0].height() + cmlLabel->boundingRect().height());

    int cmlMax = 360;
    int cmlStep = 45;
    int cmlSteps = cmlMax / cmlStep;
    int xStep = m_jupiterImages[0].width() / cmlSteps;

    for (int i = 0; i <= cmlSteps; i++)
    {
        QString cmlStr = QString::number(i * cmlStep);
        QGraphicsTextItem *xLabel = m_jupiterScene->addText(cmlStr);
        int xw = xLabel->boundingRect().width();
        xLabel->setPos(i * xStep - xw / 2, m_jupiterImages[0].height());
    }

    // Moon images

    const qreal moonScale = 0.1;

    QPixmap *ioPixmap = getPlanetPixmap("io");
    m_ioItem = m_jupiterScene->addPixmap(*ioPixmap);
    m_ioItem->setOffset(-ioPixmap->width() / 2, -ioPixmap->height() / 2);
    m_ioItem->setScale(moonScale);
    m_ioItem->setZValue(2);

    QPixmap *ganymedePixmap = getPlanetPixmap("ganymede");
    m_ganymedeItem = m_jupiterScene->addPixmap(*ganymedePixmap);
    m_ganymedeItem->setOffset(-ganymedePixmap->width() / 2, -ganymedePixmap->height() / 2);
    m_ganymedeItem->setScale(moonScale);
    m_ganymedeItem->setZValue(2);

    int yLabelAreaWidth = maxYLabelWidth + phaseLabel->boundingRect().height();
    int topMargin = (ioPixmap->height() * moonScale) / 2;

    m_jupiterRect.setRect(
        -yLabelAreaWidth,
        -topMargin,
        yLabelAreaWidth + m_jupiterImages[0].width() + legendSpacing + m_jupiterImages[2].width(),
        m_jupiterImages[0].height() + 2 * cmlLabel->boundingRect().height() + topMargin
    );
}

void StarTrackerGUI::updateJupiterMoonPosition(double cml, double ioPhase, double ganymedePhase)
{
    int x = (cml / 360.0) * m_jupiterImages[0].width();
    int y = m_jupiterImages[0].height() - ((ioPhase / 360.0) * m_jupiterImages[0].height());
    m_ioItem->setPos(x, y);

    y = m_jupiterImages[0].height() - ((ganymedePhase / 360.0) * m_jupiterImages[0].height());
    m_ganymedeItem->setPos(x, y);
}

void StarTrackerGUI::updateJupiterMoonPositions(const StarTrackerReport::MsgReportJupiterData& report)
{
    QList<StarTrackerReport::JupiterData> jupiterData = report.getJupiterData();
    QList<StarTrackerReport::JupiterMoonData> moonData = report.getMoonData();

    qDeleteAll(m_jupiterLines);
    m_jupiterLines.clear();
    qDeleteAll(m_jupiterTexts);
    m_jupiterTexts.clear();

    for (int i = 0; i < jupiterData.size() - 1; i++)
    {
        QString hour = QString::number(jupiterData[i].m_dateTime.time().hour());
        double cml1 = moonData[i].m_cml;
        double cml2 = moonData[i+1].m_cml;
        double phase1 = moonData[i].m_phase;
        double phase2 = moonData[i+1].m_phase;

        const int w = m_jupiterImages[0].width();
        const int h = m_jupiterImages[0].height();
        int x1 = (cml1 / 360.0) * w;
        int y1 = h - ((phase1 / 360.0) * h);
        int x2 = (cml2 / 360.0) * w;
        int y2 = h - ((phase2 / 360.0) * h);
        int x2n, y2n;

        bool interpolated = false;
        if ((x2 < x1) && (y2 > y1))
        {
            x2n = 0;
            y2n = h - Interpolation::interpolate(x1 - w, y1, x2, h - y2, 0);
            x2 = Interpolation::interpolate(y1, x1, y2 - h, x2 + w, 0);
            y2 = 0;
            interpolated = true;
        }
        else if (x2 < x1)
        {
            y2 = Interpolation::interpolate(x1, y1, x2 + w, y2, w);
            x2 = w;
            interpolated = true;
            x2n = 0;
            y2n = y2;
        }
        else if (y2 > y1)
        {
            x2 = Interpolation::interpolate(y1, x1, y2 - h, x2, 0);
            y2 = 0;
            interpolated = true;
            x2n = x2;
            y2n = h;
        }

        QGraphicsLineItem *line = m_jupiterScene->addLine(x1, y1, x2, y2, QPen(Qt::white));
        m_jupiterLines.append(line);

        if (interpolated && (i < jupiterData.size() - 1))
        {
            int x3 = (cml2 / 360.0) * w;
            int y3 = h - ((phase2 / 360.0) * h);

            x2 = x2n;
            y2 = y2n;

            line = m_jupiterScene->addLine(x2, y2, x3, y3, QPen(Qt::white));
            m_jupiterLines.append(line);
        }

        QGraphicsLineItem *dash = m_jupiterScene->addLine(x1, y1, x1, y1 + 3, QPen(Qt::white));
        m_jupiterLines.append(dash);

        QGraphicsTextItem *text = m_jupiterScene->addText(hour);
        const int tw = text->boundingRect().width();
        text->setPos(x1 - tw / 2, y1);
        m_jupiterTexts.append(text);
    }

    if (jupiterData.size() > 0)
    {
        int i = jupiterData.size() - 1;

        QString hour = QString::number(jupiterData[i].m_dateTime.time().hour());
        double cml1 = moonData[i].m_cml;
        double phase1 = moonData[i].m_phase;

        int x1 = (cml1 / 360.0) * m_jupiterImages[0].width();
        int y1 = m_jupiterImages[0].height() - ((phase1 / 360.0) * m_jupiterImages[0].height());

        QGraphicsLineItem *dash = m_jupiterScene->addLine(x1, y1, x1, y1 + 3, QPen(Qt::white));
        m_jupiterLines.append(dash);

        QGraphicsTextItem *text = m_jupiterScene->addText(hour);
        int w = text->boundingRect().width();
        text->setPos(x1 - w / 2, y1);
        m_jupiterTexts.append(text);
    }

}

void StarTrackerGUI::plotJupiter()
{
    // Select which Moon to show
    int imageIdx = ui->chartSubSelect->currentIndex();
    bool plotIO = imageIdx == 0;
    bool ployGanymede = imageIdx == 1;

    ui->chart->setVisible(false);
    ui->image->setVisible(true);
    ui->night->setVisible(false);
    ui->logScale->setVisible(false);
    ui->drawSun->setVisible(false);
    ui->drawMoon->setVisible(false);
    ui->darkTheme->setVisible(false);
    ui->zoomIn->setVisible(false);
    ui->zoomOut->setVisible(false);
    ui->addAnimationFrame->setVisible(false);
    ui->clearAnimation->setVisible(false);
    ui->saveAnimation->setVisible(false);
    ui->jupiterElevationLabel->setVisible(true);
    ui->jupiterElevation->setVisible(true);
    ui->cmlLabel->setVisible(true);
    ui->cml->setVisible(true);
    ui->ioPhaseLabel->setVisible(plotIO);
    ui->ioPhase->setVisible(plotIO);
    ui->ganymedePhaseLabel->setVisible(ployGanymede);
    ui->ganymedePhase->setVisible(ployGanymede);

    if (ui->image->scene() != m_jupiterScene)
    {
        ui->image->setScene(m_jupiterScene);
        ui->image->setDragMode(QGraphicsView::NoDrag);
    }

    if (m_zoom)
    {
        delete m_zoom;
        m_zoom = nullptr;
    }

    m_jupiterItems[0]->setVisible(plotIO);
    m_jupiterItems[1]->setVisible(ployGanymede);

    m_ioItem->setVisible(plotIO);
    m_ganymedeItem->setVisible(ployGanymede);
    for (auto& item : m_ioLegendLabels) {
        item->setVisible(plotIO);
    }
    for (auto& item : m_ganymedeLegendLabels) {
        item->setVisible(ployGanymede);
    }

    // Expand to available view
    ui->image->fitInView(m_jupiterRect, Qt::KeepAspectRatio);
}

void StarTrackerGUI::resizeEvent(QResizeEvent *event)
{
    if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_JUPITER) {
        ui->image->fitInView(m_jupiterRect, Qt::KeepAspectRatio);
    }
    FeatureGUI::resizeEvent(event);
}

void StarTrackerGUI::plotSkyTemperatureChart()
{
    ui->chart->setVisible(true);
    ui->image->setVisible(false);
    ui->night->setVisible(false);
    ui->logScale->setVisible(false);
    ui->drawSun->setVisible(true);
    ui->drawMoon->setVisible(true);
    ui->darkTheme->setVisible(false);
    ui->zoomIn->setVisible(false);
    ui->zoomOut->setVisible(false);
    ui->addAnimationFrame->setVisible(false);
    ui->clearAnimation->setVisible(false);
    ui->saveAnimation->setVisible(false);
    ui->jupiterElevationLabel->setVisible(false);
    ui->jupiterElevation->setVisible(false);
    ui->cmlLabel->setVisible(false);
    ui->cml->setVisible(false);
    ui->ioPhaseLabel->setVisible(false);
    ui->ioPhase->setVisible(false);
    ui->ganymedePhaseLabel->setVisible(false);
    ui->ganymedePhase->setVisible(false);

    bool galactic = (ui->chartSubSelect->currentIndex() & 1) == 1;

    m_chart.removeAllSeries();
    removeAllAxes();

    // Plot drift scan path
    QList<QLineSeries*> lineSeries;
    if (m_settings.m_target == "Custom Az/El")
    {
        lineSeries = createDriftScan(galactic);
        QPen pen(getSeriesColor(1), 2, Qt::SolidLine);

        for (int i = 0; i < lineSeries.length(); i++) {
            lineSeries[i]->setPen(pen);
        }
    }

    QScatterSeries *series = new QScatterSeries();
    float ra = Units::raToDecimal(m_settings.m_ra);
    float dec = Units::decToDecimal(m_settings.m_dec);

    double beamWidth = m_settings.m_beamwidth;
    // Ellipse not supported, so draw circle on shorter axis
    double degPerPixelW = 360.0/m_chart.plotArea().width();
    double degPerPixelH = 180.0/m_chart.plotArea().height();
    double degPerPixel = std::min(degPerPixelW, degPerPixelH);
    double markerSize;

    double x, y;
    mapRaDec(ra, dec, galactic, x, y);
    series->append(x, y);

    // Get temperature
    int idx = ui->chartSubSelect->currentIndex();

    if ((idx == 6) || (idx == 7))
    {
        double temp;

        if (m_starTracker->calcSkyTemperature(m_settings.m_frequency, m_settings.m_beamwidth, ra, dec, temp))
        {
            series->setPointLabelsVisible(true);
            series->setPointLabelsColor(Qt::red);
            series->setPointLabelsFormat(QString("%1 K").arg(std::round(temp)));

            // Scale marker size by beamwidth
            markerSize = std::max((int)round(beamWidth * degPerPixel), 5);
        }
    }
    else
    {
        // Read temperature from selected FITS file at target RA/Dec
        QImage *img = &m_images[idx];
        const FITS *fits = m_starTracker->getTempFITS(idx/2);
        double x;

        if (ra <= 12.0) {
            x = (12.0 - ra) / 24.0;
        } else {
            x = (24 - ra + 12) / 24.0;
        }

        int imgX = x * (img->width() - 1);

        if (imgX >= img->width()) {
            imgX = img->width() - 1;
        }

        int imgY = (90.0-dec)/180.0 * (img->height() - 1);

        if (imgY >= img->height()) {
            imgY = img->height() - 1;
        }

        if (fits->valid())
        {
            double temp = fits->scaledValue(imgX, imgY);
            series->setPointLabelsVisible(true);
            series->setPointLabelsColor(Qt::red);
            series->setPointLabelsFormat(QString("%1 K").arg(std::round(temp)));
        }
        else
        {
            qDebug() << "FITS not valid";
        }

        // Temperature from just one pixel, but need to make marker visbile
        markerSize = 5;
    }

    series->setMarkerSize(markerSize);
    series->setColor(getSeriesColor(0));
    m_chart.setTitle("");

    // We want scatter (for the beam) to be on top of line, but same color even when other series
    for (int i = 0; i < lineSeries.length(); i++) {
        m_chart.addSeries(lineSeries[i]);
    }

    // Draw Sun on chart if requested
    QScatterSeries *sunSeries = nullptr;
    if (m_settings.m_drawSunOnSkyTempChart)
    {
        sunSeries = new QScatterSeries();
        mapRaDec(m_sunRA, m_sunDec, galactic, x, y);
        sunSeries->append(x, y);
        sunSeries->setMarkerSize((int)std::max(std::round(0.53 * degPerPixel), 5.0));
        sunSeries->setColor(Qt::yellow);
        sunSeries->setBorderColor(Qt::yellow);

        if (m_settings.m_target != "Sun") // Avoid labels on top of each other
        {
            sunSeries->setPointLabelsVisible(true);
            sunSeries->setPointLabelsColor(Qt::red);
            sunSeries->setPointLabelsFormat("Sun");
        }

        m_chart.addSeries(sunSeries);
    }

    // Draw moon on chart if requested
    QScatterSeries *moonSeries = nullptr;
    if (m_settings.m_drawMoonOnSkyTempChart)
    {
        moonSeries = new QScatterSeries();
        mapRaDec(m_moonRA, m_moonDec, galactic, x, y);
        moonSeries->append(x, y);
        moonSeries->setMarkerSize((int)std::max(std::round(0.53 * degPerPixel), 5.0));
        moonSeries->setColor(qRgb(150, 150, 150));
        moonSeries->setBorderColor(qRgb(150, 150, 150));

        if (m_settings.m_target != "Moon") // Avoid labels on top of each other
        {
            moonSeries->setPointLabelsVisible(true);
            moonSeries->setPointLabelsColor(Qt::red);
            moonSeries->setPointLabelsFormat("Moon");
        }

        m_chart.addSeries(moonSeries);
    }

    m_chart.addSeries(series);

    if (galactic)
    {
        m_chart.addAxis(&m_skyTempGalacticLXAxis, Qt::AlignBottom);
        series->attachAxis(&m_skyTempGalacticLXAxis);

        if (sunSeries) {
            sunSeries->attachAxis(&m_skyTempGalacticLXAxis);
        }
        if (moonSeries) {
            moonSeries->attachAxis(&m_skyTempGalacticLXAxis);
        }

        m_skyTempYAxis.setTitleText(QString("Galactic latitude (%1)").arg(QChar(0xb0)));
        m_chart.addAxis(&m_skyTempYAxis, Qt::AlignLeft);
        series->attachAxis(&m_skyTempYAxis);

        if (sunSeries) {
            sunSeries->attachAxis(&m_skyTempYAxis);
        }
        if (moonSeries) {
            moonSeries->attachAxis(&m_skyTempYAxis);
        }

        for (int i = 0; i < lineSeries.length(); i++)
        {
            lineSeries[i]->attachAxis(&m_skyTempGalacticLXAxis);
            lineSeries[i]->attachAxis(&m_skyTempYAxis);
        }
    }
    else
    {
        m_chart.addAxis(&m_skyTempRAXAxis, Qt::AlignBottom);
        series->attachAxis(&m_skyTempRAXAxis);

        if (sunSeries) {
            sunSeries->attachAxis(&m_skyTempRAXAxis);
        }
        if (moonSeries) {
            moonSeries->attachAxis(&m_skyTempRAXAxis);
        }

        m_skyTempYAxis.setTitleText(QString("Declination (%1)").arg(QChar(0xb0)));
        m_chart.addAxis(&m_skyTempYAxis, Qt::AlignLeft);
        series->attachAxis(&m_skyTempYAxis);

        if (sunSeries) {
            sunSeries->attachAxis(&m_skyTempYAxis);
        }
        if (moonSeries) {
            moonSeries->attachAxis(&m_skyTempYAxis);
        }

        for (int i = 0; i < lineSeries.length(); i++)
        {
            lineSeries[i]->attachAxis(&m_skyTempRAXAxis);
            lineSeries[i]->attachAxis(&m_skyTempYAxis);
        }
    }
    ui->chart->setChart(&m_chart);
    plotAreaChanged(m_chart.plotArea());
}

void StarTrackerGUI::plotAreaChanged(const QRectF &plotArea)
{
    int width = static_cast<int>(plotArea.width());
    int height = static_cast<int>(plotArea.height());
    int viewW = static_cast<int>(ui->chart->width());
    int viewH = static_cast<int>(ui->chart->height());

    // Scale the image to fit plot area
    int imageIdx = ui->chartSubSelect->currentIndex();

    if (imageIdx == -1) {
        return;
    } else if (imageIdx == 6) {
        imageIdx = 2;
    } else if (imageIdx == 7) {
        imageIdx = 3;
    }

    QImage image = m_images[imageIdx].scaled(QSize(width, height), Qt::IgnoreAspectRatio);
    QImage translated(viewW, viewH, QImage::Format_ARGB32);
    translated.fill(Qt::white);
    QPainter painter(&translated);
    painter.drawImage(plotArea.topLeft(), image);

    m_chart.setPlotAreaBackgroundBrush(translated);
    m_chart.setPlotAreaBackgroundVisible(true);
}

void StarTrackerGUI::removeAllAxes()
{
    QList<QAbstractAxis *> axes;
    axes = m_chart.axes(Qt::Horizontal);

    for (QAbstractAxis *axis : axes) {
        m_chart.removeAxis(axis);
    }

    axes = m_chart.axes(Qt::Vertical);

    for (QAbstractAxis *axis : axes) {
        m_chart.removeAxis(axis);
    }
}

// Plot target elevation angle over the day
void StarTrackerGUI::plotElevationLineChart()
{
    ui->chart->setVisible(true);
    ui->image->setVisible(false);
    ui->night->setVisible(true);
    ui->logScale->setVisible(false);
    ui->drawSun->setVisible(false);
    ui->drawMoon->setVisible(false);
    ui->darkTheme->setVisible(true);
    ui->zoomIn->setVisible(false);
    ui->zoomOut->setVisible(false);
    ui->addAnimationFrame->setVisible(false);
    ui->clearAnimation->setVisible(false);
    ui->saveAnimation->setVisible(false);
    ui->jupiterElevationLabel->setVisible(false);
    ui->jupiterElevation->setVisible(false);
    ui->cmlLabel->setVisible(false);
    ui->cml->setVisible(false);
    ui->ioPhaseLabel->setVisible(false);
    ui->ioPhase->setVisible(false);
    ui->ganymedePhaseLabel->setVisible(false);
    ui->ganymedePhase->setVisible(false);

    QChart *oldChart = m_azElLineChart;

    m_azElLineChart = new QChart();
    m_azElLineChart->setTheme(m_settings.m_chartsDarkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    QDateTimeAxis *xAxis = new QDateTimeAxis();
    QValueAxis *yLeftAxis = new QValueAxis();
    QValueAxis *yRightAxis = new QValueAxis();

    m_azElLineChart->legend()->hide();

    m_azElLineChart->layout()->setContentsMargins(0, 0, 0, 0);
    m_azElLineChart->setMargins(QMargins(1, 1, 1, 1));

    QLineSeries *elSeries = new QLineSeries();
    QList<QLineSeries *> azSeriesList;
    QLineSeries *azSeries = new QLineSeries();
    azSeriesList.append(azSeries);
    QPen pen(getSeriesColor(1), 2, Qt::SolidLine);
    azSeries->setPen(pen);

    double maxElevation = -90.0;
    double prevAz;
    QDateTime startTime;
    QDateTime endTime;

    if (m_settings.m_target == m_azElVsTimeTarget)
    {
        if (!m_dateTimes.isEmpty()) {
            startTime = m_dateTimes[0];
        }

        for (int i = 0; i < m_dateTimes.size(); i++)
        {
            QDateTime dt = m_dateTimes[i];

            if (m_elevations[i] > maxElevation) {
                maxElevation = m_elevations[i];
            }

            if (i == 0) {
                prevAz = m_azimuths[i];
            }
            if (((prevAz >= 270) && (m_azimuths[i] < 90)) || ((prevAz < 90) && (m_azimuths[i] >= 270)))
            {
                azSeries = new QLineSeries();
                azSeriesList.append(azSeries);
                azSeries->setPen(pen);
            }

            azSeries->append(dt.toMSecsSinceEpoch(), m_azimuths[i]);
            elSeries->append(dt.toMSecsSinceEpoch(), m_elevations[i]);

            endTime = dt;
            prevAz = m_azimuths[i];
        }
    }

    QDateTime currentTime;

    if (m_settings.m_dateTime.isEmpty())
    {
        currentTime = QDateTime::currentDateTime();
    }
    else
    {
        currentTime = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs);
        if (m_settings.m_utc) {
            currentTime.setTimeZone(QTimeZone::utc());
        }
    }

    m_azElLineChart->addAxis(xAxis, Qt::AlignBottom);
    m_azElLineChart->addAxis(yLeftAxis, Qt::AlignLeft);
    m_azElLineChart->addAxis(yRightAxis, Qt::AlignRight);
    m_azElLineChart->addSeries(elSeries);

    for (int i = 0; i < azSeriesList.size(); i++)
    {
        m_azElLineChart->addSeries(azSeriesList[i]);
        azSeriesList[i]->attachAxis(xAxis);
        azSeriesList[i]->attachAxis(yRightAxis);
    }

    // Plot current target on elevation series
    if (ui->azimuth->hasValue() && ui->elevation->hasValue() && (ui->elevation->value() > 0.0))
    {
        QScatterSeries *posSeries = new QScatterSeries();
        posSeries->setMarkerSize(3);
        posSeries->append(currentTime.toMSecsSinceEpoch(), ui->elevation->value());
        if (m_settings.m_target.startsWith("Custom"))
        {
            posSeries->setPointLabelsVisible(false);
            posSeries->setPointLabelsFormat("");
        }
        else
        {
            posSeries->setPointLabelsVisible(true);
            posSeries->setPointLabelsFormat(m_settings.m_target);
        }
        posSeries->setPointLabelsClipping(false);
        m_azElLineChart->addSeries(posSeries);
        posSeries->attachAxis(xAxis);
        posSeries->attachAxis(yLeftAxis);
    }

    elSeries->attachAxis(xAxis);
    elSeries->attachAxis(yLeftAxis);
    if (m_settings.m_night) {
        xAxis->setTitleText(QString("%1 - %2 %3").arg(startTime.date().toString()).arg(startTime.date().addDays(1).toString()).arg(startTime.timeZoneAbbreviation()));
    } else {
        xAxis->setTitleText(QString("%1 %2").arg(startTime.date().toString()).arg(startTime.timeZoneAbbreviation()));
    }
    xAxis->setFormat("hh");
    xAxis->setTickCount(7);
    xAxis->setRange(startTime, endTime);
    yLeftAxis->setRange(0.0, 90.0);
    yLeftAxis->setTitleText(QString("Elevation (%1)").arg(QChar(0xb0)));
    yRightAxis->setRange(0.0, 360.0);
    yRightAxis->setTitleText(QString("Azimuth (%1)").arg(QChar(0xb0)));

    if ((m_settings.m_target != m_azElVsTimeTarget) || m_dateTimes.isEmpty()) {
        m_azElLineChart->setTitle("Waiting for data");
    } else if (maxElevation < 0) {
        m_azElLineChart->setTitle("Not visible from this latitude");
    } else if (m_settings.m_target.contains("SatelliteTracker")) {
        m_azElLineChart->setTitle("See Satellite Tracker for chart that accounts for satellite's movement");
    } else {
        m_azElLineChart->setTitle("");
    }

    ui->chart->setChart(m_azElLineChart);
    delete oldChart;
}

// Reduce az/el range from 450,180 to 360,90
void StarTrackerGUI::limitAzElRange(double& azimuth, double& elevation) const
{
    if (elevation > 90.0)
    {
        elevation = 180.0 - elevation;
        if (azimuth < 180.0) {
            azimuth += 180.0;
        } else {
            azimuth -= 180.0;
        }
    }
    if (azimuth > 360.0) {
        azimuth -= 360.0f;
    }
    if (azimuth == 0) {
        azimuth = 360.0;
    }
}

// Plot target elevation angle over the day
void StarTrackerGUI::plotElevationPolarChart()
{
    ui->chart->setVisible(true);
    ui->image->setVisible(false);
    ui->night->setVisible(true);
    ui->logScale->setVisible(false);
    ui->drawSun->setVisible(false);
    ui->drawMoon->setVisible(false);
    ui->darkTheme->setVisible(true);
    ui->zoomIn->setVisible(false);
    ui->zoomOut->setVisible(false);
    ui->addAnimationFrame->setVisible(false);
    ui->clearAnimation->setVisible(false);
    ui->saveAnimation->setVisible(false);
    ui->jupiterElevationLabel->setVisible(false);
    ui->jupiterElevation->setVisible(false);
    ui->cmlLabel->setVisible(false);
    ui->cml->setVisible(false);
    ui->ioPhaseLabel->setVisible(false);
    ui->ioPhase->setVisible(false);
    ui->ganymedePhaseLabel->setVisible(false);
    ui->ganymedePhase->setVisible(false);

    QChart *oldChart = m_azElPolarChart;

    m_azElPolarChart = new QPolarChart();
    m_azElPolarChart->setTheme(m_settings.m_chartsDarkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    QValueAxis *angularAxis = new QValueAxis();
    QCategoryAxis *radialAxis = new QCategoryAxis();

    angularAxis->setTickCount(9);
    angularAxis->setMinorTickCount(1);
    angularAxis->setLabelFormat("%d");
    angularAxis->setRange(0, 360);

    radialAxis->setMin(0);
    radialAxis->setMax(90);
    radialAxis->append("90", 0);
    radialAxis->append("60", 30);
    radialAxis->append("30", 60);
    radialAxis->append("0", 90);
    radialAxis->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);

    m_azElPolarChart->addAxis(angularAxis, QPolarChart::PolarOrientationAngular);
    m_azElPolarChart->addAxis(radialAxis, QPolarChart::PolarOrientationRadial);
    m_azElPolarChart->legend()->hide();
    m_azElPolarChart->layout()->setContentsMargins(0, 0, 0, 0);
    m_azElPolarChart->setMargins(QMargins(1, 1, 1, 1));

    double maxElevation = -90.0;

    QLineSeries *polarSeries = new QLineSeries();
    QDateTime currentTime;
    QDateTime dt;

    if (m_settings.m_dateTime.isEmpty()) {
        currentTime = QDateTime::currentDateTime();
    } else {
        currentTime = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs);
    }
    dt = currentTime;
    dt.setTime(QTime(0,0));
    QDateTime endTime = dt;
    QDateTime riseTime;
    QDateTime setTime;
    int riseIdx = -1;
    int setIdx = -1;
    int idx = 0;
    double prevAlt;

    if (m_settings.m_target == m_azElVsTimeTarget)
    {
        for (int i = 0; i < m_dateTimes.size(); i++)
        {
            QDateTime dt = m_dateTimes[i];

            if (m_elevations[i] > maxElevation) {
                maxElevation = m_elevations[i];
            }

            if (i == 0) {
                prevAlt = m_elevations[i];
            }

            // We can have set before rise in a day, if the object starts > 0
            if ((m_elevations[i] >= 0.0) && (prevAlt < 0.0))
            {
                riseTime = dt;
                riseIdx = idx;
            }

            if (( m_elevations[i] < 0.0) && (prevAlt >= 0.0))
            {
                setTime = endTime;
                setIdx = idx;
            }

            polarSeries->append(m_azimuths[i], 90 -  m_elevations[i]);
            idx++;
            endTime = dt;
            prevAlt = m_elevations[i];
        }

        // Polar charts can't handle points that are more than 180 degrees apart, so
        // we need to split passes that cross from 359 -> 0 degrees (or the reverse)
        QList<QLineSeries *> series;
        series.append(new QLineSeries());
        QLineSeries *s = series.first();
        QPen pen(getSeriesColor(0), 2, Qt::SolidLine);
        s->setPen(pen);

        qreal prevAz = polarSeries->at(0).x();
        qreal prevEl = polarSeries->at(0).y();

        for (int i = 1; i < polarSeries->count(); i++)
        {
            qreal az = polarSeries->at(i).x();
            qreal el = polarSeries->at(i).y();

            if ((prevAz > 270.0) && (az <= 90.0))
            {
                double elMid = Interpolation::interpolate(prevAz, prevEl, az+360.0, el, 360.0);
                s->append(360.0, elMid);
                series.append(new QLineSeries());
                s = series.last();
                s->setPen(pen);
                s->append(0.0, elMid);
                s->append(az, el);
            }
            else if ((prevAz <= 90.0) && (az > 270.0))
            {
                double elMid = Interpolation::interpolate(prevAz, prevEl, az-360.0, el, 0.0);
                s->append(0.0, elMid);
                series.append(new QLineSeries());
                s = series.last();
                s->setPen(pen);
                s->append(360.0, elMid);
                s->append(az, el);
            }
            else
            {
                s->append(polarSeries->at(i));
            }

            prevAz = az;
            prevEl = el;
        }

        for (int i = 0; i < series.length(); i++)
        {
            m_azElPolarChart->addSeries(series[i]);
            series[i]->attachAxis(angularAxis);
            series[i]->attachAxis(radialAxis);
        }
    }

    if (m_settings.m_drawRotators != StarTrackerSettings::NO_ROTATORS)
    {
        int redrawTime = 0;
        // Plot rotator position
        QString ourSourceName = QString("F:%1 %2").arg(m_starTracker->getIndexInFeatureSet()).arg(m_starTracker->getIdentifier());
        std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();
        for (int featureSetIndex = 0; featureSetIndex < (int)featureSets.size(); featureSetIndex++)
        {
            FeatureSet *featureSet = featureSets[featureSetIndex];
            for (int featureIndex = 0; featureIndex < featureSet->getNumberOfFeatures(); featureIndex++)
            {
                Feature *feature = featureSet->getFeatureAt(featureIndex);
                if (FeatureUtils::compareFeatureURIs(feature->getURI(), "sdrangel.feature.gs232controller"))
                {
                    QString source;
                    ChannelWebAPIUtils::getFeatureSetting(featureSetIndex, featureIndex, "source", source); // Will return false if source isn't set in Controller
                    int track = 0;
                    ChannelWebAPIUtils::getFeatureSetting(featureSetIndex, featureIndex, "track", track);
                    if ((m_settings.m_drawRotators == StarTrackerSettings::ALL_ROTATORS) || ((source == ourSourceName) && track))
                    {
                        int onTarget = 0;
                        ChannelWebAPIUtils::getFeatureReportValue(featureSetIndex, featureIndex, "onTarget", onTarget);

                        if (!onTarget)
                        {
                            // Target azimuth red dotted line
                            double targetAzimuth, targetElevation;
                            bool targetAzimuthOk = ChannelWebAPIUtils::getFeatureReportValue(featureSetIndex, featureIndex, "targetAzimuth", targetAzimuth);
                            bool targetElevationOk = ChannelWebAPIUtils::getFeatureReportValue(featureSetIndex, featureIndex, "targetElevation", targetElevation);
                            if (targetAzimuthOk && targetElevationOk)
                            {
                                limitAzElRange(targetAzimuth, targetElevation);

                                QScatterSeries *rotatorSeries = new QScatterSeries();
                                QColor color(255, 0, 0, 150);
                                QPen pen(color);
                                rotatorSeries->setPen(pen);
                                rotatorSeries->setColor(color.darker());
                                rotatorSeries->setMarkerSize(20);
                                rotatorSeries->append(targetAzimuth, 90-targetElevation);
                                m_azElPolarChart->addSeries(rotatorSeries);
                                rotatorSeries->attachAxis(angularAxis);
                                rotatorSeries->attachAxis(radialAxis);

                                redrawTime = 333;
                            }
                        }

                        // Current azimuth line. Yellow while off target, green on target.
                        double currentAzimuth, currentElevation;
                        bool currentAzimuthOk = ChannelWebAPIUtils::getFeatureReportValue(featureSetIndex, featureIndex, "currentAzimuth", currentAzimuth);
                        bool currentElevationOk = ChannelWebAPIUtils::getFeatureReportValue(featureSetIndex, featureIndex, "currentElevation", currentElevation);
                        if (currentAzimuthOk && currentElevationOk)
                        {
                            limitAzElRange(currentAzimuth, currentElevation);

                            QScatterSeries *rotatorSeries = new QScatterSeries();
                            QColor color;
                            if (onTarget) {
                                color = QColor(0, 255, 0, 150);
                            } else {
                                color = QColor(255, 255, 0, 150);
                            }
                            rotatorSeries->setPen(QPen(color));
                            rotatorSeries->setColor(color.darker());
                            rotatorSeries->setMarkerSize(20);
                            rotatorSeries->append(currentAzimuth, 90-currentElevation);
                            m_azElPolarChart->addSeries(rotatorSeries);
                            rotatorSeries->attachAxis(angularAxis);
                            rotatorSeries->attachAxis(radialAxis);

                            redrawTime = 333;
                        }
                    }
                }
            }
        }
        if (redrawTime > 0)
        {
            // Redraw to show updated rotator position
            // Update period may be long or custom time might be fixed
            m_redrawTimer.setSingleShot(true);
            m_redrawTimer.start(redrawTime);
        }
    }

    // Create series with single point, so we can plot time of rising
    if (riseTime.isValid())
    {
        QLineSeries *riseSeries = new QLineSeries();
        riseSeries->append(polarSeries->at(riseIdx));
        riseSeries->setPointLabelsFormat(QString("Rise %1").arg(riseTime.time().toString("hh:mm")));
        riseSeries->setPointLabelsVisible(true);
        riseSeries->setPointLabelsClipping(false);
        m_azElPolarChart->addSeries(riseSeries);
        riseSeries->attachAxis(angularAxis);
        riseSeries->attachAxis(radialAxis);
    }

    // Create series with single point, so we can plot time of setting
    if (setTime.isValid())
    {
        QLineSeries *setSeries = new QLineSeries();
        setSeries->append(polarSeries->at(setIdx));
        setSeries->setPointLabelsFormat(QString("Set %1").arg(setTime.time().toString("hh:mm")));
        setSeries->setPointLabelsVisible(true);
        setSeries->setPointLabelsClipping(false);
        m_azElPolarChart->addSeries(setSeries);
        setSeries->attachAxis(angularAxis);
        setSeries->attachAxis(radialAxis);
    }

    // Plot target current position
    if (ui->azimuth->hasValue() && ui->elevation->hasValue() && (ui->elevation->value() > 0.0))
    {
        QScatterSeries *posSeries = new QScatterSeries();
        posSeries->setMarkerSize(3);
        posSeries->append(ui->azimuth->value(), 90 - ui->elevation->value());
        if (m_settings.m_target.startsWith("Custom"))
        {
            posSeries->setPointLabelsVisible(false);
            posSeries->setPointLabelsFormat("");
        }
        else
        {
            posSeries->setPointLabelsVisible(true);
            posSeries->setPointLabelsFormat(m_settings.m_target);
        }
        posSeries->setPointLabelsClipping(false);
        m_azElPolarChart->addSeries(posSeries);
        posSeries->attachAxis(angularAxis);
        posSeries->attachAxis(radialAxis);
    }

    if ((m_settings.m_target != m_azElVsTimeTarget) || m_dateTimes.isEmpty()) {
        m_azElPolarChart->setTitle("Waiting for data");
    } else if (maxElevation < 0) {
        m_azElPolarChart->setTitle("Not visible from this latitude");
    } else if (m_settings.m_target.contains("SatelliteTracker")) {
        m_azElPolarChart->setTitle("See Satellite Tracker for chart that accounts for satellite's movement");
    } else {
        m_azElPolarChart->setTitle("");
    }

    ui->chart->setChart(m_azElPolarChart);

    delete polarSeries;
    delete oldChart;
}

// Find target on the Map
void StarTrackerGUI::on_viewOnMap_clicked()
{
    QString target = m_settings.m_target == "Sun" || m_settings.m_target == "Moon" ? m_settings.m_target : "Star";
    FeatureWebAPIUtils::mapFind(target);
}

void StarTrackerGUI::updateChartSubSelect()
{
    if (ui->chartSelect->currentIndex() == StarTrackerSettings::CHART_SKY_TEMPERATURE)
    {
        ui->chartSubSelect->setItemText(6, QString("%1 MHz %2%3 Equatorial")
                                        .arg((int)std::round(m_settings.m_frequency/1e6))
                                        .arg((int)std::round(m_settings.m_beamwidth))
                                        .arg(QChar(0xb0)));
        ui->chartSubSelect->setItemText(7, QString("%1 MHz %2%3 Galactic")
                                        .arg((int)std::round(m_settings.m_frequency/1e6))
                                        .arg((int)std::round(m_settings.m_beamwidth))
                                        .arg(QChar(0xb0)));
    }
}

void StarTrackerGUI::on_chartSelect_currentIndexChanged(int index)
{
    bool oldState = ui->chartSubSelect->blockSignals(true);
    ui->chartSubSelect->clear();

    QString currentText = ui->chartSubSelect->currentText();

    if (index == 0)
    {
        ui->chartSubSelect->addItem("Az/El vs time");
        ui->chartSubSelect->addItem("Polar");
    }
    else if (index == 2)
    {
        ui->chartSubSelect->addItem(QString("150 MHz 5%1 Equatorial").arg(QChar(0xb0)));
        ui->chartSubSelect->addItem(QString("150 MHz 5%1 Galactic").arg(QChar(0xb0)));
        ui->chartSubSelect->addItem("408 MHz 51' Equatorial");
        ui->chartSubSelect->addItem("408 MHz 51' Galactic");
        ui->chartSubSelect->addItem("1420 MHz 35' Equatorial");
        ui->chartSubSelect->addItem("1420 MHz 35' Galactic");
        ui->chartSubSelect->addItem("Custom Equatorial");
        ui->chartSubSelect->addItem("Custom Galactic");
        ui->chartSubSelect->setCurrentIndex(2);
        updateChartSubSelect();
    }
    else if (index == 3)
    {
        ui->chartSubSelect->addItem("Milky Way");
        ui->chartSubSelect->addItem("Milky Way annotated");
    }
    else if (index == 4)
    {
        for (const auto& body : m_settings.m_solarSystemBodies) {
            ui->chartSubSelect->addItem(body);
        }
        ui->chartSubSelect->addItem("-");
    }
    else if (index == 5)
    {
        ui->chartSubSelect->addItem("Io phase vs CML");
        ui->chartSubSelect->addItem("Ganymede phase vs CML");
    }

    int idx = ui->chartSubSelect->findText(currentText);
    if (idx >= 0) {
        ui->chartSubSelect->setCurrentIndex(idx);
    }

    ui->chartSubSelect->blockSignals(oldState);
    plotChart();

    m_settings.m_chartSelect = (StarTrackerSettings::ChartSelect) index;
    m_settingsKeys.append("chartSelect");
    m_settings.m_chartSubSelect = ui->chartSubSelect->currentIndex();
    m_settingsKeys.append("chartSubSelect");
    applySettings();
}

void StarTrackerGUI::on_chartSubSelect_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_chartSubSelect = index;
    m_settingsKeys.append("chartSubSelect");
    applySettings();

    plotChart();
}

double StarTrackerGUI::convertSolarFluxUnits(double sfu)
{
    switch (m_settings.m_solarFluxUnits)
    {
    case StarTrackerSettings::SFU:
        return sfu;
    case StarTrackerSettings::JANSKY:
        return Units::solarFluxUnitsToJansky(sfu);
    case StarTrackerSettings::WATTS_M_HZ:
        return Units::solarFluxUnitsToWattsPerMetrePerHertz(sfu);
    }
    return 0.0;
}

QString StarTrackerGUI::solarFluxUnit()
{
    switch (m_settings.m_solarFluxUnits)
    {
    case StarTrackerSettings::SFU:
        return "sfu";
    case StarTrackerSettings::JANSKY:
        return "Jy";
    case StarTrackerSettings::WATTS_M_HZ:
        return "Wm^-2Hz^-1";
    }
    return "";
}

// Calculate solar flux at given frequency in SFU
double StarTrackerGUI::calcSolarFlux(double freqMhz)
{
    if (m_solarFluxesValid)
    {
        double solarFlux;
        const int fluxes = sizeof(m_solarFluxFrequencies)/sizeof(*m_solarFluxFrequencies);
        int i;

        for (i = 0; i < fluxes; i++)
        {
            if (freqMhz < m_solarFluxFrequencies[i]) {
                break;
            }
        }

        if (i == 0)
        {
            solarFlux = Interpolation::extrapolate(
                (double)m_solarFluxFrequencies[0],
                (double)m_solarFluxes[0],
                (double)m_solarFluxFrequencies[1],
                (double)m_solarFluxes[1],
                freqMhz
            );
        }
        else if (i == fluxes)
        {
            solarFlux = Interpolation::extrapolate(
                (double)m_solarFluxFrequencies[fluxes-2],
                (double)m_solarFluxes[fluxes-2],
                (double)m_solarFluxFrequencies[fluxes-1],
                (double)m_solarFluxes[fluxes-1],
                freqMhz
            );
        }
        else
        {
            solarFlux = Interpolation::interpolate(
                (double)m_solarFluxFrequencies[i-1],
                (double)m_solarFluxes[i-1],
                (double)m_solarFluxFrequencies[i],
                (double)m_solarFluxes[i],
                freqMhz
            );
        }

        return solarFlux;
    }
    else
    {
        return 0.0;
    }
}

void StarTrackerGUI::displaySolarFlux()
{
    if (((m_settings.m_solarFluxData == StarTrackerSettings::DRAO_2800) && (m_solarFlux == 0.0))
     || ((m_settings.m_solarFluxData != StarTrackerSettings::DRAO_2800) && !m_solarFluxesValid))
    {
        ui->solarFlux->setText("");
    }
    else
    {
        double solarFlux;
        double freqMhz = m_settings.m_frequency/1000000.0;

        if (m_settings.m_solarFluxData == StarTrackerSettings::DRAO_2800)
        {
            solarFlux = m_solarFlux;
            ui->solarFlux->setToolTip(QString("Solar flux density at 2800 MHz"));
        }
        else if (m_settings.m_solarFluxData == StarTrackerSettings::TARGET_FREQ)
        {
            solarFlux = calcSolarFlux(freqMhz);
            ui->solarFlux->setToolTip(QString("Solar flux density interpolated to %1 MHz").arg(freqMhz));
        }
        else
        {
            int idx = m_settings.m_solarFluxData-StarTrackerSettings::L_245;
            solarFlux = m_solarFluxes[idx];
            ui->solarFlux->setToolTip(QString("Solar flux density at %1 MHz").arg(m_solarFluxFrequencies[idx]));
        }

        ui->solarFlux->setText(QString("%1 %2").arg(convertSolarFluxUnits(solarFlux)).arg(solarFluxUnit()));
        ui->solarFlux->setCursorPosition(0);

        // Calculate value at target frequency in Jy for Radio Astronomy plugin
        m_starTracker->getInputMessageQueue()->push(StarTracker::MsgSetSolarFlux::create(Units::solarFluxUnitsToJansky(calcSolarFlux(freqMhz))));
    }
}

bool StarTrackerGUI::readSolarFlux()
{
    QFile file(getSolarFluxFilename());
    QDateTime lastModified = file.fileTime(QFileDevice::FileModificationTime);

    if (QDateTime::currentDateTime().secsTo(lastModified) >= -(60*60*24))
    {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QByteArray bytes = file.readLine();
            QString string(bytes);
            // HHMMSS 245    410     610    1415   2695   4995   8800  15400   Mhz
            // 000000 000019 000027 000037 000056 000073 000116 000202 000514  sfu
            // Occasionally, file will contain ////// in a column, presumably to indicate no data
            // Values can be negative
            QRegularExpression re("([0-9]{2})([0-9]{2})([0-9]{2}) (-?[0-9\\/]+) (-?[0-9\\/]+) (-?[0-9\\/]+) (-?[0-9\\/]+) (-?[0-9\\/]+) (-?[0-9\\/]+) (-?[0-9\\/]+) (-?[0-9\\/]+)");
            QRegularExpressionMatch match = re.match(string);

            if (match.hasMatch())
            {
                for (int i = 0; i < 8; i++)
                    m_solarFluxes[i] = match.capturedTexts()[i+4].toInt();
                m_solarFluxesValid = true;
                displaySolarFlux();
                plotChart();
                return true;
            }
            else
            {
                qDebug() << "StarTrackerGUI::readSolarFlux: No match for " << string;
            }
        }
    }
    else
    {
        qDebug() << "StarTrackerGUI::readSolarFlux: Solar flux data is more than 1 day old";
    }

    return false;
}

void StarTrackerGUI::networkManagerFinished(QNetworkReply *reply)
{
    ui->solarFlux->setText(""); // Don't show obsolete data
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "StarTrackerGUI::networkManagerFinished:"
            << " error(" << (int) replyError
            << "): " << replyError
            << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        QRegularExpression re("\\<th\\>Observed Flux Density\\<\\/th\\>\\<td\\>([0-9]+(\\.[0-9]+)?)\\<\\/td\\>");
        QRegularExpressionMatch match = re.match(answer);

        if (match.hasMatch())
        {
            m_solarFlux = match.capturedTexts()[1].toDouble();
            displaySolarFlux();
        }
        else
        {
            qDebug() << "StarTrackerGUI::networkManagerFinished - No Solar flux found: " << answer;
        }
    }

    reply->deleteLater();
}

QString StarTrackerGUI::getSolarFluxFilename()
{
    return HttpDownloadManager::downloadDir() + "/solar_flux.srd";
}

void StarTrackerGUI::updateSolarFlux(bool all)
{
    qDebug() << "StarTrackerGUI: Updating Solar flux data";

    if ((m_settings.m_solarFluxData != StarTrackerSettings::DRAO_2800) || all)
    {
        QDate today = QDateTime::currentDateTimeUtc().date();
        QString solarFluxFile = getSolarFluxFilename();

        if (m_dlm.confirmDownload(solarFluxFile, nullptr, 0))
        {
            QString urlString = QString("https://downloads.sws.bom.gov.au/wdc/wdc_solradio/data/learmonth/SRD/%1/L%2.SRD")
                                    .arg(today.year()).arg(today.toString("yyMMdd"));
            m_dlm.download(QUrl(urlString), solarFluxFile, this);
        }
    }

    if ((m_settings.m_solarFluxData == StarTrackerSettings::DRAO_2800) || all)
    {
        m_networkRequest.setUrl(QUrl("https://www.spaceweather.gc.ca/forecast-prevision/solar-solaire/solarflux/sx-4-en.php"));
        m_networkManager->get(m_networkRequest);
    }
}

void StarTrackerGUI::autoUpdateSolarFlux()
{
    updateSolarFlux(false);
}

void StarTrackerGUI::on_downloadSolarFlux_clicked()
{
    updateSolarFlux(true);
}

void StarTrackerGUI::on_darkTheme_clicked(bool checked)
{
    m_settings.m_chartsDarkTheme = checked;

    if (m_solarFluxChart) {
        m_solarFluxChart->setTheme(m_settings.m_chartsDarkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    }

    m_chart.setTheme(m_settings.m_chartsDarkTheme ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    plotChart();
    m_settingsKeys.append("chartsDarkTheme");
    applySettings();
}

void StarTrackerGUI::on_night_clicked(bool checked)
{
    m_settings.m_night = checked;
    plotChart();
    m_settingsKeys.append("night");
    applySettings();
}

void StarTrackerGUI::on_logScale_clicked(bool checked)
{
    m_settings.m_logScale = checked;

    if (m_settings.m_logScale)
    {
        // Pluto is ~56 from Sun on log scale
        const double r = 57;
        m_solarSystemScene->setSceneRect(-r, -r, r * 2, r * 2);
    }
    else
    {
        // Mercury is 0.4AU. Neptune is 30 AU
        const double max = 32.0;
        const double pixelScale = 20.0;
        m_solarSystemScene->setSceneRect(-max * pixelScale, -max * pixelScale, 2.0 * max * pixelScale, 2.0 * max * pixelScale);
    }

    plotChart();
    m_settingsKeys.append("logScale");
    applySettings();
}

void StarTrackerGUI::on_drawSun_clicked(bool checked)
{
    m_settings.m_drawSunOnSkyTempChart = checked;
    plotChart();
    m_settingsKeys.append("drawSunOnSkyTempChart");
    applySettings();
}

void StarTrackerGUI::on_drawMoon_clicked(bool checked)
{
    m_settings.m_drawMoonOnSkyTempChart = checked;
    plotChart();
    m_settingsKeys.append("drawMoonOnSkyTempChart");
    applySettings();
}

void StarTrackerGUI::downloadFinished(const QString& filename, bool success)
{
    (void) filename;

    if (success && filename.endsWith("solar_flux.srd")) {
        readSolarFlux();
    } else if (!success) {
        QMessageBox::warning(this, "Failed to download file", QString("Failed to download %1").arg(filename));
    }
}

void StarTrackerGUI::majorBodiesUpdated(const QHash<QString, JPLHorizons::BodyID>& bodies)
{
    m_jplBodies = QStringList();

    QHashIterator<QString, JPLHorizons::BodyID> itr(bodies);
    while (itr.hasNext())
    {
        itr.next();
        const JPLHorizons::BodyID& body = itr.value();
        m_jplBodies.append(body.m_name);
    }

    m_jplBodies.sort();
    updateTargetList();
}

void StarTrackerGUI::updateTargetList()
{
    bool block = ui->target->blockSignals(true);

    ui->target->clear();

    if (m_settings.m_targetSource == "SDRangel")
    {
        QStringList builtinTargets = {
            "Sun",
            "Moon",
            "PSR B0329+54",
            "PSR B0833-45",
            "Sagittarius A",
            "Cassiopeia A",
            "Cygnus A",
            "Taurus A (M1)",
            "Virgo A (M87)",
            "Custom RA/Dec",
            "Custom Az/El",
            "Custom l/b",
            "S7",
            "S8",
            "S9"
        };

        for (const auto& target : builtinTargets) {
            ui->target->addItem(target);
        }
        for (const auto& target : m_availableFeatures) {
            ui->target->addItem(target);
        }
    }
    else if (m_settings.m_targetSource == "SPICE")
    {
        for (const auto& target : m_spiceEphemerides.getTargets(m_settings.m_spiceEphemerides)) {
            ui->target->addItem(target);
        }
    }
    else if (m_settings.m_targetSource == "Horizons")
    {
        for (const auto& body : m_jplBodies) {
            ui->target->addItem(body);
        }
    }

    ui->target->blockSignals(block);

    int index = ui->target->findText(m_settings.m_target, Qt::MatchFixedString); // Case insensitive
    if (index >= 0) {
        ui->target->setCurrentIndex(index);
    }
}

void StarTrackerGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &StarTrackerGUI::on_startStop_toggled);
    QObject::connect(ui->link, &ButtonSwitch::clicked, this, &StarTrackerGUI::on_link_clicked);
    QObject::connect(ui->useMyPosition, &QToolButton::clicked, this, &StarTrackerGUI::on_useMyPosition_clicked);
    QObject::connect(ui->latitude, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &StarTrackerGUI::on_latitude_valueChanged);
    QObject::connect(ui->longitude, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &StarTrackerGUI::on_longitude_valueChanged);
    QObject::connect(ui->rightAscension, &QLineEdit::editingFinished, this, &StarTrackerGUI::on_rightAscension_editingFinished);
    QObject::connect(ui->declination, &QLineEdit::editingFinished, this, &StarTrackerGUI::on_declination_editingFinished);
    QObject::connect(ui->azimuth, &DMSSpinBox::valueChanged, this, &StarTrackerGUI::on_azimuth_valueChanged);
    QObject::connect(ui->elevation, &DMSSpinBox::valueChanged, this, &StarTrackerGUI::on_elevation_valueChanged);
    QObject::connect(ui->azimuthOffset, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &StarTrackerGUI::on_azimuthOffset_valueChanged);
    QObject::connect(ui->elevationOffset, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &StarTrackerGUI::on_elevationOffset_valueChanged);
    QObject::connect(ui->galacticLatitude, &DMSSpinBox::valueChanged, this, &StarTrackerGUI::on_galacticLatitude_valueChanged);
    QObject::connect(ui->galacticLongitude, &DMSSpinBox::valueChanged, this, &StarTrackerGUI::on_galacticLongitude_valueChanged);
    QObject::connect(ui->frequency, qOverload<int>(&QSpinBox::valueChanged), this, &StarTrackerGUI::on_frequency_valueChanged);
    QObject::connect(ui->beamwidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &StarTrackerGUI::on_beamwidth_valueChanged);
    QObject::connect(ui->target, qOverload<int>(&QComboBox::currentIndexChanged), this, &StarTrackerGUI::on_target_currentIndexChanged);
    QObject::connect(ui->targetSource, qOverload<int>(&QComboBox::currentIndexChanged), this, &StarTrackerGUI::on_targetSource_currentIndexChanged);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &StarTrackerGUI::on_displaySettings_clicked);
    QObject::connect(ui->dateTimeSelect, &QComboBox::currentTextChanged, this, &StarTrackerGUI::on_dateTimeSelect_currentTextChanged);
    QObject::connect(ui->dateTime, &WrappingDateTimeEdit::dateTimeChanged, this, &StarTrackerGUI::on_dateTime_dateTimeChanged);
    QObject::connect(ui->utc, &QToolButton::clicked, this, &StarTrackerGUI::on_utc_clicked);
    QObject::connect(ui->setTimeToNow, &QToolButton::clicked, this, &StarTrackerGUI::on_setTimeToNow_clicked);
    QObject::connect(ui->viewOnMap, &QToolButton::clicked, this, &StarTrackerGUI::on_viewOnMap_clicked);
    QObject::connect(ui->chartSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &StarTrackerGUI::on_chartSelect_currentIndexChanged);
    QObject::connect(ui->chartSubSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &StarTrackerGUI::on_chartSubSelect_currentIndexChanged);
    QObject::connect(ui->downloadSolarFlux, &QToolButton::clicked, this, &StarTrackerGUI::on_downloadSolarFlux_clicked);
    QObject::connect(ui->darkTheme, &QToolButton::clicked, this, &StarTrackerGUI::on_darkTheme_clicked);
    QObject::connect(ui->zoomIn, &QToolButton::clicked, this, &StarTrackerGUI::on_zoomIn_clicked);
    QObject::connect(ui->zoomOut, &QToolButton::clicked, this, &StarTrackerGUI::on_zoomOut_clicked);
    QObject::connect(ui->addAnimationFrame, &QToolButton::clicked, this, &StarTrackerGUI::on_addAnimationFrame_clicked);
    QObject::connect(ui->clearAnimation, &QToolButton::clicked, this, &StarTrackerGUI::on_clearAnimation_clicked);
    QObject::connect(ui->saveAnimation, &QToolButton::clicked, this, &StarTrackerGUI::on_saveAnimation_clicked);
    QObject::connect(ui->logScale, &QToolButton::clicked, this, &StarTrackerGUI::on_logScale_clicked);
    QObject::connect(ui->night, &QToolButton::clicked, this, &StarTrackerGUI::on_night_clicked);
    QObject::connect(ui->drawSun, &QToolButton::clicked, this, &StarTrackerGUI::on_drawSun_clicked);
    QObject::connect(ui->drawMoon, &QToolButton::clicked, this, &StarTrackerGUI::on_drawMoon_clicked);
}
