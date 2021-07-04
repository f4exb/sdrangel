///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include <QDesktopServices>
#include <QAction>

#include "feature/featureuiset.h"
#include "feature/featurewebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"

#include "ui_aisgui.h"
#include "ais.h"
#include "aisgui.h"

#include "SWGMapItem.h"

AISGUI* AISGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    AISGUI* gui = new AISGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void AISGUI::destroy()
{
    delete this;
}

void AISGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray AISGUI::serialize() const
{
    return m_settings.serialize();
}

bool AISGUI::deserialize(const QByteArray& data)
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

bool AISGUI::handleMessage(const Message& message)
{
    if (AIS::MsgConfigureAIS::match(message))
    {
        qDebug("AISGUI::handleMessage: AIS::MsgConfigureAIS");
        const AIS::MsgConfigureAIS& cfg = (AIS::MsgConfigureAIS&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (MainCore::MsgPacket::match(message))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) message;

        // Decode the message
        AISMessage *ais = AISMessage::decode(report.getPacket());
        // Update table
        updateVessels(ais);
    }

    return false;
}

void AISGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AISGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

AISGUI::AISGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::AISGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setChannelWidget(false);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    m_ais = reinterpret_cast<AIS*>(feature);
    m_ais->setMessageQueueToGUI(&m_inputMessageQueue);

    m_featureUISet->addRollupWidget(this);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(1000);

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->vessels->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->vessels->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    vesselsMenu = new QMenu(ui->vessels);
    for (int i = 0; i < ui->vessels->horizontalHeader()->count(); i++)
    {
        QString text = ui->vessels->horizontalHeaderItem(i)->text();
        vesselsMenu->addAction(createCheckableItem(text, i, true, SLOT(vesselsColumnSelectMenuChecked())));
    }
    ui->vessels->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->vessels->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(vesselsColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->vessels->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(vessels_sectionMoved(int, int, int)));
    connect(ui->vessels->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(vessels_sectionResized(int, int, int)));

    displaySettings();
    applySettings(true);
}

AISGUI::~AISGUI()
{
    delete ui;
}

void AISGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AISGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    blockApplySettings(true);

    // Order and size columns
    QHeaderView *header = ui->vessels->horizontalHeader();
    for (int i = 0; i < AIS_VESSEL_COLUMNS; i++)
    {
        bool hidden = m_settings.m_vesselColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        vesselsMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_vesselColumnSizes[i] > 0) {
            ui->vessels->setColumnWidth(i, m_settings.m_vesselColumnSizes[i]);
        }
        header->moveSection(header->visualIndex(i), m_settings.m_vesselColumnIndexes[i]);
    }

    blockApplySettings(false);
    arrangeRollups();
}

void AISGUI::leaveEvent(QEvent*)
{
}

void AISGUI::enterEvent(QEvent*)
{
}

void AISGUI::onMenuDialogCalled(const QPoint &p)
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

void AISGUI::updateStatus()
{
}

void AISGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        AIS::MsgConfigureAIS* message = AIS::MsgConfigureAIS::create(m_settings, force);
        m_ais->getInputMessageQueue()->push(message);
    }
}

void AISGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->vessels->rowCount();
    ui->vessels->setRowCount(row + 1);
    ui->vessels->setItem(row, VESSEL_COL_MMSI, new QTableWidgetItem("123456789"));
    ui->vessels->setItem(row, VESSEL_COL_TYPE, new QTableWidgetItem("Base station"));
    ui->vessels->setItem(row, VESSEL_COL_LATITUDE, new QTableWidgetItem("90.000000-"));
    ui->vessels->setItem(row, VESSEL_COL_LONGITUDE, new QTableWidgetItem("180.00000-"));
    ui->vessels->setItem(row, VESSEL_COL_COURSE, new QTableWidgetItem("360.0"));
    ui->vessels->setItem(row, VESSEL_COL_SPEED, new QTableWidgetItem("120"));
    ui->vessels->setItem(row, VESSEL_COL_HEADING, new QTableWidgetItem("360"));
    ui->vessels->setItem(row, VESSEL_COL_STATUS, new QTableWidgetItem("Under way using engine"));
    ui->vessels->setItem(row, VESSEL_COL_IMO, new QTableWidgetItem("123456789"));
    ui->vessels->setItem(row, VESSEL_COL_NAME, new QTableWidgetItem("12345678901234567890"));
    ui->vessels->setItem(row, VESSEL_COL_CALLSIGN, new QTableWidgetItem("1234567"));
    ui->vessels->setItem(row, VESSEL_COL_SHIP_TYPE, new QTableWidgetItem("Passenger"));
    ui->vessels->setItem(row, VESSEL_COL_DESTINATION, new QTableWidgetItem("12345678901234567890"));
    ui->vessels->resizeColumnsToContents();
    ui->vessels->removeRow(row);
}

// Columns in table reordered
void AISGUI::vessels_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_vesselColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void AISGUI::vessels_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_vesselColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void AISGUI::vesselsColumnSelectMenu(QPoint pos)
{
    vesselsMenu->popup(ui->vessels->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void AISGUI::vesselsColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->vessels->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *AISGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

void AISGUI::updateVessels(AISMessage *ais)
{
    QTableWidgetItem *mmsiItem;
    QTableWidgetItem *typeItem;
    QTableWidgetItem *latitudeItem;
    QTableWidgetItem *longitudeItem;
    QTableWidgetItem *courseItem;
    QTableWidgetItem *speedItem;
    QTableWidgetItem *headingItem;
    QTableWidgetItem *statusItem;
    QTableWidgetItem *imoItem;
    QTableWidgetItem *nameItem;
    QTableWidgetItem *callsignItem;
    QTableWidgetItem *shipTypeItem;
    QTableWidgetItem *destinationItem;

    // See if vessel is already in table
    QString messageMMSI = QString("%1").arg(ais->m_mmsi, 9, 10, QChar('0'));
    bool found = false;
    for (int row = 0; row < ui->vessels->rowCount(); row++)
    {
        QString itemMMSI = ui->vessels->item(row, VESSEL_COL_MMSI)->text();
        if (messageMMSI == itemMMSI)
        {
            // Update existing item
            mmsiItem = ui->vessels->item(row, VESSEL_COL_MMSI);
            typeItem = ui->vessels->item(row, VESSEL_COL_TYPE);
            latitudeItem = ui->vessels->item(row, VESSEL_COL_LATITUDE);
            longitudeItem = ui->vessels->item(row, VESSEL_COL_LONGITUDE);
            courseItem = ui->vessels->item(row, VESSEL_COL_COURSE);
            speedItem = ui->vessels->item(row, VESSEL_COL_SPEED);
            headingItem = ui->vessels->item(row, VESSEL_COL_HEADING);
            statusItem = ui->vessels->item(row, VESSEL_COL_STATUS);
            imoItem = ui->vessels->item(row, VESSEL_COL_IMO);
            nameItem = ui->vessels->item(row, VESSEL_COL_NAME);
            callsignItem = ui->vessels->item(row, VESSEL_COL_CALLSIGN);
            shipTypeItem = ui->vessels->item(row, VESSEL_COL_SHIP_TYPE);
            destinationItem = ui->vessels->item(row, VESSEL_COL_DESTINATION);
            found = true;
            break;
        }
    }
    if (!found)
    {
        // Add new vessel
        ui->vessels->setSortingEnabled(false);
        int row = ui->vessels->rowCount();
        ui->vessels->setRowCount(row + 1);

        mmsiItem = new QTableWidgetItem();
        typeItem = new QTableWidgetItem();
        latitudeItem = new QTableWidgetItem();
        longitudeItem = new QTableWidgetItem();
        courseItem = new QTableWidgetItem();
        speedItem = new QTableWidgetItem();
        headingItem = new QTableWidgetItem();
        statusItem = new QTableWidgetItem();
        imoItem = new QTableWidgetItem();
        nameItem = new QTableWidgetItem();
        callsignItem = new QTableWidgetItem();
        shipTypeItem = new QTableWidgetItem();
        destinationItem = new QTableWidgetItem();
        ui->vessels->setItem(row, VESSEL_COL_MMSI, mmsiItem);
        ui->vessels->setItem(row, VESSEL_COL_TYPE, typeItem);
        ui->vessels->setItem(row, VESSEL_COL_LATITUDE, latitudeItem);
        ui->vessels->setItem(row, VESSEL_COL_LONGITUDE, longitudeItem);
        ui->vessels->setItem(row, VESSEL_COL_COURSE, courseItem);
        ui->vessels->setItem(row, VESSEL_COL_SPEED, speedItem);
        ui->vessels->setItem(row, VESSEL_COL_HEADING, headingItem);
        ui->vessels->setItem(row, VESSEL_COL_STATUS, statusItem);
        ui->vessels->setItem(row, VESSEL_COL_IMO, imoItem);
        ui->vessels->setItem(row, VESSEL_COL_NAME, nameItem);
        ui->vessels->setItem(row, VESSEL_COL_CALLSIGN, callsignItem);
        ui->vessels->setItem(row, VESSEL_COL_SHIP_TYPE, shipTypeItem);
        ui->vessels->setItem(row, VESSEL_COL_DESTINATION, destinationItem);
    }

    mmsiItem->setText(QString("%1").arg(ais->m_mmsi, 9, 10, QChar('0')));
    if ((ais->m_id <= 3) || (ais->m_id == 5) || (ais->m_id == 18) || (ais->m_id == 19)) {
        typeItem->setText("Vessel");
    } else if (ais->m_id == 4) {
        typeItem->setText("Base station");
    } else if (ais->m_id == 9) {
        typeItem->setText("Aircraft");
    } else if (ais->m_id == 21) {
        typeItem->setText("Aid-to-nav");
    }
    if (ais->m_id == 21)
    {
         AISAidsToNavigationReport *aids = dynamic_cast<AISAidsToNavigationReport*>(ais);
         if (aids) {
             nameItem->setText(aids->m_name);
         }
    }
    if (ais->m_id == 5)
    {
         AISShipStaticAndVoyageData *vd = dynamic_cast<AISShipStaticAndVoyageData*>(ais);
         if (vd)
         {
             if (vd->m_imo != 0) {
                 imoItem->setData(Qt::DisplayRole, vd->m_imo);
             }
             nameItem->setText(vd->m_name);
             callsignItem->setText(vd->m_callsign);
             shipTypeItem->setText(AISMessage::typeToString(vd->m_type));
             destinationItem->setText(vd->m_destination);
         }
    }
    else
    {
        if (ais->hasPosition())
        {
            latitudeItem->setData(Qt::DisplayRole, ais->getLatitude());
            longitudeItem->setData(Qt::DisplayRole, ais->getLongitude());
        }
        if (ais->hasCourse()) {
            courseItem->setData(Qt::DisplayRole, ais->getCourse());
        }
        if (ais->hasSpeed()) {
            speedItem->setData(Qt::DisplayRole, ais->getSpeed());
        }
        if (ais->hasHeading()) {
            headingItem->setData(Qt::DisplayRole, ais->getHeading());
        }
        AISPositionReport *pr = dynamic_cast<AISPositionReport*>(ais);
        if (pr) {
            statusItem->setText(AISPositionReport::getStatusString(pr->m_status));
        }
        AISLongRangePositionReport *lrpr = dynamic_cast<AISLongRangePositionReport*>(ais);
        if (lrpr) {
            statusItem->setText(AISPositionReport::getStatusString(lrpr->m_status));
        }
    }
    if (ais->m_id == 19)
    {
        AISExtendedClassBPositionReport *ext = dynamic_cast<AISExtendedClassBPositionReport*>(ais);
        if (ext) {
             shipTypeItem->setText(AISMessage::typeToString(ext->m_type));
        }
    }
    if (ais->m_id == 24)
    {
        AISStaticDataReport *dr = dynamic_cast<AISStaticDataReport*>(ais);
        if (dr)
        {
            if (dr->m_partNumber == 0)
            {
                nameItem->setText(dr->m_name);
            }
            else if (dr->m_partNumber == 1)
            {
                callsignItem->setText(dr->m_callsign);
                shipTypeItem->setText(AISMessage::typeToString(dr->m_type));
            }
        }
    }
    ui->vessels->setSortingEnabled(true);

    QVariant latitudeV = latitudeItem->data(Qt::DisplayRole);
    QVariant longitudeV = longitudeItem->data(Qt::DisplayRole);
    QString type = typeItem->text();

    if (!latitudeV.isNull() && !longitudeV.isNull() && !type.isEmpty())
    {
        // Send to Map feature
        MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
        QList<MessageQueue*> *mapMessageQueues = messagePipes.getMessageQueues(m_ais, "mapitems");
        if (mapMessageQueues)
        {
            QList<MessageQueue*>::iterator it = mapMessageQueues->begin();

            for (; it != mapMessageQueues->end(); ++it)
            {
                SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
                swgMapItem->setName(new QString(QString("%1").arg(mmsiItem->text())));
                swgMapItem->setLatitude(latitudeV.toFloat());
                swgMapItem->setLongitude(longitudeV.toFloat());
                swgMapItem->setAltitude(0);
                QString image;
                if (type == "Aircraft") {
                    // I presume search and rescue aircraft are more likely to be helicopters
                    image = "helicopter.png";
                } else if (type == "Base station") {
                    image = "anchor.png";
                } else if (type == "Aid-to-nav") {
                    image = "bouy.png";
                } else {
                    image = "ship.png";
                    QString shipType = shipTypeItem->text();
                    if (!shipType.isEmpty())
                    {
                        if (shipType == "Tug") {
                            image = "tug.png";
                        } else if (shipType == "Cargo") {
                            image = "cargo.png";
                        } else if (shipType == "Tanker") {
                            image = "tanker.png";
                        }
                    }
                }
                swgMapItem->setImage(new QString(QString("qrc:///ais/map/%1").arg(image)));

                swgMapItem->setImageMinZoom(11);
                QStringList text;
                QVariant courseV = courseItem->data(Qt::DisplayRole);
                QVariant speedV = speedItem->data(Qt::DisplayRole);
                QVariant headingV = headingItem->data(Qt::DisplayRole);
                QString name = nameItem->text();
                QString callsign = callsignItem->text();
                QString destination = destinationItem->text();
                QString shipType = shipTypeItem->text();
                QString status = statusItem->text();
                if (!name.isEmpty()) {
                    text.append(QString("Name: %1").arg(name));
                }
                if (!callsign.isEmpty()) {
                    text.append(QString("Callsign: %1").arg(callsign));
                }
                if (!destination.isEmpty()) {
                    text.append(QString("Destination: %1").arg(destination));
                }
                if (!courseV.isNull())
                {
                    float course = courseV.toFloat();
                    text.append(QString("Course: %1%2").arg(course).arg(QChar(0xb0)));
                    swgMapItem->setImageRotation(course);
                }
                if (!speedV.isNull()) {
                    text.append(QString("Speed: %1 knts").arg(speedV.toFloat()));
                }
                if (!headingV.isNull())
                {
                    float heading = headingV.toFloat();
                    text.append(QString("Heading: %1%2").arg(heading).arg(QChar(0xb0)));
                    swgMapItem->setImageRotation(heading); // heading takes precedence over course
                }
                if (!shipType.isEmpty()) {
                    text.append(QString("Ship type: %1").arg(shipType));
                }
                if (!status.isEmpty()) {
                    text.append(QString("Status: %1").arg(status));
                }
                swgMapItem->setText(new QString(text.join("\n")));

                MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_ais, swgMapItem);
                (*it)->push(msg);
            }
        }
    }
}

void AISGUI::on_vessels_cellDoubleClicked(int row, int column)
{
    if (column == VESSEL_COL_MMSI)
    {
        // Get MMSI of vessel in row double clicked
        QString mmsi = ui->vessels->item(row, VESSEL_COL_MMSI)->text();
        // Search for MMSI on www.vesselfinder.com
        QDesktopServices::openUrl(QUrl(QString("https://www.vesselfinder.com/vessels?name=%1").arg(mmsi)));
    }
    else if ((column == VESSEL_COL_LATITUDE) || (column == VESSEL_COL_LONGITUDE))
    {
        // Get MMSI of vessel in row double clicked
        QString mmsi = ui->vessels->item(row, VESSEL_COL_MMSI)->text();
        // Find MMSI on Map
        FeatureWebAPIUtils::mapFind(mmsi);
    }
    else if (column == VESSEL_COL_IMO)
    {
        QString imo = ui->vessels->item(row, VESSEL_COL_IMO)->text();
        if (!imo.isEmpty() && (imo != "0"))
        {
            // Search for IMO on www.vesselfinder.com
            QDesktopServices::openUrl(QUrl(QString("https://www.vesselfinder.com/vessels?name=%1").arg(imo)));
        }
    }
    else if (column == VESSEL_COL_NAME)
    {
        QString name = ui->vessels->item(row, VESSEL_COL_NAME)->text();
        if (!name.isEmpty())
        {
            // Search for name on www.vesselfinder.com
            QDesktopServices::openUrl(QUrl(QString("https://www.vesselfinder.com/vessels?name=%1").arg(name)));
        }
    }
    else if (column == VESSEL_COL_DESTINATION)
    {
        QString destination = ui->vessels->item(row, VESSEL_COL_DESTINATION)->text();
        if (!destination.isEmpty())
        {
            // Find destination on Map
            FeatureWebAPIUtils::mapFind(destination);
        }
    }
}
