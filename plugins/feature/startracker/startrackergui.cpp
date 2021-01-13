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
#include <QMessageBox>
#include <QLineEdit>
#include <QRegExp>

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
    m_lastFeatureState(0)
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

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(1000);

    // Intialise chart
    m_chart.legend()->hide();
    ui->elevationChart->setChart(&m_chart);
    ui->elevationChart->setRenderHint(QPainter::Antialiasing);
    m_chart.addAxis(&m_chartXAxis, Qt::AlignBottom);
    m_chart.addAxis(&m_chartYAxis, Qt::AlignLeft);

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
}

StarTrackerGUI::~StarTrackerGUI()
{
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

// Plot target elevation angle over the day
void StarTrackerGUI::plotChart()
{
    m_chart.removeAllSeries();
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
    m_chart.addSeries(series);
    series->attachAxis(&m_chartXAxis);
    series->attachAxis(&m_chartYAxis);
    m_chartXAxis.setTitleText(QString("%1 %2").arg(startTime.date().toString()).arg(startTime.timeZoneAbbreviation()));
    m_chartXAxis.setFormat("hh");
    m_chartXAxis.setTickCount(7);
    m_chartXAxis.setRange(startTime, endTime);
    m_chartYAxis.setRange(0.0, 90.0);
    m_chartYAxis.setTitleText(QString("Elevation (%1)").arg(QChar(0xb0)));
}

// Find target on the Map
void StarTrackerGUI::on_viewOnMap_clicked()
{
    QString target = m_settings.m_target == "Sun" || m_settings.m_target == "Moon" ? m_settings.m_target : "Star";
    FeatureWebAPIUtils::mapFind(target);
}
