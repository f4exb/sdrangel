///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#include <QDesktopServices>
#include <QAction>
#include <QClipboard>
#include <QMediaPlayer>
#include <QVideoWidget>

#include "feature/featureuiset.h"
#include "feature/featurewebapiutils.h"
#include "channel/channelwebapiutils.h"
#include "gui/crightclickenabler.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "util/csv.h"
#include "util/astronomy.h"
#include "util/vlftransmitters.h"
#include "maincore.h"

#include "ui_sidgui.h"
#include "sid.h"
#include "sidgui.h"
#include "sidsettingsdialog.h"
#include "sidaddchannelsdialog.h"

#include "SWGMapItem.h"

SIDGUI* SIDGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    SIDGUI* gui = new SIDGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void SIDGUI::destroy()
{
    delete this;
}

void SIDGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
}

QByteArray SIDGUI::serialize() const
{
    return m_settings.serialize();
}

bool SIDGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applyAllSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool SIDGUI::handleMessage(const Message& message)
{
    if (SIDMain::MsgConfigureSID::match(message))
    {
        qDebug("SIDGUI::handleMessage: SID::MsgConfigureSID");
        const SIDMain::MsgConfigureSID& cfg = (SIDMain::MsgConfigureSID&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (SIDMain::MsgMeasurement::match(message))
    {
        // Measurements from SIDWorker
        const SIDMain::MsgMeasurement& measurementsMsg = (SIDMain::MsgMeasurement&) message;
        QDateTime dt = measurementsMsg.getDateTime();
        const QStringList& ids = measurementsMsg.getIds();
        const QList<double>& measurements = measurementsMsg.getMeasurements();

        for (int i = 0; i < ids.size(); i++) {
            addMeasurement(ids[i], dt, measurements[i]);
        }
        return true;
    }

    return false;
}

void SIDGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void SIDGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();
    rollupContents->saveState(m_rollupState);
    applySetting("rollupState");
}

SIDGUI::SIDGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::SIDGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0),
    m_fileDialog(nullptr, "Select CSV file", "", "*.csv"),
    m_chartXAxis(nullptr),
    m_chartY1Axis(nullptr),
    m_chartY2Axis(nullptr),
    m_minMeasurement(std::numeric_limits<double>::quiet_NaN()),
    m_maxMeasurement(std::numeric_limits<double>::quiet_NaN()),
    m_xRayChartXAxis(nullptr),
    m_xRayChartYAxis(nullptr),
    m_goesXRay(nullptr),
    m_solarDynamicsObservatory(nullptr),
    m_player(nullptr),
    m_grb(nullptr),
    m_grbSeries(nullptr),
    m_stix(nullptr),
    m_stixSeries(nullptr),
    m_availableFeatureHandler({"sdrangel.feature.map"}),
    m_availableChannelHandler({}, "RM")
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/sid/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_sid = reinterpret_cast<SIDMain*>(feature);
    m_sid->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->startDateTime->blockSignals(true);
    ui->endDateTime->blockSignals(true);
    ui->startDateTime->setDateTime(QDateTime(QDate::currentDate(), QTime(0, 0, 0)));
    ui->endDateTime->setDateTime(QDateTime(QDate::currentDate().addDays(1), QTime(0, 0, 0)));
    ui->startDateTime->blockSignals(false);
    ui->endDateTime->blockSignals(false);

    // Initialise chart
    ui->chart->setRenderHint(QPainter::Antialiasing);
    ui->xRayChart->setRenderHint(QPainter::Antialiasing);

    connect(&m_statusTimer, &QTimer::timeout, this, &SIDGUI::updateStatus);
    m_statusTimer.start(250);

    connect(&m_autosaveTimer, &QTimer::timeout, this, &SIDGUI::autosave);

    m_settings.setRollupState(&m_rollupState);

    CRightClickEnabler *autoscaleXRightClickEnabler = new CRightClickEnabler(ui->autoscaleX);
    connect(autoscaleXRightClickEnabler, &CRightClickEnabler::rightClick, this, &SIDGUI::autoscaleXRightClicked);
    CRightClickEnabler *autoscaleYRightClickEnabler = new CRightClickEnabler(ui->autoscaleY);
    connect(autoscaleYRightClickEnabler, &CRightClickEnabler::rightClick, this, &SIDGUI::autoscaleYRightClicked);
    CRightClickEnabler *todayRightClickEnabler = new CRightClickEnabler(ui->today);
    connect(todayRightClickEnabler, &CRightClickEnabler::rightClick, this, &SIDGUI::todayRightClicked);

    makeUIConnections(); // Enable connections before displaySettings, so autoscaling works
    displaySettings();
    applyAllSettings();
    m_resizer.enableChildMouseTracking();

    // Initialisation for Solar Dynamics Observatory image/video display
    ui->sdoEnabled->setChecked(true);
    ui->sdoProgressBar->setVisible(false);
    ui->sdoImage->setStyleSheet("background-color: black;");
    ui->sdoVideo->setStyleSheet("background-color: black;");
    m_solarDynamicsObservatory = SolarDynamicsObservatory::create();
    if (m_solarDynamicsObservatory)
    {
        m_player = new QMediaPlayer();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        connect(m_player, qOverload<QMediaPlayer::Error>(&QMediaPlayer::error), this, &SIDGUI::sdoVideoError);
        connect(m_player, &QMediaPlayer::bufferStatusChanged, this, &SIDGUI::sdoBufferStatusChanged);
#else
        connect(m_player, &QMediaPlayer::errorOccurred, this, &SIDGUI::sdoVideoError);
        connect(m_player, &QMediaPlayer::bufferProgressChanged, this, &SIDGUI::sdoBufferProgressChanged);
#endif
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &SIDGUI::sdoVideoStatusChanged);
        m_player->setVideoOutput(ui->sdoVideo);

        ui->sdoData->blockSignals(true);
        connect(m_solarDynamicsObservatory, &SolarDynamicsObservatory::imageUpdated, this, &SIDGUI::sdoImageUpdated);
        for (const auto& name : SolarDynamicsObservatory::getImageNames()) {
            ui->sdoData->addItem(name);
        }
        ui->sdoData->blockSignals(false);
        ui->sdoData->setCurrentIndex(1);
        m_settings.m_sdoData = ui->sdoData->currentText();
    }

    // Initialisation for GOES X-Ray data
    m_goesXRay = GOESXRay::create();
    if (m_goesXRay)
    {
        connect(m_goesXRay, &GOESXRay::xRayDataUpdated, this, &SIDGUI::xRayDataUpdated);
        connect(m_goesXRay, &GOESXRay::protonDataUpdated, this, &SIDGUI::protonDataUpdated);
        m_goesXRay->getDataPeriodically();
    }

    // Get Gamma Ray Bursts
    m_grb = GRB::create();
    if (m_grb)
    {
        connect(m_grb, &GRB::dataUpdated, this, &SIDGUI::grbDataUpdated);
        m_grb->getDataPeriodically();
    }

    // Get STIX Solar Flare data
    m_stix = STIX::create();
    if (m_stix)
    {
         connect(m_stix, &STIX::dataUpdated, this, &SIDGUI::stixDataUpdated);
         m_stix->getDataPeriodically();
    }

    plotChart();

    QObject::connect(
        &m_availableFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &SIDGUI::featuresChanged
    );
    m_availableFeatureHandler.scanAvailableChannelsAndFeatures();
    QObject::connect(
        &m_availableChannelHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &SIDGUI::channelsChanged
    );
    m_availableChannelHandler.scanAvailableChannelsAndFeatures();

    QObject::connect(ui->chartSplitter, &QSplitter::splitterMoved, this, &SIDGUI::chartSplitterMoved);
    QObject::connect(ui->sdoSplitter, &QSplitter::splitterMoved, this, &SIDGUI::sdoSplitterMoved);
}

SIDGUI::~SIDGUI()
{
    delete m_grb;
    delete m_stix;

    m_statusTimer.stop();

    clearFromMap();

    delete m_goesXRay;
    if (m_solarDynamicsObservatory)
    {
        delete m_player;
        delete m_solarDynamicsObservatory;
    }
    delete ui;
}


void SIDGUI::connectDataUpdates()
{
    if (m_goesXRay)
    {
        connect(m_goesXRay, &GOESXRay::xRayDataUpdated, this, &SIDGUI::xRayDataUpdated);
        connect(m_goesXRay, &GOESXRay::protonDataUpdated, this, &SIDGUI::protonDataUpdated);
    }
}

void SIDGUI::disconnectDataUpdates()
{
    if (m_goesXRay)
    {
        disconnect(m_goesXRay, &GOESXRay::xRayDataUpdated, this, &SIDGUI::xRayDataUpdated);
        disconnect(m_goesXRay, &GOESXRay::protonDataUpdated, this, &SIDGUI::protonDataUpdated);
    }
}

void SIDGUI::getData()
{
    if (m_goesXRay) {
        m_goesXRay->getData();
    }
}

void SIDGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
    m_settingsKeys.append("workspaceIndex");
}

void SIDGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SIDGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);

    ui->samples->setValue(m_settings.m_samples);
    ui->separateCharts->setChecked(m_settings.m_separateCharts);
    ui->displayLegend->setChecked(m_settings.m_displayLegend);
    ui->plotXRayLongPrimary->setChecked(m_settings.m_plotXRayLongPrimary);
    ui->plotXRayLongSecondary->setChecked(m_settings.m_plotXRayLongSecondary);
    ui->plotXRayShortPrimary->setChecked(m_settings.m_plotXRayShortPrimary);
    ui->plotXRayShortSecondary->setChecked(m_settings.m_plotXRayShortSecondary);
    ui->plotGRB->setChecked(m_settings.m_plotGRB);
    ui->plotSTIX->setChecked(m_settings.m_plotSTIX);
    ui->plotProton->setChecked(m_settings.m_plotProton);
    ui->autoscaleX->setChecked(m_settings.m_autoscaleX);
    ui->autoscaleY->setChecked(m_settings.m_autoscaleY);
    ui->startDateTime->clearMaximumDateTime();
    ui->endDateTime->clearMinimumDateTime();
    if (m_settings.m_startDateTime.isValid()) {
        ui->startDateTime->setDateTime(m_settings.m_startDateTime);
    }
    if (m_settings.m_endDateTime.isValid()) {
        ui->endDateTime->setDateTime(m_settings.m_endDateTime);
    }
    ui->startDateTime->setMaximumDateTime(ui->endDateTime->dateTime());
    ui->endDateTime->setMinimumDateTime(ui->startDateTime->dateTime());
    ui->y1Min->setValue(m_settings.m_y1Min);
    ui->y1Max->setValue(m_settings.m_y1Max);
    setAutoscaleX();
    setAutoscaleY();
    setXAxisRange();
    setY1AxisRange();
    setAutosaveTimer();

    ui->sdoEnabled->setChecked(m_settings.m_sdoEnabled);
    ui->sdoVideoEnabled->setChecked(m_settings.m_sdoVideoEnabled);
    ui->sdoData->setCurrentText(m_settings.m_sdoData);
    ui->sdoNow->setChecked(m_settings.m_sdoNow);
    ui->sdoDateTime->setEnabled(!m_settings.m_sdoNow);
    ui->mapLabel->setEnabled(!m_settings.m_sdoNow);
    ui->map->setEnabled(!m_settings.m_sdoNow);
    ui->sdoDateTime->setDateTime(m_settings.m_sdoDateTime);
    ui->map->setCurrentText(m_settings.m_map);
    applySDO();
    applyDateTime();

    if (m_settings.m_autoload) {
        readCSV(m_settings.m_filename, true);
    }

    getRollupContents()->restoreState(m_rollupState);

    if (m_settings.m_chartSplitterSizes.size() > 0) {
        ui->chartSplitter->setSizes(m_settings.m_chartSplitterSizes);
    }
    if (m_settings.m_sdoSplitterSizes.size() > 0) {
        ui->sdoSplitter->setSizes(m_settings.m_sdoSplitterSizes);
    }

    blockApplySettings(false);
    getRollupContents()->arrangeRollups();
}

void SIDGUI::setAutosaveTimer()
{
    if (m_settings.m_autosave) {
        m_autosaveTimer.start(1000*60*m_settings.m_autosavePeriod);
    } else {
        m_autosaveTimer.stop();
    }
}

void SIDGUI::onMenuDialogCalled(const QPoint &p)
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

        QStringList settingsKeys({
            "rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
        });

        applySettings(m_settingsKeys);
    }

    resetContextMenuType();
}

void SIDGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void SIDGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        SIDMain::MsgConfigureSID* message = SIDMain::MsgConfigureSID::create(m_settings, m_settingsKeys, force);
        m_sid->getInputMessageQueue()->push(message);
        m_settingsKeys.clear();
    }

    m_settingsKeys.clear();
}

void SIDGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void SIDGUI::chartSplitterMoved(int pos, int index)
{
    (void) pos;
    (void) index;

    m_settings.m_chartSplitterSizes = ui->chartSplitter->sizes();
    applySetting("chartSplitterSizes");
}

void SIDGUI::sdoSplitterMoved(int pos, int index)
{
    (void) pos;
    (void) index;

    m_settings.m_sdoSplitterSizes = ui->sdoSplitter->sizes();
    applySetting("chartSplitterSizes");
}

void SIDGUI::on_samples_valueChanged(int value)
{
    m_settings.m_samples = value;
    applySetting("samples");
    plotChart();
}

void SIDGUI::on_separateCharts_toggled(bool checked)
{
    m_settings.m_separateCharts = checked;
    applySetting("separateCharts");
    plotChart();
}

void SIDGUI::on_displayLegend_toggled(bool checked)
{
    m_settings.m_displayLegend = checked;
    applySetting("displayLegend");
    plotChart();
}

void SIDGUI::on_plotXRayLongPrimary_toggled(bool checked)
{
    m_settings.m_plotXRayLongPrimary = checked;
    applySetting("plotXRayLongPrimary");
    plotChart();
}

void SIDGUI::on_plotXRayLongSecondary_toggled(bool checked)
{
    m_settings.m_plotXRayLongSecondary = checked;
    applySetting("plotXRayLongSecondary");
    plotChart();
}

void SIDGUI::on_plotXRayShortPrimary_toggled(bool checked)
{
    m_settings.m_plotXRayShortPrimary = checked;
    applySetting("plotXRayShortPrimary");
    plotChart();
}

void SIDGUI::on_plotXRayShortSecondary_toggled(bool checked)
{
    m_settings.m_plotXRayShortSecondary = checked;
    applySetting("plotXRayShortSecondary");
    plotChart();
}

void SIDGUI::on_plotGRB_toggled(bool checked)
{
    m_settings.m_plotGRB = checked;
    applySetting("plotGRB");
    plotChart();
}

void SIDGUI::on_plotSTIX_toggled(bool checked)
{
    m_settings.m_plotSTIX = checked;
    applySetting("plotSTIX");
    plotChart();
}

void SIDGUI::on_plotProton_toggled(bool checked)
{
    m_settings.m_plotProton = checked;
    applySetting("plotProton");
    plotChart();
}

void SIDGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SIDMain::MsgStartStop *message = SIDMain::MsgStartStop::create(checked);
        m_sid->getInputMessageQueue()->push(message);
    }
}

void SIDGUI::createGRBSeries(QChart *chart, QDateTimeAxis *xAxis, QLogValueAxis *yAxis)
{
    bool secondaryAxis = plotAnyXRay() || m_settings.m_plotSTIX;
    yAxis->setLabelFormat("%.0e");
    yAxis->setGridLineVisible(!secondaryAxis);
    yAxis->setTitleText("GRB Fluence (erg/cm<sup>2</sup>)");
    yAxis->setTitleVisible(m_settings.m_displayAxisTitles);
    yAxis->setVisible(!secondaryAxis || m_settings.m_displaySecondaryAxis);

    if (m_settings.m_plotGRB)
    {
        m_grbSeries = new QScatterSeries();
        m_grbSeries->setName("GRB");
        m_grbSeries->setColor(m_settings.m_grbColor);
        m_grbSeries->setBorderColor(m_settings.m_grbColor);
        m_grbSeries->setMarkerSize(8);

        for (int i = 0; i < m_grbData.size(); i++)
        {
            float value = m_grbData[i].m_fluence;
            if ((value <= 0.0f) || std::isnan(value)) {
                value = m_grbMin; // <= 0 will result in series not being plotted, as log axis used
            }
            m_grbSeries->append(m_grbData[i].m_dateTime.toMSecsSinceEpoch(), value);
        }
        yAxis->setMin(m_grbMin);
        yAxis->setMax(m_grbMax);
        chart->addSeries(m_grbSeries);
        m_grbSeries->attachAxis(xAxis);
        m_grbSeries->attachAxis(yAxis);
    }
    else
    {
        m_grbSeries = nullptr;
    }
}

void SIDGUI::createFlareAxis(QCategoryAxis *yAxis)
{
    // Solar flare classification
    yAxis->setMin(-8);
    yAxis->setMax(-3);
    yAxis->setStartValue(-8);
    yAxis->append("A", -7);
    yAxis->append("B", -6);
    yAxis->append("C", -5);
    yAxis->append("M", -4);
    yAxis->append("X", -3);
    yAxis->setTitleText("Flare Class");
    yAxis->setTitleVisible(m_settings.m_displayAxisTitles);
    yAxis->setLineVisible(m_settings.m_displaySecondaryAxis);
    yAxis->setGridLineVisible(m_settings.m_separateCharts);
}

void SIDGUI::createXRaySeries(QChart *chart, QDateTimeAxis *xAxis, QCategoryAxis *yAxis)
{
    createFlareAxis(yAxis);

    for (int i = 0; i < 2; i++)
    {
        QString name = i == 0 ? "Primary" : "Secondary";

        if (((i == 0) && m_settings.m_plotXRayShortPrimary) || ((i == 1) && m_settings.m_plotXRayShortSecondary))
        {
            m_xrayShortMeasurements[i].m_series = new QLineSeries();
            m_xrayShortMeasurements[i].m_series->setName(QString("0.05-0.4nm X-Ray %1").arg(name));
            m_xrayShortMeasurements[i].m_series->setColor(m_settings.m_xrayShortColors[i]);
            for (int j = 0; j < m_xrayShortMeasurements[i].m_measurements.size(); j++) {
                m_xrayShortMeasurements[i].m_series->append(m_xrayShortMeasurements[i].m_measurements[j].m_dateTime.toMSecsSinceEpoch(), m_xrayShortMeasurements[i].m_measurements[j].m_measurement);
            }
            chart->addSeries(m_xrayShortMeasurements[i].m_series);
            m_xrayShortMeasurements[i].m_series->attachAxis(xAxis);
            m_xrayShortMeasurements[i].m_series->attachAxis(yAxis);
        }
        else
        {
            m_xrayShortMeasurements[i].m_series = nullptr;
        }

        if (((i == 0) && m_settings.m_plotXRayLongPrimary) || ((i == 1) && m_settings.m_plotXRayLongSecondary))
        {
            m_xrayLongMeasurements[i].m_series = new QLineSeries();
            m_xrayLongMeasurements[i].m_series->setName(QString("0.1-0.8nm X-Ray %1").arg(name));
            m_xrayLongMeasurements[i].m_series->setColor(m_settings.m_xrayLongColors[i]);
            for (int j = 0; j < m_xrayLongMeasurements[i].m_measurements.size(); j++) {
                m_xrayLongMeasurements[i].m_series->append(m_xrayLongMeasurements[i].m_measurements[j].m_dateTime.toMSecsSinceEpoch(), m_xrayLongMeasurements[i].m_measurements[j].m_measurement);
            }
            chart->addSeries(m_xrayLongMeasurements[i].m_series);
            m_xrayLongMeasurements[i].m_series->attachAxis(xAxis);
            m_xrayLongMeasurements[i].m_series->attachAxis(yAxis);
        }
        else
        {
            m_xrayLongMeasurements[i].m_series = nullptr;
        }
    }
}

const QStringList SIDGUI::m_protonEnergies = {"10 MeV", "50 MeV", "100 MeV", "500 MeV"};


void SIDGUI::createProtonSeries(QChart *chart, QDateTimeAxis *xAxis, QLogValueAxis *yAxis)
{
    bool secondaryAxis = plotAnyXRay() || m_settings.m_plotSTIX || m_settings.m_plotGRB;
    yAxis->setLabelFormat("%.0e");
    yAxis->setMin(0.01);
    yAxis->setMax(1000.0);
    yAxis->setGridLineVisible(!secondaryAxis);
    yAxis->setTitleText("Proton Flux (Particles / (cm<sup>2</sup> s sr))");
    yAxis->setTitleVisible(m_settings.m_displayAxisTitles);
    yAxis->setVisible(!secondaryAxis || m_settings.m_displaySecondaryAxis);

    for (int i = 0; i < 4; i += 2) // Only plot 10 and 100 MeV so graph isn't too cluttered
    //for (int i = 0; i < 4; i++)
    {
        m_protonMeasurements[i].m_series = new QLineSeries();
        m_protonMeasurements[i].m_series->setName(QString("%1 Proton").arg(SIDGUI::m_protonEnergies[i]));
        m_protonMeasurements[i].m_series->setColor(m_settings.m_protonColors[i]);

        for (int j = 0; j < m_protonMeasurements[i].m_measurements.size(); j++)
        {
            double value =  m_protonMeasurements[i].m_measurements[j].m_measurement;
            if (value >= 0.0) {
                m_protonMeasurements[i].m_series->append(m_protonMeasurements[i].m_measurements[j].m_dateTime.toMSecsSinceEpoch(), value);
            }
        }
        chart->addSeries(m_protonMeasurements[i].m_series);
        m_protonMeasurements[i].m_series->attachAxis(xAxis);
        m_protonMeasurements[i].m_series->attachAxis(yAxis);
    }
}

void SIDGUI::createSTIXSeries(QChart *chart, QDateTimeAxis *xAxis, QCategoryAxis *yAxis)
{
    createFlareAxis(yAxis);

    if (m_settings.m_plotSTIX)
    {
        m_stixSeries = new QScatterSeries();
        m_stixSeries->setName("STIX");
        m_stixSeries->setColor(m_settings.m_stixColor);
        m_stixSeries->setBorderColor(m_settings.m_stixColor);
        m_stixSeries->setMarkerSize(5);
        for (int i = 0; i < m_stixData.size(); i++)
        {
            double value = m_stixData[i].m_flux;
            if (value == 0.0)
            {
                value = -8;
            }
            else
            {
                value = log10(value);
            }
            m_stixSeries->append(m_stixData[i].m_startDateTime.toMSecsSinceEpoch(), value);
        }
        chart->addSeries(m_stixSeries);
        m_stixSeries->attachAxis(xAxis);
        m_stixSeries->attachAxis(yAxis);
    }
    else
    {
        m_stixSeries = nullptr;
    }
}

void SIDGUI::plotChart()
{
    QChart *oldChart = ui->chart->chart();
    QChart *chart;

    chart = new QChart();
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setMargins(QMargins(1, 1, 1, 1));
    chart->setTheme(QChart::ChartThemeDark);
    chart->legend()->setVisible(m_settings.m_displayLegend);
    chart->legend()->setAlignment(m_settings.m_legendAlignment);

    m_chartXAxis = new QDateTimeAxis();
    m_chartY1Axis = new QValueAxis();
    m_chartY2Axis = nullptr;
    m_chartY3Axis = nullptr;
    m_chartProtonAxis = nullptr;
    if (!m_settings.m_separateCharts)
    {
        // XRay flux
        if (plotAnyXRay() || m_settings.m_plotSTIX)
        {
            m_chartY2Axis = new QCategoryAxis();
            chart->addAxis(m_chartY2Axis, Qt::AlignRight);
        }

        // GRB fluence
        if (m_settings.m_plotGRB)
        {
            m_chartY3Axis = new QLogValueAxis();
            chart->addAxis(m_chartY3Axis, Qt::AlignRight);
        }

        // Proton flux
        if (m_settings.m_plotProton)
        {
            m_chartProtonAxis = new QLogValueAxis();
            chart->addAxis(m_chartProtonAxis, Qt::AlignRight);
        }
    }

    chart->addAxis(m_chartXAxis, Qt::AlignBottom);
    chart->addAxis(m_chartY1Axis, Qt::AlignLeft);
    m_chartY1Axis->setTitleText("Power (dB)");
    m_chartY1Axis->setTitleVisible(m_settings.m_displayAxisTitles);

    // Power measurements
    for (auto& measurement : m_channelMeasurements)
    {
        SIDSettings::ChannelSettings *channelSettings = m_settings.getChannelSettings(measurement.m_id);
        if (!channelSettings) {
            qDebug() << "SIDGUI::plotChart: No settings for channel" << measurement.m_id;
        }
        if (channelSettings && channelSettings->m_enabled)
        {
            QLineSeries *series = new QLineSeries();
            series->setName(channelSettings->m_label);
            series->setColor(channelSettings->m_color);

            measurement.newSeries(series, m_settings.m_samples);
            for (int i = 0; i < measurement.m_measurements.size(); i++)
            {
                measurement.appendSeries(measurement.m_measurements[i].m_dateTime, measurement.m_measurements[i].m_measurement);
                updateMeasurementRange(measurement.m_measurements[i].m_measurement);
                updateTimeRange(measurement.m_measurements[i].m_dateTime);
            }
            chart->addSeries(measurement.m_series);
            measurement.m_series->attachAxis(m_chartXAxis);
            measurement.m_series->attachAxis(m_chartY1Axis);
        }
    }

    for (int i = 0; i < 2; i++)
    {
        m_xrayShortMeasurements[i].m_series = nullptr;
        m_xrayLongMeasurements[i].m_series = nullptr;
    }
    m_grbSeries = nullptr;
    m_stixSeries = nullptr;
    for (int i = 0; i < 4; i++) {
        m_protonMeasurements[i].m_series = nullptr;
    }

    if (!m_settings.m_separateCharts)
    {
        // XRay
        if (plotAnyXRay()) {
            createXRaySeries(chart, m_chartXAxis, m_chartY2Axis);
        }

        // GRB
        if (m_settings.m_plotGRB) {
            createGRBSeries(chart, m_chartXAxis, m_chartY3Axis);
        }

        // STIX flares
        if (m_settings.m_plotSTIX) {
            createSTIXSeries(chart, m_chartXAxis, m_chartY2Axis);
        }

        // Proton flux
        if (m_settings.m_plotProton) {
            createProtonSeries(chart, m_chartXAxis, m_chartProtonAxis);
        }
    }

    autoscaleX();
    autoscaleY();
    setXAxisRange();
    setY1AxisRange();

    ui->chart->setChart(chart);
    ui->chart->installEventFilter(this);

    delete oldChart;

    const auto markers = chart->legend()->markers();
    for (QLegendMarker *marker : markers)
    {
        connect(marker, &QLegendMarker::clicked, this, &SIDGUI::legendMarkerClicked);
    }

    for (const auto series : chart->series())
    {
        QXYSeries *s = qobject_cast<QXYSeries *>(series);
        if (s) {
            connect(s, &QXYSeries::clicked, this, &SIDGUI::seriesClicked);
        }
    }

    if (m_settings.m_separateCharts)
    {
        ui->xRayChart->setVisible(true);
        plotXRayChart();
    }
    else
    {
        ui->xRayChart->setVisible(false);
    }
}

bool SIDGUI::plotAnyXRay() const
{
    return m_settings.m_plotXRayLongPrimary || m_settings.m_plotXRayLongSecondary
        || m_settings.m_plotXRayShortPrimary || m_settings.m_plotXRayShortSecondary;
}

void SIDGUI::plotXRayChart()
{
    QChart *oldChart = ui->xRayChart->chart();
    QChart *chart;

    chart = new QChart();
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setMargins(QMargins(1, 1, 1, 1));
    chart->setTheme(QChart::ChartThemeDark);
    chart->legend()->setVisible(m_settings.m_displayLegend);
    chart->legend()->setAlignment(m_settings.m_legendAlignment);

    m_xRayChartXAxis = new QDateTimeAxis();
    chart->addAxis(m_xRayChartXAxis, Qt::AlignBottom);

    if (plotAnyXRay() || m_settings.m_plotSTIX)
    {
        m_xRayChartYAxis = new QCategoryAxis();
        chart->addAxis(m_xRayChartYAxis, Qt::AlignLeft);
    }

    if (m_settings.m_plotGRB)
    {
        m_chartY3Axis = new QLogValueAxis();
        chart->addAxis(m_chartY3Axis, (plotAnyXRay() || m_settings.m_plotSTIX) ? Qt::AlignRight : Qt::AlignLeft);
    }

    if (m_settings.m_plotProton)
    {
        m_chartProtonAxis = new QLogValueAxis();
        chart->addAxis(m_chartProtonAxis, (plotAnyXRay() || m_settings.m_plotSTIX || m_settings.m_plotGRB) ? Qt::AlignRight : Qt::AlignLeft);
    }

    // XRay
    if (plotAnyXRay()) {
        createXRaySeries(chart, m_xRayChartXAxis, m_xRayChartYAxis);
    }

    // GRB
    if (m_settings.m_plotGRB) {
        createGRBSeries(chart, m_xRayChartXAxis, m_chartY3Axis);
    }

    // STIX flares
    if (m_settings.m_plotSTIX) {
        createSTIXSeries(chart, m_xRayChartXAxis, m_xRayChartYAxis);
    }

    // Proton flux
    if (m_settings.m_plotProton) {
        createProtonSeries(chart, m_xRayChartXAxis, m_chartProtonAxis);
    }

    setXAxisRange();

    ui->xRayChart->setChart(chart);
    ui->xRayChart->installEventFilter(this);

    delete oldChart;

    const auto markers = chart->legend()->markers();
    for (QLegendMarker *marker : markers) {
        connect(marker, &QLegendMarker::clicked, this, &SIDGUI::legendMarkerClicked);
    }

    for (const auto series : chart->series())
    {
        QXYSeries *s = qobject_cast<QXYSeries *>(series);
        if (s) {
            connect(s, &QXYSeries::clicked, this, &SIDGUI::seriesClicked);
        }
    }

    if (!(plotAnyXRay() || m_settings.m_plotGRB || m_settings.m_plotSTIX || m_settings.m_plotProton))
    {
        ui->xRayChart->setVisible(false); // Hide empty chart
    }
}

void SIDGUI::legendMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*>(sender());
    marker->series()->setVisible(!marker->series()->isVisible());
    marker->setVisible(true);

    // Dim the marker, if series is not visible
    qreal alpha = 1.0;

    if (!marker->series()->isVisible()) {
        alpha = 0.5;
    }

    QColor color;
    QBrush brush = marker->labelBrush();
    color = brush.color();
    color.setAlphaF(alpha);
    brush.setColor(color);
    marker->setLabelBrush(brush);

    brush = marker->brush();
    color = brush.color();
    color.setAlphaF(alpha);
    brush.setColor(color);
    marker->setBrush(brush);

    QPen pen = marker->pen();
    color = pen.color();
    color.setAlphaF(alpha);
    pen.setColor(color);
    marker->setPen(pen);
}

void SIDGUI::seriesClicked(const QPointF &point)
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(point.x());
    ui->sdoDateTime->setDateTime(dt);
}

static qreal distance(const QPointF& a, const QPointF& b)
{
    qreal dx = a.x() - b.x();
    qreal dy = a.y() - b.y();
    return qSqrt(dx * dx + dy * dy);
}

qreal SIDGUI::pixelDistance(QChart *chart, QAbstractSeries *series, QPointF a, QPointF b)
{
    a = chart->mapToPosition(a, series);
    b = chart->mapToPosition(b, series);
    return distance(a, b);
}

void SIDGUI::sendToSkyMap(const AvailableChannelOrFeature& skymap, float ra, float dec)
{
    QString target = QString("%1 %2").arg(ra).arg(dec);
    FeatureWebAPIUtils::skyMapFind(target, skymap.m_superIndex, skymap.m_index);
}

void SIDGUI::showGRBContextMenu(QContextMenuEvent *contextEvent, QChartView *chartView, int closestPoint)
{
    QMenu *contextMenu = new QMenu(chartView);
    connect(contextMenu, &QMenu::aboutToHide, contextMenu, &QMenu::deleteLater);

    contextMenu->addSection(m_grbData[closestPoint].m_name);

    // Display GRB Fermi data
    QString url = m_grbData[closestPoint].getFermiURL();
    if (!url.isEmpty())
    {
        QAction* fermiDataAction = new QAction("View Fermi data directory...", contextMenu);
        connect(fermiDataAction, &QAction::triggered, this, [url]()->void {
            QDesktopServices::openUrl(QUrl(url));
        });
        contextMenu->addAction(fermiDataAction);

        QString plotURL = m_grbData[closestPoint].getFermiPlotURL();
        QAction* fermiPlotAction = new QAction("View Fermi data plot...", contextMenu);
        connect(fermiPlotAction, &QAction::triggered, this, [plotURL]()->void {
            QDesktopServices::openUrl(QUrl(plotURL));
        });
        contextMenu->addAction(fermiPlotAction);

        QString mapURL = m_grbData[closestPoint].getFermiSkyMapURL();
        QAction* fermiMapDataAction = new QAction("View Fermi sky map...", contextMenu);
        connect(fermiMapDataAction, &QAction::triggered, this, [mapURL]()->void {
            QDesktopServices::openUrl(QUrl(mapURL));
        });
        contextMenu->addAction(fermiMapDataAction);
    }

    // Display Swift link
    if (!m_grbData[closestPoint].m_name.endsWith("*"))
    {
        QAction* swiftDataAction = new QAction("View Swift data...", contextMenu);
        QString switftURL = m_grbData[closestPoint].getSwiftURL();
        connect(swiftDataAction, &QAction::triggered, this, [switftURL]()->void {
            QDesktopServices::openUrl(QUrl(switftURL));
        });
        contextMenu->addAction(swiftDataAction);
    }

    // View GRB ra/dec in SkyMap
    AvailableChannelOrFeatureHandler skymaps({"sdrangel.feature.skymap"});
    skymaps.scanAvailableChannelsAndFeatures();
    if (skymaps.getAvailableChannelOrFeatureList().size() > 0)
    {
        for (const auto& skymap : skymaps.getAvailableChannelOrFeatureList())
        {
            QString label = QString("View coords in %1...").arg(skymap.getLongId());
            QAction* skyMapAction = new QAction(label, contextMenu);
            float ra = m_grbData[closestPoint].m_ra;
            float dec = m_grbData[closestPoint].m_dec;
            connect(skyMapAction, &QAction::triggered, this, [this, skymap, ra, dec]()->void {
                sendToSkyMap(skymap, ra, dec);
            });
            contextMenu->addAction(skyMapAction);
        }
    }
    else
    {
        QAction* skyMapAction = new QAction("View coords in SkyMap...", contextMenu);
        float ra = m_grbData[closestPoint].m_ra;
        float dec = m_grbData[closestPoint].m_dec;
        QString target = QString("%1 %2").arg(ra).arg(dec);
        connect(skyMapAction, &QAction::triggered, this, [target]()->void {
            FeatureWebAPIUtils::openSkyMapAndFind(target);
        });
        contextMenu->addAction(skyMapAction);
    }

    contextMenu->popup(chartView->viewport()->mapToGlobal(contextEvent->pos()));
}

void SIDGUI::showStixContextMenu(QContextMenuEvent *contextEvent, QChartView *chartView, int closestPoint)
{
    QMenu *contextMenu = new QMenu(chartView);
    connect(contextMenu, &QMenu::aboutToHide, contextMenu, &QMenu::deleteLater);

    contextMenu->addSection(m_stixData[closestPoint].m_id);

    // Display GRB Fermi data
    QString lcURL = m_stixData[closestPoint].getLightCurvesURL();
    QAction* lcAction = new QAction("View light curves...", contextMenu);
    connect(lcAction, &QAction::triggered, this, [lcURL]()->void {
        QDesktopServices::openUrl(QUrl(lcURL));
    });
    contextMenu->addAction(lcAction);

    QString dataURL = m_stixData[closestPoint].getDataURL();
    QAction* stixDataAction = new QAction("View STIX data...", contextMenu);
    connect(stixDataAction, &QAction::triggered, this, [dataURL]()->void {
        QDesktopServices::openUrl(QUrl(dataURL));
    });
    contextMenu->addAction(stixDataAction);

    contextMenu->popup(chartView->viewport()->mapToGlobal(contextEvent->pos()));
}

bool SIDGUI::findClosestPoint(QContextMenuEvent *contextEvent, QChart *chart, QScatterSeries *series, int& closestPoint)
{
    QPointF point = chart->mapToValue(contextEvent->pos(), series);
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(point.x());

    // Find nearest point - GRB/Stix data is ordered newest first
    QVector<QPointF> points = series->pointsVector();
    if (points.size() > 0)
    {
        qint64 startTime = m_settings.m_startDateTime.toMSecsSinceEpoch();
        qreal closestDistance = pixelDistance(chart, series, point, points[0]);
        closestPoint = 0;

        for (int i = 1; i < points.size(); i++)
        {
            qreal d = pixelDistance(chart, series, point, points[i]);
            if (d < closestDistance)
            {
                closestDistance = d;
                closestPoint = i;
            }
            if (points[i].x() < startTime) {
                break;
            }
        }
        return closestDistance <= series->markerSize();
    }
    else
    {
        return false;
    }
}

void SIDGUI::showContextMenu(QContextMenuEvent *contextEvent)
{
    QChartView *chartView;

    if (m_settings.m_separateCharts) {
        chartView = ui->xRayChart;
    } else {
        chartView = ui->chart;
    }

    if (chartView)
    {
        int closestPoint;

        if (m_grbSeries && findClosestPoint(contextEvent, chartView->chart(), m_grbSeries, closestPoint)) {
            showGRBContextMenu(contextEvent, chartView, closestPoint);
        } else if (m_stixSeries && findClosestPoint(contextEvent, chartView->chart(), m_stixSeries, closestPoint)) {
            showStixContextMenu(contextEvent, chartView, closestPoint);
        }
    }
}

bool SIDGUI::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == ui->chart) || (obj == ui->xRayChart))
    {
        if (event->type() == QEvent::ContextMenu)
        {
            // Show context menu on chart for GRBs/Flares
            QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent *>(event);

            showContextMenu(contextEvent);
            contextEvent->accept();
            return true;
        }
        else if (event->type() == QEvent::Wheel)
        {
            // Use wheel to zoom in / out of X axis or Y axis if shift held
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);

            int delta = wheelEvent->angleDelta().y(); // delta is typically 120 for one click of wheel

            if (wheelEvent->modifiers() & Qt::ShiftModifier)
            {
                double min = ui->y1Min->value();
                double max = ui->y1Max->value();
                double adj = (max - min) * 0.20 * delta / 120.0;
                min += adj;
                max -= adj;
                ui->y1Min->setValue(min);
                ui->y1Max->setValue(max);
            }
            else
            {
                QDateTime start = ui->startDateTime->dateTime();
                QDateTime end = ui->endDateTime->dateTime();
                qint64 startMS = start.toMSecsSinceEpoch();
                qint64 endMS = end.toMSecsSinceEpoch();
                qint64 diff = endMS - startMS;
                qint64 adj = diff * 0.20 * delta / 120.0;
                endMS -= adj;
                startMS += adj;
                start = QDateTime::fromMSecsSinceEpoch(startMS);
                end = QDateTime::fromMSecsSinceEpoch(endMS);
                ui->startDateTime->setDateTime(start);
                ui->endDateTime->setDateTime(end);
            }

            wheelEvent->accept();
            return true;
        }
    }
    return FeatureGUI::eventFilter(obj, event);
}

void SIDGUI::updateMeasurementRange(double measurement)
{
    if (std::isnan(m_minMeasurement)) {
        m_minMeasurement = measurement;
    } else {
        m_minMeasurement = std::min(m_minMeasurement, measurement);
    }
    if (std::isnan(m_maxMeasurement)) {
        m_maxMeasurement = measurement;
    } else {
        m_maxMeasurement = std::max(m_maxMeasurement, measurement);
    }
}

void SIDGUI::updateTimeRange(QDateTime dateTime)
{
    if (!m_minDateTime.isValid() || (dateTime < m_minDateTime)) {
        m_minDateTime = dateTime;
    }
    if (!m_maxDateTime.isValid() || (dateTime > m_maxDateTime)) {
        m_maxDateTime = dateTime;
    }
}

void SIDGUI::setXAxisRange()
{
    if (m_chartXAxis) {
        m_chartXAxis->setRange(m_settings.m_startDateTime, m_settings.m_endDateTime);
    }
    if (m_xRayChartXAxis) {
        m_xRayChartXAxis->setRange(m_settings.m_startDateTime, m_settings.m_endDateTime);
    }
}

void SIDGUI::setY1AxisRange()
{
    if (m_chartY1Axis) {
        m_chartY1Axis->setRange(m_settings.m_y1Min, m_settings.m_y1Max);
    }
}

void SIDGUI::setButtonBackground(QToolButton *button, bool checked)
{
    if (!checked)
    {
        button->setStyleSheet("");
    }
    else
    {
        button->setStyleSheet(QString("QToolButton{ background-color: %1;  }")
            .arg(palette().highlight().color().darker(150).name()));
    }
}

void SIDGUI::setAutoscaleX()
{
    setButtonBackground(ui->autoscaleX, m_settings.m_autoscaleX);
}

void SIDGUI::setAutoscaleY()
{
    setButtonBackground(ui->autoscaleY, m_settings.m_autoscaleY);
}

void SIDGUI::on_autoscaleX_clicked()
{
    ui->startDateTime->clearMaximumDateTime();
    ui->endDateTime->clearMinimumDateTime();

    if (m_minDateTime.isValid())
    {
        ui->startDateTime->setDateTime(m_minDateTime);
    }
    if (m_maxDateTime.isValid())
    {
        ui->endDateTime->setDateTime(m_maxDateTime);
    }

    ui->startDateTime->setMaximumDateTime(ui->endDateTime->dateTime());
    ui->endDateTime->setMinimumDateTime(ui->startDateTime->dateTime());
}

void SIDGUI::on_autoscaleY_clicked()
{
    if (!std::isnan(m_minMeasurement) && !std::isnan(m_maxMeasurement) && (m_minMeasurement == m_maxMeasurement))
    {
        // Graph doesn't display properly if min is the same as max
        ui->y1Min->setValue(m_minMeasurement * 0.99);
        ui->y1Max->setValue(m_maxMeasurement * 1.01);
    }
    else
    {
        if (!std::isnan(m_minMeasurement)) {
            ui->y1Min->setValue(m_minMeasurement);
        }
        if (!std::isnan(m_maxMeasurement)) {
            ui->y1Max->setValue(m_maxMeasurement);
        }
    }
}

void SIDGUI::on_today_clicked()
{
    QDate today = QDate::currentDate();
    QDateTime start = QDateTime(today, QTime(0,0));
    QDateTime end = QDateTime(today.addDays(1), QTime(0,0));

    ui->startDateTime->clearMaximumDateTime();
    ui->endDateTime->clearMinimumDateTime();

    ui->startDateTime->setDateTime(start);
    ui->endDateTime->setDateTime(end);

    ui->startDateTime->setMaximumDateTime(ui->endDateTime->dateTime());
    ui->endDateTime->setMinimumDateTime(ui->startDateTime->dateTime());
}

void SIDGUI::todayRightClicked()
{
    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();

    QDate today = QDate::currentDate();

    QDateTime sunRise, sunSet;
    Astronomy::sunrise(today, stationLatitude, stationLongitude, sunRise, sunSet);

    ui->startDateTime->clearMaximumDateTime();
    ui->endDateTime->clearMinimumDateTime();

    ui->startDateTime->setDateTime(sunRise);
    ui->endDateTime->setDateTime(sunSet);

    ui->startDateTime->setMaximumDateTime(ui->endDateTime->dateTime());
    ui->endDateTime->setMinimumDateTime(ui->startDateTime->dateTime());
}

void SIDGUI::on_prevDay_clicked()
{
    ui->startDateTime->clearMaximumDateTime();
    ui->endDateTime->clearMinimumDateTime();

    ui->startDateTime->setDateTime(ui->startDateTime->dateTime().addDays(-1));
    ui->endDateTime->setDateTime(ui->endDateTime->dateTime().addDays(-1));

    ui->startDateTime->setMaximumDateTime(ui->endDateTime->dateTime());
    ui->endDateTime->setMinimumDateTime(ui->startDateTime->dateTime());
}

void SIDGUI::on_nextDay_clicked()
{
    ui->startDateTime->clearMaximumDateTime();
    ui->endDateTime->clearMinimumDateTime();

    ui->endDateTime->setDateTime(ui->endDateTime->dateTime().addDays(1));
    ui->startDateTime->setDateTime(ui->startDateTime->dateTime().addDays(1));

    ui->startDateTime->setMaximumDateTime(ui->endDateTime->dateTime());
    ui->endDateTime->setMinimumDateTime(ui->startDateTime->dateTime());
}

void SIDGUI::autoscaleXRightClicked()
{
    m_settings.m_autoscaleX = !m_settings.m_autoscaleX;
    applySetting("autoscaleX");
    setAutoscaleX();
}

void SIDGUI::autoscaleYRightClicked()
{
    m_settings.m_autoscaleY = !m_settings.m_autoscaleY;
    applySetting("autoscaleY");
    setAutoscaleY();
}

void SIDGUI::on_startDateTime_dateTimeChanged(QDateTime value)
{
    m_settings.m_startDateTime = value;
    applySetting("startDateTime");
    setXAxisRange();
    ui->endDateTime->setMinimumDateTime(value);
}

void SIDGUI::on_endDateTime_dateTimeChanged(QDateTime value)
{
    m_settings.m_endDateTime = value;
    applySetting("endDateTime");
    setXAxisRange();
    ui->startDateTime->setMaximumDateTime(value);
}

void SIDGUI::on_y1Min_valueChanged(double value)
{
    m_settings.m_y1Min = (float) value;
    applySetting("y1Min");
    setY1AxisRange();
}

void SIDGUI::on_y1Max_valueChanged(double value)
{
    m_settings.m_y1Max = (float) value;
    applySetting("y1Max");
    setY1AxisRange();
}

void SIDGUI::clearMinMax()
{
    m_minDateTime = QDateTime();
    m_maxDateTime = QDateTime();
    m_minMeasurement = std::numeric_limits<double>::quiet_NaN();
    m_maxMeasurement = std::numeric_limits<double>::quiet_NaN();
}

void SIDGUI::clearAllData()
{
    m_channelMeasurements.clear();
    for (int i = 0; i < 2; i++)
    {
        m_xrayShortMeasurements[i].clear();
        m_xrayLongMeasurements[i].clear();
    }
    for (int i = 0; i < 4; i++) {
        m_protonMeasurements[i].clear();
    }
    clearMinMax();
}

void SIDGUI::on_deleteAll_clicked()
{
    clearAllData();
    plotChart();
    getData();
}

void SIDGUI::on_addChannels_clicked()
{
    SIDAddChannelsDialog dialog(&m_settings);

    new DialogPositioner(&dialog, true);

    dialog.exec();
}


void SIDGUI::on_settings_clicked()
{
    SIDSettingsDialog dialog(&m_settings);

    QObject::connect(
        &dialog,
        &SIDSettingsDialog::removeChannels,
        this,
        &SIDGUI::removeChannels
    );

    new DialogPositioner(&dialog, true);

    if (dialog.exec() == QDialog::Accepted)
    {
        setAutosaveTimer();
        QStringList settingsKeys;
        settingsKeys.append("period");
        settingsKeys.append("autosave");
        settingsKeys.append("autoload");
        settingsKeys.append("filename");
        settingsKeys.append("autosavePeriod");
        settingsKeys.append("legendAlignment");
        settingsKeys.append("displayAxisTitles");
        settingsKeys.append("displayAxisLabels");
        settingsKeys.append("channelSettings");
        settingsKeys.append("xrayShortColors");
        settingsKeys.append("xrayLongColors");
        settingsKeys.append("protonColors");
        settingsKeys.append("grbColor");
        settingsKeys.append("stixColor");
        applySettings(settingsKeys);
        plotChart();
    }
}

void SIDGUI::updateStatus()
{
    int state = m_sid->getState();

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
                QMessageBox::critical(this, m_settings.m_title, m_sid->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void SIDGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &SIDGUI::on_startStop_toggled);
    QObject::connect(ui->samples, QOverload<int>::of(&QSpinBox::valueChanged), this, &SIDGUI::on_samples_valueChanged);
    QObject::connect(ui->separateCharts, &ButtonSwitch::toggled, this, &SIDGUI::on_separateCharts_toggled);
    QObject::connect(ui->displayLegend, &ButtonSwitch::toggled, this, &SIDGUI::on_displayLegend_toggled);
    QObject::connect(ui->plotXRayLongPrimary, &ButtonSwitch::toggled, this, &SIDGUI::on_plotXRayLongPrimary_toggled);
    QObject::connect(ui->plotXRayLongSecondary, &ButtonSwitch::toggled, this, &SIDGUI::on_plotXRayLongSecondary_toggled);
    QObject::connect(ui->plotXRayShortPrimary, &ButtonSwitch::toggled, this, &SIDGUI::on_plotXRayShortPrimary_toggled);
    QObject::connect(ui->plotXRayShortSecondary, &ButtonSwitch::toggled, this, &SIDGUI::on_plotXRayShortSecondary_toggled);
    QObject::connect(ui->plotGRB, &ButtonSwitch::toggled, this, &SIDGUI::on_plotGRB_toggled);
    QObject::connect(ui->plotSTIX, &ButtonSwitch::toggled, this, &SIDGUI::on_plotSTIX_toggled);
    QObject::connect(ui->plotProton, &ButtonSwitch::toggled, this, &SIDGUI::on_plotProton_toggled);
    QObject::connect(ui->sdoEnabled, &ButtonSwitch::toggled, this, &SIDGUI::on_sdoEnabled_toggled);
    QObject::connect(ui->sdoVideoEnabled, &ButtonSwitch::toggled, this, &SIDGUI::on_sdoVideoEnabled_toggled);
    QObject::connect(ui->sdoData, qOverload<int>(&QComboBox::currentIndexChanged), this, &SIDGUI::on_sdoData_currentIndexChanged);
    QObject::connect(ui->sdoNow, &ButtonSwitch::toggled, this, &SIDGUI::on_sdoNow_toggled);
    QObject::connect(ui->sdoDateTime, &WrappingDateTimeEdit::dateTimeChanged, this, &SIDGUI::on_sdoDateTime_dateTimeChanged);
    QObject::connect(ui->showSats, &QToolButton::clicked, this, &SIDGUI::on_showSats_clicked);
    QObject::connect(ui->map, &QComboBox::currentTextChanged, this, &SIDGUI::on_map_currentTextChanged);
    QObject::connect(ui->showPaths, &QToolButton::clicked, this, &SIDGUI::on_showPaths_clicked);
    QObject::connect(ui->autoscaleX, &QPushButton::clicked, this, &SIDGUI::on_autoscaleX_clicked);
    QObject::connect(ui->autoscaleY, &QPushButton::clicked, this, &SIDGUI::on_autoscaleY_clicked);
    QObject::connect(ui->today, &QPushButton::clicked, this, &SIDGUI::on_today_clicked);
    QObject::connect(ui->prevDay, &QPushButton::clicked, this, &SIDGUI::on_prevDay_clicked);
    QObject::connect(ui->nextDay, &QPushButton::clicked, this, &SIDGUI::on_nextDay_clicked);
    QObject::connect(ui->startDateTime, &WrappingDateTimeEdit::dateTimeChanged, this, &SIDGUI::on_startDateTime_dateTimeChanged);
    QObject::connect(ui->endDateTime, &WrappingDateTimeEdit::dateTimeChanged, this, &SIDGUI::on_endDateTime_dateTimeChanged);
    QObject::connect(ui->y1Min, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SIDGUI::on_y1Min_valueChanged);
    QObject::connect(ui->y1Max, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SIDGUI::on_y1Max_valueChanged);
    QObject::connect(ui->deleteAll, &QToolButton::clicked, this, &SIDGUI::on_deleteAll_clicked);
    QObject::connect(ui->saveData, &QToolButton::clicked, this, &SIDGUI::on_saveData_clicked);
    QObject::connect(ui->loadData, &QToolButton::clicked, this, &SIDGUI::on_loadData_clicked);
    QObject::connect(ui->saveChartImage, &QToolButton::clicked, this, &SIDGUI::on_saveChartImage_clicked);
    QObject::connect(ui->addChannels, &QToolButton::clicked, this, &SIDGUI::on_addChannels_clicked);
    QObject::connect(ui->settings, &QToolButton::clicked, this, &SIDGUI::on_settings_clicked);
}

SIDGUI::ChannelMeasurement& SIDGUI::addMeasurements(const QString& id)
{
    ChannelMeasurement measurements = ChannelMeasurement(id, m_settings.m_samples);
    m_channelMeasurements.append(measurements);
    return m_channelMeasurements.last();
}

SIDGUI::ChannelMeasurement& SIDGUI::getMeasurements(const QString& id)
{
    for (int i = 0; i < m_channelMeasurements.size(); i++)
    {
        if (m_channelMeasurements[i].m_id == id)
        {
            return m_channelMeasurements[i];
        }
    }
    return addMeasurements(id);
}

void SIDGUI::addMeasurement(const QString& id, QDateTime dateTime, double measurement)
{
    ChannelMeasurement& measurements = getMeasurements(id);
    measurements.append(dateTime, measurement);
    if (m_chartXAxis)
    {
        if (measurements.m_series)
        {
            updateMeasurementRange(measurement);
            updateTimeRange(dateTime);
            autoscaleX();
            autoscaleY();
        }
        else
        {
            qDebug() << "addMeasurement - measurement has no series calling plotChart";
            plotChart();
        }
    }
    else
    {
        qDebug() << "addMeasurement with no m_chartXAxis - calling plotChart";
        plotChart();
    }
}

void SIDGUI::autoscaleX()
{
    if (m_settings.m_autoscaleX)
    {
        if (m_maxDateTime.isValid() && (!m_settings.m_endDateTime.isValid() || (m_maxDateTime > m_settings.m_endDateTime))) {
            ui->endDateTime->setDateTime(m_maxDateTime);
        }
        if (m_minDateTime.isValid() && (!m_settings.m_startDateTime.isValid() || (m_minDateTime < m_settings.m_startDateTime))) {
            ui->startDateTime->setDateTime(m_minDateTime);
        }
    }
}

void SIDGUI::autoscaleY()
{
    if (m_settings.m_autoscaleY)
    {
        if (!std::isnan(m_minMeasurement) && !std::isnan(m_maxMeasurement) && (m_minMeasurement == m_maxMeasurement))
        {
            // Graph doesn't display properly if min is the same as max
            ui->y1Min->setValue(m_minMeasurement * 0.99);
            ui->y1Max->setValue(m_maxMeasurement * 1.01);
        }
        else
        {
            if (!std::isnan(m_minMeasurement) && (m_minMeasurement != m_settings.m_y1Min)) {
                ui->y1Min->setValue(m_minMeasurement);
            }
            if (!std::isnan(m_maxMeasurement) && (m_maxMeasurement != m_settings.m_y1Max)) {
                ui->y1Max->setValue(m_maxMeasurement);
            }
        }
    }
}

void SIDGUI::xRayDataUpdated(const QList<GOESXRay::XRayData>& data, bool primary)
{
    // Data is at 1-minute intervals, for last 6 hours, so we want to merge with data with already have
    // Assuems oldest data is first in the array
    QDateTime start;
    int idx = primary ? 0 : 1;
    if (m_xrayShortMeasurements[idx].m_measurements.size() > 0) {
        start = m_xrayShortMeasurements[idx].m_measurements.last().m_dateTime;
    }

    for (const auto& measurement : data)
    {
        if (!start.isValid() || (measurement.m_dateTime > start))
        {
            ChannelMeasurement* measurements;

            switch (measurement.m_band)
            {
            case GOESXRay::XRayData::SHORT:
                measurements = &m_xrayShortMeasurements[idx];
                break;
            case GOESXRay::XRayData::LONG:
                measurements = &m_xrayLongMeasurements[idx];
                break;
            default:
                measurements = nullptr;
                break;
            }
            // Ignore flux measurements of 0, as log10(0) is -Inf
            if (measurements && (measurement.m_flux != 0.0))
            {
                double logFlux = log10(measurement.m_flux);
                measurements->append(measurement.m_dateTime, logFlux);
            }
        }
    }

    plotChart();
}

void SIDGUI::protonDataUpdated(const QList<GOESXRay::ProtonData>& data, bool primary)
{
    (void) primary;

    QDateTime start;

    if (m_protonMeasurements[0].m_measurements.size() > 0) {
        start = m_protonMeasurements[0].m_measurements.last().m_dateTime;
    }
    for (const auto& measurement : data)
    {
        if (!start.isValid() || (measurement.m_dateTime > start))
        {
            ChannelMeasurement* measurements = nullptr;

            switch (measurement.m_energy)
            {
            case 10:
                measurements = &m_protonMeasurements[0];
                break;
            case 50:
                measurements = &m_protonMeasurements[1];
                break;
            case 100:
                measurements = &m_protonMeasurements[2];
                break;
            case 500:
                measurements = &m_protonMeasurements[3];
                break;
            }

            if (measurements) {
                measurements->append(measurement.m_dateTime, measurement.m_flux);
            }
        }
    }

    plotChart();
}

void SIDGUI::stixDataUpdated(const QList<STIX::FlareData>& data)
{
    m_stixData = data;
    plotChart();
}

void SIDGUI::grbDataUpdated(const QList<GRB::Data>& data)
{
    m_grbData = data;

    // Calculate min/max of data
    if (m_grbData.size() > 0)
    {
        m_grbMin = std::numeric_limits<float>::max();
        m_grbMax = std::numeric_limits<float>::min();
        for (int i = 0; i < m_grbData.size(); i++)
        {
            if ((m_grbData[i].m_fluence != 0.0f) && (m_grbData[i].m_fluence != -999.0f))
            {
                m_grbMin = std::min(m_grbMin, m_grbData[i].m_fluence);
                m_grbMax = std::max(m_grbMax, m_grbData[i].m_fluence);
            }
        }
    }

    plotChart();
}

void SIDGUI::sdoImageUpdated(const QImage& image)
{
    bool setSize = ui->sdoImage->pixmap(Qt::ReturnByValueConstant()).isNull();

    QPixmap pixmap;
    pixmap.convertFromImage(image);
    ui->sdoImage->setPixmap(pixmap);

    if (setSize)
    {
        QList<int> sizes = ui->sdoSplitter->sizes();
        if (!((sizes[0] == 0) && (sizes[1] == 0)))
        {
            sizes[1] = std::max(sizes[1], 256); // Default size can be a bit small
            ui->sdoSplitter->setSizes(sizes);
        }
    }
}

void SIDGUI::on_sdoEnabled_toggled(bool checked)
{
    m_settings.m_sdoEnabled = checked;
    ui->sdoData->setVisible(checked);
    ui->sdoVideoEnabled->setVisible(checked);
    ui->sdoContainer->setVisible(checked);
    ui->sdoNow->setVisible(checked);
    ui->sdoDateTime->setVisible(checked);
    applySetting("sdoEnabled");
    applySDO();
}

void SIDGUI::on_sdoVideoEnabled_toggled(bool checked)
{
    m_settings.m_sdoVideoEnabled = checked;
    applySetting("sdoVideoEnabled");

    QString currentText = ui->sdoData->currentText();
    ui->sdoData->blockSignals(true);
    ui->sdoData->clear();
    if (checked)
    {
        for (const auto& name : SolarDynamicsObservatory::getVideoNames()) {
            ui->sdoData->addItem(name);
        }
    }
    else
    {
        for (const auto& name : SolarDynamicsObservatory::getImageNames()) {
            ui->sdoData->addItem(name);
        }
    }
    ui->sdoData->blockSignals(false);
    int idx = ui->sdoData->findText(currentText);
    if (idx != -1) {
        ui->sdoData->setCurrentIndex(idx);
    } else {
        ui->sdoData->setCurrentIndex(0);
    }

    applySDO();
}

void SIDGUI::on_sdoNow_toggled(bool checked)
{
    m_settings.m_sdoNow = checked;
    applySetting("sdoNow");
    ui->sdoDateTime->setEnabled(!m_settings.m_sdoNow);
    ui->mapLabel->setEnabled(!m_settings.m_sdoNow);
    ui->map->setEnabled(!m_settings.m_sdoNow);
    applySDO();
    applyDateTime();
}

void SIDGUI::on_sdoData_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_sdoData = ui->sdoData->currentText();
    applySetting("sdoData");
    applySDO();
}

void SIDGUI::on_sdoDateTime_dateTimeChanged(QDateTime value)
{
    m_settings.m_sdoDateTime = value;
    applySetting("sdoDateTime");
    if (!m_settings.m_sdoNow)
    {
        applySDO();
        applyDateTime();
    }
}

void SIDGUI::applySDO()
{
    if (m_solarDynamicsObservatory)
    {
        ui->sdoImage->setVisible(!m_settings.m_sdoVideoEnabled);
        ui->sdoVideo->setVisible(m_settings.m_sdoVideoEnabled);
        if (m_player) {
            m_player->stop();
        }
        if (m_settings.m_sdoVideoEnabled)
        {
            QString videoURL = SolarDynamicsObservatory::getVideoURL(m_settings.m_sdoData);
            if (!videoURL.isEmpty() && m_player)
            {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
                m_player->setMedia(QUrl(videoURL));
#else
                m_player->setSource(QUrl(videoURL));
#endif
                m_player->play();
            }
            // Stop image updates
            m_solarDynamicsObservatory->getImagePeriodically(m_settings.m_sdoData, 512, 0);
        }
        else
        {
            if (m_settings.m_sdoNow) {
                m_solarDynamicsObservatory->getImagePeriodically(m_settings.m_sdoData);
            } else {
                m_solarDynamicsObservatory->getImage(m_settings.m_sdoData, m_settings.m_sdoDateTime);
            }
        }
    }
}

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
// This doesn't seem to get called on Qt5 on Windows
void SIDGUI::sdoBufferStatusChanged(int percentFilled)
{
    ui->sdoProgressBar->setValue(percentFilled);
}
#else
void SIDGUI::sdoBufferProgressChanged(float filled)
{
    ui->sdoProgressBar->setValue((int)std::round(filled * 100.0f));
}
#endif

void SIDGUI::sdoVideoError(QMediaPlayer::Error error)
{
    qWarning() << "SIDGUI::sdoVideoError: " << error << m_player->errorString();
#ifdef _MSC_VER
    // Qt5/Windows doesn't support mp4 by default, so suggest K-Lite codecs
    // Qt6 doesn't need these
    if (error == QMediaPlayer::FormatError) {
        QMessageBox::warning(this, "Video Error", "Unable to play video. Please try installing mp4/h264 codec, such as: <a href='https://www.codecguide.com/download_k-lite_codec_pack_basic.htm'>K-Lite codedcs</a>.");
    }
#elif LINUX
    if (error == QMediaPlayer::FormatError) {
        QMessageBox::warning(this, "Video Error", "Unable to play video. Please try installing mp4/h264 codec, such as gstreamer libav.");
    }
#else
    if (error == QMediaPlayer::FormatError) {
        QMessageBox::warning(this, "Video Error", "Unable to play video. Please try installing an mp4/h264 codec.");
    }
#endif
}

void SIDGUI::sdoVideoStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::LoadingMedia)
    {
        ui->sdoProgressBar->setValue(0);
        ui->sdoProgressBar->setVisible(true);
    }
    else if (status == QMediaPlayer::BufferedMedia)
    {
        ui->sdoProgressBar->setValue(100);
        ui->sdoProgressBar->setVisible(false);
    }
    else if (status == QMediaPlayer::EndOfMedia)
    {
        m_player->setPosition(0);
        m_player->play();
    }
}

void SIDGUI::applyDateTime()
{
    if (!m_settings.m_map.isEmpty() && (m_settings.m_map != "None"))
    {
        if (m_settings.m_sdoNow) {
            FeatureWebAPIUtils::mapSetDateTime(QDateTime::currentDateTime());
        } else {
            FeatureWebAPIUtils::mapSetDateTime(m_settings.m_sdoDateTime);
        }
    }
}

void SIDGUI::on_showSats_clicked()
{
    // Create a Satellite Tracker feature
    MainCore *mainCore = MainCore::instance();
    PluginAPI::FeatureRegistrations *featureRegistrations = mainCore->getPluginManager()->getFeatureRegistrations();
    int nbRegistrations = featureRegistrations->size();
    int index = 0;

    for (; index < nbRegistrations; index++)
    {
        if (featureRegistrations->at(index).m_featureId == "SatelliteTracker") {
            break;
        }
    }

    if (index < nbRegistrations)
    {
        connect(mainCore, &MainCore::featureAdded, this, &SIDGUI::onSatTrackerAdded);

        MainCore::MsgAddFeature *msg = MainCore::MsgAddFeature::create(0, index);
        mainCore->getMainMessageQueue()->push(msg);
    }
    else
    {
        QMessageBox::warning(this, "Error", "Satellite Tracker feature not available");
    }
}

void SIDGUI::onSatTrackerAdded(int featureSetIndex, Feature *feature)
{
    if (feature->getURI() == "sdrangel.feature.satellitetracker")
    {
        disconnect(MainCore::instance(), &MainCore::featureAdded, this, &SIDGUI::onSatTrackerAdded);

        QJsonArray sats = {"SDO", "GOES 16", "GOES-18"};

        ChannelWebAPIUtils::patchFeatureSetting(featureSetIndex, feature->getIndexInFeatureSet(), "satellites", sats);

        ChannelWebAPIUtils::patchFeatureSetting(featureSetIndex, feature->getIndexInFeatureSet(), "target", "SDO");

        ChannelWebAPIUtils::runFeature(featureSetIndex, feature->getIndexInFeatureSet());
    }
}

void SIDGUI::on_map_currentTextChanged(const QString& text)
{
    m_settings.m_map = text;
    applySetting("map");
    applyDateTime();
}

// Plot paths from transmitters to receivers on map
void SIDGUI::on_showPaths_clicked()
{
    clearFromMap();

    for (int i = 0; i < m_settings.m_channelSettings.size(); i++)
    {
        unsigned int deviceSetIndex;
        unsigned int channelIndex;

        if (MainCore::getDeviceAndChannelIndexFromId(m_settings.m_channelSettings[i].m_id, deviceSetIndex, channelIndex))
        {
            // Get position of device, defaulting to My Position
            QGeoCoordinate rxPosition;
            float latitude, longitude, altitude;

            if (ChannelWebAPIUtils::getDevicePosition(deviceSetIndex, latitude, longitude, altitude))
            {
                rxPosition.setLatitude(latitude);
                rxPosition.setLongitude(longitude);
                rxPosition.setAltitude(altitude);
            }
            else
            {
                rxPosition.setLatitude(MainCore::instance()->getSettings().getLatitude());
                rxPosition.setLongitude(MainCore::instance()->getSettings().getLongitude());
                rxPosition.setAltitude(MainCore::instance()->getSettings().getAltitude());
            }

            // Get position of transmitter
            if (VLFTransmitters::m_callsignHash.contains(m_settings.m_channelSettings[i].m_label))
            {
                const VLFTransmitters::Transmitter *transmitter = VLFTransmitters::m_callsignHash.value(m_settings.m_channelSettings[i].m_label);
                QGeoCoordinate txPosition;
                txPosition.setLatitude(transmitter->m_latitude);
                txPosition.setLongitude(transmitter->m_longitude);
                txPosition.setAltitude(0);

                // Calculate mid point for position of label
                qreal distance = txPosition.distanceTo(rxPosition);
                qreal az = txPosition.azimuthTo(rxPosition);
                QGeoCoordinate midPoint = txPosition.atDistanceAndAzimuth(distance / 2.0, az);

                // Create a path from transmitter to receiver
                QList<ObjectPipe*> mapPipes;
                MainCore::instance()->getMessagePipes().getMessagePipes(m_sid, "mapitems", mapPipes);
                if (mapPipes.size() > 0)
                {
                    for (const auto& pipe : mapPipes)
                    {
                        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

                        QString deviceId = QString("%1%2").arg(m_settings.m_channelSettings[i].m_id[0]).arg(deviceSetIndex);

                        QString name = QString("SID %1 to %2").arg(m_settings.m_channelSettings[i].m_label).arg(deviceId);
                        QString details = QString("%1<br>Distance: %2 km").arg(name).arg((int) std::round(distance / 1000.0));

                        swgMapItem->setName(new QString(name));
                        swgMapItem->setLatitude(midPoint.latitude());
                        swgMapItem->setLongitude(midPoint.longitude());
                        swgMapItem->setAltitude(midPoint.altitude());
                        QString image = QString("none");
                        swgMapItem->setImage(new QString(image));
                        swgMapItem->setImageRotation(0);
                        swgMapItem->setText(new QString(details));   // Not used - label is used instead for now
                        swgMapItem->setFixedPosition(true);
                        swgMapItem->setLabel(new QString(details));
                        swgMapItem->setAltitudeReference(0);
                        QList<SWGSDRangel::SWGMapCoordinate *> *coords = new QList<SWGSDRangel::SWGMapCoordinate *>();

                        SWGSDRangel::SWGMapCoordinate* c = new SWGSDRangel::SWGMapCoordinate();
                        c->setLatitude(rxPosition.latitude());
                        c->setLongitude(rxPosition.longitude());
                        c->setAltitude(rxPosition.altitude());
                        coords->append(c);

                        c = new SWGSDRangel::SWGMapCoordinate();
                        c->setLatitude(txPosition.latitude());
                        c->setLongitude(txPosition.longitude());
                        c->setAltitude(txPosition.altitude());
                        coords->append(c);

                        swgMapItem->setColorValid(1);
                        swgMapItem->setColor(m_settings.m_channelSettings[i].m_color.rgba());

                        swgMapItem->setCoordinates(coords);
                        swgMapItem->setType(3);

                        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_sid, swgMapItem);
                        messageQueue->push(msg);

                        m_mapItemNames.append(name);
                    }
                }
            }
        }
    }
}

void SIDGUI::clearFromMap()
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_sid, "mapitems", mapPipes);

    for (const auto& name : m_mapItemNames)
    {
        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(name));
            swgMapItem->setImage(new QString(""));
            swgMapItem->setType(3);
            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_sid, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void SIDGUI::featuresChanged(const QStringList& renameFrom, const QStringList& renameTo)
{
    const AvailableChannelOrFeatureList availableFeatures = m_availableFeatureHandler.getAvailableChannelOrFeatureList();

    if (renameFrom.contains(m_settings.m_map))
    {
        m_settings.m_map = renameTo[renameFrom.indexOf(m_settings.m_map)];
        applySetting("map");
    }

    ui->map->blockSignals(true);
    ui->map->clear();
    ui->map->addItem("None");
    for (const auto& map : availableFeatures) {
        ui->map->addItem(map.getId());
    }

    int idx = ui->map->findText(m_settings.m_map);
    if (idx >= 0) {
        ui->map->setCurrentIndex(idx);
    } else {
        ui->map->setCurrentIndex(-1);
    }

    ui->map->blockSignals(false);

    // If no setting, default to first available map
    if (m_settings.m_map.isEmpty() && (ui->map->count() >= 2)) {
        ui->map->setCurrentIndex(1);
    }
}

void SIDGUI::channelsChanged(const QStringList& renameFrom, const QStringList& renameTo, const QStringList& removed, const QStringList& added)
{
    removeChannels(removed);

    // Rename measurements and settings that have had their id changed
    for (int i = 0; i < renameFrom.size(); i++)
    {
        for (int j = 0; j < m_channelMeasurements.size(); j++)
        {
            if (m_channelMeasurements[j].m_id == renameFrom[i]) {
                m_channelMeasurements[j].m_id = renameTo[i];
            }
        }
        for (int j = 0; j < m_settings.m_channelSettings.size(); j++)
        {
            if (m_settings.m_channelSettings[j].m_id == renameFrom[i]) {
                m_settings.m_channelSettings[j].m_id = renameTo[i];
            }
        }
    }

    // Create settings for any new channels
    // Don't call createChannelSettings when channels are removed, as ids might not have been updated yet
    if (added.size() > 0)
    {
        if (m_settings.createChannelSettings()) {
            applySetting("channelSettings");
        }
    }
}

void SIDGUI::removeChannels(const QStringList& ids)
{
    for (int i = 0; i < ids.size(); i++)
    {
        for (int j = 0; j < m_channelMeasurements.size(); j++)
        {
            if (ids[i] == m_channelMeasurements[j].m_id)
            {
                m_channelMeasurements.removeAt(j);
                break;
            }
        }

        for (int j = 0; j < m_settings.m_channelSettings.size(); j++)
        {
            if (ids[i] == m_settings.m_channelSettings[j].m_id)
            {
                m_settings.m_channelSettings.removeAt(j);
                break;
            }
        }
    }
}

void SIDGUI::autosave()
{
    qDebug() << "SIDGUI::autosave start";
    writeCSV(m_settings.m_filename);
    qDebug() << "SIDGUI::autosave done";
}

void SIDGUI::on_saveData_clicked()
{
    m_fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (m_fileDialog.exec())
    {
        QStringList fileNames = m_fileDialog.selectedFiles();
        if (fileNames.size() > 0) {
            writeCSV(fileNames[0]);
        }
    }
}

void SIDGUI::on_loadData_clicked()
{
    m_fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    if (m_fileDialog.exec())
    {
        QStringList fileNames = m_fileDialog.selectedFiles();
        if (fileNames.size() > 0) {
            readCSV(fileNames[0], false);
        }
    }
}

void SIDGUI::writeCSV(const QString& filename)
{
    if (m_channelMeasurements.size() < 1) {
        return;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "SID", QString("Failed to open file %1").arg(filename));
        return;
    }
    QTextStream out(&file);

    // Create a CSV file from the values in the table
    QList<int> idx;
    QList<ChannelMeasurement *> measurements;
    out << "Date and Time,";
    for (int i = 0; i < m_channelMeasurements.size(); i++)
    {
        SIDSettings::ChannelSettings *channelSettings = m_settings.getChannelSettings(m_channelMeasurements[i].m_id);
        QString name = m_channelMeasurements[i].m_id;
        if (channelSettings)
        {
            name.append("-");
            name.append(channelSettings->m_label);
        }
        out <<  name << ",";
        measurements.append(&m_channelMeasurements[i]);
        idx.append(0);
    }

    out << "X-Ray Primary Short,";
    measurements.append(&m_xrayShortMeasurements[0]);
    idx.append(0);
    out << "X-Ray Primary Long,";
    measurements.append(&m_xrayLongMeasurements[0]);
    idx.append(0);
    out << "X-Ray Secondary Short,";
    measurements.append(&m_xrayShortMeasurements[1]);
    idx.append(0);
    out << "X-Ray Secondary Long,";
    measurements.append(&m_xrayLongMeasurements[1]);
    idx.append(0);

    for (int i = 0; i < 4; i++)
    {
        out << QString("%1 Proton,").arg(SIDGUI::m_protonEnergies[i]);
        measurements.append(&m_protonMeasurements[i]);
        idx.append(0);
    }

    out << "\n";

    // Find earliest time
    QDateTime t;
    for (int i = 0; i < measurements.size(); i++)
    {
        ChannelMeasurement *cm = measurements[i];
        Measurement *m = &cm->m_measurements[idx[i]];
        if (!t.isValid() || (m->m_dateTime < t)) {
            t = m->m_dateTime;
        }
    }

    bool done = false;
    while (!done)
    {
        out << t.toUTC().toString(Qt::ISODateWithMs);
        out << ",";

        // Output data at this time
        for (int i = 0; i < measurements.size(); i++)
        {
            ChannelMeasurement *cm = measurements[i];
            if (cm->m_measurements.size() > idx[i])
            {
                Measurement *m = &cm->m_measurements[idx[i]];
                if (m->m_dateTime == t)
                {
                    out << m->m_measurement;
                    idx[i]++;
                }
            }
            out << ",";
        }
        out << "\n";

        // Find next time
        t = QDateTime();
        for (int i = 0; i < measurements.size(); i++)
        {
            ChannelMeasurement *cm = measurements[i];
            if (cm->m_measurements.size() > idx[i])
            {
                Measurement *m = &cm->m_measurements[idx[i]];
                if (!t.isValid() || (m->m_dateTime < t)) {
                    t = m->m_dateTime;
                }
            }
        }
        if (!t.isValid()) {
            done = true;
        }
    }
}

void SIDGUI::readCSV(const QString& filename, bool autoload)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (!autoload) {
            QMessageBox::critical(this, "SID", QString("Failed to open file %1").arg(filename));
        }
        return;
    }
    QTextStream in(&file);

    // Prevent data updates while reading CSV
    disconnectDataUpdates();

    // Delete existing data

    clearAllData();

    // Get list of colors to use
    QList<QRgb> colors = SIDSettings::m_defaultColors;
    for (const auto& channelSettings : m_settings.m_channelSettings) {
        colors.removeAll(channelSettings.m_color.rgb());
    }

    bool channelSettingsChanged = false;
    QStringList colNames;
    if (CSV::readRow(in, &colNames))
    {
        QList<ChannelMeasurement *> measurements;
        for (int i = 0; i < colNames.size() - 1; i++) {
            measurements.append(nullptr);
        }
        for (int i = 1; i < colNames.size(); i++)
        {
            QString name = colNames[i];
            if (name == "X-Ray Primary Short")
            {
                measurements[i-1] = &m_xrayShortMeasurements[0];
            }
            else if (name == "X-Ray Primary Long")
            {
                measurements[i-1] = &m_xrayLongMeasurements[0];
            }
            else if (name == "X-Ray Secondary Short")
            {
                measurements[i-1] = &m_xrayShortMeasurements[1];
            }
            else if (name == "X-Ray Secondary Long")
            {
                measurements[i-1] = &m_xrayLongMeasurements[1];
            }
            else if (name.endsWith("Proton"))
            {
                for (int j = 0; j < m_protonEnergies.size(); j++)
                {
                    if (name.startsWith(m_protonEnergies[j]))
                    {
                        measurements[i-1] = &m_protonMeasurements[j];
                        break;
                    }
                }
            }
            else if (name.contains(":"))
            {
                QString id;

                int idx = name.indexOf('-');
                if (idx >= 0) {
                    id = name.left(idx);
                } else {
                    id = name;
                }
                measurements[i-1] = &addMeasurements(id);

                // Create settings, if we don't have them
                SIDSettings::ChannelSettings *channelSettings = m_settings.getChannelSettings(id);
                if (!channelSettings)
                {
                    if (colors.size() == 0) {
                        colors = SIDSettings::m_defaultColors;
                    }

                    SIDSettings::ChannelSettings newSettings;
                    newSettings.m_id = id;
                    newSettings.m_enabled = true;
                    newSettings.m_label = name.mid(idx + 1);
                    newSettings.m_color = colors.takeFirst();
                    m_settings.m_channelSettings.append(newSettings);
                    channelSettingsChanged = true;
                }
            }
        }

        QMessageBox dialog(this);
        dialog.setText("Reading data");
        dialog.addButton(QMessageBox::Cancel);
        dialog.show();
        QApplication::processEvents();

        bool cancelled = false;
        QStringList cols;
        int row = 1;

        while(!cancelled && CSV::readRow(in, &cols))
        {
            if (cols.size() == measurements.size() + 1)
            {
                QDateTime dateTime = QDateTime::fromString(cols[0], Qt::ISODateWithMs);

                for (int i = 0; i < measurements.size(); i++)
                {
                    QString valueStr = cols[i+1];
                    if (!valueStr.isEmpty())
                    {
                        double value = valueStr.toDouble();
                        measurements[i]->append(dateTime, value, false);
                    }
                }
            }
            else
            {
                qDebug() << "SIDGUI::readCSV: Not enough data on row " << row;
            }
            if (row % 10000 == 0)
            {
                QApplication::processEvents();
                if (dialog.clickedButton()) {
                    cancelled = true;
                }
            }
            row++;
        }

        dialog.close();

        autoscaleX();
        autoscaleY();
        plotChart();
        connectDataUpdates();
        getData();
        if (channelSettingsChanged) {
            applySetting("channelSettings");
        }
    }
}

void SIDGUI::on_saveChartImage_clicked()
{
    QFileDialog fileDialog(nullptr, "Select file to save image to", "", "*.png *.jpg *.jpeg *.bmp *.ppm *.xbm *.xpm");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QImage image(ui->chart->size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            ui->chart->render(&painter);
            if (!image.save(fileNames[0])) {
                QMessageBox::critical(this, "SID", QString("Failed to save image to %1").arg(fileNames[0]));
            }
        }
    }
}
