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
#include <QClipboard>

#include "feature/featureuiset.h"
#include "feature/featurewebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/tabletapandhold.h"
#include "gui/dialogpositioner.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"

#include "ui_aisgui.h"
#include "ais.h"
#include "aisgui.h"

#include "SWGMapItem.h"

// Models to use for ships when type is unknown
// Use as many as possibly, so it doesn't look too samey, but don't use
// the massive ships
QStringList AISGUI::m_shipModels = {
    "ship_27m.glbe", "ship_65m.glbe",
    "tug_20m.glbe", "tug_30m_1.glbe", "tug_30m_2.glbe", "tug_30m_3.glbe",
    "cargo_75m.glbe", "tanker_50m.glbe", "dredger_53m.glbe",
    "trawler_22m.glbe",
    "speedboat_8m.glbe", "yacht_10m.glbe", "yacht_20m.glbe", "yacht_42m.glbe"
};

QStringList AISGUI::m_sailboatModels = {
    "sailboat_8m.glbe", "sailboat_17m.glbe"
};

QHash<QString, float> AISGUI::m_labelOffset = {
    {"helicopter.glb", 4.0f},
    {"antenna.glb", 4.5f},
    {"buoy.glb", 1.5f},
    {"ship_27m.glbe", 13.0f},
    {"dredger_53m.glbe", 19.0f},
    {"ship_65m.glbe", 26.0f},
    {"tug_20m.glbe", 10.0f},
    {"tug_30m_1.glbe", 17.0f},
    {"tug_30m_2.glbe", 17.0f},
    {"tug_30m_3.glbe", 17.0f},
    {"coastguard.glbe", 4.0},
    {"cargo_75m.glbe", 22.0f},
    {"cargo_190m.glbe", 42.0f},
    {"cargo_230m.glbe", 42.0f},
    {"tanker_50m.glbe", 12.0f},
    {"tanker_180m.glbe", 35.0f},
    {"tanker_245m_1.glbe", 30.0f},
    {"tanker_380m_1.glbe", 42.0f},
    {"passenger_100m.glbe", 34.0f},
    {"dredger_53m.glbe", 19.0f},
    {"trawler_22m.glbe", 15.0f},
    {"sailboat_8m.glbe", 11.0f},
    {"sailboat_17m.glbe", 24.0f},
    {"speedboat_8m.glbe", 3.0f},
    {"yacht_10m.glbe", 3.0f},
    {"yacht_20m.glbe", 7.5f},
    {"yacht_42m.glbe", 10.0f},
};

QHash<QString, float> AISGUI::m_modelOffset = {
    {"helicopter.glb", 4.0f},
};

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

bool AISGUI::handleMessage(const Message& message)
{
    if (AIS::MsgConfigureAIS::match(message))
    {
        qDebug("AISGUI::handleMessage: AIS::MsgConfigureAIS");
        const AIS::MsgConfigureAIS& cfg = (AIS::MsgConfigureAIS&) message;

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
    else if (MainCore::MsgPacket::match(message))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) message;

        // Decode the message
        AISMessage *ais = AISMessage::decode(report.getPacket());
        // Update table
        updateVessels(ais, report.getDateTime());
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

    RollupContents *rollupContents = getRollupContents();

    rollupContents->saveState(m_rollupState);
    applySettings();
}

AISGUI::AISGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::AISGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/ais/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_ais = reinterpret_cast<AIS*>(feature);
    m_ais->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Timer to remove vessels we haven't heard from in a while
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(removeOldVessels()));
    m_timer.start(60*1000);

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
    // Context menu
    ui->vessels->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->vessels, SIGNAL(customContextMenuRequested(QPoint)), SLOT(vessels_customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->vessels);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &AISGUI::vessels_customContextMenuRequested);

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    applySettings(true);
}

AISGUI::~AISGUI()
{
    qDeleteAll(m_vessels);
    delete ui;
}

void AISGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void AISGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AISGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
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

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    getRollupContents()->arrangeRollups();
}

void AISGUI::onMenuDialogCalled(const QPoint &p)
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

void AISGUI::removeOldVessels()
{
    // Remove if we haven't received a message in 10 minutes
    QDateTime currentDateTime = QDateTime::currentDateTime();
    for (int row = ui->vessels->rowCount() - 1; row >= 0; row--)
    {
        QDateTime lastDateTime = ui->vessels->item(row, VESSEL_COL_LAST_UPDATE)->data(Qt::DisplayRole).toDateTime();
        if (lastDateTime.isValid())
        {
            qint64 diff = lastDateTime.secsTo(currentDateTime);
            if (diff > 10*60)
            {
                QString mmsi = ui->vessels->item(row, VESSEL_COL_MMSI)->text();
                // Remove from map
                sendToMap(mmsi, "",
                    "", "",
                    "", 0.0f, 0.0f,
                    0.0f, 0.0f, QDateTime(),
                    0.0f);
                // Remove from table
                ui->vessels->removeRow(row);
                // Remove from hash
                m_vessels.remove(mmsi);
            }
        }
    }
}

void AISGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        AIS::MsgConfigureAIS* message = AIS::MsgConfigureAIS::create(m_settings, m_settingsKeys, force);
        m_ais->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
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
    ui->vessels->setItem(row, VESSEL_COL_LENGTH, new QTableWidgetItem("400"));
    ui->vessels->setItem(row, VESSEL_COL_DESTINATION, new QTableWidgetItem("12345678901234567890"));
    ui->vessels->setItem(row, VESSEL_COL_POSITION_UPDATE, new QTableWidgetItem("12/12/2022 12:00"));
    ui->vessels->setItem(row, VESSEL_COL_LAST_UPDATE, new QTableWidgetItem("12/12/2022 12:00"));
    ui->vessels->setItem(row, VESSEL_COL_MESSAGES, new QTableWidgetItem("1000"));
    ui->vessels->resizeColumnsToContents();
    ui->vessels->removeRow(row);
}

// Columns in table reordered
void AISGUI::vessels_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_vesselColumnIndexes[logicalIndex] = newVisualIndex;
    m_settingsKeys.append("vesselColumnIndexes");
    applySettings();
}

// Column in table resized (when hidden size is 0)
void AISGUI::vessels_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_vesselColumnSizes[logicalIndex] = newSize;
    m_settingsKeys.append("vesselColumnSizes");
    applySettings();
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

// Send to Map feature
void AISGUI::sendToMap(const QString &name, const QString &label,
    const QString &image, const QString &text,
    const QString &model, float modelOffset, float labelOffset,
    float latitude, float longitude, QDateTime positionDateTime,
    float heading
    )
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_ais, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(latitude);
        swgMapItem->setLongitude(longitude);
        swgMapItem->setAltitude(0);
        swgMapItem->setAltitudeReference(1); // CLAMP_TO_GROUND

        if (positionDateTime.isValid()) {
            swgMapItem->setPositionDateTime(new QString(positionDateTime.toString(Qt::ISODateWithMs)));
        }

        swgMapItem->setImageRotation(heading);
        swgMapItem->setText(new QString(text));

        if (image.isEmpty()) {
            swgMapItem->setImage(new QString(""));
        } else {
            swgMapItem->setImage(new QString(QString("qrc:///ais/map/%1").arg(image)));
        }

        swgMapItem->setModel(new QString(model));
        swgMapItem->setModelAltitudeOffset(modelOffset);
        swgMapItem->setLabel(new QString(label));
        swgMapItem->setLabelAltitudeOffset(labelOffset);
        swgMapItem->setFixedPosition(false);
        swgMapItem->setOrientation(1);
        swgMapItem->setHeading(heading);
        swgMapItem->setPitch(0.0);
        swgMapItem->setRoll(0.0);

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_ais, swgMapItem);
        messageQueue->push(msg);
    }
}

// Update table with received message
void AISGUI::updateVessels(AISMessage *ais, QDateTime dateTime)
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
    QTableWidgetItem *lengthItem;
    QTableWidgetItem *destinationItem;
    QTableWidgetItem *positionUpdateItem;
    QTableWidgetItem *lastUpdateItem;
    QTableWidgetItem *messagesItem;

    QString previousType;
    QString previousShipType;
    Vessel *vessel;

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
            lengthItem = ui->vessels->item(row, VESSEL_COL_LENGTH);
            destinationItem = ui->vessels->item(row, VESSEL_COL_DESTINATION);
            positionUpdateItem = ui->vessels->item(row, VESSEL_COL_POSITION_UPDATE);
            lastUpdateItem = ui->vessels->item(row, VESSEL_COL_LAST_UPDATE);
            messagesItem = ui->vessels->item(row, VESSEL_COL_MESSAGES);
            vessel = m_vessels.value(messageMMSI);
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
        lengthItem = new QTableWidgetItem();
        destinationItem = new QTableWidgetItem();
        positionUpdateItem = new QTableWidgetItem();
        lastUpdateItem = new QTableWidgetItem();
        messagesItem = new QTableWidgetItem();
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
        ui->vessels->setItem(row, VESSEL_COL_LENGTH, lengthItem);
        ui->vessels->setItem(row, VESSEL_COL_DESTINATION, destinationItem);
        ui->vessels->setItem(row, VESSEL_COL_POSITION_UPDATE, positionUpdateItem);
        ui->vessels->setItem(row, VESSEL_COL_LAST_UPDATE, lastUpdateItem);
        ui->vessels->setItem(row, VESSEL_COL_MESSAGES, messagesItem);
        messagesItem->setData(Qt::DisplayRole, 0);

        vessel = new Vessel();
        m_vessels.insert(messageMMSI, vessel);
    }

    previousType = typeItem->text();
    previousShipType = shipTypeItem->text();

    mmsiItem->setText(QString("%1").arg(ais->m_mmsi, 9, 10, QChar('0')));
    lastUpdateItem->setData(Qt::DisplayRole, dateTime);
    messagesItem->setData(Qt::DisplayRole, messagesItem->data(Qt::DisplayRole).toInt() + 1);

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
             lengthItem->setData(Qt::DisplayRole, vd->m_a + vd->m_b);
             destinationItem->setText(vd->m_destination);
         }
    }
    else
    {
        if (ais->hasPosition())
        {
            latitudeItem->setData(Qt::DisplayRole, ais->getLatitude());
            longitudeItem->setData(Qt::DisplayRole, ais->getLongitude());
            positionUpdateItem->setData(Qt::DisplayRole, dateTime);
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
        // Image and model to use on map
        QString shipType = shipTypeItem->text();
        int length = lengthItem->data(Qt::DisplayRole).toInt();
        QString status = statusItem->text();

        // Only update model if change in type - so we don't keeping picking new
        // random models.
        // Check if image is empty to handle case where ShipStaticData is received
        // before position
        if ((previousType != type) || (previousShipType != shipType) || vessel->m_image.isEmpty()) {
            getImageAndModel(type, shipType, length, status, vessel);
        }

        float labelOffset = m_labelOffset.value(vessel->m_model);
        float modelOffset = 0.0f;
        if (m_modelOffset.contains(vessel->m_model)) {
            modelOffset = m_modelOffset.value(vessel->m_model);
        }

        // Text to display in info box
        QStringList text;
        QVariant courseV = courseItem->data(Qt::DisplayRole);
        QVariant speedV = speedItem->data(Qt::DisplayRole);
        QVariant headingV = headingItem->data(Qt::DisplayRole);
        QString name = nameItem->text();
        QString callsign = callsignItem->text();
        QString destination = destinationItem->text();
        float heading = 0.0f;
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
            heading = course;
        }
        if (!speedV.isNull()) {
            text.append(QString("Speed: %1 knts").arg(speedV.toFloat()));
        }
        if (!headingV.isNull())
        {
            heading = headingV.toFloat();
            text.append(QString("Heading: %1%2").arg(heading).arg(QChar(0xb0)));
        }
        if (!shipType.isEmpty()) {
            text.append(QString("Ship type: %1").arg(shipType));
        }
        if (!status.isEmpty()) {
            text.append(QString("Status: %1").arg(status));
        }
        // Send to map feature
        sendToMap(mmsiItem->text(), callsign,
            vessel->m_image, text.join("<br>"),
            vessel->m_model, modelOffset, labelOffset,
            latitudeV.toFloat(), longitudeV.toFloat(), positionUpdateItem->data(Qt::DisplayRole).toDateTime(),
            heading);
    }
}

void AISGUI::getImageAndModel(const QString &type, const QString &shipType, int length, const QString &status, Vessel *vessel)
{
    if (type == "Aircraft")
    {
        // I presume search and rescue aircraft are more likely to be helicopters
        vessel->m_image = "helicopter.png";
        vessel->m_model = "helicopter.glb";
    }
    else if (type == "Base station")
    {
        vessel->m_image = "anchor.png";
        vessel->m_model = "antenna.glb";
    }
    else if (type == "Aid-to-nav")
    {
        vessel->m_image = "bouy.png";
        vessel->m_model = "buoy.glb";
    }
    else
    {
        vessel->m_image = "ship.png";
        if (status == "Under way sailing") {
            vessel->m_model = m_sailboatModels[m_random.bounded(m_sailboatModels.size())];
        } else {
            vessel->m_model = m_shipModels[m_random.bounded(m_shipModels.size())];
        }

        if (!shipType.isEmpty())
        {
            if (shipType == "Ship")
            {
                if (length < 40)
                {
                    vessel->m_model = "ship_27m.glbe";
                }
                else if (length < 60)
                {
                    vessel->m_model = "dredger_53m.glbe";
                }
                else
                {
                    vessel->m_model = "ship_65m.glbe";
                }
            }
            else if ((shipType == "Tug") || (shipType == "Port tender") || (shipType == "Pilot vessel") || (shipType == "Vessel - Towing"))
            {
                vessel->m_image = "tug.png";
                if (length < 25)
                {
                    vessel->m_model = "tug_20m.glbe";
                }
                else
                {
                    int rand = m_random.bounded(1, 3);
                    vessel->m_model = QString("tug_30m_%1.glbe").arg(rand);
                }
            }
            else if ((shipType == "Law enforcement vessel") || (shipType == "Search and rescue vessel"))
            {
                vessel->m_model = "coastguard.glbe";
            }
            else if (shipType == "Cargo")
            {
                vessel->m_image = "cargo.png";
                if (length < 120)
                {
                    vessel->m_model = "cargo_75m.glbe";
                }
                else if (length < 200)
                {
                    vessel->m_model = "cargo_190m.glbe";
                }
                else
                {
                    vessel->m_model = "cargo_230m.glbe";
                }
            }
            else if (shipType == "Tanker")
            {
                vessel->m_image = "tanker.png";
                if (length < 120)
                {
                    vessel->m_model = "tanker_50m.glbe";
                }
                else if (length < 210)
                {
                    vessel->m_model = "tanker_180m.glbe";
                }
                else if (length < 300)
                {
                    int rand = m_random.bounded(1, 4);
                    vessel->m_model = QString("tanker_245m_%1.glbe").arg(rand);
                }
                else
                {
                    int rand = m_random.bounded(1, 3);
                    vessel->m_model = QString("tanker_380m_%1.glbe").arg(rand);
                }
            }
            else if (shipType == "Passenger")
            {
                vessel->m_model = "passenger_100m.glbe";
            }
            else if (shipType == "Vessel - Dredging or underwater operations")
            {
                vessel->m_model = "dredger_53m.glbe";
            }
            else if (shipType == "Vessel - Fishing")
            {
                vessel->m_model = "trawler_22m.glbe";
            }
            else if (shipType == "Vessel - Sailing")
            {
                if (length < 13)
                {
                    vessel->m_model = "sailboat_8m.glbe";
                }
                else
                {
                    vessel->m_model = "sailboat_17m.glbe";
                }
            }
            else if (shipType.contains("Pleasure craft"))
            {
                if (length < 9)
                {
                    vessel->m_model = "speedboat_8m.glbe";
                }
                else if (length < 18)
                {
                    vessel->m_model = "yacht_10m.glbe";
                }
                else if (length < 32)
                {
                    vessel->m_model = "yacht_20m.glbe";
                }
                else
                {
                    vessel->m_model = "yacht_42m.glbe";
                }
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
    else if ((column == VESSEL_COL_LATITUDE) || (column == VESSEL_COL_LONGITUDE) || (column == VESSEL_COL_SHIP_TYPE))
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

// Table cells context menu
void AISGUI::vessels_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item =  ui->vessels->itemAt(pos);
    if (item)
    {
        int row = item->row();
        QString mmsi = ui->vessels->item(row, VESSEL_COL_MMSI)->text();
        QString imo = ui->vessels->item(row, VESSEL_COL_IMO)->text();
        QString name = ui->vessels->item(row, VESSEL_COL_NAME)->text();
        QVariant latitudeV = ui->vessels->item(row, VESSEL_COL_LATITUDE)->data(Qt::DisplayRole);
        QVariant longitudeV = ui->vessels->item(row, VESSEL_COL_LONGITUDE)->data(Qt::DisplayRole);
        QString destination = ui->vessels->item(row, VESSEL_COL_DESTINATION)->text();

        QMenu* tableContextMenu = new QMenu(ui->vessels);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->addSeparator();

        // View vessel on various websites

        QAction* mmsiAISHubAction = new QAction(QString("View MMSI %1 on aishub.net...").arg(mmsi), tableContextMenu);
        connect(mmsiAISHubAction, &QAction::triggered, this, [mmsi]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://www.aishub.net/vessels?Ship%5Bmmsi%5D=%1&mmsi=%1").arg(mmsi)));
        });
        tableContextMenu->addAction(mmsiAISHubAction);

        QAction* mmsiAction = new QAction(QString("View MMSI %1 on vesselfinder.com...").arg(mmsi), tableContextMenu);
        connect(mmsiAction, &QAction::triggered, this, [mmsi]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://www.vesselfinder.net/vessels?name=%1").arg(mmsi)));
        });
        tableContextMenu->addAction(mmsiAction);

        if (!imo.isEmpty())
        {
            QAction* imoAction = new QAction(QString("View IMO %1 on vesselfinder.net...").arg(imo), tableContextMenu);
            connect(imoAction, &QAction::triggered, this, [imo]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.vesselfinder.net/vessels?name=%1").arg(imo)));
            });
            tableContextMenu->addAction(imoAction);
        }

        if (!name.isEmpty())
        {
            QAction* nameAction = new QAction(QString("View %1 on vesselfinder.net...").arg(name), tableContextMenu);
            connect(nameAction, &QAction::triggered, this, [name]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.vesselfinder.net/vessels?name=%1").arg(name)));
            });
            tableContextMenu->addAction(nameAction);
        }

        // Find on Map
        if (!latitudeV.isNull())
        {
            tableContextMenu->addSeparator();

            QAction* findMapFeatureAction = new QAction(QString("Find MMSI %1 on map").arg(mmsi), tableContextMenu);
            connect(findMapFeatureAction, &QAction::triggered, this, [mmsi]()->void {
                FeatureWebAPIUtils::mapFind(mmsi);
            });
            tableContextMenu->addAction(findMapFeatureAction);
        }

        if (!destination.isEmpty())
        {
            tableContextMenu->addSeparator();

            QAction* findDestinationFeatureAction = new QAction(QString("Find %1 on map").arg(destination), tableContextMenu);
            connect(findDestinationFeatureAction, &QAction::triggered, this, [destination]()->void {
                FeatureWebAPIUtils::mapFind(destination);
            });
            tableContextMenu->addAction(findDestinationFeatureAction);
        }

        tableContextMenu->popup(ui->vessels->viewport()->mapToGlobal(pos));
    }
}
