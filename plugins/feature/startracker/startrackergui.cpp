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

#include <cmath>
#include <algorithm>
#include <QMessageBox>
#include <QLineEdit>
#include <QRegExp>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#include "feature/featureuiset.h"
#include "feature/featurewebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"
#include "util/units.h"
#include "util/astronomy.h"

#include "ui_startrackergui.h"
#include "startracker.h"
#include "startrackergui.h"
#include "startrackerreport.h"
#include "startrackersettingsdialog.h"


StarTrackerGUI* StarTrackerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    StarTrackerGUI* gui = new StarTrackerGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void StarTrackerGUI::destroy()
{
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

QString StarTrackerGUI::convertDegreesToText(double degrees)
{
    if (m_settings.m_azElUnits == StarTrackerSettings::DMS)
        return Units::decimalDegreesToDegreeMinutesAndSeconds(degrees);
    else if (m_settings.m_azElUnits == StarTrackerSettings::DM)
        return Units::decimalDegreesToDegreesAndMinutes(degrees);
    else if (m_settings.m_azElUnits == StarTrackerSettings::D)
        return Units::decimalDegreesToDegrees(degrees);
    else
        return QString("%1").arg(degrees, 0, 'f', 2);
}

bool StarTrackerGUI::handleMessage(const Message& message)
{
    if (StarTracker::MsgConfigureStarTracker::match(message))
    {
        qDebug("StarTrackerGUI::handleMessage: StarTracker::MsgConfigureStarTracker");
        const StarTracker::MsgConfigureStarTracker& cfg = (StarTracker::MsgConfigureStarTracker&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (StarTrackerReport::MsgReportAzAl::match(message))
    {
        StarTrackerReport::MsgReportAzAl& azAl = (StarTrackerReport::MsgReportAzAl&) message;
        ui->azimuth->setText(convertDegreesToText(azAl.getAzimuth()));
        ui->elevation->setText(convertDegreesToText(azAl.getElevation()));
        return true;
    }
    else if (StarTrackerReport::MsgReportRADec::match(message))
    {
        StarTrackerReport::MsgReportRADec& raDec = (StarTrackerReport::MsgReportRADec&) message;
        m_settings.m_ra = Units::decimalHoursToHoursMinutesAndSeconds(raDec.getRA());
        m_settings.m_dec = Units::decimalDegreesToDegreeMinutesAndSeconds(raDec.getDec());
        ui->rightAscension->setText(m_settings.m_ra);
        ui->declination->setText(m_settings.m_dec);
        raDecChanged();
        return true;
    }

    return false;
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
}

StarTrackerGUI::StarTrackerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::StarTrackerGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0),
    m_networkManager(nullptr),
    m_progressDialog(nullptr),
    m_solarFlux(0.0),
    m_solarFluxesValid(false),
    m_images{QImage(":/startracker/startracker/150mhz_ra_dec.png"),
        QImage(":/startracker/startracker/150mhz_galactic.png"),
        QImage(":/startracker/startracker/408mhz_ra_dec.png"),
        QImage(":/startracker/startracker/408mhz_galactic.png"),
        QImage(":/startracker/startracker/1420mhz_ra_dec.png"),
        QImage(":/startracker/startracker/1420mhz_galactic.png")},
    m_temps{FITS(":/startracker/startracker/150mhz_ra_dec.fits"),
        FITS(":/startracker/startracker/408mhz_ra_dec.fits"),
        FITS(":/startracker/startracker/1420mhz_ra_dec.fits")},
    m_spectralIndex(":/startracker/startracker/408mhz_ra_dec_spectral_index.fits")
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setChannelWidget(false);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    m_starTracker = reinterpret_cast<StarTracker*>(feature);
    m_starTracker->setMessageQueueToGUI(&m_inputMessageQueue);

    m_featureUISet->addRollupWidget(this);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &StarTrackerGUI::downloadFinished);

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(1000);

    // Intialise chart
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

    // Use My Position from preferences, if none set
    if ((m_settings.m_latitude == 0.0) && (m_settings.m_longitude == 0.0))
        on_useMyPosition_clicked();

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
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));

    readSolarFlux();
    connect(&m_solarFluxTimer, SIGNAL(timeout()), this, SLOT(autoUpdateSolarFlux()));
    m_solarFluxTimer.start(1000*60*60*24); // Update every 24hours
    autoUpdateSolarFlux();
}

StarTrackerGUI::~StarTrackerGUI()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    delete ui;
}

void StarTrackerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void StarTrackerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->latitude->setValue(m_settings.m_latitude);
    ui->longitude->setValue(m_settings.m_longitude);
    ui->target->setCurrentIndex(ui->target->findText(m_settings.m_target));
    if (m_settings.m_target == "Custom")
    {
        ui->rightAscension->setText(m_settings.m_ra);
        ui->declination->setText(m_settings.m_dec);
    }
    if (m_settings.m_dateTime == "")
    {
        ui->dateTimeSelect->setCurrentIndex(0);
        ui->dateTime->setVisible(false);
    }
    else
    {
        ui->dateTime->setDateTime(QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs));
        ui->dateTime->setVisible(true);
        ui->dateTimeSelect->setCurrentIndex(1);
    }
    if ((m_settings.m_solarFluxData != StarTrackerSettings::DRAO_2800) && !m_solarFluxesValid)
        autoUpdateSolarFlux();
    ui->frequency->setValue(m_settings.m_frequency/1000000.0);
    ui->beamwidth->setValue(m_settings.m_beamwidth);
    updateForTarget();
    plotChart();
    blockApplySettings(false);
}

void StarTrackerGUI::leaveEvent(QEvent*)
{
}

void StarTrackerGUI::enterEvent(QEvent*)
{
}

void StarTrackerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setColor(m_settings.m_rgbColor);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);

        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = dialog.getColor().rgb();
        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }

    resetContextMenuType();
}

void StarTrackerGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        StarTracker::MsgStartStop *message = StarTracker::MsgStartStop::create(checked);
        m_starTracker->getInputMessageQueue()->push(message);
    }
}

void StarTrackerGUI::on_latitude_valueChanged(double value)
{
    m_settings.m_latitude = value;
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_longitude_valueChanged(double value)
{
    m_settings.m_longitude = value;
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_rightAscension_editingFinished()
{
    m_settings.m_ra = ui->rightAscension->text();
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_declination_editingFinished()
{
    m_settings.m_dec = ui->declination->text();
    applySettings();
    plotChart();
}

void StarTrackerGUI::updateForTarget()
{
    if (m_settings.m_target == "Sun")
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->rightAscension->setText("");
        ui->declination->setText("");
    }
    else if (m_settings.m_target == "Moon")
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
        ui->rightAscension->setText("");
        ui->declination->setText("");
    }
    else if (m_settings.m_target == "Custom")
    {
        ui->rightAscension->setReadOnly(false);
        ui->declination->setReadOnly(false);
    }
    else
    {
        ui->rightAscension->setReadOnly(true);
        ui->declination->setReadOnly(true);
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
    // Clear as no longer valid when target has changed
    ui->azimuth->setText("");
    ui->elevation->setText("");
}

void StarTrackerGUI::on_target_currentTextChanged(const QString &text)
{
    m_settings.m_target = text;
    applySettings();
    updateForTarget();
    plotChart();
}

void StarTrackerGUI::updateLST()
{
    QDateTime dt;

    if (m_settings.m_dateTime.isEmpty())
        dt = QDateTime::currentDateTime();
    else
        dt = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs);

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
        StarTracker::MsgConfigureStarTracker* message = StarTracker::MsgConfigureStarTracker::create(m_settings, force);
        m_starTracker->getInputMessageQueue()->push(message);
    }
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
    applySettings();
    plotChart();
}

// Show settings dialog
void StarTrackerGUI::on_displaySettings_clicked()
{
    StarTrackerSettingsDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings();
        displaySolarFlux();
        if (ui->chartSelect->currentIndex() == 1)
            plotChart();
    }
}

void StarTrackerGUI::on_dateTimeSelect_currentTextChanged(const QString &text)
{
    if (text == "Now")
    {
        m_settings.m_dateTime = "";
        ui->dateTime->setVisible(false);
    }
    else
    {
        m_settings.m_dateTime = ui->dateTime->dateTime().toString(Qt::ISODateWithMs);
        ui->dateTime->setVisible(true);
    }
    applySettings();
    plotChart();
}

void StarTrackerGUI::on_dateTime_dateTimeChanged(const QDateTime &datetime)
{
    (void) datetime;
    if (ui->dateTimeSelect->currentIndex() == 1)
    {
        m_settings.m_dateTime = ui->dateTime->dateTime().toString(Qt::ISODateWithMs);
        applySettings();
        plotChart();
    }
}

void StarTrackerGUI::plotChart()
{
    if (ui->chartSelect->currentIndex() == 0)
        plotElevationChart();
    else if (ui->chartSelect->currentIndex() == 1)
        plotSolarFluxChart();
    else if (ui->chartSelect->currentIndex() == 2)
        plotSkyTemperatureChart();
}

void StarTrackerGUI::raDecChanged()
{
    if (ui->chartSelect->currentIndex() == 2)
        plotSkyTemperatureChart();
}

void StarTrackerGUI::on_frequency_valueChanged(int value)
{
    m_settings.m_frequency = value*1000000.0;
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
    applySettings();
    updateChartSubSelect();
    if (ui->chartSelect->currentIndex() == 2)
        plotChart();
}

void StarTrackerGUI::plotSolarFluxChart()
{
    m_chart.removeAllSeries();
    removeAllAxes();
    if (m_solarFluxesValid)
    {
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
        m_chart.setTitle("");
        m_chart.addAxis(&m_chartSolarFluxXAxis, Qt::AlignBottom);
        m_chart.addAxis(&m_chartYAxis, Qt::AlignLeft);
        m_chart.addSeries(series);
        series->attachAxis(&m_chartSolarFluxXAxis);
        series->attachAxis(&m_chartYAxis);
        m_chartSolarFluxXAxis.setTitleText(QString("Frequency (MHz)"));
        m_chartSolarFluxXAxis.setMinorTickCount(-1);
        if (m_settings.m_solarFluxUnits == StarTrackerSettings::SFU)
        {
            m_chartYAxis.setLabelFormat("%d");
            m_chartYAxis.setRange(0.0, ((((int)maxValue)+99)/100)*100);
        }
        else if (m_settings.m_solarFluxUnits == StarTrackerSettings::JANSKY)
        {
            m_chartYAxis.setLabelFormat("%.2g");
            m_chartYAxis.setRange(0, ((((int)maxValue)+999999)/100000)*100000);
        }
        else
        {
            m_chartYAxis.setLabelFormat("%.2g");
            m_chartYAxis.setRange(minValue, maxValue);
        }
        m_chartYAxis.setTitleText(QString("Solar flux density (%1)").arg(solarFluxUnit()));
    }
    else
        m_chart.setTitle("Press download Solar flux density data to view");
    m_chart.setPlotAreaBackgroundVisible(false);
    disconnect(&m_chart, SIGNAL(plotAreaChanged(QRectF)), this, SLOT(plotAreaChanged(QRectF)));
}

void StarTrackerGUI::plotSkyTemperatureChart()
{
    bool galactic = (ui->chartSubSelect->currentIndex() & 1) == 1;

    m_chart.removeAllSeries();
    removeAllAxes();

    QScatterSeries *series = new QScatterSeries();
    float ra = Astronomy::raToDecimal(m_settings.m_ra);
    float dec = Astronomy::decToDecimal(m_settings.m_dec);

    double beamWidth = m_settings.m_beamwidth;
    // Ellipse not supported, so draw circle on shorter axis
    double degPerPixelW = 360.0/m_chart.plotArea().width();
    double degPerPixelH = 180.0/m_chart.plotArea().height();
    double degPerPixel = std::min(degPerPixelW, degPerPixelH);
    double markerSize;

    if (galactic)
    {
        // Convert to category coordinates
        double l, b;
        Astronomy::equatorialToGalactic(ra, dec, l, b);

        // Map to linear axis
        double lAxis;
        if (l < 180.0)
            lAxis = 180.0 - l;
        else
            lAxis = 360.0 - l + 180.0;
        series->append(lAxis, b);
    }
    else
    {
        // Map to category axis
        double raAxis;
        if (ra <= 12.0)
            raAxis = 12.0 - ra;
        else
            raAxis = 24 - ra + 12;
        series->append(raAxis, dec);
    }

    // Get temperature
    int idx = ui->chartSubSelect->currentIndex();
    if ((idx == 6) || (idx == 7))
    {
        // Adjust temperature from 408MHz FITS file, taking in to account
        // observation frequency and beamwidth
        FITS *fits = &m_temps[1];
        if (fits->valid())
        {
            const double beamwidth = m_settings.m_beamwidth;
            const double halfBeamwidth = beamwidth/2.0;
            // Use cos^p(x) for approximation of radiation pattern
            // (Essentially the same as Gaussian of exp(-4*ln(theta^2/beamwidth^2))
            // (See a2 in https://arxiv.org/pdf/1812.10084.pdf for Elliptical equivalent))
            // We have gain of 0dB (1) at 0 degrees, and -3dB (~0.5) at half-beamwidth degrees
            // Find exponent that correponds to -3dB at that angle
            double minus3dBLinear = pow(10.0, -3.0/10.0);
            double p = log(minus3dBLinear)/log(cos(Units::degreesToRadians(halfBeamwidth)));
            // Create an matrix with gain as a function of angle
            double degreesPerPixelH = abs(fits->degreesPerPixelH());
            double degreesPerPixelV = abs(fits->degreesPerPixelV());
            int numberOfCoeffsH = ceil(beamwidth/degreesPerPixelH);
            int numberOfCoeffsV = ceil(beamwidth/degreesPerPixelV);
            if ((numberOfCoeffsH & 1) == 0)
                numberOfCoeffsH++;
            if ((numberOfCoeffsV & 1) == 0)
                numberOfCoeffsV++;
            double *beam = new double[numberOfCoeffsH*numberOfCoeffsV];
            double sum = 0.0;
            int y0 = numberOfCoeffsV/2;
            int x0 =  numberOfCoeffsH/2;
            int nonZeroCount = 0;
            for (int y = 0; y < numberOfCoeffsV; y++)
            {
                for (int x = 0; x < numberOfCoeffsH; x++)
                {
                    double xp = (x - x0) * degreesPerPixelH;
                    double yp = (y - y0) * degreesPerPixelV;
                    double r = sqrt(xp*xp+yp*yp);
                    if (r < halfBeamwidth)
                    {
                        beam[y*numberOfCoeffsH+x] = pow(cos(Units::degreesToRadians(r)), p);
                        sum += beam[y*numberOfCoeffsH+x];
                        nonZeroCount++;
                    }
                    else
                        beam[y*numberOfCoeffsH+x] = 0.0;
                }
            }

            // Get centre pixel coordinates
            double centreX;
            if (ra <= 12.0)
                centreX = (12.0 - ra) / 24.0;
            else
                centreX = (24 - ra + 12) / 24.0;
            double centreY = (90.0-dec) / 180.0;
            int imgX = centreX * fits->width();
            int imgY = centreY * fits->height();

            // Apply weighting to temperature data
            double weightedSum = 0.0;
            for (int y = 0; y < numberOfCoeffsV; y++)
            {
                for (int x = 0; x < numberOfCoeffsH; x++)
                {
                    weightedSum += beam[y*numberOfCoeffsH+x] * fits->scaledWrappedValue(imgX + (x-x0), imgY + (y-y0));
                }
            }
            // From: https://www.cv.nrao.edu/~sransom/web/Ch3.html
            // The antenna temperature equals the source brightness temperature multiplied by the fraction of the beam solid angle filled by the source
            // So we scale the sum by the total number of non-zero pixels (i.e. beam area)
            // If we compare to some maps with different beamwidths here: https://www.cv.nrao.edu/~demerson/radiosky/sky_jun96.pdf
            // The values we've computed are a bit higher..
            double temp408 = weightedSum/nonZeroCount;

            // Scale according to frequency - CMB contribution constant
            // Power law at low frequencies, with slight variation in spectral index
            // See:
            // Global Sky Model: https://ascl.net/1011.010
            // An improved Model of Diffuse Galactic Radio Emission: https://arxiv.org/pdf/1605.04920.pdf
            // A high-resolution self-consistent whole sky foreground model: https://arxiv.org/abs/1812.10084
            // (De-striping:) Full sky study of diffuse Galactic emission at decimeter wavelength  https://www.aanda.org/articles/aa/pdf/2003/42/aah4363.pdf
            //               Data here: http://cdsarc.u-strasbg.fr/viz-bin/cat/J/A+A/410/847
            // LFmap: https://www.faculty.ece.vt.edu/swe/lwa/memo/lwa0111.pdf
            double iso408 = 50 * pow(150e6/408e6, 2.75);                 // Extra-galactic isotropic in reference map at 408MHz
            double isoT = 50 * pow(150e6/m_settings.m_frequency, 2.75);  // Extra-galactic isotropic at target frequency
            double cmbT = 2.725; // Cosmic microwave backgroud;
            double spectralIndex;
            if (m_spectralIndex.valid())
            {
                 // See https://www.aanda.org/articles/aa/pdf/2003/42/aah4363.pdf
                 spectralIndex = m_spectralIndex.scaledValue(imgX, imgY);
            }
            else
            {
                // See https://arxiv.org/abs/1812.10084 fig 2
                if (m_settings.m_frequency < 200e6)
                    spectralIndex = 2.55;
                else if (m_settings.m_frequency < 20e9)
                     spectralIndex = 2.695;
                else
                     spectralIndex = 3.1;
            }
            double galactic480 = temp408 - cmbT - iso408;
            double galacticT = galactic480 * pow(408e6/m_settings.m_frequency, spectralIndex); // Scale galactic contribution by frequency
            double temp = galacticT + cmbT + isoT;      // Final temperature

            series->setPointLabelsVisible(true);
            series->setPointLabelsColor(Qt::red);
            series->setPointLabelsFormat(QString("%1 K").arg(std::round(temp)));

            // Scale marker size by beamwidth
            markerSize = std::max((int)round(beamWidth * degPerPixel), 5);
        }
        else
            qDebug() << "StarTrackerGUI::plotSkyTemperatureChart: FITS temperature file not valid";
    }
    else
    {
        // Read temperature from selected FITS file at target RA/Dec
        QImage *img = &m_images[idx];
        FITS *fits = &m_temps[idx/2];
        double x;
        if (ra <= 12.0)
            x = (12.0 - ra) / 24.0;
        else
            x = (24 - ra + 12) / 24.0;
        int imgX = x * img->width();
        if (imgX >= img->width())
            imgX = img->width();
        int imgY = (90.0-dec)/180.0 * img->height();
        if (imgY >= img->height())
            imgY = img->height();

        if (fits->valid())
        {
            double temp = fits->scaledValue(imgX, imgY);
            series->setPointLabelsVisible(true);
            series->setPointLabelsColor(Qt::red);
            series->setPointLabelsFormat(QString("%1 K").arg(std::round(temp)));
        }

        // Temperature from just one pixel, but need to make marker visbile
        markerSize = 5;
    }
    series->setMarkerSize(markerSize);

    m_chart.setTitle("");
    m_chart.addSeries(series);
    if (galactic)
    {
        m_chart.addAxis(&m_skyTempGalacticLXAxis, Qt::AlignBottom);
        series->attachAxis(&m_skyTempGalacticLXAxis);

        m_skyTempYAxis.setTitleText(QString("Galactic latitude (%1)").arg(QChar(0xb0)));
        m_chart.addAxis(&m_skyTempYAxis, Qt::AlignLeft);
        series->attachAxis(&m_skyTempYAxis);
    }
    else
    {
        m_chart.addAxis(&m_skyTempRAXAxis, Qt::AlignBottom);
        series->attachAxis(&m_skyTempRAXAxis);

        m_skyTempYAxis.setTitleText(QString("Declination (%1)").arg(QChar(0xb0)));
        m_chart.addAxis(&m_skyTempYAxis, Qt::AlignLeft);
        series->attachAxis(&m_skyTempYAxis);
    }

    plotAreaChanged(m_chart.plotArea());
    connect(&m_chart, SIGNAL(plotAreaChanged(QRectF)), this, SLOT(plotAreaChanged(QRectF)));
}

void StarTrackerGUI::plotAreaChanged(const QRectF &plotArea)
{
    int width = static_cast<int>(m_chart.plotArea().width());
    int height = static_cast<int>(m_chart.plotArea().height());
    int viewW = static_cast<int>(ui->chart->width());
    int viewH = static_cast<int>(ui->chart->height());

    // Scale the image to fit plot area
    int imageIdx = ui->chartSubSelect->currentIndex();
    if (imageIdx == 6)
        imageIdx = 2;
    else if (imageIdx == 7)
        imageIdx = 3;
    QImage image = m_images[imageIdx].scaled(QSize(width, height), Qt::IgnoreAspectRatio);
    QImage translated(viewW, viewH, QImage::Format_ARGB32);
    translated.fill(Qt::white);
    QPainter painter(&translated);
    QPointF topLeft = m_chart.plotArea().topLeft();
    painter.drawImage(topLeft, image);

    m_chart.setPlotAreaBackgroundBrush(translated);
    m_chart.setPlotAreaBackgroundVisible(true);
}

void StarTrackerGUI::removeAllAxes()
{
    QList<QAbstractAxis *> axes;
    axes = m_chart.axes(Qt::Horizontal);
    for (QAbstractAxis *axis : axes)
        m_chart.removeAxis(axis);
    axes = m_chart.axes(Qt::Vertical);
    for (QAbstractAxis *axis : axes)
        m_chart.removeAxis(axis);
}

// Plot target elevation angle over the day
void StarTrackerGUI::plotElevationChart()
{
    m_chart.removeAllSeries();
    removeAllAxes();

    double maxElevation = -90.0;

    QLineSeries *series = new QLineSeries();
    QDateTime dt;
    if (m_settings.m_dateTime.isEmpty())
        dt = QDateTime::currentDateTime();
    else
        dt = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs);
    dt.setTime(QTime(0,0));
    QDateTime startTime = dt;
    QDateTime endTime = dt;
    for (int hour = 0; hour <= 24; hour++)
    {
        AzAlt aa;
        RADec rd;

        // Calculate elevation of desired object
        if (m_settings.m_target == "Sun")
            Astronomy::sunPosition(aa, rd, m_settings.m_latitude, m_settings.m_longitude, dt);
        else if (m_settings.m_target == "Moon")
            Astronomy::moonPosition(aa, rd, m_settings.m_latitude, m_settings.m_longitude, dt);
        else
        {
            rd.ra = Astronomy::raToDecimal(m_settings.m_ra);
            rd.dec = Astronomy::decToDecimal(m_settings.m_dec);
            aa = Astronomy::raDecToAzAlt(rd, m_settings.m_latitude, m_settings.m_longitude, dt, !m_settings.m_jnow);
        }

        if (aa.alt > maxElevation)
            maxElevation = aa.alt;

        // Adjust for refraction
        if (m_settings.m_refraction == "Positional Astronomy Library")
        {
            aa.alt += Astronomy::refractionPAL(aa.alt, m_settings.m_pressure, m_settings.m_temperature, m_settings.m_humidity,
                                                m_settings.m_frequency, m_settings.m_latitude, m_settings.m_heightAboveSeaLevel,
                                                m_settings.m_temperatureLapseRate);
            if (aa.alt > 90.0)
                aa.alt = 90.0f;
        }
        else if (m_settings.m_refraction == "Saemundsson")
        {
            aa.alt += Astronomy::refractionSaemundsson(aa.alt, m_settings.m_pressure, m_settings.m_temperature);
            if (aa.alt > 90.0)
                aa.alt = 90.0f;
        }

        series->append(dt.toMSecsSinceEpoch(), aa.alt);

        endTime = dt;
        dt = dt.addSecs(60*60); // addSecs accounts for daylight savings jumps
    }
    if (maxElevation < 0)
        m_chart.setTitle("Not visible from this latitude");
    else
        m_chart.setTitle("");
    m_chart.addAxis(&m_chartXAxis, Qt::AlignBottom);
    m_chart.addAxis(&m_chartYAxis, Qt::AlignLeft);
    m_chart.addSeries(series);
    series->attachAxis(&m_chartXAxis);
    series->attachAxis(&m_chartYAxis);
    m_chartXAxis.setTitleText(QString("%1 %2").arg(startTime.date().toString()).arg(startTime.timeZoneAbbreviation()));
    m_chartXAxis.setFormat("hh");
    m_chartXAxis.setTickCount(7);
    m_chartXAxis.setRange(startTime, endTime);
    m_chartYAxis.setRange(0.0, 90.0);
    m_chartYAxis.setTitleText(QString("Elevation (%1)").arg(QChar(0xb0)));
    m_chart.setPlotAreaBackgroundVisible(false);
    disconnect(&m_chart, SIGNAL(plotAreaChanged(QRectF)), this, SLOT(plotAreaChanged(QRectF)));
}

// Find target on the Map
void StarTrackerGUI::on_viewOnMap_clicked()
{
    QString target = m_settings.m_target == "Sun" || m_settings.m_target == "Moon" ? m_settings.m_target : "Star";
    FeatureWebAPIUtils::mapFind(target);
}

void StarTrackerGUI::updateChartSubSelect()
{
    if (ui->chartSelect->currentIndex() == 2)
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
    if (ui->chartSelect->currentIndex() == 2)
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
    ui->chartSubSelect->blockSignals(oldState);
    plotChart();
}

void StarTrackerGUI::on_chartSubSelect_currentIndexChanged(int index)
{
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

// Linear extrapolation
static double extrapolate(double x0, double y0, double x1, double y1, double x)
{
    return y0 + ((x-x0)/(x1-x0)) * (y1-y0);
}

// Linear interpolation
static double interpolate(double x0, double y0, double x1, double y1, double x)
{
    return (y0*(x1-x) + y1*(x-x0)) / (x1-x0);
}

void StarTrackerGUI::displaySolarFlux()
{
    if (((m_settings.m_solarFluxData == StarTrackerSettings::DRAO_2800) && (m_solarFlux == 0.0))
        || ((m_settings.m_solarFluxData != StarTrackerSettings::DRAO_2800) && !m_solarFluxesValid))
        ui->solarFlux->setText("");
    else
    {
        double solarFlux;
        if (m_settings.m_solarFluxData == StarTrackerSettings::DRAO_2800)
        {
            solarFlux = m_solarFlux;
            ui->solarFlux->setToolTip(QString("Solar flux density at 2800 MHz"));
        }
        else if (m_settings.m_solarFluxData == StarTrackerSettings::TARGET_FREQ)
        {
            double freqMhz = m_settings.m_frequency/1000000.0;
            const int fluxes = sizeof(m_solarFluxFrequencies)/sizeof(*m_solarFluxFrequencies);
            int i;
            for (i = 0; i < fluxes; i++)
            {
                if (freqMhz < m_solarFluxFrequencies[i])
                    break;
            }
            if (i == 0)
            {
                solarFlux = extrapolate(m_solarFluxFrequencies[0], m_solarFluxes[0],
                                        m_solarFluxFrequencies[1], m_solarFluxes[1],
                                        freqMhz
                                        );
            }
            else if (i == fluxes)
            {
                solarFlux = extrapolate(m_solarFluxFrequencies[fluxes-2], m_solarFluxes[fluxes-2],
                                        m_solarFluxFrequencies[fluxes-1], m_solarFluxes[fluxes-1],
                                        freqMhz
                                        );
            }
            else
            {
                solarFlux = interpolate(m_solarFluxFrequencies[i-1], m_solarFluxes[i-1],
                                        m_solarFluxFrequencies[i], m_solarFluxes[i],
                                        freqMhz
                                        );
            }
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
    }
}

bool StarTrackerGUI::readSolarFlux()
{
    QDate today = QDateTime::currentDateTimeUtc().date();
    QFile file(getSolarFluxFilename(today));
    QDateTime lastModified = file.fileTime(QFileDevice::FileModificationTime);
    if (QDateTime::currentDateTime().secsTo(lastModified) >= -(60*60*24))
    {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QByteArray bytes = file.readLine();
            QString string(bytes);
            // HHMMSS 245    410     610    1415   2695   4995   8800  15400   Mhz
            // 000000 000019 000027 000037 000056 000073 000116 000202 000514  sfu
            QRegExp re("([0-9]{2})([0-9]{2})([0-9]{2}) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)");
            if (re.indexIn(string) != -1)
            {
                for (int i = 0; i < 8; i++)
                    m_solarFluxes[i] = re.capturedTexts()[i+4].toInt();
                m_solarFluxesValid = true;
                displaySolarFlux();
                plotChart();
                return true;
            }
        }
    }
    else
        qDebug() << "StarTrackerGUI::readSolarFlux: Solar flux data is more than 1 day old";
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
        QRegExp re("\\<th\\>Observed Flux Density\\<\\/th\\>\\<td\\>([0-9]+(\\.[0-9]+)?)\\<\\/td\\>");
        if (re.indexIn(answer) != -1)
        {
            m_solarFlux = re.capturedTexts()[1].toDouble();
            displaySolarFlux();
        }
        else
            qDebug() << "StarTrackerGUI::networkManagerFinished - No Solar flux found: " << answer;
    }

    reply->deleteLater();
}

QString StarTrackerGUI::getSolarFluxFilename(QDate date)
{
    return HttpDownloadManager::downloadDir() + "/solar_flux.srd";
}

void StarTrackerGUI::updateSolarFlux(bool all)
{
    qDebug() << "StarTrackerGUI: Updating Solar flux data";
    if ((m_settings.m_solarFluxData != StarTrackerSettings::DRAO_2800) || all)
    {
        QDate today = QDateTime::currentDateTimeUtc().date();
        QString solarFluxFile = getSolarFluxFilename(today);
        if (m_dlm.confirmDownload(solarFluxFile))
        {
            QString urlString = QString("http://www.sws.bom.gov.au/Category/World Data Centre/Data Display and Download/Solar Radio/station/learmonth/SRD/%1/L%2.SRD")
                                    .arg(today.year()).arg(today.toString("yyMMdd"));
            m_dlm.download(QUrl(urlString), solarFluxFile, this);
        }
    }
    if ((m_settings.m_solarFluxData == StarTrackerSettings::DRAO_2800) || all)
    {
        m_networkRequest.setUrl(QUrl("https://www.spaceweather.gc.ca/solarflux/sx-4-en.php"));
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

void StarTrackerGUI::downloadFinished(const QString& filename, bool success)
{
    if (success)
        readSolarFlux();
}
