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

#include <QDesktopServices>
#include <QAction>
#include <QClipboard>

#include "feature/featureuiset.h"
#include "feature/featurewebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/datetimedelegate.h"
#include "gui/decimaldelegate.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"

#include "ui_radiosondegui.h"
#include "radiosonde.h"
#include "radiosondegui.h"

#include "SWGMapItem.h"

RadiosondeGUI* RadiosondeGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    RadiosondeGUI* gui = new RadiosondeGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void RadiosondeGUI::destroy()
{
    delete this;
}

void RadiosondeGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray RadiosondeGUI::serialize() const
{
    return m_settings.serialize();
}

bool RadiosondeGUI::deserialize(const QByteArray& data)
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

bool RadiosondeGUI::handleMessage(const Message& message)
{
    if (Radiosonde::MsgConfigureRadiosonde::match(message))
    {
        qDebug("RadiosondeGUI::handleMessage: Radiosonde::MsgConfigureRadiosonde");
        const Radiosonde::MsgConfigureRadiosonde& cfg = (Radiosonde::MsgConfigureRadiosonde&) message;

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
        RS41Frame *frame = RS41Frame::decode(report.getPacket());
        // Update table
        updateRadiosondes(frame, report.getDateTime());
    }

    return false;
}

void RadiosondeGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void RadiosondeGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    RollupContents *rollupContents = getRollupContents();
    rollupContents->saveState(m_rollupState);
    applySettings();
}

RadiosondeGUI::RadiosondeGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::RadiosondeGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_lastFeatureState(0)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/radiosonde/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_radiosonde = reinterpret_cast<Radiosonde*>(feature);
    m_radiosonde->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Intialise chart
    ui->chart->setRenderHint(QPainter::Antialiasing);

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->radiosondes->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->radiosondes->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    radiosondesMenu = new QMenu(ui->radiosondes);
    for (int i = 0; i < ui->radiosondes->horizontalHeader()->count(); i++)
    {
        QString text = ui->radiosondes->horizontalHeaderItem(i)->text();
        radiosondesMenu->addAction(createCheckableItem(text, i, true, SLOT(radiosondesColumnSelectMenuChecked())));
    }
    ui->radiosondes->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->radiosondes->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(radiosondesColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->radiosondes->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(radiosondes_sectionMoved(int, int, int)));
    connect(ui->radiosondes->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(radiosondes_sectionResized(int, int, int)));
    // Context menu
    ui->radiosondes->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->radiosondes, SIGNAL(customContextMenuRequested(QPoint)), SLOT(radiosondes_customContextMenuRequested(QPoint)));

    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_LATITUDE, new DecimalDelegate(5));
    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_LONGITUDE, new DecimalDelegate(5));
    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_ALTITUDE, new DecimalDelegate(1));
    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_SPEED, new DecimalDelegate(1));
    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_VERTICAL_RATE, new DecimalDelegate(1));
    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_HEADING, new DecimalDelegate(1));
    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_ALT_MAX, new DecimalDelegate(1));
    ui->radiosondes->setItemDelegateForColumn(RADIOSONDE_COL_LAST_UPDATE, new DateTimeDelegate());

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
    applySettings(true);
    makeUIConnections();

    plotChart();
}

RadiosondeGUI::~RadiosondeGUI()
{
    qDeleteAll(m_radiosondes);
    delete ui;
}

void RadiosondeGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
    m_settingsKeys.append("workspaceIndex");
}

void RadiosondeGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RadiosondeGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);

    // Order and size columns
    QHeaderView *header = ui->radiosondes->horizontalHeader();
    for (int i = 0; i < RADIOSONDES_COLUMNS; i++)
    {
        bool hidden = m_settings.m_radiosondesColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        radiosondesMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_radiosondesColumnSizes[i] > 0) {
            ui->radiosondes->setColumnWidth(i, m_settings.m_radiosondesColumnSizes[i]);
        }
        header->moveSection(header->visualIndex(i), m_settings.m_radiosondesColumnIndexes[i]);
    }

    ui->y1->setCurrentIndex((int)m_settings.m_y1);
    ui->y2->setCurrentIndex((int)m_settings.m_y2);

    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    getRollupContents()->arrangeRollups();
}

void RadiosondeGUI::onMenuDialogCalled(const QPoint &p)
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

void RadiosondeGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        Radiosonde::MsgConfigureRadiosonde* message = Radiosonde::MsgConfigureRadiosonde::create(m_settings, m_settingsKeys, force);
        m_radiosonde->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

void RadiosondeGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->radiosondes->rowCount();
    ui->radiosondes->setRowCount(row + 1);
    ui->radiosondes->setItem(row, RADIOSONDE_COL_SERIAL, new QTableWidgetItem("123456789"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_TYPE, new QTableWidgetItem("RS41-SGP"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_LATITUDE, new QTableWidgetItem("90.000000-"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_LONGITUDE, new QTableWidgetItem("180.00000-"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_ALTITUDE, new QTableWidgetItem("10000"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_SPEED, new QTableWidgetItem("120"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_VERTICAL_RATE, new QTableWidgetItem("120"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_HEADING, new QTableWidgetItem("360"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_STATUS, new QTableWidgetItem("Ascent"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_PRESSURE, new QTableWidgetItem("1234"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_TEMPERATURE, new QTableWidgetItem("-50.0U"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_HUMIDITY, new QTableWidgetItem("100.0"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_ALT_MAX, new QTableWidgetItem("10000"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_FREQUENCY, new QTableWidgetItem("400.000"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_BURSTKILL_STATUS, new QTableWidgetItem("0"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_BURSTKILL_TIMER, new QTableWidgetItem("00:00"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_LAST_UPDATE, new QTableWidgetItem("2022/12/12 12:00:00"));
    ui->radiosondes->setItem(row, RADIOSONDE_COL_MESSAGES, new QTableWidgetItem("1000"));
    ui->radiosondes->resizeColumnsToContents();
    ui->radiosondes->removeRow(row);
}

// Columns in table reordered
void RadiosondeGUI::radiosondes_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_radiosondesColumnIndexes[logicalIndex] = newVisualIndex;
    m_settingsKeys.append("radiosondesColumnIndexes");
}

// Column in table resized (when hidden size is 0)
void RadiosondeGUI::radiosondes_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_radiosondesColumnSizes[logicalIndex] = newSize;
    m_settingsKeys.append("radiosondesColumnSizes");
}

// Right click in table header - show column select menu
void RadiosondeGUI::radiosondesColumnSelectMenu(QPoint pos)
{
    radiosondesMenu->popup(ui->radiosondes->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void RadiosondeGUI::radiosondesColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->radiosondes->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *RadiosondeGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

// Send to Map feature
void RadiosondeGUI::sendToMap(const QString &name, const QString &label,
    const QString &image, const QString &text,
    const QString &model, float labelOffset,
    float latitude, float longitude, float altitude, QDateTime positionDateTime,
    float heading
    )
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_radiosonde, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(latitude);
        swgMapItem->setLongitude(longitude);
        swgMapItem->setAltitude(altitude);
        swgMapItem->setAltitudeReference(0); // ABSOLUTE

        if (positionDateTime.isValid()) {
            swgMapItem->setPositionDateTime(new QString(positionDateTime.toString(Qt::ISODateWithMs)));
        }

        swgMapItem->setImageRotation(heading);
        swgMapItem->setText(new QString(text));

        if (image.isEmpty()) {
            swgMapItem->setImage(new QString(""));
        } else {
            swgMapItem->setImage(new QString(QString("qrc:///radiosonde/map/%1").arg(image)));
        }

        swgMapItem->setModel(new QString(model));
        swgMapItem->setModelAltitudeOffset(0.0f);
        swgMapItem->setLabel(new QString(label));
        swgMapItem->setLabelAltitudeOffset(labelOffset);
        swgMapItem->setFixedPosition(false);
        swgMapItem->setOrientation(1);
        swgMapItem->setHeading(heading);
        swgMapItem->setPitch(0.0);
        swgMapItem->setRoll(0.0);

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_radiosonde, swgMapItem);
        messageQueue->push(msg);
    }
}

// Update table with received message
void RadiosondeGUI::updateRadiosondes(RS41Frame *message, QDateTime dateTime)
{
    QTableWidgetItem *serialItem;
    QTableWidgetItem *typeItem;
    QTableWidgetItem *latitudeItem;
    QTableWidgetItem *longitudeItem;
    QTableWidgetItem *alitutudeItem;
    QTableWidgetItem *speedItem;
    QTableWidgetItem *verticalRateItem;
    QTableWidgetItem *headingItem;
    QTableWidgetItem *statusItem;
    QTableWidgetItem *pressureItem;
    QTableWidgetItem *temperatureItem;
    QTableWidgetItem *humidityItem;
    QTableWidgetItem *altMaxItem;
    QTableWidgetItem *frequencyItem;
    QTableWidgetItem *burstKillStatusItem;
    QTableWidgetItem *burstKillTimerItem;
    QTableWidgetItem *lastUpdateItem;
    QTableWidgetItem *messagesItem;

    if (!message->m_statusValid)
    {
        // Nothing to display if no serial number
        return;
    }

    RadiosondeData *radiosonde;

    // See if radiosonde is already in table
    QString messageSerial = message->m_serial;
    bool found = false;
    for (int row = 0; row < ui->radiosondes->rowCount(); row++)
    {
        QString itemSerial = ui->radiosondes->item(row, RADIOSONDE_COL_SERIAL)->text();
        if (messageSerial == itemSerial)
        {
            // Update existing item
            serialItem = ui->radiosondes->item(row, RADIOSONDE_COL_SERIAL);
            typeItem = ui->radiosondes->item(row, RADIOSONDE_COL_TYPE);
            latitudeItem = ui->radiosondes->item(row, RADIOSONDE_COL_LATITUDE);
            longitudeItem = ui->radiosondes->item(row, RADIOSONDE_COL_LONGITUDE);
            alitutudeItem = ui->radiosondes->item(row, RADIOSONDE_COL_ALTITUDE);
            speedItem = ui->radiosondes->item(row, RADIOSONDE_COL_SPEED);
            verticalRateItem = ui->radiosondes->item(row, RADIOSONDE_COL_VERTICAL_RATE);
            headingItem = ui->radiosondes->item(row, RADIOSONDE_COL_HEADING);
            statusItem = ui->radiosondes->item(row, RADIOSONDE_COL_STATUS);
            pressureItem = ui->radiosondes->item(row, RADIOSONDE_COL_PRESSURE);
            temperatureItem = ui->radiosondes->item(row, RADIOSONDE_COL_TEMPERATURE);
            humidityItem = ui->radiosondes->item(row, RADIOSONDE_COL_HUMIDITY);
            altMaxItem = ui->radiosondes->item(row, RADIOSONDE_COL_ALT_MAX);
            frequencyItem = ui->radiosondes->item(row, RADIOSONDE_COL_FREQUENCY);
            burstKillStatusItem = ui->radiosondes->item(row, RADIOSONDE_COL_BURSTKILL_STATUS);
            burstKillTimerItem = ui->radiosondes->item(row, RADIOSONDE_COL_BURSTKILL_TIMER);
            lastUpdateItem = ui->radiosondes->item(row, RADIOSONDE_COL_LAST_UPDATE);
            messagesItem = ui->radiosondes->item(row, RADIOSONDE_COL_MESSAGES);
            radiosonde = m_radiosondes.value(messageSerial);
            found = true;
            break;
        }
    }
    if (!found)
    {
        // Add new radiosonde
        ui->radiosondes->setSortingEnabled(false);
        int row = ui->radiosondes->rowCount();
        ui->radiosondes->setRowCount(row + 1);

        serialItem = new QTableWidgetItem();
        typeItem = new QTableWidgetItem();
        latitudeItem = new QTableWidgetItem();
        longitudeItem = new QTableWidgetItem();
        alitutudeItem = new QTableWidgetItem();
        speedItem = new QTableWidgetItem();
        verticalRateItem = new QTableWidgetItem();
        headingItem = new QTableWidgetItem();
        statusItem = new QTableWidgetItem();
        temperatureItem = new QTableWidgetItem();
        humidityItem = new QTableWidgetItem();
        pressureItem = new QTableWidgetItem();
        altMaxItem = new QTableWidgetItem();
        frequencyItem = new QTableWidgetItem();
        burstKillStatusItem = new QTableWidgetItem();
        burstKillTimerItem = new QTableWidgetItem();
        lastUpdateItem = new QTableWidgetItem();
        messagesItem = new QTableWidgetItem();
        ui->radiosondes->setItem(row, RADIOSONDE_COL_SERIAL, serialItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_TYPE, typeItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_LATITUDE, latitudeItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_LONGITUDE, longitudeItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_ALTITUDE, alitutudeItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_SPEED, speedItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_VERTICAL_RATE, verticalRateItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_HEADING, headingItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_STATUS, statusItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_PRESSURE, pressureItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_TEMPERATURE, temperatureItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_HUMIDITY, humidityItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_ALT_MAX, altMaxItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_FREQUENCY, frequencyItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_BURSTKILL_STATUS, burstKillStatusItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_BURSTKILL_TIMER, burstKillTimerItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_LAST_UPDATE, lastUpdateItem);
        ui->radiosondes->setItem(row, RADIOSONDE_COL_MESSAGES, messagesItem);
        messagesItem->setData(Qt::DisplayRole, 0);

        radiosonde = new RadiosondeData();
        m_radiosondes.insert(messageSerial, radiosonde);
    }

    serialItem->setText(message->m_serial);
    lastUpdateItem->setData(Qt::DisplayRole, dateTime);
    messagesItem->setData(Qt::DisplayRole, messagesItem->data(Qt::DisplayRole).toInt() + 1);

    if (message->m_posValid)
    {
        latitudeItem->setData(Qt::DisplayRole, message->m_latitude);
        longitudeItem->setData(Qt::DisplayRole, message->m_longitude);
        alitutudeItem->setData(Qt::DisplayRole, message->m_height);
        float altMax = altMaxItem->data(Qt::DisplayRole).toFloat();
        if (message->m_height > altMax) {
            altMaxItem->setData(Qt::DisplayRole, message->m_height);
        }
        speedItem->setData(Qt::DisplayRole, Units::kmpsToKPH(message->m_speed/1000.0));
        verticalRateItem->setData(Qt::DisplayRole, message->m_verticalRate);
        headingItem->setData(Qt::DisplayRole, message->m_heading);
    }
    statusItem->setText(message->m_flightPhase);

    radiosonde->m_subframe.update(message);

    if (message->m_measValid)
    {
        pressureItem->setData(Qt::DisplayRole, message->getPressureString(&radiosonde->m_subframe));
        temperatureItem->setData(Qt::DisplayRole, message->getTemperatureString(&radiosonde->m_subframe));
        humidityItem->setData(Qt::DisplayRole,  message->getHumidityString(&radiosonde->m_subframe));
    }

    if (message->m_measValid && message->m_posValid) {
        radiosonde->addMessage(dateTime, message);
    }

    typeItem->setText(radiosonde->m_subframe.getType());
    frequencyItem->setText(radiosonde->m_subframe.getFrequencyMHz());
    burstKillStatusItem->setText(radiosonde->m_subframe.getBurstKillStatus());
    burstKillTimerItem->setText(radiosonde->m_subframe.getBurstKillTimer());

    ui->radiosondes->setSortingEnabled(true);

    if (message->m_posValid)
    {
        // Text to display in info box
        QStringList text;
        QString type = typeItem->text();
        QVariant altitudeV = alitutudeItem->data(Qt::DisplayRole);
        QVariant speedV = speedItem->data(Qt::DisplayRole);
        QVariant verticalRateV = verticalRateItem->data(Qt::DisplayRole);
        QVariant headingV = headingItem->data(Qt::DisplayRole);
        QString pressure = pressureItem->text();
        QString temperature = temperatureItem->text();
        QString humidity = humidityItem->text();
        QString status = statusItem->text();
        QString frequency = frequencyItem->text();
        text.append(QString("Serial: %1").arg(serialItem->text()));
        if (!type.isEmpty()) {
            text.append(QString("Type: %1").arg(type));
        }
        if (!altitudeV.isNull()) {
            text.append(QString("Altitude: %1m").arg(altitudeV.toDouble(), 0, 'f', 1));
        }
        if (!speedV.isNull()) {
            text.append(QString("Speed: %1km/h").arg(speedV.toDouble(), 0, 'f', 1));
        }
        if (!verticalRateV.isNull()) {
            text.append(QString("Vertical rate: %1m/s").arg(verticalRateV.toDouble(), 0, 'f', 1));
        }
        if (!headingV.isNull()) {
            text.append(QString("Heading: %1%2").arg(headingV.toDouble(), 0, 'f', 1).arg(QChar(0xb0)));
        }
        if (!status.isEmpty()) {
            text.append(QString("Status: %1").arg(status));
        }
        if (!pressure.isEmpty()) {
            text.append(QString("Pressure: %1hPa").arg(pressure));
        }
        if (!temperature.isEmpty()) {
            text.append(QString("Temperature: %1C").arg(temperature));
        }
        if (!humidity.isEmpty()) {
            text.append(QString("Humidity: %1%").arg(humidity));
        }
        if (!frequency.isEmpty()) {
            text.append(QString("Frequency: %1MHz").arg(frequency));
        }

        QString image = message->m_flightPhase == "Descent" ? "parachute.png" : "ballon.png";
        QString model = message->m_flightPhase == "Descent" ? "radiosondeparachute.glb" : "radiosondeballon.glb";
        // Send to map feature
        sendToMap(serialItem->text(), serialItem->text(),
            image, text.join("<br>"),
            model, 0.0,
            message->m_latitude, message->m_longitude, message->m_height, dateTime,
            0.0f);
    }

    // If this is the first row in the table, select it, so that the chart is plotted
    QList<QTableWidgetItem *> selectedItems = ui->radiosondes->selectedItems();
    if (selectedItems.size() == 0)
    {
        QTableWidgetSelectionRange r(0, 0, 0, RADIOSONDES_COLUMNS);
        ui->radiosondes->setRangeSelected(r, true);
    }

    plotChart();
}

void RadiosondeGUI::on_radiosondes_itemSelectionChanged()
{
    plotChart();
}

void RadiosondeGUI::on_radiosondes_cellDoubleClicked(int row, int column)
{
    if (column == RADIOSONDE_COL_SERIAL)
    {
        // Get serial of Radiosonde in row double clicked
        QString serial = ui->radiosondes->item(row, RADIOSONDE_COL_SERIAL)->text();
        // Search for MMSI on www.radiosondefinder.com
        QDesktopServices::openUrl(QUrl(QString("https://sondehub.org/?f=%1#!mt=Mapnik&f=%1&q=%1").arg(serial)));
    }
    else if ((column == RADIOSONDE_COL_LATITUDE) || (column == RADIOSONDE_COL_LONGITUDE))
    {
        // Get serial of Radiosonde in row double clicked
        QString serial = ui->radiosondes->item(row, RADIOSONDE_COL_SERIAL)->text();
        // Find serial on Map
        FeatureWebAPIUtils::mapFind(serial);
    }
}

// Table cells context menu
void RadiosondeGUI::radiosondes_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item =  ui->radiosondes->itemAt(pos);
    if (item)
    {
        int row = item->row();
        QString serial = ui->radiosondes->item(row, RADIOSONDE_COL_SERIAL)->text();
        QVariant latitudeV = ui->radiosondes->item(row, RADIOSONDE_COL_LATITUDE)->data(Qt::DisplayRole);
        QVariant longitudeV = ui->radiosondes->item(row, RADIOSONDE_COL_LONGITUDE)->data(Qt::DisplayRole);

        QMenu* tableContextMenu = new QMenu(ui->radiosondes);
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

        // View radiosonde on various websites
        QAction* mmsiRadiosondeHubAction = new QAction(QString("View %1 on sondehub.net...").arg(serial), tableContextMenu);
        connect(mmsiRadiosondeHubAction, &QAction::triggered, this, [serial]()->void {
            QDesktopServices::openUrl(QUrl(QString("https://sondehub.org/?f=%1#!mt=Mapnik&f=%1&q=%1").arg(serial)));
        });
        tableContextMenu->addAction(mmsiRadiosondeHubAction);

        // Find on Map
        tableContextMenu->addSeparator();
        QAction* findMapFeatureAction = new QAction(QString("Find %1 on map").arg(serial), tableContextMenu);
        connect(findMapFeatureAction, &QAction::triggered, this, [serial]()->void {
            FeatureWebAPIUtils::mapFind(serial);
        });
        tableContextMenu->addAction(findMapFeatureAction);

        tableContextMenu->popup(ui->radiosondes->viewport()->mapToGlobal(pos));
    }
}

void RadiosondeGUI::on_y1_currentIndexChanged(int index)
{
    m_settings.m_y1 = (RadiosondeSettings::ChartData)index;
    m_settingsKeys.append("y1");
    applySettings();
    plotChart();
}

void RadiosondeGUI::on_y2_currentIndexChanged(int index)
{
    m_settings.m_y2 = (RadiosondeSettings::ChartData)index;
    m_settingsKeys.append("y2");
    applySettings();
    plotChart();
}

float RadiosondeGUI::getData(RadiosondeSettings::ChartData dataType, RadiosondeData *radiosonde, RS41Frame *message)
{
    float data;
    switch (dataType)
    {
    case RadiosondeSettings::ALTITUDE:
        data = message->m_height;
        break;
    case RadiosondeSettings::TEMPERATURE:
        data = message->getTemperatureFloat(&radiosonde->m_subframe);
        break;
    case RadiosondeSettings::HUMIDITY:
        data = message->getHumidityFloat(&radiosonde->m_subframe);
        break;
    case RadiosondeSettings::PRESSURE:
        data = 0.0f;
        break;
    case RadiosondeSettings::SPEED:
        data = Units::kmpsToKPH(message->m_speed/1000.0);
        break;
    case RadiosondeSettings::VERTICAL_RATE:
        data = message->m_verticalRate;
        break;
    case RadiosondeSettings::HEADING:
        data = message->m_heading;
        break;
    case RadiosondeSettings::BATTERY_VOLTAGE:
        data = message->m_batteryVoltage;
        break;
    default:
        data = 0.0f;
        break;
    }
    return data;
}

static QString getAxisTitle(RadiosondeSettings::ChartData dataType)
{
    switch (dataType)
    {
    case RadiosondeSettings::ALTITUDE:
        return "Altitude (m)";
        break;
    case RadiosondeSettings::TEMPERATURE:
        return QString("Temperature (%1C)").arg(QChar(0xb0));
        break;
    case RadiosondeSettings::HUMIDITY:
        return "Relative humidty (%)";
        break;
    case RadiosondeSettings::PRESSURE:
        return "Pressure (hPa)";
        break;
    case RadiosondeSettings::SPEED:
        return "Speed (km/h)";
        break;
    case RadiosondeSettings::VERTICAL_RATE:
        return "Vertical rate (m/s)";
        break;
    case RadiosondeSettings::HEADING:
        return QString("Heading (%1)").arg(QChar(0xb0));
        break;
    case RadiosondeSettings::BATTERY_VOLTAGE:
        return "Battery Voltage (V)";
        break;
    default:
        return "";
    }
}

void RadiosondeGUI::plotChart()
{
    QChart *oldChart = ui->chart->chart();
    QChart *m_chart;

    m_chart = new QChart();
    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setMargins(QMargins(1, 1, 1, 1));
    m_chart->setTheme(QChart::ChartThemeDark);

    // Get selected radiosonde
    QList<QTableWidgetItem *> selectedItems = ui->radiosondes->selectedItems();
    if (selectedItems.size() >= 1)
    {
        int row = selectedItems[0]->row();
        QString serial = ui->radiosondes->item(row, RADIOSONDE_COL_SERIAL)->text();
        RadiosondeData *radiosonde = m_radiosondes.value(serial);
        if (radiosonde)
        {
            // Plot selected data
            QDateTimeAxis *m_chartXAxis;
            QValueAxis *m_chartY1Axis;
            QValueAxis *m_chartY2Axis;

            m_chartXAxis = new QDateTimeAxis();
            if (m_settings.m_y1 != RadiosondeSettings::NONE) {
                m_chartY1Axis = new QValueAxis();
            }
            if (m_settings.m_y2 != RadiosondeSettings::NONE) {
                m_chartY2Axis = new QValueAxis();
            }

            m_chart->legend()->hide();
            m_chart->addAxis(m_chartXAxis, Qt::AlignBottom);

            QLineSeries *y1Series = new QLineSeries();
            QLineSeries *y2Series = new QLineSeries();

            int idx = 0;
            for (auto message : radiosonde->m_messages)
            {
                float y1, y2;
                if (m_settings.m_y1 != RadiosondeSettings::NONE)
                {
                    y1 = getData(m_settings.m_y1, radiosonde, message);
                    y1Series->append(radiosonde->m_messagesDateTime[idx].toMSecsSinceEpoch(), y1);
                }
                if (m_settings.m_y2 != RadiosondeSettings::NONE)
                {
                    y2 = getData(m_settings.m_y2, radiosonde, message);
                    y2Series->append(radiosonde->m_messagesDateTime[idx].toMSecsSinceEpoch(), y2);
                }
                idx++;
            }
            if (m_settings.m_y1 != RadiosondeSettings::NONE)
            {
                m_chart->addSeries(y1Series);
                m_chart->addAxis(m_chartY1Axis, Qt::AlignLeft);
                y1Series->attachAxis(m_chartXAxis);
                y1Series->attachAxis(m_chartY1Axis);
                m_chartY1Axis->setTitleText(getAxisTitle(m_settings.m_y1));
            }
            if (m_settings.m_y2 != RadiosondeSettings::NONE)
            {
                m_chart->addSeries(y2Series);
                m_chart->addAxis(m_chartY2Axis, Qt::AlignRight);
                y2Series->attachAxis(m_chartXAxis);
                y2Series->attachAxis(m_chartY2Axis);
                m_chartY2Axis->setTitleText(getAxisTitle(m_settings.m_y2));
            }
        }
    }
    ui->chart->setChart(m_chart);
    delete oldChart;
}

void RadiosondeGUI::on_deleteAll_clicked()
{
    for (int row = ui->radiosondes->rowCount() - 1; row >= 0; row--)
    {
        QString serial = ui->radiosondes->item(row, RADIOSONDE_COL_SERIAL)->text();
        // Remove from map
        sendToMap(serial, "",
            "", "",
            "", 0.0f,
            0.0f, 0.0f, 0.0f, QDateTime(),
            0.0f);
        // Remove from table
        ui->radiosondes->removeRow(row);
        // Remove from hash
        m_radiosondes.remove(serial);
    }
}

void RadiosondeGUI::makeUIConnections()
{
    QObject::connect(ui->radiosondes, &QTableWidget::itemSelectionChanged, this, &RadiosondeGUI::on_radiosondes_itemSelectionChanged);
    QObject::connect(ui->radiosondes, &QTableWidget::cellDoubleClicked, this, &RadiosondeGUI::on_radiosondes_cellDoubleClicked);
    QObject::connect(ui->y1, qOverload<int>(&QComboBox::currentIndexChanged), this, &RadiosondeGUI::on_y1_currentIndexChanged);
    QObject::connect(ui->y2, qOverload<int>(&QComboBox::currentIndexChanged), this, &RadiosondeGUI::on_y2_currentIndexChanged);
    QObject::connect(ui->deleteAll, &QPushButton::clicked, this, &RadiosondeGUI::on_deleteAll_clicked);
}
