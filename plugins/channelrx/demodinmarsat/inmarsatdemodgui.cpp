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

#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QRegularExpression>
#include <QClipboard>
#include <QFileDialog>
#include <QScrollBar>

#include "inmarsatdemodgui.h"

#include "ui_inmarsatdemodgui.h"
#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "feature/featurewebapiutils.h"
#include "plugin/pluginapi.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/units.h"
#include "util/osndb.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "dsp/glscopesettings.h"
#include "gui/tabletapandhold.h"
#include "maincore.h"
#include "SWGMapItem.h"

#include "inmarsatdemod.h"

MultipartMessage::MultipartMessage(int id, std::map<std::string, std::string> params, const QDateTime& dateTime) :
    m_id(id),
    m_icon(nullptr),
    m_latitude(0.0f),
    m_longitude(0.0f)
{
    update(params, dateTime);
}

void MultipartMessage::update(std::map<std::string, std::string> params, const QDateTime& dateTime)
{
    m_dateTime = dateTime;
    m_service = QString::fromStdString(params["serviceCodeAndAddressName"]);
    m_priority = QString::fromStdString(params["priorityText"]);
    m_address = decodeAddress(
        QString::fromStdString(params["messageType"]),
        QString::fromStdString(params["addressHex"]),
        &m_latitude,
        &m_longitude,
        &m_addressCoordinates,
        &m_icon
    );
}

void MultipartMessage::addPart(const MessagePart& part)
{
    int i;
    bool insert = false;

    for (i = 0; i < m_parts.size(); i++)
    {
        if ((m_parts[i].m_packet == part.m_packet) && (m_parts[i].m_part == part.m_part))
        {
            m_parts[i] = part;
            break;
        }
        else if ((m_parts[i].m_packet == part.m_packet) && (m_parts[i].m_part > part.m_part))
        {
            insert = true;
            break;
        }
        else if (m_parts[i].m_packet > part.m_packet)
        {
            insert = true;
            break;
        }
    }
    if (insert) {
        m_parts.insert(i, part);
    } else if (i == m_parts.size()) {
        m_parts.append(part);
    }
    parseMessage();
}

QString MultipartMessage::getMessage() const
{
    QString msg;

    for (const auto& part : m_parts) {
        msg = msg.append(part.m_text);
    }
    return msg;
}

// Cordinates of the form: 61-02.04N 059-32.47W
QRegularExpression MultipartMessage::m_re(QStringLiteral("(\\d+)-(\\d+)(.(\\d+))?([NS]) (\\d+)-(\\d+)(.(\\d+))?([EW])"));

void MultipartMessage::parseMessage()
{
    if (getParts() == getTotalParts())
    {
        QString message = getMessage();
        m_messageCoordinates.clear();

        QRegularExpressionMatchIterator i = m_re.globalMatch(message);
        while (i.hasNext())
        {
            QRegularExpressionMatch match = i.next();
            if (match.hasMatch())
            {
                int latDeg = match.captured(1).toInt();
                int latMin = match.captured(2).toInt();
                int latSec = match.captured(4).toInt();
                bool north = match.captured(5) == "N";
                int lonDeg = match.captured(6).toInt();
                int lonMin = match.captured(7).toInt();
                int lonSec = match.captured(9).toInt();
                bool east = match.captured(10) == "E";

                float latitude = latDeg + latMin/60.0f + latSec/3600.0f;
                if (!north) {
                    latitude = -latitude;
                }
                float longitude = lonDeg + lonMin/60.0f + lonSec/3600.0f;
                if (!east) {
                    longitude = -longitude;
                }

                QGeoCoordinate coord(latitude, longitude);
                m_messageCoordinates.append(coord);
            }
        }
    }
}

void InmarsatDemodGUI::packetsCustomContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->packets->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(ui->packets);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);
        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
            });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->popup(ui->packets->viewport()->mapToGlobal(pos));
    }
}

void InmarsatDemodGUI::messagesCustomContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->messages->itemAt(pos);
    if (item)
    {
        QMenu* tableContextMenu = new QMenu(ui->messages);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
            });
        tableContextMenu->addAction(copyAction);

        QString id = ui->messages->item(item->row(), MESSAGE_COL_ID)->data(Qt::DisplayRole).toString();
        MultipartMessage *message = m_messages[id.toInt()];
        if (message->getCoordinates().size() > 0)
        {
            QAction* findAction = new QAction("Find on map", tableContextMenu);
            connect(findAction, &QAction::triggered, this, [id]()->void {
                FeatureWebAPIUtils::mapFind(id);
                });
            tableContextMenu->addAction(findAction);
        }

        tableContextMenu->popup(ui->messages->viewport()->mapToGlobal(pos));
    }
}

void InmarsatDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->packets->rowCount();
    ui->packets->setRowCount(row + 1);
    ui->packets->setItem(row, PACKET_COL_DATE, new QTableWidgetItem("Frid Apr 15 2016-"));
    ui->packets->setItem(row, PACKET_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->packets->setItem(row, PACKET_COL_SAT, new QTableWidgetItem("Atlantic Ocean"));
    ui->packets->setItem(row, PACKET_COL_LES, new QTableWidgetItem("123456-15-"));
    ui->packets->setItem(row, PACKET_COL_MSG_ID, new QTableWidgetItem("80555301-"));
    ui->packets->setItem(row, PACKET_COL_TYPE, new QTableWidgetItem("Multiframe Packet Start-"));
    ui->packets->setItem(row, PACKET_COL_FRAME_NO, new QTableWidgetItem("123456"));
    ui->packets->setItem(row, PACKET_COL_LCN, new QTableWidgetItem("888"));
    ui->packets->setItem(row, PACKET_COL_ULF, new QTableWidgetItem("15,888.888"));
    ui->packets->setItem(row, PACKET_COL_DLF, new QTableWidgetItem("15,888.888"));
    ui->packets->setItem(row, PACKET_COL_PRIORITY, new QTableWidgetItem("Urgency"));
    ui->packets->setItem(row, PACKET_COL_ADDRESS, new QTableWidgetItem("90S 180W 1000 nm"));
    ui->packets->setItem(row, PACKET_COL_MESSAGE, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->packets->setItem(row, PACKET_COL_DECODE, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->packets->setItem(row, PACKET_COL_DATA_HEX, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->packets->resizeColumnsToContents();
    ui->packets->removeRow(row);

    row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);
    ui->messages->setItem(row, MESSAGE_COL_DATE, new QTableWidgetItem("Frid Apr 15 2016-"));
    ui->messages->setItem(row, MESSAGE_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->messages->setItem(row, MESSAGE_COL_ID, new QTableWidgetItem("Atlantic Ocean"));
    ui->messages->setItem(row, MESSAGE_COL_SERVICE, new QTableWidgetItem("SafetyNET "));
    ui->messages->setItem(row, MESSAGE_COL_PRIORITY, new QTableWidgetItem("Urgency-"));
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS, new QTableWidgetItem("90S 180W 1000 nm"));
    ui->messages->setItem(row, MESSAGE_COL_MESSAGE, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->messages->setItem(row, MESSAGE_COL_PARTS, new QTableWidgetItem("8/88"));
    ui->messages->resizeColumnsToContents();
    ui->messages->removeRow(row);
}

// Columns in table reordered
void InmarsatDemodGUI::packets_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_packetsColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void InmarsatDemodGUI::packets_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_packetsColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void InmarsatDemodGUI::packetsColumnSelectMenu(QPoint pos)
{
    m_packetsMenu->popup(ui->packets->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void InmarsatDemodGUI::packetsColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->packets->setColumnHidden(idx, !action->isChecked());
    }
}

// Columns in table reordered
void InmarsatDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_messagesColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void InmarsatDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_messagesColumnSizes[logicalIndex] = newSize;
}


// Right click in table header - show column select menu
void InmarsatDemodGUI::messagesColumnSelectMenu(QPoint pos)
{
    m_messagesMenu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void InmarsatDemodGUI::messagesColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->messages->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *InmarsatDemodGUI::createCheckableItem(QString &text, int idx, bool checked, bool packets)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    if (packets) {
        connect(action, &QAction::triggered, this, &InmarsatDemodGUI::packetsColumnSelectMenuChecked);
    } else {
        connect(action, &QAction::triggered, this, &InmarsatDemodGUI::messagesColumnSelectMenuChecked);
    }
    return action;
}

InmarsatDemodGUI* InmarsatDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    InmarsatDemodGUI* gui = new InmarsatDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void InmarsatDemodGUI::destroy()
{
    delete this;
}

void InmarsatDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
}

QByteArray InmarsatDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool InmarsatDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applyAllSettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

static QString formatFreqMHz(const QString& freq)
{
    if (freq.isEmpty())
    {
        return "";
    }
    else
    {
        QLocale l;
        l.setNumberOptions(l.numberOptions() & ~QLocale::OmitGroupSeparator);

        double d = freq.toDouble();

        qint64 f = (qint64) (d * 1000000.0);

        int precision;

        if (f % 1000000 == 0) {
            precision = 0;
        } else if (f % 1000 == 0) {
            precision = 3;
        } else {
            precision = 6;
        }

        return l.toString(d, 'f', precision);
    }
}

static QString formatFreqMHz(std::string string)
{
    return formatFreqMHz(QString::fromStdString(string));
}

// https://en.wikipedia.org/wiki/NAVAREA
const QStringList MultipartMessage::m_navAreas = {
    "0",
    "I United Kingdom",
    "II France",
    "III Spain",
    "IV USA",
    "V Brazil",
    "VI Argentina",
    "VII South Africa",
    "VIII India",
    "IX Pakistan",
    "X Australia",
    "XI Japan",
    "XII USA",
    "XIII Russian Federation",
    "XIV New Zealand",
    "XV Chile",
    "XVI Peru",
    "XVII Canada",
    "XVIII Canada",
    "XIX Norway",
    "XX Russian Federation",
    "XXI Russian Federation"
};

// We use flags from ADS-B demod
const QStringList MultipartMessage::m_navAreaFlags = {
    "",
    "united_kingdom",
    "france",
    "spain",
    "united_states",
    "brazil",
    "argentina",
    "south_africa",
    "india",
    "pakistan",
    "australia",
    "japan",
    "united_states",
    "russia",
    "new_zealand",
    "chile",
    "peru",
    "canada",
    "canada",
    "norway",
    "russia",
    "russia"
};

QString MultipartMessage::decodeAddress(QString messageType, QString addressHex, float *latitude, float *longitude, QList<QGeoCoordinate> *coordinates, QIcon **icon)
{
    bool ok;
    int messageTypeNum = messageType.toInt(&ok);

    if (ok)
    {
        switch (messageTypeNum)
        {
        case 0x31:
        {
            // Navarea
            int navArea = addressHex.left(2).toInt(&ok, 16);
            if (ok && (navArea > 0) && (navArea < m_navAreas.size()))
            {
                if (icon) {
                    *icon = AircraftInformation::getFlagIcon(m_navAreaFlags[navArea]);
                }
                return QString("NAVAREA %1").arg(m_navAreas[navArea]);
            }
            else
            {
                return QString("NAVAREA Unknown %1").arg(navArea);
            }
        }
        break;

        case 0x13:
        {
            QByteArray addressBytes = QByteArray::fromHex(addressHex.toLatin1());
            // Navarea - TBD B1/B2
            int navArea = addressBytes[0];
            if ((navArea > 0) && (navArea < m_navAreas.size()))
            {
                if (icon) {
                    *icon = AircraftInformation::getFlagIcon(m_navAreaFlags[navArea]);
                }
                return QString("NAVAREA %1").arg(m_navAreas[navArea]);
            }
            else
            {
                return QString("NAVAREA Unknown %1").arg(navArea);
            }
        }
        break;

        case 0x04:
        case 0x34:
        {
            // Rectangular area
            QByteArray addressBytes = QByteArray::fromHex(addressHex.toLatin1());
            int south = (addressBytes[0] >> 7) & 1;
            int latDeg = addressBytes[0] & 0x7f;
            int lonDeg = (addressBytes[1] & 0xff) << 1;
            int west = (addressBytes[2] >> 7) & 1;
            int latExtentNorth = addressBytes[2] & 0x7f;
            int latExtentEast = addressBytes[3] & 0xff;

            if (coordinates)
            {
                int lat1Deg = west ? -latDeg : latDeg;
                int lon1Deg = south ? -lonDeg : lonDeg;
                int lat2Deg = lat1Deg + latExtentEast;
                int lon2Deg = lon1Deg + latExtentNorth;
                *latitude = lat1Deg + lat2Deg / 2;
                *longitude = lon1Deg + lon2Deg / 2;
                coordinates->clear();
                coordinates->append(QGeoCoordinate(latDeg, lonDeg));
                coordinates->append(QGeoCoordinate(lat2Deg, lonDeg));
                coordinates->append(QGeoCoordinate(lat2Deg, lon2Deg));
                coordinates->append(QGeoCoordinate(latDeg, lon2Deg));
                coordinates->append(QGeoCoordinate(latDeg, lonDeg));
            }

            return QString("%1%2 %3%4 %5%7N %6%7E")
                .arg(south ? "S" : "N")
                .arg(latDeg)
                .arg(west ? "W" : "E")
                .arg(lonDeg)
                .arg(latExtentNorth)
                .arg(latExtentEast)
                .arg(QChar(0x00B0));
        }
        break;

        case 0x14:
        case 0x24:
        case 0x44:
        {
            // Circular area
            QByteArray addressBytes = QByteArray::fromHex(addressHex.toLatin1());
            int south = (addressBytes[0] >> 7) & 1;
            int latDeg = addressBytes[0] & 0x7f;
            int lonDeg = (addressBytes[1] & 0xff) << 1;
            int west = (addressBytes[2] >> 7) & 1;
            int radius = ((addressBytes[2] & 0x7f) << 8) | (addressBytes[3] & 0xff);

            if (coordinates)
            {
                int lat1Deg = west ? -latDeg : latDeg;
                int lon1Deg = south ? -lonDeg : lonDeg;
                QGeoCoordinate centre(lat1Deg, lon1Deg);

                *latitude = lat1Deg;
                *longitude = lon1Deg;
                coordinates->clear();
                for (int theta = 0; theta <= 360; theta += 10) {
                    coordinates->append(centre.atDistanceAndAzimuth(Units::nauticalMilesToMetres(radius), theta));
                }
            }

            return QString("%1%2 %3%4 %5nm")
                .arg(south ? "S" : "N")
                .arg(latDeg)
                .arg(west ? "W" : "E")
                .arg(lonDeg)
                .arg(radius);
        }

        case 0x11:
        {
            // System message address
            QStringList systemAddresses = {
                "All mobiles",
                "AOR (East)",
                "AOR (West)",
                "POR",
                "IOR",
                "Inmarsat-A MES",
                "Inmarsat-C MES",
                "Inmarsat-B MES",
                "Inmarsat-M MES",
                "Inmarsat-B/M MESs",
                "Inmarsat Aero-C AMESs"
            };
            QByteArray addressBytes = QByteArray::fromHex(addressHex.toLatin1());
            int address = addressBytes[0] & 0xff;
            if (address < systemAddresses.size()) {
                return systemAddresses[address];
            } else {
                return "Unknown";
            }
        }
        break;

        case 23:
        case 33:
        {
            // MES Id
            QByteArray addressBytes = QByteArray::fromHex(addressHex.toLatin1());

            unsigned address = ((addressBytes[0] & 0xff) << 16) | ((addressBytes[1] & 0xff) << 8) | (addressBytes[2] & 0xff);

            return QString::number(address);
        }
        break;

        case 73:
        {
            // Fixed area number
            QByteArray addressBytes = QByteArray::fromHex(addressHex.toLatin1());

            unsigned address = ((addressBytes[0] & 0xff) << 16) | ((addressBytes[1] & 0xff) << 8) | (addressBytes[2] & 0xff);

            return QString::number(address);
        }
        break;

        default:
            return addressHex;
        }
    }
    else
    {
        return addressHex;
    }
}

void InmarsatDemodGUI::updateMessageTable(MultipartMessage *message)
{
    QScrollBar *sb = ui->messages->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    // Find if message already in table

    int row;
    bool found = false;

    for (row = 0; row < ui->messages->rowCount(); row++)
    {
        if (ui->messages->item(row, MESSAGE_COL_ID)->data(Qt::DisplayRole) == message->getId())
        {
            found = true;
            break;
        }
    }

    QTableWidgetItem *dateItem;
    QTableWidgetItem *timeItem;
    QTableWidgetItem *serviceItem;
    QTableWidgetItem *priorityItem;
    QTableWidgetItem *addressItem;
    QTableWidgetItem *messageItem;
    QTableWidgetItem *partsItem;

    if (!found)
    {
        ui->messages->setSortingEnabled(false);
        row = ui->messages->rowCount();
        ui->messages->setRowCount(row + 1);

        dateItem = new QTableWidgetItem();
        timeItem = new QTableWidgetItem();
        QTableWidgetItem *idItem = new QTableWidgetItem();
        serviceItem = new QTableWidgetItem();
        priorityItem = new QTableWidgetItem();
        addressItem = new QTableWidgetItem();
        messageItem = new QTableWidgetItem();
        partsItem = new QTableWidgetItem();

        ui->messages->setItem(row, MESSAGE_COL_DATE, dateItem);
        ui->messages->setItem(row, MESSAGE_COL_TIME, timeItem);
        ui->messages->setItem(row, MESSAGE_COL_ID, idItem);
        ui->messages->setItem(row, MESSAGE_COL_SERVICE, serviceItem);
        ui->messages->setItem(row, MESSAGE_COL_PRIORITY, priorityItem);
        ui->messages->setItem(row, MESSAGE_COL_ADDRESS, addressItem);
        ui->messages->setItem(row, MESSAGE_COL_MESSAGE, messageItem);
        ui->messages->setItem(row, MESSAGE_COL_PARTS, partsItem);

        idItem->setData(Qt::DisplayRole, message->getId());
    }
    else
    {
        dateItem = ui->messages->item(row, MESSAGE_COL_DATE);
        timeItem = ui->messages->item(row, MESSAGE_COL_TIME);
        serviceItem = ui->messages->item(row, MESSAGE_COL_SERVICE);
        priorityItem = ui->messages->item(row, MESSAGE_COL_PRIORITY);
        addressItem = ui->messages->item(row, MESSAGE_COL_ADDRESS);
        messageItem = ui->messages->item(row, MESSAGE_COL_MESSAGE);
        partsItem = ui->messages->item(row, MESSAGE_COL_PARTS);
    }

    dateItem->setData(Qt::DisplayRole, message->getDateTime().date());
    timeItem->setData(Qt::DisplayRole, message->getDateTime().time());
    serviceItem->setText(message->getService());
    priorityItem->setText(message->getPriority());
    addressItem->setText(message->getAddress());
    QIcon *icon = message->getAddressIcon();
    if (icon) {
        addressItem->setIcon(*icon);
    }
    messageItem->setText(message->getMessage());
    QString parts = QString("%1 / %2").arg(message->getParts()).arg(message->getTotalParts());
    partsItem->setData(Qt::DisplayRole, parts);

    filterRow(ui->messages, row, -1, MESSAGE_COL_MESSAGE);

    if (!found && !m_loadingData)
    {
        ui->messages->setSortingEnabled(true);
        if (scrollToBottom) {
            ui->messages->scrollToBottom();
        }
    }
}

// Add row to table
void InmarsatDemodGUI::packetReceived(const QByteArray& bytes, QDateTime dateTime)
{
    inmarsatc::decoder::Decoder::decoder_result decoderResult;

    memcpy(&decoderResult, bytes.data(), sizeof(decoderResult));

    std::vector<inmarsatc::frameParser::FrameParser::frameParser_result> parserResults = m_parser.parseFrame(decoderResult);

    for (auto& frame : parserResults)
    {
        if (!(!frame.decoding_result.isDecodedPacket || !frame.decoding_result.isCrc))
        {
            // Is scroll bar at bottom
            QScrollBar *sb = ui->packets->verticalScrollBar();
            bool scrollToBottom = sb->value() == sb->maximum();

            ui->packets->setSortingEnabled(false);
            int row = ui->packets->rowCount();
            ui->packets->setRowCount(row + 1);

            QTableWidgetItem *dateItem = new QTableWidgetItem();
            QTableWidgetItem *timeItem = new QTableWidgetItem();
            QTableWidgetItem *satItem = new QTableWidgetItem();
            QTableWidgetItem *lesItem = new QTableWidgetItem();
            QTableWidgetItem *msgIdItem = new QTableWidgetItem();
            QTableWidgetItem *typeItem = new QTableWidgetItem();
            QTableWidgetItem *frameNoItem = new QTableWidgetItem();
            QTableWidgetItem *lcnItem = new QTableWidgetItem();
            QTableWidgetItem *ulfItem = new QTableWidgetItem();
            QTableWidgetItem *dlfItem = new QTableWidgetItem();
            QTableWidgetItem *priorityItem = new QTableWidgetItem();
            QTableWidgetItem *addressItem = new QTableWidgetItem();
            QTableWidgetItem *messageItem = new QTableWidgetItem();
            QTableWidgetItem *decodeItem = new QTableWidgetItem();
            QTableWidgetItem *dataHexItem = new QTableWidgetItem();
            QLabel *decodeLabel = new QLabel();
            ui->packets->setItem(row, PACKET_COL_DATE, dateItem);
            ui->packets->setItem(row, PACKET_COL_TIME, timeItem);
            ui->packets->setItem(row, PACKET_COL_SAT, satItem);
            ui->packets->setItem(row, PACKET_COL_LES, lesItem);
            ui->packets->setItem(row, PACKET_COL_MSG_ID, msgIdItem);
            ui->packets->setItem(row, PACKET_COL_TYPE, typeItem);
            ui->packets->setItem(row, PACKET_COL_FRAME_NO, frameNoItem);
            ui->packets->setItem(row, PACKET_COL_LCN, lcnItem);
            ui->packets->setItem(row, PACKET_COL_ULF, ulfItem);
            ui->packets->setItem(row, PACKET_COL_DLF, dlfItem);
            ui->packets->setItem(row, PACKET_COL_PRIORITY, priorityItem);
            ui->packets->setItem(row, PACKET_COL_ADDRESS, addressItem);
            ui->packets->setItem(row, PACKET_COL_MESSAGE, messageItem);
            ui->packets->setItem(row, PACKET_COL_DECODE, decodeItem);
            ui->packets->setItem(row, PACKET_COL_DATA_HEX, dataHexItem);
            ui->packets->setCellWidget(row, PACKET_COL_DECODE, decodeLabel);

            dateItem->setText(dateTime.date().toString());
            timeItem->setText(dateTime.time().toString());

            typeItem->setText(QString::fromStdString(frame.decoding_result.packetVars["packetDescriptorText"]));
            frameNoItem->setText(QString::number(frame.decoding_result.frameNumber));

            satItem->setText(QString::fromStdString(frame.decoding_result.packetVars["satName"]));
            QString lesName = QString::fromStdString(frame.decoding_result.packetVars["lesName"]);
            if (!lesName.isEmpty()) {
                lesItem->setText(lesName);
            } else{
                lesItem->setText(QString::fromStdString(frame.decoding_result.packetVars["les"]));
            }
            QString msgId = QString::fromStdString(frame.decoding_result.packetVars["mesId"]);
            if (!msgId.isEmpty()) {
                msgIdItem->setText(msgId);
            } else {
                msgIdItem->setText(QString::fromStdString(frame.decoding_result.packetVars["messageId"]));
            }
            lcnItem->setText(QString::fromStdString(frame.decoding_result.packetVars["logicalChannelNo"]));
            ulfItem->setText(formatFreqMHz(frame.decoding_result.packetVars["uplinkChannelMhz"]));
            dlfItem->setText(formatFreqMHz(frame.decoding_result.packetVars["downlinkChannelMhz"]));
            priorityItem->setText(QString::fromStdString(frame.decoding_result.packetVars["priorityText"]));

            QIcon *icon = nullptr;
            addressItem->setText(MultipartMessage::decodeAddress(
                QString::fromStdString(frame.decoding_result.packetVars["messageType"]),
                QString::fromStdString(frame.decoding_result.packetVars["addressHex"]),
                nullptr,
                nullptr,
                nullptr,
                &icon
            ));
            if (icon) {
                addressItem->setIcon(*icon);
            }

            QString message;
            QString decode;

            switch(frame.decoding_result.packetDescriptor)
            {
            case 0x27:
                decode = decode.append("<tr><th align=left>Msg Id<td> " + frame.decoding_result.packetVars["mesId"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);
                decode = decode.append("<tr><th align=left>LCN<td> " + frame.decoding_result.packetVars["logicalChannelNo"]);
                break;

            case 0x2A:
                decode = decode.append("<tr><th align=left>Msg Id<td> " + frame.decoding_result.packetVars["mesId"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);
                decode = decode.append("<tr><th align=left>LCN<td> " + frame.decoding_result.packetVars["logicalChannelNo"]);
                break;

            case 0x08:
                decode = decode.append("<tr><th align=left>Msg Id<td> " + frame.decoding_result.packetVars["mesId"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LCN<td> " + frame.decoding_result.packetVars["logicalChannelNo"]);
                decode = decode.append("<tr><th align=left>ULF<td> " + formatFreqMHz(frame.decoding_result.packetVars["uplinkChannelMhz"]));
                break;

            case 0x6C:
            {
                decode = decode.append("<tr><th align=left>ULF<td>" + formatFreqMHz(frame.decoding_result.packetVars["uplinkChannelMhz"]));
                decode = decode.append("<tr><th align=left>Services<td>");

                std::string services = frame.decoding_result.packetVars["services"];
                for (int k = 0; k < (int)services.length(); k++)
                {
                    if (services[k] != '\n') {
                        decode = decode.append(services[k]);
                    } else {
                        decode = decode.append("<br>");
                    }
                }
                decode = decode.append("<tr><th align=left>Tdm slots<td>");
                std::string tdmSlots = frame.decoding_result.packetVars["tdmSlots"];
                for (int k = 0; k < (int)tdmSlots.length(); k++)
                {
                    if (tdmSlots[k] != '\n') {
                        decode = decode.append(tdmSlots[k]);
                    } else {
                        decode = decode.append("<br>");
                    }
                }
            }
            break;

            case 0x7D:
            {
                decode = decode.append("<tr><th align=left>Network Version<td> " + frame.decoding_result.packetVars["networkVersion"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);
                decode = decode.append("<tr><th align=left>Signalling Channel<td> " + frame.decoding_result.packetVars["signallingChannel"]);
                decode = decode.append("<tr><th align=left>Count<td> " + frame.decoding_result.packetVars["count"]);
                decode = decode.append("<tr><th align=left>Channel Type Name<td> " + frame.decoding_result.packetVars["channelTypeName"]);
                decode = decode.append("<tr><th align=left>Local<td> " + frame.decoding_result.packetVars["local"]);
                decode = decode.append("<tr><th align=left>Random Interval<td> " + frame.decoding_result.packetVars["randomInterval"]);

                decode = decode.append("<tr><th align=left>Status<td>");
                std::string status = frame.decoding_result.packetVars["status"];
                for (int k = 0; k < (int)status.length(); k++)
                {
                    if (status[k] != '\n') {
                        decode = decode.append(status[k]);
                    } else {
                        decode = decode.append("<br>");
                    }
                }
                decode = decode.append("<tr><th align=left>Services<td>");
                std::string services = frame.decoding_result.packetVars["services"];
                for (int k = 0; k < (int)services.length(); k++)
                {
                    if (services[k] != '\n') {
                        decode = decode.append(services[k]);
                    } else {
                        decode = decode.append("<br>");
                    }
                }
            }
            break;

            case 0x81:
                decode = decode.append("<tr><th align=left>Msg Id<td> " + frame.decoding_result.packetVars["mesId"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);
                decode = decode.append("<tr><th align=left>LCN<td> " + frame.decoding_result.packetVars["logicalChannelNo"]);
                decode = decode.append("<tr><th align=left>DLF<td> " + formatFreqMHz(frame.decoding_result.packetVars["downlinkChannelMhz"]));
                decode = decode.append("<tr><th align=left>Presentation<td> " + frame.decoding_result.packetVars["presentation"]);
                break;

            case 0x83:
                decode = decode.append("<tr><th align=left>Msg Id<td> " + frame.decoding_result.packetVars["mesId"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);
                decode = decode.append("<tr><th align=left>Status Bits<td> " + frame.decoding_result.packetVars["status_bits"]);
                decode = decode.append("<tr><th align=left>LCN<td> " + frame.decoding_result.packetVars["logicalChannelNo"]);
                decode = decode.append("<tr><th align=left>Frame Length<td> " + frame.decoding_result.packetVars["frameLength"]);
                decode = decode.append("<tr><th align=left>Duration<td> " + frame.decoding_result.packetVars["duration"]);
                decode = decode.append("<tr><th align=left>DLF<td> " + formatFreqMHz(frame.decoding_result.packetVars["downlinkChannelMhz"]));
                decode = decode.append("<tr><th align=left>ULF<td> " + formatFreqMHz(frame.decoding_result.packetVars["uplinkChannelMhz"]));
                decode = decode.append("<tr><th align=left>Frame Offset<td> " + frame.decoding_result.packetVars["frameOffset"]);
                decode = decode.append("<tr><th align=left>Packet Descriptor 1<td> " + frame.decoding_result.packetVars["packetDescriptor1"]);
                break;

            case 0x92:
                decode = decode.append("<tr><th align=left>Login Ack Length<td>" + frame.decoding_result.packetVars["loginAckLength"]);
                decode = decode.append("<tr><th align=left>DLF<td>" + formatFreqMHz(frame.decoding_result.packetVars["downlinkChannelMhz"]));
                decode = decode.append("<tr><th align=left>LES<td>" + frame.decoding_result.packetVars["les"]);
                decode = decode.append("<tr><th align=left>Station Start Hex<td>" + frame.decoding_result.packetVars["stationStartHex"]);
                if (frame.decoding_result.packetVars.find("stationCount") != frame.decoding_result.packetVars.end())
                {
                    decode = decode.append("<tr><th align=left>Station Count<td>" + frame.decoding_result.packetVars["stationCnt"]);
                    decode = decode.append("<tr><th align=left>Stations<td>");
                    std::string stations = frame.decoding_result.packetVars["stations"];
                    for (int k = 0; k < (int)stations.length(); k++)
                    {
                        if (stations[k] != '\n') {
                            decode = decode.append(stations[k]);
                        } else {
                            decode = decode.append("<br>");
                        }
                    }
                }
                break;

            case 0xA3:
                decode = decode.append("<tr><th align=left>Msg Id<td> " + frame.decoding_result.packetVars["mesId"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);

                if (frame.decoding_result.packetVars.find("shortMessage") != frame.decoding_result.packetVars.end())
                {
                    std::string shortMessage = frame.decoding_result.packetVars["shortMessage"];
                    for (int k = 0; k < (int)shortMessage.length(); k++)
                    {
                        char chr = shortMessage[k] & 0x7F;
                        if ((chr < 0x20 && chr != '\n' && chr != '\r')) {
                            message = message.append("(" + QString::number(chr) + ")");
                        } else if(chr != '\n'  && chr != '\r') {
                            message = message.append(chr);
                        }
                    }
                    decode = decode.append("<tr><th align=left>Payload<td>" + message);
                }
                break;

            case 0xA8:
                decode = decode.append("<tr><th align=left>Msg Id<td> " + frame.decoding_result.packetVars["mesId"]);
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);
                if (frame.decoding_result.packetVars.find("shortMessage") != frame.decoding_result.packetVars.end())
                {
                    std::string shortMessage = frame.decoding_result.packetVars["shortMessage"];
                    for (int k = 0; k < (int)shortMessage.length(); k++)
                    {
                        char chr = shortMessage[k] & 0x7F;
                        if ((chr < 0x20 && chr != '\n' && chr != '\r')) {
                            message = message.append("(" + QString::number(chr) + ")");
                        } else if(chr != '\n'  && chr != '\r') {
                            message = message.append(chr);
                        }
                    }
                    decode = decode.append("<tr><th align=left>Payload<td>" + message);
                }
                break;

            case 0xAA:
            {
                decode = decode.append("<tr><th align=left>Sat<td> " + frame.decoding_result.packetVars["satName"]);
                decode = decode.append("<tr><th align=left>LES<td> " + frame.decoding_result.packetVars["lesName"]);
                decode = decode.append("<tr><th align=left>LCN<td> " + frame.decoding_result.packetVars["logicalChannelNo"]);
                decode = decode.append("<tr><th align=left>Packet No<td> " + frame.decoding_result.packetVars["packetNo"]);

                bool isBinary = frame.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_BINARY;
                if (frame.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_IA5)
                {
                    for (int i = 0; i < (int)frame.decoding_result.payload.data8Bit.size(); i++)
                    {
                        char chr = frame.decoding_result.payload.data8Bit[i] & 0x7F;
                        if ((chr < 0x20 && chr != '\n' && chr != '\r')) {
                            message = message.append("(" + QString::number((uint16_t)chr, 16) + ")");
                        } else if(chr != '\n' && chr != '\r') {
                            message = message.append(chr);
                        }
                    }
                }
                else if (frame.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_ITA2)
                {

                }
                else
                {
                    for (int i = 0; i < (int)frame.decoding_result.payload.data8Bit.size(); i++)
                    {
                        uint8_t data = frame.decoding_result.payload.data8Bit[i];
                        message = message.append(QString::number((uint16_t)data, 16) + " ");
                    }
                }
            }
            decode = decode.append("<tr><th align=left>Payload<td>" + message);
            break;

            case 0xAB:
            {
                decode = decode.append("<tr><th align=left>LES List Length<td>" + frame.decoding_result.packetVars["lesListLength"]);
                decode = decode.append("<tr><th align=left>Station Start Hex<td>" + frame.decoding_result.packetVars["stationStartHex"]);
                decode = decode.append("<tr><th align=left>Station Count<td>" + frame.decoding_result.packetVars["stationCount"]);

                decode = decode.append("<tr><th align=left>Stations<td>");
                std::string stations = frame.decoding_result.packetVars["stations"];
                for (int k = 0; k < (int)stations.length(); k++)
                {
                    if (stations[k] != '\n') {
                        decode = decode.append(stations[k]);
                    } else {
                        decode = decode.append("<br>");
                    }
                }
            }
            break;

            case 0xB1:
            {
                decode = decode.append("<tr><th align=left>Msg Type<td>" + frame.decoding_result.packetVars["messageType"]);
                decode = decode.append("<tr><th align=left>Service Code & Address Name<td>" + frame.decoding_result.packetVars["serviceCodeAndAddressName"]);
                decode = decode.append("<tr><th align=left>Continuation<td>" + frame.decoding_result.packetVars["continuation"]);
                decode = decode.append("<tr><th align=left>Priority<td>" + frame.decoding_result.packetVars["priorityText"]);
                decode = decode.append("<tr><th align=left>Repetition<td>" + frame.decoding_result.packetVars["rep"]);
                decode = decode.append("<tr><th align=left>Msg Id<td>" + frame.decoding_result.packetVars["messageId"]);
                decode = decode.append("<tr><th align=left>Packet No<td>" + frame.decoding_result.packetVars["packetNo"]);
                decode = decode.append("<tr><th align=left>isNewPayload<td>" + frame.decoding_result.packetVars["isNewPayl"]);
                decode = decode.append("<tr><th align=left>Address<td>" + frame.decoding_result.packetVars["addrHex"]);

                bool isBinary = frame.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_BINARY;
                if (!isBinary)
                {
                    for (int k = 0; k < (int)frame.decoding_result.payload.data8Bit.size(); k++) {
                        char chr = frame.decoding_result.payload.data8Bit[k] & 0x7F;
                        if ((chr < 0x20 && chr != '\n' && chr != '\r')) {
                            message = message.append("(" + QString::number((uint16_t)chr, 16) + ")");
                        } else /*if (chr != '\n'  && chr != '\r')*/ {
                            message = message.append(chr);
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < (int)frame.decoding_result.payload.data8Bit.size(); i++)
                    {
                        uint8_t data = frame.decoding_result.payload.data8Bit[i];
                        message = message.append(QString::number(data, 16) + " ");
                    }
                }
                decode = decode.append("<tr><th align=left>Payload<td>" + message);
            }
            break;

            case 0xB2:
            {
                decode = decode.append("<tr><th align=left>Msg Type<td>" + frame.decoding_result.packetVars["messageType"]);
                decode = decode.append("<tr><th align=left>Service Code & Address Name<td>" + frame.decoding_result.packetVars["serviceCodeAndAddressName"]);
                decode = decode.append("<tr><th align=left>Continuation<td>" + frame.decoding_result.packetVars["continuation"]);
                decode = decode.append("<tr><th align=left>Priority<td>" + frame.decoding_result.packetVars["priorityText"]);
                decode = decode.append("<tr><th align=left>Repetition<td>" + frame.decoding_result.packetVars["rep"]);
                decode = decode.append("<tr><th align=left>Msg Id<td>" + frame.decoding_result.packetVars["messageId"]);
                decode = decode.append("<tr><th align=left>Packet No<td>" + frame.decoding_result.packetVars["packetNo"]);
                decode = decode.append("<tr><th align=left>isNewPayload<td>" + frame.decoding_result.packetVars["isNewPayl"]);
                decode = decode.append("<tr><th align=left>Address<td>" + frame.decoding_result.packetVars["addrHex"]);

                bool isBinary = frame.decoding_result.payload.presentation == PACKETDECODER_PRESENTATION_BINARY;
                if (!isBinary)
                {
                    for (int k = 0; k < (int)frame.decoding_result.payload.data8Bit.size(); k++)
                    {
                        char chr = frame.decoding_result.payload.data8Bit[k] & 0x7F;
                        if ((chr < 0x20 && chr != '\n' && chr != '\r')) {
                            message = message.append("(" + QString::number((uint16_t)chr, 16) + ")");
                        } else /*if (chr != '\n'  && chr != '\r')*/ {
                            message = message.append(chr);
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < (int)frame.decoding_result.payload.data8Bit.size(); i++)
                    {
                        uint8_t data = frame.decoding_result.payload.data8Bit[i];
                        message = message.append(QString::number(data, 16) + " ");
                    }
                }
                decode = decode.append("<tr><th align=left>Payload<td>" + message);
            }
            break;

            }

            messageItem->setText(message);
            decodeLabel->setText(decode);

            QByteArray frameBytes((char *) decoderResult.decodedFrame, decoderResult.length);
            dataHexItem->setText(frameBytes.toHex());

            filterRow(ui->packets, row, PACKET_COL_TYPE, PACKET_COL_MESSAGE);
            if (!m_loadingData)
            {
                ui->packets->setSortingEnabled(true);
                if (scrollToBottom) {
                    ui->packets->scrollToBottom();
                }
            }

            QString messageId = QString::fromStdString(frame.decoding_result.packetVars["messageId"]);
            if (!messageId.isEmpty())
            {
                int id = messageId.toInt();
                MultipartMessage *multipartMessage = nullptr;
                if (m_messages.contains(id))
                {
                    multipartMessage = m_messages[id];
                    multipartMessage->update(frame.decoding_result.packetVars, dateTime);
                }
                else
                {
                    multipartMessage = new MultipartMessage(id, frame.decoding_result.packetVars, dateTime);
                    m_messages.insert(id, multipartMessage);
                }
                MessagePart part;
                part.m_part = frame.decoding_result.packetDescriptor == 0xb1 ? 1 : 2;
                part.m_packet = QString::fromStdString(frame.decoding_result.packetVars["packetNo"]).toInt();
                part.m_text = message;
                multipartMessage->addPart(part);
                updateMessageTable(multipartMessage);

                if (!multipartMessage->getCoordinates().isEmpty())
                {
                    QString name = QString::number(multipartMessage->getId());
                    QString messageText = multipartMessage->getMessage().replace("\n", "<br>");
                    sendAreaToMapFeature(name, messageText, multipartMessage->getLatitude(), multipartMessage->getLongitude(), multipartMessage->getCoordinates());
                }
            }
        }
    }
}

void InmarsatDemodGUI::sendAreaToMapFeature(const QString& name, const QString& text, float latitude, float longitude, const QList<QGeoCoordinate>& coordinates)
{
    // Send to Map feature
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_inmarsatDemod, "mapitems", mapPipes);

    if (mapPipes.size() > 0)
    {
        if (!m_mapItems.contains(name)) {
            m_mapItems.append(name);
        }

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();

            swgMapItem->setName(new QString(name));
            swgMapItem->setLatitude(latitude);
            swgMapItem->setLongitude(longitude);
            swgMapItem->setAltitude(0.0);
            QString image = QString("none");
            swgMapItem->setImage(new QString(image));
            swgMapItem->setImageRotation(0);
            swgMapItem->setText(new QString(text));                   // Not used - label is used instead for now
            swgMapItem->setLabel(new QString(text));
            swgMapItem->setAltitudeReference(0);

            QList<SWGSDRangel::SWGMapCoordinate *> *coords = new QList<SWGSDRangel::SWGMapCoordinate *>();

            for (const auto& geoC : coordinates)
            {
                SWGSDRangel::SWGMapCoordinate* c = new SWGSDRangel::SWGMapCoordinate();
                c->setLatitude(geoC.latitude());
                c->setLongitude(geoC.longitude());
                c->setAltitude(geoC.altitude());
                coords->append(c);
            }

            swgMapItem->setCoordinates(coords);
            swgMapItem->setType(3);

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_inmarsatDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }
}

void InmarsatDemodGUI::clearAreaFromMapFeature(const QString& name)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_inmarsatDemod, "mapitems", mapPipes);
    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setImage(new QString(""));
        swgMapItem->setType(3);
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_inmarsatDemod, swgMapItem);
        messageQueue->push(msg);
    }
    m_mapItems.removeAll(name);
}

void InmarsatDemodGUI::clearAllFromMapFeature()
{
    while (m_mapItems.size() > 0) {
        clearAreaFromMapFeature(m_mapItems[0]);
    }
}


void InmarsatDemodGUI::on_messages_itemSelectionChanged()
{
    QList<QTableWidgetItem *> items = ui->messages->selectedItems();
    if (items.size() > 0)
    {
        ui->packets->clearSelection();

        int row = items[0]->row();
        QString message = ui->messages->item(row, MESSAGE_COL_MESSAGE)->text();
        ui->decode->setText(message);
    }
}

void InmarsatDemodGUI::on_packets_itemSelectionChanged()
{
    QList<QTableWidgetItem *> items = ui->packets->selectedItems();
    if (items.size() > 0)
    {
        ui->messages->clearSelection();

        int row = items[0]->row();
        // Display decoded packet
        QLabel *label = qobject_cast<QLabel *>(ui->packets->cellWidget(row, PACKET_COL_DECODE));
        QString decode = label->text();
        ui->decode->setText(decode);
    }
}

bool InmarsatDemodGUI::handleMessage(const Message& message)
{
    if (InmarsatDemod::MsgConfigureInmarsatDemod::match(message))
    {
        qDebug("InmarsatDemodGUI::handleMessage: InmarsatDemod::MsgConfigureInmarsatDemod");
        const InmarsatDemod::MsgConfigureInmarsatDemod& cfg = (InmarsatDemod::MsgConfigureInmarsatDemod&) message;
        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (MainCore::MsgPacket::match(message))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) message;
        packetReceived(report.getPacket(), report.getDateTime());
        return true;
    }

    return false;
}

void InmarsatDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void InmarsatDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void InmarsatDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void InmarsatDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void InmarsatDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySetting("rfBandwidth");
}

void InmarsatDemodGUI::on_rrcRolloff_valueChanged(int value)
{
    ui->rrcRolloffText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_rrcRolloff = value / 100.0;
    applySetting("rrfRolloff");
}

void InmarsatDemodGUI::on_pll_clicked(bool checked)
{
    ui->rrcRolloff->setVisible(checked);
    ui->rrcRolloffLabel->setVisible(checked);
    ui->rrcRolloffText->setVisible(checked);
    ui->rrcRolloffLine->setVisible(checked);
    ui->pllBW->setVisible(checked);
    ui->pllBWText->setVisible(checked);
    ui->pllBWLabel->setVisible(checked);
    ui->ssBW->setVisible(checked);
    ui->ssBWText->setVisible(checked);
    ui->ssBWLabel->setVisible(checked);
    ui->ssBWLine->setVisible(checked);
    ui->pllCoarseLine->setVisible(checked);
    ui->pllCoarseFreq->setVisible(checked);
    ui->pllCoarseFreqLabel->setVisible(checked);
    ui->pllFineLine->setVisible(checked);
    ui->pllFineFreq->setVisible(checked);
    ui->pllFineFreqLabel->setVisible(checked);
    ui->pllFineFreqUnits->setVisible(checked);
}

void InmarsatDemodGUI::on_pllBW_valueChanged(int value)
{
    ui->pllBWText->setText(QString("%1").arg(value / 1000.0, 0, 'f', 3));
    m_settings.m_pllBW = value / 1000.0;
    applySetting("pllBandwidth");
}

void InmarsatDemodGUI::on_ssBW_valueChanged(int value)
{
    ui->ssBWText->setText(QString("%1").arg(value / 1000.0, 0, 'f', 3));
    m_settings.m_ssBW = value / 1000.0;
    applySetting("ssBandwidth");
}

void InmarsatDemodGUI::on_filterType_editingFinished()
{
    m_settings.m_filterType = ui->filterType->text();
    updateRegularExpressions();
    filter();
    applySetting("filterType");
}

void InmarsatDemodGUI::on_filterMessage_editingFinished()
{
    m_settings.m_filterMessage = ui->filterMessage->text();
    updateRegularExpressions();
    filter();
    applySetting("filterMessage");
}

void InmarsatDemodGUI::on_clearTable_clicked()
{
    ui->packets->setRowCount(0);
    ui->messages->setRowCount(0);
    ui->decode->setText("");
    qDeleteAll(m_messages);
    m_messages.clear();
    clearAllFromMapFeature();
}

void InmarsatDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySetting("udpEnabled");
}

void InmarsatDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySetting("udpAddress");
}

void InmarsatDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySetting("udpPort");
}

void InmarsatDemodGUI::filterRow(QTableWidget *table, int row, int typeCol, int messageCol)
{
    bool hidden = false;

    if ((m_settings.m_filterType != "") && (typeCol != -1))
    {
        QTableWidgetItem *typeItem = table->item(row, typeCol);
        QRegularExpressionMatch match = m_typeRE.match(typeItem->text());
        if (!match.hasMatch()) {
            hidden = true;
        }
    }
    if (m_settings.m_filterMessage != "")
    {
        QTableWidgetItem *messageItem = table->item(row, messageCol);
        QRegularExpressionMatch match = m_messageRE.match(messageItem->text());
        if (!match.hasMatch()) {
            hidden = true;
        }
    }
    table->setRowHidden(row, hidden);
}

void InmarsatDemodGUI::filter()
{
    for (int i = 0; i < ui->packets->rowCount(); i++) {
        filterRow(ui->packets, i, PACKET_COL_TYPE, PACKET_COL_MESSAGE);
    }
    for (int i = 0; i < ui->messages->rowCount(); i++) {
        filterRow(ui->messages, i, -1, MESSAGE_COL_MESSAGE);
    }
}

void InmarsatDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

void InmarsatDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_inmarsatDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        QList<QString> settingsKeys({
            "rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
            });

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

InmarsatDemodGUI::InmarsatDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::InmarsatDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0),
    m_loadingData(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodinmarsat/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(packetsCustomContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_inmarsatDemod = reinterpret_cast<InmarsatDemod*>(rxChannel);
    m_inmarsatDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_inmarsatDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setStreams(QStringList({"IQ", "MagSq", "IQ AGC", "AGC gain/avg", "IQ CFO", "IQ RRC", "TED Er", "TED Er Sum", "CL", "IQ Derot", "Bit/mu"}));

    // Scope settings to display the IQ waveforms
    ui->scopeGUI->setPreTrigger(1);
    GLScopeSettings::TraceData traceDataI, traceDataQ;
    traceDataI.m_projectionType = Projector::ProjectionReal;
    traceDataI.m_amp = 1.0;      // for -1 to +1
    traceDataI.m_ofs = 0.0;      // vertical offset
    traceDataQ.m_projectionType = Projector::ProjectionImag;
    traceDataQ.m_amp = 1.0;
    traceDataQ.m_ofs = 0.0;
    ui->scopeGUI->changeTrace(0, traceDataI);
    ui->scopeGUI->addTrace(traceDataQ);
    ui->scopeGUI->setDisplayMode(GLScopeSettings::DisplayXYV);
    ui->scopeGUI->focusOnTrace(0); // re-focus to take changes into account in the GUI

    GLScopeSettings::TriggerData triggerData;
    triggerData.m_triggerLevel = 0.1;
    triggerData.m_triggerLevelCoarse = 10;
    triggerData.m_triggerPositiveEdge = true;
    ui->scopeGUI->changeTrigger(0, triggerData);
    ui->scopeGUI->focusOnTrigger(0); // re-focus to take changes into account in the GUI

    m_scopeVis->setLiveRate(48000);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Inmarsat C Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setScopeGUI(ui->scopeGUI);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->packets->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->packets->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    m_packetsMenu = new QMenu(ui->packets);
    for (int i = 0; i < ui->packets->horizontalHeader()->count(); i++)
    {
        QString text = ui->packets->horizontalHeaderItem(i)->text();
        m_packetsMenu->addAction(createCheckableItem(text, i, true, true));
    }
    ui->packets->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->packets->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(packetsColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->packets->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(packets_sectionMoved(int, int, int)));
    connect(ui->packets->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(packets_sectionResized(int, int, int)));
    ui->packets->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->packets, SIGNAL(customContextMenuRequested(QPoint)), SLOT(packetsCustomContextMenuRequested(QPoint)));
    TableTapAndHold *packetsTableTapAndHold = new TableTapAndHold(ui->packets);
    connect(packetsTableTapAndHold, &TableTapAndHold::tapAndHold, this, &InmarsatDemodGUI::packetsCustomContextMenuRequested);

    // Allow user to reorder columns
    ui->messages->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->messages->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    m_messagesMenu = new QMenu(ui->messages);
    for (int i = 0; i < ui->messages->horizontalHeader()->count(); i++)
    {
        QString text = ui->messages->horizontalHeaderItem(i)->text();
        m_messagesMenu->addAction(createCheckableItem(text, i, true, false));
    }
    ui->messages->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(messagesColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(messages_sectionMoved(int, int, int)));
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(messages_sectionResized(int, int, int)));
    ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages, SIGNAL(customContextMenuRequested(QPoint)), SLOT(messagesCustomContextMenuRequested(QPoint)));
    TableTapAndHold *messagesTableTapAndHold = new TableTapAndHold(ui->messages);
    connect(messagesTableTapAndHold, &TableTapAndHold::tapAndHold, this, &InmarsatDemodGUI::messagesCustomContextMenuRequested);

    on_pll_clicked(false);
    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applyAllSettings();
    m_resizer.enableChildMouseTracking();
}

InmarsatDemodGUI::~InmarsatDemodGUI()
{
    clearAllFromMapFeature();
    delete ui;
}

void InmarsatDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void InmarsatDemodGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void InmarsatDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        InmarsatDemod::MsgConfigureInmarsatDemod* message = InmarsatDemod::MsgConfigureInmarsatDemod::create(m_settings, m_settingsKeys, force);
        m_inmarsatDemod->getInputMessageQueue()->push(message);
        m_settingsKeys.clear();
    }
}

void InmarsatDemodGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void InmarsatDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    updateRegularExpressions();

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->rrcRolloffText->setText(QString("%1").arg(m_settings.m_rrcRolloff, 0, 'f', 2));
    ui->rrcRolloff->setValue(m_settings.m_rrcRolloff * 100.0);

    ui->pllBWText->setText(QString("%1").arg(m_settings.m_pllBW, 0, 'f', 3));
    ui->pllBW->setValue(m_settings.m_pllBW * 1000.0);

    ui->ssBWText->setText(QString("%1").arg(m_settings.m_ssBW, 0, 'f', 3));
    ui->ssBW->setValue(m_settings.m_ssBW * 1000.0);

    updateIndexLabel();

    ui->filterType->setText(m_settings.m_filterType);
    ui->filterMessage->setText(m_settings.m_filterMessage);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->useFileTime->setChecked(m_settings.m_useFileTime);

    // Order and size columns
    QHeaderView *header = ui->packets->horizontalHeader();
    for (int i = 0; i < INMARSATDEMOD_PACKETS_COLUMNS; i++)
    {
        bool hidden = m_settings.m_packetsColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        m_packetsMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_packetsColumnSizes[i] > 0)
            ui->packets->setColumnWidth(i, m_settings.m_packetsColumnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_packetsColumnIndexes[i]);
    }
    header = ui->messages->horizontalHeader();
    for (int i = 0; i < INMARSATDEMOD_MESSAGES_COLUMNS; i++)
    {
        bool hidden = m_settings.m_messagesColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        m_messagesMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_messagesColumnSizes[i] > 0)
            ui->messages->setColumnWidth(i, m_settings.m_messagesColumnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_messagesColumnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void InmarsatDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void InmarsatDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void InmarsatDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_inmarsatDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    bool locked;
    Real coarseFreqCurrent;
    Real coarseFreqCurrentPower;
    Real coarseFreq;
    Real coarseFreqPower;
    Real fineFreq;
    m_inmarsatDemod->getPLLStatus(locked, coarseFreqCurrent, coarseFreqCurrentPower, coarseFreq, coarseFreqPower, fineFreq);

    if (locked)
    {
        ui->pll->setStyleSheet("QToolButton { background-color : green; }");
        ui->pll->setIcon(QIcon(":/locked.png").pixmap(QSize(20, 20)));
    }
    else
    {
        ui->pll->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        ui->pll->setIcon(QIcon(":/unlocked.png").pixmap(QSize(20, 20)));
    }

    ui->pllFineFreq->setText(QString::number(std::round(fineFreq)));
    ui->pllCoarseFreq->setText(
        QString("Current: %1 Hz %2 dB Locked: %3 Hz %4 dB")
        .arg(std::round(coarseFreqCurrent))
        .arg(std::round(CalcDb::dbPower(coarseFreqCurrentPower)))
        .arg(std::round(coarseFreq))
        .arg(std::round(CalcDb::dbPower(coarseFreqPower)))
    );

    m_tickCount++;
}

void InmarsatDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySetting("logEnabled");
}

void InmarsatDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received frames to", "", "*.csv");
    fileDialog.setDefaultSuffix("csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySetting("logFilename");
        }
    }
}

// Read .csv log and process as received frames
void InmarsatDemodGUI::on_logOpen_clicked()
{
    QFileDialog fileDialog(nullptr, "Select .csv log file to read", "", "*.csv");
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            QFile file(fileNames[0]);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Date", "Time", "Data"}, error);
                if (error.isEmpty())
                {
                    m_loadingData = true;

                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int dataCol = colIndexes.value("Data");
                    int maxCol = std::max({dateCol, timeCol, dataCol});

                    QMessageBox dialog(this);
                    dialog.setText("Reading packet data");
                    dialog.addButton(QMessageBox::Cancel);
                    dialog.show();
                    QApplication::processEvents();
                    int count = 0;
                    bool cancelled = false;
                    QStringList cols;
                    while (!cancelled && CSV::readRow(in, &cols))
                    {
                        if (cols.size() > maxCol)
                        {
                            QDate date = QDate::fromString(cols[dateCol]);
                            QTime time = QTime::fromString(cols[timeCol]);
                            QDateTime dateTime(date, time);
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());
                            packetReceived(bytes, dateTime);
                            if (count % 1000 == 0)
                            {
                                QApplication::processEvents();
                                if (dialog.clickedButton()) {
                                    cancelled = true;
                                }
                            }
                            count++;
                        }
                    }
                    dialog.close();

                    m_loadingData = false;
                    ui->packets->setSortingEnabled(true);
                    ui->messages->setSortingEnabled(true);
                }
                else
                {
                    QMessageBox::critical(this, "Inmarsat Demod", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "Inmarsat Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void InmarsatDemodGUI::on_useFileTime_toggled(bool checked)
{
    m_settings.m_useFileTime = checked;
    applySetting("useFileTime");
}

void InmarsatDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &InmarsatDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &InmarsatDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->rrcRolloff, &QDial::valueChanged, this, &InmarsatDemodGUI::on_rrcRolloff_valueChanged);
    QObject::connect(ui->pllBW, &QDial::valueChanged, this, &InmarsatDemodGUI::on_pllBW_valueChanged);
    QObject::connect(ui->ssBW, &QDial::valueChanged, this, &InmarsatDemodGUI::on_ssBW_valueChanged);
    QObject::connect(ui->pll, &QToolButton::clicked, this, &InmarsatDemodGUI::on_pll_clicked);
    QObject::connect(ui->filterType, &QLineEdit::editingFinished, this, &InmarsatDemodGUI::on_filterType_editingFinished);
    QObject::connect(ui->filterMessage, &QLineEdit::editingFinished, this, &InmarsatDemodGUI::on_filterMessage_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &InmarsatDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &InmarsatDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &InmarsatDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &InmarsatDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &InmarsatDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &InmarsatDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &InmarsatDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->useFileTime, &ButtonSwitch::toggled, this, &InmarsatDemodGUI::on_useFileTime_toggled);
    QObject::connect(ui->packets, &QTableWidget::itemSelectionChanged, this, &InmarsatDemodGUI::on_packets_itemSelectionChanged);
    QObject::connect(ui->messages, &QTableWidget::itemSelectionChanged, this, &InmarsatDemodGUI::on_messages_itemSelectionChanged);
}

void InmarsatDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void InmarsatDemodGUI::updateRegularExpressions()
{
    m_typeRE = QRegularExpression(QRegularExpression::anchoredPattern(m_settings.m_filterType));
    m_messageRE = QRegularExpression(QRegularExpression::anchoredPattern(m_settings.m_filterMessage), QRegularExpression::DotMatchesEverythingOption);
}
