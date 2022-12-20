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
#include <limits>
#include <QMessageBox>
#include <QLineEdit>
#include <QPixmap>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#include "SWGMapItem.h"

#include "feature/featureuiset.h"
#include "feature/featurewebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "mainwindow.h"
#include "maincore.h"
#include "device/deviceuiset.h"

#include "ui_aprsgui.h"
#include "aprs.h"
#include "aprsgui.h"
#include "aprssettingsdialog.h"

#define PACKETS_COL_DATE                0
#define PACKETS_COL_TIME                1
#define PACKETS_COL_FROM                2
#define PACKETS_COL_TO                  3
#define PACKETS_COL_VIA                 4
#define PACKETS_COL_DATA                5

#define WEATHER_COL_DATE                0
#define WEATHER_COL_TIME                1
#define WEATHER_COL_WIND_DIRECTION      2
#define WEATHER_COL_WIND_SPEED          3
#define WEATHER_COL_GUSTS               4
#define WEATHER_COL_TEMPERATURE         5
#define WEATHER_COL_HUMIDITY            6
#define WEATHER_COL_PRESSURE            7
#define WEATHER_COL_RAIN_LAST_HOUR      8
#define WEATHER_COL_RAIN_LAST_24_HOURS  9
#define WEATHER_COL_RAIN_SINCE_MIDNIGHT 10
#define WEATHER_COL_LUMINOSITY          11
#define WEATHER_COL_SNOWFALL            12
#define WEATHER_COL_RADIATION_LEVEL     13
#define WEATHER_COL_FLOOD_LEVEL         14

#define STATUS_COL_DATE                 0
#define STATUS_COL_TIME                 1
#define STATUS_COL_STATUS               2
#define STATUS_COL_SYMBOL               3
#define STATUS_COL_MAIDENHEAD           4
#define STATUS_COL_BEAM_HEADING         5
#define STATUS_COL_BEAM_POWER           6

#define MESSAGE_COL_DATE                0
#define MESSAGE_COL_TIME                1
#define MESSAGE_COL_ADDRESSEE           2
#define MESSAGE_COL_MESSAGE             3
#define MESSAGE_COL_NO                  4

#define TELEMETRY_COL_DATE              0
#define TELEMETRY_COL_TIME              1
#define TELEMETRY_COL_SEQ_NO            2
#define TELEMETRY_COL_A1                3
#define TELEMETRY_COL_A2                4
#define TELEMETRY_COL_A3                5
#define TELEMETRY_COL_A4                6
#define TELEMETRY_COL_A5                7
#define TELEMETRY_COL_B1                8
#define TELEMETRY_COL_B2                9
#define TELEMETRY_COL_B3                10
#define TELEMETRY_COL_B4                11
#define TELEMETRY_COL_B5                12
#define TELEMETRY_COL_B6                13
#define TELEMETRY_COL_B7                14
#define TELEMETRY_COL_B8                15
#define TELEMETRY_COL_COMMENT           16

#define MOTION_COL_DATE                 0
#define MOTION_COL_TIME                 1
#define MOTION_COL_LATITUDE             2
#define MOTION_COL_LONGITUDE            3
#define MOTION_COL_ALTITUDE             4
#define MOTION_COL_COURSE               5
#define MOTION_COL_SPEED                6

APRSGUI* APRSGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    APRSGUI* gui = new APRSGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void APRSGUI::destroy()
{
    delete this;
}

void APRSGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray APRSGUI::serialize() const
{
    return m_settings.serialize();
}

bool APRSGUI::deserialize(const QByteArray& data)
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

bool APRSGUI::handleMessage(const Message& message)
{
    if (APRS::MsgConfigureAPRS::match(message))
    {
        qDebug("APRSGUI::handleMessage: APRS::MsgConfigureAPRS");
        const APRS::MsgConfigureAPRS& cfg = (APRS::MsgConfigureAPRS&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        qDebug() << m_settings.m_igateCallsign;
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (APRS::MsgReportAvailableChannels::match(message))
    {
        APRS::MsgReportAvailableChannels& report = (APRS::MsgReportAvailableChannels&) message;
        m_availableChannels = report.getChannels();
        updateChannelList();

        return true;
    }
    else if (MainCore::MsgPacket::match(message))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) message;
        AX25Packet ax25;
        APRSPacket *aprs = new APRSPacket();
        if (ax25.decode(report.getPacket()))
        {
            if (aprs->decode(ax25))
            {
                aprs->m_dateTime = report.getDateTime();

                APRSStation *station;
                bool addToStationSel = false;

                // Is packet for an existing object or station?
                if (!aprs->m_objectName.isEmpty() && m_stations.contains(aprs->m_objectName))
                {
                    // Add packet to existing object
                    station = m_stations.value(aprs->m_objectName);
                    station->addPacket(aprs);
                }
                else if (!aprs->m_objectName.isEmpty())
                {
                    // Add new object
                    station = new APRSStation(aprs->m_objectName);
                    station->m_isObject = true;
                    station->m_reportingStation = aprs->m_from;
                    station->addPacket(aprs);
                    m_stations.insert(aprs->m_objectName, station);
                    addToStationSel = true;
                }
                else if (m_stations.contains(aprs->m_from))
                {
                    // Add packet to existing station
                    station = m_stations.value(aprs->m_from);
                    station->addPacket(aprs);
                }
                else
                {
                    // Add new station
                    station = new APRSStation(aprs->m_from);
                    station->addPacket(aprs);
                    m_stations.insert(aprs->m_from, station);
                    addToStationSel = true;
                }

                // Update station
                if (aprs->m_hasSymbol)
                    station->m_symbolImage = aprs->m_symbolImage;
                if (aprs->m_hasTimestamp)
                    station->m_latestPacket = aprs->dateTime();
                if (aprs->m_hasStatus)
                    station->m_latestStatus = aprs->m_status;
                if (!aprs->m_comment.isEmpty())
                    station->m_latestComment = aprs->m_comment;
                if (aprs->m_hasPosition)
                    station->m_latestPosition = aprs->position();
                if (aprs->m_hasAltitude)
                    station->m_latestAltitude = QString("%1").arg(aprs->m_altitudeFt);
                if (aprs->m_hasCourseAndSpeed)
                {
                    station->m_latestCourse = QString("%1").arg(aprs->m_course);
                    station->m_latestSpeed = QString("%1").arg(aprs->m_speed);
                }
                if (aprs->m_hasStationDetails)
                {
                    station->m_powerWatts = QString("%1").arg(aprs->m_powerWatts);
                    station->m_antennaHeightFt = QString("%1").arg(aprs->m_antennaHeightFt);
                    station->m_antennaGainDB = QString("%1").arg(aprs->m_antennaGainDB);
                    station->m_antennaDirectivity = aprs->m_antennaDirectivity;
                }
                if (aprs->m_hasRadioRange)
                    station->m_radioRange = QString("%1").arg(aprs->m_radioRangeMiles);
                if (aprs->m_hasWeather)
                    station->m_hasWeather = true;
                if (aprs->m_hasTelemetry)
                    station->m_hasTelemetry = true;
                if (aprs->m_hasCourseAndSpeed)
                    station->m_hasCourseAndSpeed = true;

                // Update messages, which aren't station specific
                if (aprs->m_hasMessage)
                {
                    int row = ui->messagesTable->rowCount();
                    ui->messagesTable->setRowCount(row + 1);
                    QTableWidgetItem *messageDateItem = new QTableWidgetItem();
                    QTableWidgetItem *messageTimeItem = new QTableWidgetItem();
                    QTableWidgetItem *messageAddresseeItem = new QTableWidgetItem();
                    QTableWidgetItem *messageMessageItem = new QTableWidgetItem();
                    QTableWidgetItem *messageNoItem = new QTableWidgetItem();
                    ui->messagesTable->setItem(row, MESSAGE_COL_DATE, messageDateItem);
                    ui->messagesTable->setItem(row, MESSAGE_COL_TIME, messageTimeItem);
                    ui->messagesTable->setItem(row, MESSAGE_COL_ADDRESSEE, messageAddresseeItem);
                    ui->messagesTable->setItem(row, MESSAGE_COL_MESSAGE, messageMessageItem);
                    ui->messagesTable->setItem(row, MESSAGE_COL_NO, messageNoItem);
                    messageDateItem->setData(Qt::DisplayRole, formatDate(aprs));
                    messageTimeItem->setData(Qt::DisplayRole, formatTime(aprs));
                    messageAddresseeItem->setText(aprs->m_addressee);
                    messageMessageItem->setText(aprs->m_message);
                    messageNoItem->setText(aprs->m_messageNo);

                    // Process telemetry messages
                    if ((aprs->m_telemetryNames.size() > 0) || (aprs->m_telemetryLabels.size() > 0) || aprs->m_hasTelemetryCoefficients || aprs->m_hasTelemetryBitSense)
                    {
                        APRSStation *telemetryStation;
                        if (m_stations.contains(aprs->m_addressee))
                            telemetryStation = m_stations.value(aprs->m_addressee);
                        else
                        {
                            telemetryStation = new APRSStation(aprs->m_addressee);
                            m_stations.insert(aprs->m_addressee, telemetryStation);
                        }
                        if (aprs->m_telemetryNames.size() > 0)
                        {
                            telemetryStation->m_telemetryNames = aprs->m_telemetryNames;
                            for (int i = 0; i < aprs->m_telemetryNames.size(); i++)
                                ui->telemetryPlotSelect->setItemText(i, aprs->m_telemetryNames[i]);
                        }
                        else
                            telemetryStation->m_telemetryLabels = aprs->m_telemetryLabels;
                        if (aprs->m_hasTelemetryCoefficients > 0)
                        {
                            for (int j = 0; j < 5; j++)
                            {
                                telemetryStation->m_telemetryCoefficientsA[j] = aprs->m_telemetryCoefficientsA[j];
                                telemetryStation->m_telemetryCoefficientsB[j] = aprs->m_telemetryCoefficientsB[j];
                                telemetryStation->m_telemetryCoefficientsC[j] = aprs->m_telemetryCoefficientsC[j];
                            }
                            telemetryStation->m_hasTelemetryCoefficients = aprs->m_hasTelemetryCoefficients;
                        }
                        if (aprs->m_hasTelemetryBitSense)
                        {
                            for (int j = 0; j < 8; j++)
                                telemetryStation->m_telemetryBitSense[j] = aprs->m_telemetryBitSense[j];
                            telemetryStation->m_hasTelemetryBitSense = true;
                            telemetryStation->m_telemetryProjectName = aprs->m_telemetryProjectName;
                        }
                        if (ui->stationSelect->currentText() == aprs->m_addressee)
                        {
                            for (int i = 0; i < station->m_telemetryNames.size(); i++)
                            {
                                QString header;
                                if (station->m_telemetryLabels.size() <= i)
                                    header = station->m_telemetryNames[i];
                                else
                                    header = QString("%1 (%2)").arg(station->m_telemetryNames[i]).arg(station->m_telemetryLabels[i]);
                                ui->telemetryTable->horizontalHeaderItem(TELEMETRY_COL_A1+i)->setText(header);
                            }
                        }
                    }
                }

                if (addToStationSel)
                {
                    if (!filterStation(station))
                        ui->stationSelect->addItem(station->m_station);
                }

                // Refresh GUI if currently selected station
                if (ui->stationSelect->currentText() == aprs->m_from)
                {
                    updateSummary(station);
                    addPacketToGUI(station, aprs);
                    if (aprs->m_hasWeather)
                        plotWeather();
                    if (aprs->m_hasTelemetry)
                        plotTelemetry();
                    if (aprs->m_hasPosition || aprs->m_hasAltitude || aprs->m_hasCourseAndSpeed)
                        plotMotion();
                }

                // Forward to map
                if (aprs->m_hasPosition && (aprs->m_from != ""))
                {
                    QList<ObjectPipe*> mapPipes;
                    MainCore::instance()->getMessagePipes().getMessagePipes(m_aprs, "mapitems", mapPipes);

                    for (const auto& pipe : mapPipes)
                    {
                        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

                        if (!aprs->m_objectName.isEmpty()) {
                            swgMapItem->setName(new QString(aprs->m_objectName));
                        } else {
                            swgMapItem->setName(new QString(aprs->m_from));
                        }

                        swgMapItem->setLatitude(aprs->m_latitude);
                        swgMapItem->setLongitude(aprs->m_longitude);
                        swgMapItem->setAltitude(aprs->m_hasAltitude ? Units::feetToMetres(aprs->m_altitudeFt) : 0);
                        swgMapItem->setAltitudeReference(1); // CLAMP_TO_GROUND

                        if (aprs->m_objectKilled)
                        {
                            swgMapItem->setImage(new QString(""));
                            swgMapItem->setText(new QString(""));
                        }
                        else
                        {
                            swgMapItem->setImage(new QString(QString("qrc:///%1").arg(aprs->m_symbolImage)));
                            swgMapItem->setText(new QString(
                                aprs->toText(
                                    true,
                                    false,
                                    '\n',
                                    m_settings.m_altitudeUnits == APRSSettings::METRES,
                                    (int)m_settings.m_speedUnits,
                                    m_settings.m_temperatureUnits == APRSSettings::CELSIUS,
                                    m_settings.m_rainfallUnits == APRSSettings::MILLIMETRE
                                )
                            ));
                        }

                        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_aprs, swgMapItem);
                        messageQueue->push(msg);
                    }
                }
            }
            else
            {
                qDebug() << "APRSGUI::handleMessage: Failed to decode as APRS";
                qDebug() << ax25.m_from << " " << ax25.m_to << " " << ax25.m_via << " " << ax25.m_type << " " << ax25.m_pid << " "<< ax25.m_dataASCII;
            }
        }
        else
        {
            qDebug() << "APRSGUI::handleMessage: Failed to decode as AX.25";
        }

        return true;
    }

    return false;
}

void APRSGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void APRSGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

APRSGUI::APRSGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::APRSGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/aprs/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_aprs = reinterpret_cast<APRS*>(feature);
    m_aprs->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(1000);

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->weatherTable->horizontalHeader()->setSectionsMovable(true);
    ui->packetsTable->horizontalHeader()->setSectionsMovable(true);
    ui->statusTable->horizontalHeader()->setSectionsMovable(true);
    ui->messagesTable->horizontalHeader()->setSectionsMovable(true);
    ui->telemetryTable->horizontalHeader()->setSectionsMovable(true);
    ui->motionTable->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->weatherTable->setSortingEnabled(true);
    ui->packetsTable->setSortingEnabled(true);
    ui->statusTable->setSortingEnabled(true);
    ui->messagesTable->setSortingEnabled(true);
    ui->telemetryTable->setSortingEnabled(true);
    ui->motionTable->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    packetsTableMenu = new QMenu(ui->packetsTable);
    for (int i = 0; i < ui->packetsTable->horizontalHeader()->count(); i++)
    {
        QString text = ui->packetsTable->horizontalHeaderItem(i)->text();
        packetsTableMenu->addAction(packetsTable_createCheckableItem(text, i, true));
    }
    ui->packetsTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->packetsTable->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(packetsTable_columnSelectMenu(QPoint)));

    weatherTableMenu = new QMenu(ui->weatherTable);
    for (int i = 0; i < ui->weatherTable->horizontalHeader()->count(); i++)
    {
        QString text = ui->weatherTable->horizontalHeaderItem(i)->text();
        weatherTableMenu->addAction(weatherTable_createCheckableItem(text, i, true));
    }
    ui->weatherTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->weatherTable->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(weatherTable_columnSelectMenu(QPoint)));

    statusTableMenu = new QMenu(ui->statusTable);
    for (int i = 0; i < ui->statusTable->horizontalHeader()->count(); i++)
    {
        QString text = ui->statusTable->horizontalHeaderItem(i)->text();
        statusTableMenu->addAction(statusTable_createCheckableItem(text, i, true));
    }
    ui->statusTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->statusTable->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(statusTable_columnSelectMenu(QPoint)));

    messagesTableMenu = new QMenu(ui->messagesTable);
    for (int i = 0; i < ui->messagesTable->horizontalHeader()->count(); i++)
    {
        QString text = ui->messagesTable->horizontalHeaderItem(i)->text();
        messagesTableMenu->addAction(messagesTable_createCheckableItem(text, i, true));
    }
    ui->messagesTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messagesTable->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(messagesTable_columnSelectMenu(QPoint)));

    telemetryTableMenu = new QMenu(ui->telemetryTable);
    for (int i = 0; i < ui->telemetryTable->horizontalHeader()->count(); i++)
    {
        QString text = ui->telemetryTable->horizontalHeaderItem(i)->text();
        telemetryTableMenu->addAction(telemetryTable_createCheckableItem(text, i, true));
    }
    ui->telemetryTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->telemetryTable->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(telemetryTable_columnSelectMenu(QPoint)));

    motionTableMenu = new QMenu(ui->motionTable);
    for (int i = 0; i < ui->motionTable->horizontalHeader()->count(); i++)
    {
        QString text = ui->motionTable->horizontalHeaderItem(i)->text();
        motionTableMenu->addAction(motionTable_createCheckableItem(text, i, true));
    }
    ui->motionTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->motionTable->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(motionTable_columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->packetsTable->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(packetsTable_sectionMoved(int, int, int)));
    connect(ui->packetsTable->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(packetsTable_sectionResized(int, int, int)));
    connect(ui->weatherTable->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(weatherTable_sectionMoved(int, int, int)));
    connect(ui->weatherTable->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(weatherTable_sectionResized(int, int, int)));
    connect(ui->statusTable->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(statusTable_sectionMoved(int, int, int)));
    connect(ui->statusTable->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(statusTable_sectionResized(int, int, int)));
    connect(ui->messagesTable->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(messagesTable_sectionMoved(int, int, int)));
    connect(ui->messagesTable->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(messagesTable_sectionResized(int, int, int)));
    connect(ui->telemetryTable->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(telemetryTable_sectionMoved(int, int, int)));
    connect(ui->telemetryTable->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(telemetryTable_sectionResized(int, int, int)));
    connect(ui->motionTable->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(motionTable_sectionMoved(int, int, int)));
    connect(ui->motionTable->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(motionTable_sectionResized(int, int, int)));

    m_weatherChart.setTheme(QChart::ChartThemeDark);
    m_weatherChart.legend()->hide();
    ui->weatherChart->setChart(&m_weatherChart);
    ui->weatherChart->setRenderHint(QPainter::Antialiasing);
    m_weatherChart.addAxis(&m_weatherChartXAxis, Qt::AlignBottom);
    m_weatherChart.addAxis(&m_weatherChartYAxis, Qt::AlignLeft);
    m_weatherChart.layout()->setContentsMargins(0, 0, 0, 0);
    m_weatherChart.setMargins(QMargins(1, 1, 1, 1));

    m_telemetryChart.setTheme(QChart::ChartThemeDark);
    m_telemetryChart.legend()->hide();
    ui->telemetryChart->setChart(&m_telemetryChart);
    ui->telemetryChart->setRenderHint(QPainter::Antialiasing);
    m_telemetryChart.addAxis(&m_telemetryChartXAxis, Qt::AlignBottom);
    m_telemetryChart.addAxis(&m_telemetryChartYAxis, Qt::AlignLeft);
    m_telemetryChart.layout()->setContentsMargins(0, 0, 0, 0);
    m_telemetryChart.setMargins(QMargins(1, 1, 1, 1));

    m_motionChart.setTheme(QChart::ChartThemeDark);
    m_motionChart.legend()->hide();
    ui->motionChart->setChart(&m_motionChart);
    ui->motionChart->setRenderHint(QPainter::Antialiasing);
    m_motionChart.addAxis(&m_motionChartXAxis, Qt::AlignBottom);
    m_motionChart.addAxis(&m_motionChartYAxis, Qt::AlignLeft);
    m_motionChart.layout()->setContentsMargins(0, 0, 0, 0);
    m_motionChart.setMargins(QMargins(1, 1, 1, 1));

    m_settings.setRollupState(&m_rollupState);

    m_aprs->getInputMessageQueue()->push(APRS::MsgQueryAvailableChannels::create());

    displaySettings();
    applySettings(true);
    makeUIConnections();
}

APRSGUI::~APRSGUI()
{
    delete ui;
}

void APRSGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void APRSGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

bool APRSGUI::filterStation(APRSStation *station)
{
    switch (m_settings.m_stationFilter)
    {
    case APRSSettings::ALL:
        return false;
    case APRSSettings::STATIONS:
        return station->m_isObject;
    case APRSSettings::OBJECTS:
        return !station->m_isObject;
    case APRSSettings::WEATHER:
        return !station->m_hasWeather;
    case APRSSettings::TELEMETRY:
        return !station->m_hasTelemetry;
    case APRSSettings::COURSE_AND_SPEED:
        return !station->m_hasCourseAndSpeed;
    }
    return false;
}

void APRSGUI::filterStations()
{
    ui->stationSelect->clear();
    QHashIterator<QString,APRSStation *> i(m_stations);
    while (i.hasNext())
    {
        i.next();
        APRSStation *station = i.value();
        if (!filterStation(station))
        {
            ui->stationSelect->addItem(station->m_station);
        }
    }
}

void APRSGUI::displayTableSettings(QTableWidget *table, QMenu *menu, int *columnSizes, int *columnIndexes, int columns)
{
    QHeaderView *header = table->horizontalHeader();
    for (int i = 0; i < columns; i++)
    {
        bool hidden = columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (columnSizes[i] > 0)
            table->setColumnWidth(i, columnSizes[i]);
        header->moveSection(header->visualIndex(i), columnIndexes[i]);
    }
}

void APRSGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->igate->setChecked(m_settings.m_igateEnabled);
    ui->stationFilter->setCurrentIndex((int)m_settings.m_stationFilter);
    ui->filterAddressee->setText(m_settings.m_filterAddressee);
    setUnits();

    // Order and size columns
    displayTableSettings(ui->packetsTable, packetsTableMenu, m_settings.m_packetsTableColumnSizes, m_settings.m_packetsTableColumnIndexes, APRS_PACKETS_TABLE_COLUMNS);
    displayTableSettings(ui->weatherTable, weatherTableMenu, m_settings.m_weatherTableColumnSizes, m_settings.m_weatherTableColumnIndexes, APRS_WEATHER_TABLE_COLUMNS);
    displayTableSettings(ui->statusTable, statusTableMenu, m_settings.m_statusTableColumnSizes, m_settings.m_statusTableColumnIndexes, APRS_STATUS_TABLE_COLUMNS);
    displayTableSettings(ui->messagesTable, messagesTableMenu, m_settings.m_messagesTableColumnSizes, m_settings.m_messagesTableColumnIndexes, APRS_MESSAGES_TABLE_COLUMNS);
    displayTableSettings(ui->telemetryTable, telemetryTableMenu, m_settings.m_telemetryTableColumnSizes, m_settings.m_telemetryTableColumnIndexes, APRS_TELEMETRY_TABLE_COLUMNS);
    displayTableSettings(ui->motionTable, motionTableMenu, m_settings.m_motionTableColumnSizes, m_settings.m_motionTableColumnIndexes, APRS_MOTION_TABLE_COLUMNS);

    getRollupContents()->restoreState(m_rollupState);

    blockApplySettings(false);
}

void APRSGUI::updateChannelList()
{
    ui->sourcePipes->blockSignals(true);
    ui->sourcePipes->clear();

    for (const auto& channel : m_availableChannels) {
        ui->sourcePipes->addItem(tr("R%1:%2 %3").arg(channel.m_deviceSetIndex).arg(channel.m_channelIndex).arg(channel.m_type));
    }

    ui->sourcePipes->blockSignals(false);
}

void APRSGUI::resizeEvent(QResizeEvent* event)
{
    // Replot graphs to ensure Axis are visible
    plotWeather();
    plotTelemetry();
    plotMotion();
    FeatureGUI::resizeEvent(event);
}

void APRSGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
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

void APRSGUI::updateSummary(APRSStation *station)
{
    ui->station->setText(station->m_station);
    ui->reportingStation->setText(station->m_reportingStation);
    ui->symbol->setPixmap(QPixmap(QString(":%1").arg(station->m_symbolImage)));
    ui->status->setText(station->m_latestStatus);
    ui->comment->setText(station->m_latestComment);
    ui->position->setText(station->m_latestPosition);
    ui->altitude->setText(convertAltitude(station->m_latestAltitude));
    ui->course->setText(station->m_latestCourse);
    ui->speed->setText(convertSpeed(station->m_latestSpeed));
    ui->txPower->setText(station->m_powerWatts);
    ui->antennaHeight->setText(convertAltitude(station->m_antennaHeightFt));
    ui->antennaGain->setText(station->m_antennaGainDB);
    ui->antennaDirectivity->setText(station->m_antennaDirectivity);
    ui->radioRange->setText(station->m_radioRange);
    if (!station->m_packets.isEmpty())
        ui->lastPacket->setText(station->m_packets.last()->m_dateTime.toString());
    else
        ui->lastPacket->setText("");
}

QString APRSGUI::formatDate(APRSPacket *aprs)
{
    if (aprs->m_hasTimestamp)
        return aprs->date();
    else
        return aprs->m_dateTime.date().toString("yyyy/MM/dd");
}

QString APRSGUI::formatTime(APRSPacket *aprs)
{
    // Add suffix T to indicate timestamp used
    if (aprs->m_hasTimestamp)
        return QString("%1 T").arg(aprs->time());
    else
        return aprs->m_dateTime.time().toString("hh:mm:ss");
}

double APRSGUI::applyCoefficients(int idx, int value, APRSStation *station)
{
    if (station->m_hasTelemetryCoefficients > idx)
        return station->m_telemetryCoefficientsA[idx] * value * value + station->m_telemetryCoefficientsB[idx] * value + station->m_telemetryCoefficientsC[idx];
    else
        return (double)idx;
}

void APRSGUI::addPacketToGUI(APRSStation *station, APRSPacket *aprs)
{
    int row;
    // Weather table
    if (aprs->m_hasWeather)
    {
        row = ui->weatherTable->rowCount();
        ui->weatherTable->setRowCount(row + 1);
        QTableWidgetItem *weatherDateItem = new QTableWidgetItem();
        QTableWidgetItem *weatherTimeItem = new QTableWidgetItem();
        QTableWidgetItem *windDirectionItem = new QTableWidgetItem();
        QTableWidgetItem *windSpeedItem = new QTableWidgetItem();
        QTableWidgetItem *gustsItem = new QTableWidgetItem();
        QTableWidgetItem *temperatureItem = new QTableWidgetItem();
        QTableWidgetItem *humidityItem = new QTableWidgetItem();
        QTableWidgetItem *pressureItem = new QTableWidgetItem();
        QTableWidgetItem *rainLastHourItem = new QTableWidgetItem();
        QTableWidgetItem *rainLast24HoursItem = new QTableWidgetItem();
        QTableWidgetItem *rainSinceMidnightItem = new QTableWidgetItem();
        QTableWidgetItem *luminosityItem = new QTableWidgetItem();
        QTableWidgetItem *snowfallItem = new QTableWidgetItem();
        QTableWidgetItem *radiationLevelItem = new QTableWidgetItem();
        QTableWidgetItem *floodLevelItem = new QTableWidgetItem();
        ui->weatherTable->setItem(row, WEATHER_COL_DATE, weatherDateItem);
        ui->weatherTable->setItem(row, WEATHER_COL_TIME, weatherTimeItem);
        ui->weatherTable->setItem(row, WEATHER_COL_WIND_DIRECTION, windDirectionItem);
        ui->weatherTable->setItem(row, WEATHER_COL_WIND_SPEED, windSpeedItem);
        ui->weatherTable->setItem(row, WEATHER_COL_GUSTS, gustsItem);
        ui->weatherTable->setItem(row, WEATHER_COL_TEMPERATURE, temperatureItem);
        ui->weatherTable->setItem(row, WEATHER_COL_HUMIDITY, humidityItem);
        ui->weatherTable->setItem(row, WEATHER_COL_PRESSURE, pressureItem);
        ui->weatherTable->setItem(row, WEATHER_COL_RAIN_LAST_HOUR, rainLastHourItem);
        ui->weatherTable->setItem(row, WEATHER_COL_RAIN_LAST_24_HOURS, rainLast24HoursItem);
        ui->weatherTable->setItem(row, WEATHER_COL_RAIN_SINCE_MIDNIGHT, rainSinceMidnightItem);
        ui->weatherTable->setItem(row, WEATHER_COL_LUMINOSITY, luminosityItem);
        ui->weatherTable->setItem(row, WEATHER_COL_SNOWFALL, snowfallItem);
        ui->weatherTable->setItem(row, WEATHER_COL_RADIATION_LEVEL, radiationLevelItem);
        ui->weatherTable->setItem(row, WEATHER_COL_FLOOD_LEVEL, floodLevelItem);
        weatherDateItem->setData(Qt::DisplayRole, formatDate(aprs));
        weatherTimeItem->setData(Qt::DisplayRole, formatTime(aprs));
        if (aprs->m_hasWindDirection)
            windDirectionItem->setData(Qt::DisplayRole, aprs->m_windDirection);
        if (aprs->m_hasWindSpeed)
            windSpeedItem->setData(Qt::DisplayRole, aprs->m_windSpeed);
        if (aprs->m_hasGust)
            gustsItem->setData(Qt::DisplayRole, aprs->m_gust);
        if (aprs->m_hasTemp)
            temperatureItem->setData(Qt::DisplayRole, convertTemperature(aprs->m_temp));
        if (aprs->m_hasHumidity)
            humidityItem->setData(Qt::DisplayRole, aprs->m_humidity);
        if (aprs->m_hasBarometricPressure)
            pressureItem->setData(Qt::DisplayRole, aprs->m_barometricPressure/10.0f);
        if (aprs->m_hasRainLastHr)
            rainLastHourItem->setData(Qt::DisplayRole, convertRainfall(aprs->m_rainLastHr));
        if (aprs->m_hasRainLast24Hrs)
            rainLast24HoursItem->setData(Qt::DisplayRole, convertRainfall(aprs->m_rainLast24Hrs));
        if (aprs->m_hasRainSinceMidnight)
            rainSinceMidnightItem->setData(Qt::DisplayRole, convertRainfall(aprs->m_rainSinceMidnight));
        if (aprs->m_hasLuminsoity)
            luminosityItem->setData(Qt::DisplayRole, aprs->m_luminosity);
        if (aprs->m_hasSnowfallLast24Hrs)
            snowfallItem->setData(Qt::DisplayRole, aprs->m_snowfallLast24Hrs);
        if (aprs->m_hasRadiationLevel)
            radiationLevelItem->setData(Qt::DisplayRole, aprs->m_hasRadiationLevel);
        if (aprs->m_hasFloodLevel)
            floodLevelItem->setData(Qt::DisplayRole, aprs->m_floodLevel);
    }

    // Status table
    if (aprs->m_hasStatus)
    {
        row = ui->statusTable->rowCount();
        ui->statusTable->setRowCount(row + 1);
        QTableWidgetItem *statusDateItem = new QTableWidgetItem();
        QTableWidgetItem *statusTimeItem = new QTableWidgetItem();
        QTableWidgetItem *statusItem = new QTableWidgetItem();
        QTableWidgetItem *statusSymbolItem = new QTableWidgetItem();
        QTableWidgetItem *statusMaidenheadItem = new QTableWidgetItem();
        QTableWidgetItem *statusBeamHeadingItem = new QTableWidgetItem();
        QTableWidgetItem *statusBeamPowerItem = new QTableWidgetItem();
        ui->statusTable->setItem(row, STATUS_COL_DATE, statusDateItem);
        ui->statusTable->setItem(row, STATUS_COL_TIME, statusTimeItem);
        ui->statusTable->setItem(row, STATUS_COL_STATUS, statusItem);
        ui->statusTable->setItem(row, STATUS_COL_SYMBOL, statusSymbolItem);
        ui->statusTable->setItem(row, STATUS_COL_MAIDENHEAD, statusMaidenheadItem);
        ui->statusTable->setItem(row, STATUS_COL_BEAM_HEADING, statusBeamHeadingItem);
        ui->statusTable->setItem(row, STATUS_COL_BEAM_POWER, statusBeamPowerItem);
        statusDateItem->setData(Qt::DisplayRole, formatDate(aprs));
        statusTimeItem->setData(Qt::DisplayRole, formatTime(aprs));
        statusItem->setText(aprs->m_status);
        if (aprs->m_hasSymbol)
        {
            statusSymbolItem->setSizeHint(QSize(24, 24));
            statusSymbolItem->setIcon(QIcon(QString(":%1").arg(station->m_symbolImage)));
        }
        statusMaidenheadItem->setText(aprs->m_maidenhead);
        if (aprs->m_hasBeam)
        {
            statusBeamHeadingItem->setData(Qt::DisplayRole, aprs->m_beamHeading);
            statusBeamPowerItem->setData(Qt::DisplayRole, aprs->m_beamPower);
        }
    }

    // Telemetry table
    if (aprs->m_hasTelemetry)
    {
        row = ui->telemetryTable->rowCount();
        ui->telemetryTable->setRowCount(row + 1);
        QTableWidgetItem *telemetryDateItem = new QTableWidgetItem();
        QTableWidgetItem *telemetryTimeItem = new QTableWidgetItem();
        QTableWidgetItem *telemetrySeqNoItem = new QTableWidgetItem();
        QTableWidgetItem *telemetryA1Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryA2Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryA3Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryA4Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryA5Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB1Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB2Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB3Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB4Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB5Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB6Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB7Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryB8Item = new QTableWidgetItem();
        QTableWidgetItem *telemetryCommentItem = new QTableWidgetItem();
        ui->telemetryTable->setItem(row, TELEMETRY_COL_DATE, telemetryDateItem);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_TIME, telemetryTimeItem);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_SEQ_NO, telemetrySeqNoItem);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_A1, telemetryA1Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_A2, telemetryA2Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_A3, telemetryA3Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_A4, telemetryA4Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_A5, telemetryA5Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B1, telemetryB1Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B2, telemetryB2Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B3, telemetryB3Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B4, telemetryB4Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B5, telemetryB5Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B6, telemetryB6Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B7, telemetryB7Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_B8, telemetryB8Item);
        ui->telemetryTable->setItem(row, TELEMETRY_COL_COMMENT, telemetryCommentItem);
        telemetryDateItem->setData(Qt::DisplayRole, formatDate(aprs));
        telemetryTimeItem->setData(Qt::DisplayRole, formatTime(aprs));
        if (aprs->m_hasSeqNo)
            telemetrySeqNoItem->setData(Qt::DisplayRole, aprs->m_seqNo);
        if (aprs->m_a1HasValue)
            telemetryA1Item->setData(Qt::DisplayRole, applyCoefficients(0, aprs->m_a1, station));
        if (aprs->m_a2HasValue)
            telemetryA2Item->setData(Qt::DisplayRole, applyCoefficients(1, aprs->m_a2, station));
        if (aprs->m_a3HasValue)
            telemetryA3Item->setData(Qt::DisplayRole, applyCoefficients(2, aprs->m_a3, station));
        if (aprs->m_a4HasValue)
            telemetryA4Item->setData(Qt::DisplayRole, applyCoefficients(3, aprs->m_a4, station));
        if (aprs->m_a5HasValue)
            telemetryA5Item->setData(Qt::DisplayRole, applyCoefficients(4, aprs->m_a5, station));
        if (aprs->m_bHasValue)
        {
            telemetryB1Item->setData(Qt::DisplayRole, aprs->m_b[0] ? 1 : 0);
            telemetryB2Item->setData(Qt::DisplayRole, aprs->m_b[1] ? 1 : 0);
            telemetryB3Item->setData(Qt::DisplayRole, aprs->m_b[2] ? 1 : 0);
            telemetryB4Item->setData(Qt::DisplayRole, aprs->m_b[3] ? 1 : 0);
            telemetryB5Item->setData(Qt::DisplayRole, aprs->m_b[4] ? 1 : 0);
            telemetryB6Item->setData(Qt::DisplayRole, aprs->m_b[5] ? 1 : 0);
            telemetryB7Item->setData(Qt::DisplayRole, aprs->m_b[6] ? 1 : 0);
            telemetryB8Item->setData(Qt::DisplayRole, aprs->m_b[7] ? 1 : 0);
        }
        telemetryCommentItem->setText(aprs->m_telemetryComment);

        for (int i = 0; i < station->m_telemetryNames.size(); i++)
        {
            QString header;
            if (station->m_telemetryLabels.size() <= i)
                header = station->m_telemetryNames[i];
            else
                header = QString("%1 (%2)").arg(station->m_telemetryNames[i]).arg(station->m_telemetryLabels[i]);
            ui->telemetryTable->horizontalHeaderItem(TELEMETRY_COL_A1+i)->setText(header);
        }
    }

    // Motion table
    if (aprs->m_hasPosition || aprs->m_hasAltitude || aprs->m_hasCourseAndSpeed)
    {
        row = ui->motionTable->rowCount();
        ui->motionTable->setRowCount(row + 1);
        QTableWidgetItem *motionDateItem = new QTableWidgetItem();
        QTableWidgetItem *motionTimeItem = new QTableWidgetItem();
        QTableWidgetItem *latitudeItem = new QTableWidgetItem();
        QTableWidgetItem *longitudeItem = new QTableWidgetItem();
        QTableWidgetItem *altitudeItem = new QTableWidgetItem();
        QTableWidgetItem *courseItem = new QTableWidgetItem();
        QTableWidgetItem *speedItem = new QTableWidgetItem();
        ui->motionTable->setItem(row, MOTION_COL_DATE, motionDateItem);
        ui->motionTable->setItem(row, MOTION_COL_TIME, motionTimeItem);
        ui->motionTable->setItem(row, MOTION_COL_LATITUDE, latitudeItem);
        ui->motionTable->setItem(row, MOTION_COL_LONGITUDE, longitudeItem);
        ui->motionTable->setItem(row, MOTION_COL_ALTITUDE, altitudeItem);
        ui->motionTable->setItem(row, MOTION_COL_COURSE, courseItem);
        ui->motionTable->setItem(row, MOTION_COL_SPEED, speedItem);
        motionDateItem->setData(Qt::DisplayRole, formatDate(aprs));
        motionTimeItem->setData(Qt::DisplayRole, formatTime(aprs));
        if (aprs->m_hasPosition)
        {
            latitudeItem->setData(Qt::DisplayRole, aprs->m_latitude);
            longitudeItem->setData(Qt::DisplayRole, aprs->m_longitude);
        }
        if (aprs->m_hasAltitude)
            altitudeItem->setData(Qt::DisplayRole, convertAltitude(aprs->m_altitudeFt));
        if (aprs->m_hasCourseAndSpeed)
        {
            courseItem->setData(Qt::DisplayRole, aprs->m_course);
            speedItem->setData(Qt::DisplayRole, convertSpeed(aprs->m_speed));
        }
    }

    // Packets table
    row = ui->packetsTable->rowCount();
    ui->packetsTable->setRowCount(row + 1);
    QTableWidgetItem *dateItem = new QTableWidgetItem();
    QTableWidgetItem *timeItem = new QTableWidgetItem();
    QTableWidgetItem *fromItem = new QTableWidgetItem();
    QTableWidgetItem *toItem = new QTableWidgetItem();
    QTableWidgetItem *viaItem = new QTableWidgetItem();
    QTableWidgetItem *dataItem = new QTableWidgetItem();
    ui->packetsTable->setItem(row, PACKETS_COL_DATE, dateItem);
    ui->packetsTable->setItem(row, PACKETS_COL_TIME, timeItem);
    ui->packetsTable->setItem(row, PACKETS_COL_FROM, fromItem);
    ui->packetsTable->setItem(row, PACKETS_COL_TO, toItem);
    ui->packetsTable->setItem(row, PACKETS_COL_VIA, viaItem);
    ui->packetsTable->setItem(row, PACKETS_COL_DATA, dataItem);
    dateItem->setData(Qt::DisplayRole, formatDate(aprs));
    timeItem->setData(Qt::DisplayRole, formatTime(aprs));
    fromItem->setText(aprs->m_from);
    toItem->setText(aprs->m_to);
    viaItem->setText(aprs->m_via);
    dataItem->setText(aprs->m_data);
}

void APRSGUI::on_stationFilter_currentIndexChanged(int index)
{
    m_settings.m_stationFilter = static_cast<APRSSettings::StationFilter>(index);
    m_settingsKeys.append("stationFilter");
    applySettings();
    filterStations();
}

void APRSGUI::on_stationSelect_currentIndexChanged(int index)
{
    (void) index;
    QString stationCallsign = ui->stationSelect->currentText();

    APRSStation *station = m_stations.value(stationCallsign);
    if (station == nullptr)
    {
        qDebug() << "APRSGUI::on_stationSelect_currentIndexChanged - station==nullptr";
        return;
    }

    // Clear tables
    ui->weatherTable->setRowCount(0);
    ui->packetsTable->setRowCount(0);
    ui->statusTable->setRowCount(0);
    ui->telemetryTable->setRowCount(0);
    ui->motionTable->setRowCount(0);

    // Set telemetry plot select combo text
    const char *telemetryNames[] = {"A1", "A2", "A3", "A4", "A5", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8"};
    int telemetryNamesSize = station->m_telemetryNames.size();
    for (int i = 0; i < 12; i++)
    {
        if (i < telemetryNamesSize)
            ui->telemetryPlotSelect->setItemText(i, station->m_telemetryNames[i]);
        else
            ui->telemetryPlotSelect->setItemText(i, telemetryNames[i]);
    }

    // Update summary table
    updateSummary(station);

    // Display/hide fields depending if station or object
    ui->stationObjectLabel->setText(station->m_isObject ? "Object" : "Station");
    ui->station->setToolTip(station->m_isObject ? "Object name" : "Station callsign and substation ID");
    ui->reportingStation->setVisible(station->m_isObject);
    ui->reportingStationLabel->setVisible(station->m_isObject);
    ui->status->setVisible(!station->m_isObject);
    ui->statusLabel->setVisible(!station->m_isObject);
    if (station->m_isObject)
    {
        if ( ui->stationsTabWidget->count() == 6)
        {
            ui->stationsTabWidget->removeTab(3);
            ui->stationsTabWidget->removeTab(3);
        }
    }
    else
    {
        if ( ui->stationsTabWidget->count() != 6)
        {
            ui->stationsTabWidget->insertTab(3, ui->telemetryTab, "Telemetry");
            ui->stationsTabWidget->insertTab(4, ui->statusTab, "Status");
        }
    }

    // Add packets to tables
    ui->packetsTable->setSortingEnabled(false);
    ui->weatherTable->setSortingEnabled(false);
    ui->statusTable->setSortingEnabled(false);
    ui->telemetryTable->setSortingEnabled(false);
    ui->motionTable->setSortingEnabled(false);
    QListIterator<APRSPacket *> i(station->m_packets);
    while (i.hasNext())
    {
        APRSPacket *aprs = i.next();
        addPacketToGUI(station, aprs);
    }
    ui->packetsTable->setSortingEnabled(true);
    ui->weatherTable->setSortingEnabled(true);
    ui->statusTable->setSortingEnabled(true);
    ui->telemetryTable->setSortingEnabled(true);
    ui->motionTable->setSortingEnabled(true);
    plotWeather();
    plotTelemetry();
    plotMotion();
}

void APRSGUI::on_filterAddressee_editingFinished()
{
    m_settings.m_filterAddressee = ui->filterAddressee->text();
    filterMessages();
    m_settingsKeys.append("filterAddressee");
    applySettings();
}

void APRSGUI::filterMessageRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterAddressee != "")
    {
        QRegExp re(m_settings.m_filterAddressee);
        QTableWidgetItem *addressee = ui->messagesTable->item(row, MESSAGE_COL_ADDRESSEE);
        if (!re.exactMatch(addressee->text()))
            hidden = true;
    }
    ui->messagesTable->setRowHidden(row, hidden);
}

void APRSGUI::filterMessages()
{
    for (int i = 0; i < ui->messagesTable->rowCount(); i++)
        filterMessageRow(i);
}

void APRSGUI::on_deleteMessages_clicked()
{
    QList<QTableWidgetItem *> list = ui->messagesTable->selectedItems();
    QList<int> rows;
    if (list.isEmpty())
    {
        // Delete all messages
        if (QMessageBox::Yes == QMessageBox::question(this, "Delete all messages", "Delete all messages?", QMessageBox::Yes|QMessageBox::No))
            ui->messagesTable->setRowCount(0);
    }
    else
    {
        // Delete selected messages (in reverse order)
        for (int i = 0; i < list.size(); i++)
        {
            int row = list[i]->row();
            if (!rows.contains(row))
                rows.append(row);
        }
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        QListIterator<int> itr(rows);
        while (itr.hasNext())
            ui->messagesTable->removeRow(itr.next());
    }
}

static void addToSeries(QLineSeries *series, const QDateTime& dt, double value, double& min, double &max)
{
    series->append(dt.toMSecsSinceEpoch(), value);
    if (value < min)
        min = value;
    if (value > max)
        max = value;
}

void APRSGUI::plotWeather()
{
    QString stationCallsign = ui->stationSelect->currentText();
    if (stationCallsign.isEmpty())
        return;
    APRSStation *station = m_stations.value(stationCallsign);
    if (station == nullptr)
        return;

    QLineSeries *series = new QLineSeries();
    double minValue = INFINITY;
    double maxValue = -INFINITY;

    int timeSelectIdx = ui->weatherTimeSelect->currentIndex();
    int plotSelectIdx = ui->weatherPlotSelect->currentIndex();
    QDateTime limit = calcTimeLimit(timeSelectIdx);

    QListIterator<APRSPacket *> i(station->m_packets);
    while (i.hasNext())
    {
        APRSPacket *aprs = i.next();
        if (aprs->m_hasWeather)
        {
            QDateTime dt;
            if (aprs->m_hasTimestamp)
                dt = aprs->m_timestamp;
            else
                dt = aprs->m_dateTime;

            if (dt >= limit)
            {
                if (plotSelectIdx == 0 && aprs->m_hasWindDirection)
                    addToSeries(series, dt, aprs->m_windDirection, minValue, maxValue);
                else if (plotSelectIdx == 1 && aprs->m_hasWindSpeed)
                    addToSeries(series, dt, aprs->m_windSpeed, minValue, maxValue);
                else if (plotSelectIdx == 2 && aprs->m_hasGust)
                    addToSeries(series, dt, aprs->m_gust, minValue, maxValue);
                else if (plotSelectIdx == 3 && aprs->m_hasTemp)
                    addToSeries(series, dt, convertTemperature(aprs->m_temp), minValue, maxValue);
                else if (plotSelectIdx == 4 && aprs->m_hasHumidity)
                    addToSeries(series, dt, aprs->m_humidity, minValue, maxValue);
                else if (plotSelectIdx == 5 && aprs->m_hasBarometricPressure)
                    addToSeries(series, dt, aprs->m_barometricPressure/10.0, minValue, maxValue);
                else if (plotSelectIdx == 6 && aprs->m_hasRainLastHr)
                    addToSeries(series, dt, convertRainfall(aprs->m_rainLastHr), minValue, maxValue);
                else if (plotSelectIdx == 7 && aprs->m_hasRainLast24Hrs)
                    addToSeries(series, dt, convertRainfall(aprs->m_rainLast24Hrs), minValue, maxValue);
                else if (plotSelectIdx == 8 && aprs->m_hasRainSinceMidnight)
                    addToSeries(series, dt, convertRainfall(aprs->m_rainSinceMidnight), minValue, maxValue);
                else if (plotSelectIdx == 9 && aprs->m_hasLuminsoity)
                    addToSeries(series, dt, aprs->m_luminosity, minValue, maxValue);
                else if (plotSelectIdx == 10 && aprs->m_hasSnowfallLast24Hrs)
                    addToSeries(series, dt, aprs->m_snowfallLast24Hrs, minValue, maxValue);
                else if (plotSelectIdx == 11 && aprs->m_hasRadiationLevel)
                    addToSeries(series, dt, aprs->m_radiationLevel, minValue, maxValue);
                else if (plotSelectIdx == 12 && aprs->m_hasFloodLevel)
                    addToSeries(series, dt, aprs->m_floodLevel, minValue, maxValue);
            }
        }
    }
    m_weatherChart.removeAllSeries();
    m_weatherChart.removeAxis(&m_weatherChartXAxis);
    m_weatherChart.removeAxis(&m_weatherChartYAxis);

    m_weatherChart.addSeries(series);

    calcTimeAxis(timeSelectIdx, &m_weatherChartXAxis, series, ui->weatherChart->width());

    m_weatherChart.addAxis(&m_weatherChartXAxis, Qt::AlignBottom);

    series->attachAxis(&m_weatherChartXAxis);

    m_weatherChartYAxis.setTitleText(ui->weatherPlotSelect->currentText());
    calcYAxis(minValue, maxValue, &m_weatherChartYAxis);
    m_weatherChart.addAxis(&m_weatherChartYAxis, Qt::AlignLeft);
    series->attachAxis(&m_weatherChartYAxis);
}

void APRSGUI::on_weatherTimeSelect_currentIndexChanged(int index)
{
    (void) index;
    plotWeather();
}

void APRSGUI::on_weatherPlotSelect_currentIndexChanged(int index)
{
    (void) index;
    plotWeather();
}

void APRSGUI::calcTimeAxis(int timeSelectIdx, QDateTimeAxis *axis, QLineSeries *series, int width)
{
    QDateTime startX = QDateTime::currentDateTime();
    QDateTime finishX = QDateTime::currentDateTime();
    finishX.setTime(QTime(0,0,0));
    finishX = finishX.addDays(1);
    int ticksScale = width < 650 ? 1 : 2; // FIXME: Should probably measure the width of some actual text
    switch (timeSelectIdx)
    {
    case 0: // Today
        startX.setTime(QTime(0,0,0));
        axis->setTickCount(6*ticksScale+1);
        axis->setFormat("hh:mm");
        axis->setTitleText(QString("Time (%1)").arg(startX.date().toString()));
        break;
    case 1: // Last hour
        startX.setTime(startX.time().addSecs(-60*60));
        finishX = QDateTime::currentDateTime();
        ticksScale = width < 750 ? 1 : 2;
        axis->setTickCount(8*ticksScale+1);
        axis->setFormat("hh:mm");
        axis->setTitleText(QString("Time (%1)").arg(startX.date().toString()));
        break;
    case 2: // Last 24h
        startX.setDate(startX.date().addDays(-1));
        finishX = QDateTime::currentDateTime();
        axis->setTickCount(6*ticksScale+1);
        axis->setFormat("hh:mm");
        axis->setTitleText(QString("Time (%1)").arg(startX.date().toString()));
        break;
    case 3: // Last 7 days
        startX.setTime(QTime(0,0,0));
        startX.setDate(finishX.date().addDays(-7));
        axis->setTickCount(4*ticksScale);
        axis->setFormat("MMM d");
        axis->setTitleText("Date");
        break;
    case 4: // Last 30 days
        startX.setTime(QTime(0,0,0));
        startX.setDate(finishX.date().addDays(-30));
        axis->setTickCount(5*ticksScale+1);
        axis->setFormat("MMM d");
        axis->setTitleText("Date");
        break;
    case 5: // All
        startX = QDateTime::fromMSecsSinceEpoch(series->at(0).x());
        finishX = QDateTime::fromMSecsSinceEpoch(series->at(series->count()-1).x());
        // FIXME: Problem when startX == finishX - axis not drawn
        if (startX.msecsTo(finishX) > 1000*60*60*24)
        {
            axis->setFormat("MMM d");
            axis->setTitleText("Date");
        }
        else if (startX.msecsTo(finishX) > 1000*60*60)
        {
            axis->setFormat("hh:mm");
            axis->setTitleText(QString("Time (hours on %1)").arg(startX.date().toString()));
        }
        else
        {
            axis->setFormat("mm:ss");
            axis->setTitleText(QString("Time (minutes on %1)").arg(startX.date().toString()));
        }
        axis->setTickCount(5*ticksScale);
        break;
    }
    axis->setRange(startX, finishX);
}

void APRSGUI::calcYAxis(double minValue, double maxValue, QValueAxis *axis, bool binary, int precision)
{
    double range = std::abs(maxValue - minValue);
    double ticks = binary ? 2 : 5;

    axis->setTickCount(ticks);

    if (binary)
    {
        minValue = 0.0;
        maxValue = 1.0;
    }
    else if (range == 0.0)
    {
        // Nothing is plotted if min and max are the same, so adjust range to non-zero
        if (precision == 1)
        {
            if ((minValue >= (ticks-1)/2) || (minValue < 0.0))
            {
                maxValue += (ticks-1)/2;
                minValue -= (ticks-1)/2;
            }
            else
                maxValue = maxValue + (ticks - 1 - minValue);
        }
        else if (maxValue == 0.0)
            maxValue = ticks-1;
        else
        {
            minValue -= minValue * (1.0/std::pow(10.0,precision));
            maxValue += maxValue * (1.0/std::pow(10.0,precision));
        }
        range = std::abs(maxValue - minValue);
    }
    axis->setRange(minValue, maxValue);
    if (((range < (ticks-1)) || (precision > 1)) && !binary)
        axis->setLabelFormat(QString("%.%1f").arg(precision));
    else
        axis->setLabelFormat("%d");
}

QDateTime APRSGUI::calcTimeLimit(int timeSelectIdx)
{
    QDateTime limit = QDateTime::currentDateTime();
    switch (timeSelectIdx)
    {
    case 0: // Today
        limit.setTime(QTime(0,0,0));
        break;
    case 1: // Last hour
        limit.setTime(limit.time().addSecs(-60*60));
        break;
    case 2: // Last 24h
        limit.setDate(limit.date().addDays(-1));
        break;
    case 3: // Last 7 days
        limit.setTime(QTime(0,0,0));
        limit.setDate(limit.date().addDays(-7));
        break;
    case 4: // Last 30 days
        limit.setTime(QTime(0,0,0));
        limit.setDate(limit.date().addDays(-30));
        break;
    case 5: // All
        limit = QDateTime(QDate(1970, 1, 1), QTime());
        break;
    }
    return limit;
}

void APRSGUI::plotMotion()
{
    QString stationCallsign = ui->stationSelect->currentText();
    if (stationCallsign.isEmpty())
        return;
    APRSStation *station = m_stations.value(stationCallsign);
    if (station == nullptr)
        return;

    QLineSeries *series = new QLineSeries();
    double minValue = INFINITY;
    double maxValue = -INFINITY;

    int timeSelectIdx = ui->motionTimeSelect->currentIndex();
    int plotSelectIdx = ui->motionPlotSelect->currentIndex();
    QDateTime limit = calcTimeLimit(timeSelectIdx);

    QListIterator<APRSPacket *> i(station->m_packets);
    while (i.hasNext())
    {
        APRSPacket *aprs = i.next();

        if (aprs->m_hasPosition || aprs->m_hasAltitude || aprs->m_hasCourseAndSpeed)
        {
            QDateTime dt;
            if (aprs->m_hasTimestamp)
                dt = aprs->m_timestamp;
            else
                dt = aprs->m_dateTime;

            if (dt >= limit)
            {
                if (plotSelectIdx == 0 && aprs->m_hasPosition)
                    addToSeries(series, dt, aprs->m_latitude, minValue, maxValue);
                else if (plotSelectIdx == 1 && aprs->m_hasPosition)
                    addToSeries(series, dt, aprs->m_longitude, minValue, maxValue);
                else if (plotSelectIdx == 2 && aprs->m_hasAltitude)
                    addToSeries(series, dt, convertAltitude(aprs->m_altitudeFt), minValue, maxValue);
                else if (plotSelectIdx == 3 && aprs->m_hasCourseAndSpeed)
                    addToSeries(series, dt, aprs->m_course, minValue, maxValue);
                else if (plotSelectIdx == 4 && aprs->m_hasCourseAndSpeed)
                    addToSeries(series, dt, convertSpeed(aprs->m_speed), minValue, maxValue);
            }
        }
    }

    m_motionChart.removeAllSeries();
    m_motionChart.removeAxis(&m_motionChartXAxis);
    m_motionChart.removeAxis(&m_motionChartYAxis);

    m_motionChart.addSeries(series);
    calcTimeAxis(timeSelectIdx, &m_motionChartXAxis, series, ui->motionChart->width());
    m_motionChart.addAxis(&m_motionChartXAxis, Qt::AlignBottom);
    series->attachAxis(&m_motionChartXAxis);

    m_motionChartYAxis.setTitleText(ui->motionPlotSelect->currentText());
    calcYAxis(minValue, maxValue, &m_motionChartYAxis, false, plotSelectIdx <= 1 ? 5 : 1);
    m_motionChart.addAxis(&m_motionChartYAxis, Qt::AlignLeft);
    series->attachAxis(&m_motionChartYAxis);
}

void APRSGUI::on_motionTimeSelect_currentIndexChanged(int index)
{
    (void) index;
    plotMotion();
}

void APRSGUI::on_motionPlotSelect_currentIndexChanged(int index)
{
    (void) index;
    plotMotion();
}

void APRSGUI::plotTelemetry()
{
    QString stationCallsign = ui->stationSelect->currentText();
    if (stationCallsign.isEmpty())
        return;
    APRSStation *station = m_stations.value(stationCallsign);
    if (station == nullptr)
        return;

    QLineSeries *series = new QLineSeries();
    double minValue = INFINITY;
    double maxValue = -INFINITY;

    int timeSelectIdx = ui->telemetryTimeSelect->currentIndex();
    int plotSelectIdx = ui->telemetryPlotSelect->currentIndex();
    QDateTime limit = calcTimeLimit(timeSelectIdx);

    QListIterator<APRSPacket *> i(station->m_packets);
    while (i.hasNext())
    {
        APRSPacket *aprs = i.next();
        if (aprs->m_hasTelemetry)
        {
            if (aprs->m_dateTime >= limit)
            {
                if (plotSelectIdx == 0 && aprs->m_a1HasValue)
                    addToSeries(series, aprs->m_dateTime, applyCoefficients(0, aprs->m_a1, station), minValue, maxValue);
                else if (plotSelectIdx == 1 && aprs->m_a2HasValue)
                    addToSeries(series, aprs->m_dateTime, applyCoefficients(1, aprs->m_a2, station), minValue, maxValue);
                else if (plotSelectIdx == 2 && aprs->m_a3HasValue)
                    addToSeries(series, aprs->m_dateTime, applyCoefficients(2, aprs->m_a3, station), minValue, maxValue);
                else if (plotSelectIdx == 3 && aprs->m_a4HasValue)
                    addToSeries(series, aprs->m_dateTime, applyCoefficients(3, aprs->m_a4, station), minValue, maxValue);
                else if (plotSelectIdx == 4 && aprs->m_a5HasValue)
                    addToSeries(series, aprs->m_dateTime, applyCoefficients(4, aprs->m_a5, station), minValue, maxValue);
                else if (plotSelectIdx == 5 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[0], minValue, maxValue);
                else if (plotSelectIdx == 6 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[1], minValue, maxValue);
                else if (plotSelectIdx == 7 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[2], minValue, maxValue);
                else if (plotSelectIdx == 8 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[3], minValue, maxValue);
                else if (plotSelectIdx == 9 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[4], minValue, maxValue);
                else if (plotSelectIdx == 10 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[5], minValue, maxValue);
                else if (plotSelectIdx == 11 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[6], minValue, maxValue);
                else if (plotSelectIdx == 12 && aprs->m_bHasValue)
                    addToSeries(series, aprs->m_dateTime, aprs->m_b[7], minValue, maxValue);
            }
        }
    }

    m_telemetryChart.removeAllSeries();
    m_telemetryChart.removeAxis(&m_telemetryChartXAxis);
    m_telemetryChart.removeAxis(&m_telemetryChartYAxis);

    m_telemetryChart.addSeries(series);
    calcTimeAxis(timeSelectIdx, &m_telemetryChartXAxis, series, ui->telemetryChart->width());
    m_telemetryChart.addAxis(&m_telemetryChartXAxis, Qt::AlignBottom);

    series->attachAxis(&m_telemetryChartXAxis);
    m_telemetryChartYAxis.setTitleText(ui->telemetryPlotSelect->currentText());
    calcYAxis(minValue, maxValue, &m_telemetryChartYAxis, plotSelectIdx >= 5);
    m_telemetryChart.addAxis(&m_telemetryChartYAxis, Qt::AlignLeft);
    series->attachAxis(&m_telemetryChartYAxis);
}

void APRSGUI::on_telemetryTimeSelect_currentIndexChanged(int index)
{
    (void) index;
    plotTelemetry();
}

void APRSGUI::on_telemetryPlotSelect_currentIndexChanged(int index)
{
    (void) index;
    plotTelemetry();
}

void APRSGUI::updateStatus()
{
    int state = m_aprs->getState();

    if (m_lastFeatureState != state)
    {
        switch (state)
        {
            case Feature::StNotStarted:
                ui->igate->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StIdle:
                ui->igate->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case Feature::StRunning:
                ui->igate->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case Feature::StError:
                ui->igate->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_aprs->getErrorMessage());
                break;
            default:
                break;
        }

        m_lastFeatureState = state;
    }
}

void APRSGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        APRS::MsgConfigureAPRS* message = APRS::MsgConfigureAPRS::create(m_settings, m_settingsKeys, force);
        m_aprs->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

void APRSGUI::resizeTable()
{
    int row;

    // Fill tables with a row of dummy data that will size the columns nicely
    row = ui->packetsTable->rowCount();
    ui->packetsTable->setRowCount(row + 1);
    ui->packetsTable->setItem(row, PACKETS_COL_DATE, new QTableWidgetItem("31/12/2020"));
    ui->packetsTable->setItem(row, PACKETS_COL_TIME, new QTableWidgetItem("23:59:39 T"));
    ui->packetsTable->setItem(row, PACKETS_COL_FROM, new QTableWidgetItem("123456-15-"));
    ui->packetsTable->setItem(row, PACKETS_COL_TO, new QTableWidgetItem("123456-15-"));
    ui->packetsTable->setItem(row, PACKETS_COL_VIA, new QTableWidgetItem("123456-15-"));
    ui->packetsTable->setItem(row, PACKETS_COL_DATA, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->packetsTable->resizeColumnsToContents();
    ui->packetsTable->removeRow(row);

    row = ui->weatherTable->rowCount();
    ui->weatherTable->setRowCount(row + 1);
    ui->weatherTable->setItem(row, WEATHER_COL_DATE, new QTableWidgetItem("31/12/2020"));
    ui->weatherTable->setItem(row, WEATHER_COL_TIME, new QTableWidgetItem("23:59:39 T"));
    ui->weatherTable->setItem(row, WEATHER_COL_WIND_DIRECTION, new QTableWidgetItem("Dir (o)-"));
    ui->weatherTable->setItem(row, WEATHER_COL_WIND_SPEED, new QTableWidgetItem("Speed (mph)-"));
    ui->weatherTable->setItem(row, WEATHER_COL_GUSTS, new QTableWidgetItem("Gusts (mph)-"));
    ui->weatherTable->setItem(row, WEATHER_COL_TEMPERATURE, new QTableWidgetItem("Temp (F)-"));
    ui->weatherTable->setItem(row, WEATHER_COL_HUMIDITY, new QTableWidgetItem("Humidity (%)-"));
    ui->weatherTable->setItem(row, WEATHER_COL_PRESSURE, new QTableWidgetItem("Pressure (mbar)-"));
    ui->weatherTable->setItem(row, WEATHER_COL_RAIN_LAST_HOUR, new QTableWidgetItem("Rain 1h-"));
    ui->weatherTable->setItem(row, WEATHER_COL_RAIN_LAST_24_HOURS, new QTableWidgetItem("Rain 24h-"));
    ui->weatherTable->setItem(row, WEATHER_COL_RAIN_SINCE_MIDNIGHT, new QTableWidgetItem("Rain-"));
    ui->weatherTable->setItem(row, WEATHER_COL_LUMINOSITY, new QTableWidgetItem("Luminosity-"));
    ui->weatherTable->setItem(row, WEATHER_COL_SNOWFALL, new QTableWidgetItem("Snowfall-"));
    ui->weatherTable->setItem(row, WEATHER_COL_RADIATION_LEVEL, new QTableWidgetItem("Radiation-"));
    ui->weatherTable->setItem(row, WEATHER_COL_FLOOD_LEVEL, new QTableWidgetItem("Flood level-"));
    ui->weatherTable->resizeColumnsToContents();
    ui->weatherTable->removeRow(row);

    row = ui->statusTable->rowCount();
    ui->statusTable->setRowCount(row + 1);
    ui->statusTable->setIconSize(QSize(24, 24));
    ui->statusTable->setItem(row, STATUS_COL_DATE, new QTableWidgetItem("31/12/2020"));
    ui->statusTable->setItem(row, STATUS_COL_TIME, new QTableWidgetItem("23:59:39 T"));
    ui->statusTable->setItem(row, STATUS_COL_STATUS, new QTableWidgetItem("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    ui->statusTable->setItem(row, STATUS_COL_SYMBOL, new QTableWidgetItem("WWW"));
    ui->statusTable->setItem(row, STATUS_COL_MAIDENHEAD, new QTableWidgetItem("WW00WW"));
    ui->statusTable->setItem(row, STATUS_COL_BEAM_HEADING, new QTableWidgetItem("359"));
    ui->statusTable->setItem(row, STATUS_COL_BEAM_POWER, new QTableWidgetItem("8000"));
    ui->statusTable->resizeColumnsToContents();
    ui->statusTable->removeRow(row);

    row = ui->messagesTable->rowCount();
    ui->messagesTable->setRowCount(row + 1);
    ui->messagesTable->setIconSize(QSize(24, 24));
    ui->messagesTable->setItem(row, MESSAGE_COL_DATE, new QTableWidgetItem("31/12/2020"));
    ui->messagesTable->setItem(row, MESSAGE_COL_TIME, new QTableWidgetItem("23:59:39 T"));
    ui->messagesTable->setItem(row, MESSAGE_COL_ADDRESSEE, new QTableWidgetItem("WWWWWWWWW"));
    ui->messagesTable->setItem(row, MESSAGE_COL_MESSAGE, new QTableWidgetItem("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    ui->messagesTable->setItem(row, MESSAGE_COL_NO, new QTableWidgetItem("Message No"));
    ui->messagesTable->resizeColumnsToContents();
    ui->messagesTable->removeRow(row);

    row = ui->motionTable->rowCount();
    ui->motionTable->setRowCount(row + 1);
    ui->motionTable->setItem(row, MOTION_COL_DATE, new QTableWidgetItem("31/12/2020"));
    ui->motionTable->setItem(row, MOTION_COL_TIME, new QTableWidgetItem("23:59:39 T"));
    ui->motionTable->setItem(row, MOTION_COL_LATITUDE, new QTableWidgetItem("Latitude"));
    ui->motionTable->setItem(row, MOTION_COL_LONGITUDE, new QTableWidgetItem("Longitude"));
    ui->motionTable->setItem(row, MOTION_COL_ALTITUDE, new QTableWidgetItem("Message No"));
    ui->motionTable->setItem(row, MOTION_COL_COURSE, new QTableWidgetItem("Course"));
    ui->motionTable->setItem(row, MOTION_COL_SPEED, new QTableWidgetItem("Speed"));
    ui->motionTable->resizeColumnsToContents();
    ui->motionTable->removeRow(row);

    row = ui->telemetryTable->rowCount();
    ui->telemetryTable->setRowCount(row + 1);
    ui->telemetryTable->setItem(row, TELEMETRY_COL_DATE, new QTableWidgetItem("31/12/2020"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_TIME, new QTableWidgetItem("23:59:39 T"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_SEQ_NO, new QTableWidgetItem("Seq No"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_A1, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_A2, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_A3, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_A4, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_A5, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B1, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B2, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B3, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B4, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B5, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B6, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B7, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->setItem(row, TELEMETRY_COL_B8, new QTableWidgetItem("ABCEDF"));
    ui->telemetryTable->resizeColumnsToContents();
    ui->telemetryTable->removeRow(row);
}

// Columns in table reordered
void APRSGUI::packetsTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_packetsTableColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void APRSGUI::packetsTable_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_packetsTableColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void APRSGUI::packetsTable_columnSelectMenu(QPoint pos)
{
    packetsTableMenu->popup(ui->packetsTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void APRSGUI::packetsTable_columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->packetsTable->setColumnHidden(idx, !action->isChecked());
    }
}

// Columns in table reordered
void APRSGUI::weatherTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_weatherTableColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void APRSGUI::weatherTable_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_weatherTableColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void APRSGUI::weatherTable_columnSelectMenu(QPoint pos)
{
    weatherTableMenu->popup(ui->weatherTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void APRSGUI::weatherTable_columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->weatherTable->setColumnHidden(idx, !action->isChecked());
    }
}

// Columns in table reordered
void APRSGUI::statusTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_statusTableColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void APRSGUI::statusTable_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_statusTableColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void APRSGUI::statusTable_columnSelectMenu(QPoint pos)
{
    statusTableMenu->popup(ui->statusTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void APRSGUI::statusTable_columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->statusTable->setColumnHidden(idx, !action->isChecked());
    }
}

// Columns in table reordered
void APRSGUI::messagesTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_messagesTableColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void APRSGUI::messagesTable_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_messagesTableColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void APRSGUI::messagesTable_columnSelectMenu(QPoint pos)
{
    messagesTableMenu->popup(ui->messagesTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void APRSGUI::messagesTable_columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->messagesTable->setColumnHidden(idx, !action->isChecked());
    }
}

// Columns in table reordered
void APRSGUI::telemetryTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_telemetryTableColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void APRSGUI::telemetryTable_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_telemetryTableColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void APRSGUI::telemetryTable_columnSelectMenu(QPoint pos)
{
    telemetryTableMenu->popup(ui->telemetryTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void APRSGUI::telemetryTable_columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->telemetryTable->setColumnHidden(idx, !action->isChecked());
    }
}

// Columns in table reordered
void APRSGUI::motionTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_motionTableColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void APRSGUI::motionTable_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_motionTableColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void APRSGUI::motionTable_columnSelectMenu(QPoint pos)
{
    motionTableMenu->popup(ui->motionTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void APRSGUI::motionTable_columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->motionTable->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu items

QAction *APRSGUI::packetsTable_createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(packetsTable_columnSelectMenuChecked()));
    return action;
}

QAction *APRSGUI::weatherTable_createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(weatherTable_columnSelectMenuChecked()));
    return action;
}

QAction *APRSGUI::statusTable_createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(statusTable_columnSelectMenuChecked()));
    return action;
}

QAction *APRSGUI::messagesTable_createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(messagesTable_columnSelectMenuChecked()));
    return action;
}

QAction *APRSGUI::telemetryTable_createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(telemetryTable_columnSelectMenuChecked()));
    return action;
}

QAction *APRSGUI::motionTable_createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(motionTable_columnSelectMenuChecked()));
    return action;
}

void APRSGUI::setUnits()
{
    ui->altitudeUnitsLabel->setText(APRSSettings::m_altitudeUnitNames[m_settings.m_altitudeUnits]);
    ui->antennaHeightUnitsLabel->setText(APRSSettings::m_altitudeUnitNames[m_settings.m_altitudeUnits]);
    ui->speedUnitsLabel->setText(APRSSettings::m_speedUnitNames[m_settings.m_speedUnits]);

    ui->weatherTable->horizontalHeaderItem(WEATHER_COL_TEMPERATURE)->setText(QString("Temp (%1)").arg(APRSSettings::m_temperatureUnitNames[m_settings.m_temperatureUnits]));

    // Display data using new units
    int idx = ui->stationSelect->currentIndex();
    if (idx >= 0)
        on_stationSelect_currentIndexChanged(idx);
}

QString APRSGUI::convertAltitude(const QString& altitude)
{
    if ((m_settings.m_altitudeUnits == APRSSettings::FEET) || altitude.isEmpty())
        return altitude;
    else
        return QString::number((int)std::round(Units::feetToMetres(altitude.toFloat())));
}

float APRSGUI::convertAltitude(float altitude)
{
    if (m_settings.m_altitudeUnits == APRSSettings::FEET)
        return altitude;
    else
        return std::round(Units::feetToMetres(altitude));
}

QString APRSGUI::convertSpeed(const QString& speed)
{
    if ((m_settings.m_speedUnits == APRSSettings::KNOTS) || speed.isEmpty())
        return speed;
    else if (m_settings.m_speedUnits == APRSSettings::MPH)
        return QString::number(Units::knotsToIntegerMPH(speed.toFloat()));
    else
        return QString::number(Units::knotsToIntegerKPH(speed.toFloat()));
}

int APRSGUI::convertSpeed(int speed)
{
    if (m_settings.m_speedUnits == APRSSettings::KNOTS)
        return speed;
    else if (m_settings.m_speedUnits == APRSSettings::MPH)
        return Units::knotsToIntegerMPH(speed);
    else
        return Units::knotsToIntegerKPH(speed);
}

int APRSGUI::convertTemperature(int temperature)
{
    if (m_settings.m_temperatureUnits == APRSSettings::FAHRENHEIT)
        return temperature;
    else
        return (int)std::round(Units::fahrenheitToCelsius(temperature));
}

int APRSGUI::convertRainfall(int rainfall)
{
    if (m_settings.m_rainfallUnits == APRSSettings::HUNDREDTHS_OF_AN_INCH)
        return rainfall;
    else
        return (int)std::round(Units::inchesToMilimetres(rainfall/100.0f));
}

// Show settings dialog
void APRSGUI::on_displaySettings_clicked()
{
    APRSSettingsDialog dialog(&m_settings);

    if (dialog.exec() == QDialog::Accepted)
    {
        setUnits();

        m_settingsKeys.append("igateServer");
        m_settingsKeys.append("igateCallsign");
        m_settingsKeys.append("igatePasscode");
        m_settingsKeys.append("igateFilter");
        m_settingsKeys.append("altitudeUnits");
        m_settingsKeys.append("speedUnits");
        m_settingsKeys.append("temperatureUnits");
        m_settingsKeys.append("rainfallUnits");

        applySettings();
    }
}

void APRSGUI::on_igate_toggled(bool checked)
{
    m_settings.m_igateEnabled = checked;
    m_settingsKeys.append("igateEnabled");
    applySettings();
}

// Find the selected station on the Map
void APRSGUI::on_viewOnMap_clicked()
{
    QString stationName = ui->stationSelect->currentText();
    if (!stationName.isEmpty())
    {
        APRSStation *station = m_stations.value(stationName);
        if (station != nullptr)
        {
            FeatureWebAPIUtils::mapFind(station->m_station);
        }
    }
}

void APRSGUI::makeUIConnections()
{
    QObject::connect(ui->stationFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_stationFilter_currentIndexChanged);
    QObject::connect(ui->stationSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_stationSelect_currentIndexChanged);
    QObject::connect(ui->filterAddressee, &QLineEdit::editingFinished, this, &APRSGUI::on_filterAddressee_editingFinished);
    QObject::connect(ui->deleteMessages, &QPushButton::clicked, this, &APRSGUI::on_deleteMessages_clicked);
    QObject::connect(ui->weatherTimeSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_weatherTimeSelect_currentIndexChanged);
    QObject::connect(ui->weatherPlotSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_weatherPlotSelect_currentIndexChanged);
    QObject::connect(ui->telemetryTimeSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_telemetryTimeSelect_currentIndexChanged);
    QObject::connect(ui->telemetryPlotSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_telemetryPlotSelect_currentIndexChanged);
    QObject::connect(ui->motionTimeSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_motionTimeSelect_currentIndexChanged);
    QObject::connect(ui->motionPlotSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &APRSGUI::on_motionPlotSelect_currentIndexChanged);
    QObject::connect(ui->displaySettings, &QPushButton::clicked, this, &APRSGUI::on_displaySettings_clicked);
    QObject::connect(ui->igate, &ButtonSwitch::toggled, this, &APRSGUI::on_igate_toggled);
    QObject::connect(ui->viewOnMap, &QPushButton::clicked, this, &APRSGUI::on_viewOnMap_clicked);
}
