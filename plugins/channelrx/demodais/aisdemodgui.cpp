///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <limits>
#include <ctype.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QDesktopServices>
#include <QMessageBox>
#include <QAction>
#include <QRegExp>
#include <QClipboard>
#include <QFileDialog>
#include <QScrollBar>
#include <QIcon>

#include "aisdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_aisdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/ais.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/mmsi.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/crightclickenabler.h"
#include "gui/tabletapandhold.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"
#include "feature/featurewebapiutils.h"

#include "aisdemod.h"
#include "aisdemodsink.h"

void AISDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);
    ui->messages->setItem(row, MESSAGE_COL_DATE, new QTableWidgetItem("Frid Apr 15 2016-"));
    ui->messages->setItem(row, MESSAGE_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->messages->setItem(row, MESSAGE_COL_MMSI, new QTableWidgetItem("123456789"));
    ui->messages->setItem(row, MESSAGE_COL_COUNTRY, new QTableWidgetItem("flag"));
    ui->messages->setItem(row, MESSAGE_COL_TYPE, new QTableWidgetItem("Position report"));
    ui->messages->setItem(row, MESSAGE_COL_ID, new QTableWidgetItem("25"));
    ui->messages->setItem(row, MESSAGE_COL_DATA, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->messages->setItem(row, MESSAGE_COL_NMEA, new QTableWidgetItem("!AIVDM,1,1,,A,AAAAAAAAAAAAAAAAAAAAAAAAAAAA,0*00"));
    ui->messages->setItem(row, MESSAGE_COL_HEX, new QTableWidgetItem("04058804002000069a0760728d9e00000040000000"));
    ui->messages->setItem(row, MESSAGE_COL_SLOT, new QTableWidgetItem("2450"));
    ui->messages->resizeColumnsToContents();
    ui->messages->removeRow(row);
}

// Columns in table reordered
void AISDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_messageColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void AISDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_messageColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void AISDemodGUI::messagesColumnSelectMenu(QPoint pos)
{
    messagesMenu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void AISDemodGUI::messagesColumnSelectMenuChecked(bool checked)
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
QAction *AISDemodGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

AISDemodGUI* AISDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    AISDemodGUI* gui = new AISDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void AISDemodGUI::destroy()
{
    delete this;
}

void AISDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray AISDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool AISDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

// Distint palette generator
// https://mokole.com/palette.html
QList<QRgb> AISDemodGUI::m_colors = {
    0xffffff,
    0xff0000,
    0x00ff00,
    0x0000ff,
    0x00ffff,
    0xff00ff,
    0x7fff00,
    0x000080,
    0xa9a9a9,
    0x2f4f4f,
    0x556b2f,
    0x8b4513,
    0x6b8e23,
    0x191970,
    0x006400,
    0x708090,
    0x8b0000,
    0x3cb371,
    0xbc8f8f,
    0x663399,
    0xb8860b,
    0xbdb76b,
    0x008b8b,
    0x4682b4,
    0xd2691e,
    0x9acd32,
    0xcd5c5c,
    0x32cd32,
    0x8fbc8f,
    0x8b008b,
    0xb03060,
    0x66cdaa,
    0x9932cc,
    0x00ced1,
    0xff8c00,
    0xffd700,
    0xc71585,
    0x0000cd,
    0xdeb887,
    0x00ff7f,
    0x4169e1,
    0xe9967a,
    0xdc143c,
    0x00bfff,
    0xf4a460,
    0x9370db,
    0xa020f0,
    0xff6347,
    0xd8bfd8,
    0xdb7093,
    0xf0e68c,
    0xffff54,
    0x6495ed,
    0xdda0dd,
    0x87ceeb,
    0xff1493,
    0xafeeee,
    0xee82ee,
    0xfaf0e6,
    0x98fb98,
    0x7fffd4,
    0xff69b4,
    0xfffacd,
    0xffb6c1,
};

QHash<QString, QRgb> m_categoryColors = {
    {"Class A Vessel", 0xff0000},
    {"Class B Vessel", 0x0000ff},
    {"Coast", 0x00ff00},
    {"Physical AtoN", 0xffff00},
    {"Virtual AtoN", 0xc0c000},
    {"Mobile AtoN", 0xa0a000},
    {"AtoN", 0x808000},
    {"SAR", 0x00ffff},
    {"SAR Aircraft", 0x00c0c0},
    {"SAR Helicopter", 0x00a0a0},
    {"Group", 0xff00ff},
    {"Man overboard", 0xc000c0},
    {"EPIRB", 0xa000a0},
    {"AMRD", 0x800080},
    {"Craft with parent ship", 0x600060}
};

QMutex AISDemodGUI::m_colorMutex;
QHash<QString, bool> AISDemodGUI::m_usedInFrame;
QHash<QString, QColor> AISDemodGUI::m_slotMapColors;
QDateTime AISDemodGUI::m_lastColorUpdate;
QHash<QString, QString> AISDemodGUI::m_category;

QColor AISDemodGUI::getColor(const QString& mmsi)
{
    if (true)
    {
        if (m_category.contains(mmsi))
        {
            QString category = m_category.value(mmsi);
            if (m_categoryColors.contains(category)) {
                return QColor(m_categoryColors.value(category));
            }
            qDebug() << "No color for " << category;
            return Qt::white;
        }
        else
        {
            // Use white for no category
            return Qt::white;
        }
    }
    else
    {
        QMutexLocker locker(&m_colorMutex);

        QColor color;
        if (m_slotMapColors.contains(mmsi))
        {
            m_usedInFrame.insert(mmsi, true);
            color = m_slotMapColors.value(mmsi);
        }
        else
        {
            if (m_colors.size() > 0)
            {
                color = m_colors.takeFirst();
                qDebug() << "Taking colour from list " << color << "for" << mmsi << " - remaining " << m_colors.size();
            }
            else
            {
                qDebug() << "Out of colors - looking to reuse";
                // Look for recently unused color
                QMutableHashIterator<QString, bool> it(m_usedInFrame);
                color = Qt::black;
                while (it.hasNext())
                {
                    it.next();
                    if (!it.value())
                    {
                        color = m_slotMapColors.value(it.key());
                        if (color != Qt::black)
                        {
                            qDebug() << "Reusing " << color << " from " << it.key();
                            m_slotMapColors.remove(it.key());
                            m_usedInFrame.remove(it.key());
                            break;
                        }
                    }
                }
            }
            if (color != Qt::black)
            {
                m_slotMapColors.insert(mmsi, color);
                m_usedInFrame.insert(mmsi, true);
            }
            else
            {
                qDebug() << "No free colours";
            }
        }

        // Don't actually draw with black, as it's the background colour
        if (color == Qt::black) {
            return Qt::white;
        } else {
            return color;
        }
    }
}

void AISDemodGUI::updateColors()
{
    QMutexLocker locker(&m_colorMutex);

    QDateTime currentDateTime = QDateTime::currentDateTime();
    if (!m_lastColorUpdate.isValid() || (m_lastColorUpdate.time().minute() != currentDateTime.time().minute()))
    {
        QHashIterator<QString, bool> it(m_usedInFrame);
        while (it.hasNext())
        {
            it.next();
            m_usedInFrame.insert(it.key(), false);
        }
    }
    m_lastColorUpdate = currentDateTime;
}

void AISDemodGUI::updateSlotMap()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();

    if (!m_lastSlotMapUpdate.isValid() || (m_lastSlotMapUpdate.time().minute() != currentDateTime.time().minute()))
    {
        // Update slot utilisation stats for previous frame
        ui->slotsFree->setText(QString::number(2250 - m_slotsUsed));
        ui->slotsUsed->setText(QString::number(m_slotsUsed));
        ui->slotUtilization->setValue(std::round(m_slotsUsed * 100.0 / 2250.0));
        m_slotsUsed = 0;
        // Draw empty grid
        m_image.fill(Qt::transparent);
        //m_image.fill(Qt::);
        m_painter.setPen(Qt::black);
        for (int x = 0; x < m_image.width(); x += 5) {
            m_painter.drawLine(x, 0, x, m_image.height() - 1);
        }
        for (int y = 0; y < m_image.height(); y += 5) {
            m_painter.drawLine(0, y, m_image.width() - 1, y);
        }
        updateColors();
    }
    ui->slotMap->setPixmap(m_image);

    m_lastSlotMapUpdate = currentDateTime;
}

void AISDemodGUI::updateCategory(const QString& mmsi, const AISMessage *message)
{
    QMutexLocker locker(&m_colorMutex);

    if (!m_category.contains(mmsi))
    {
        // Categorise by MMSI
        QString category = MMSI::getCategory(mmsi);
        if (category != "Ship")
        {
            m_category.insert(mmsi, category);
            return;
        }

        // Handle Search and Rescue Aircraft Report, where MMSI doesn't indicate SAR
        if (message->m_id == 9)
        {
            m_category.insert(mmsi, "SAR");
            return;
        }

        // If ship, determine Class A or B by message type
        // See table 42 in ITU-R M.1371-5
        if (   (message->m_id <= 12)
            || ((message->m_id >= 15) && (message->m_id <= 17))
            || ((message->m_id >= 20) && (message->m_id <= 23))
            || (message->m_id >= 25)
           )
        {
            m_category.insert(mmsi, "Class A Vessel");
            return;
        }

        // Only Class B should transmit Part B static data reports
        const AISStaticDataReport *staticDataReport = dynamic_cast<const AISStaticDataReport *>(message);
        if (   (message->m_id == 18)
            || (message->m_id == 19)
            || (staticDataReport && (staticDataReport->m_partNumber == 1))
           )
        {
            m_category.insert(mmsi, "Class B Vessel");
            return;
        }
        // Other messages (such as safety) could be broadcast from either Class A or B
    }
}

// Add row to table
void AISDemodGUI::messageReceived(const QByteArray& message, const QDateTime& dateTime, int slot, int totalSlots)
{
    AISMessage *ais;

    // Decode the message
    ais = AISMessage::decode(message);

    // Is scroll bar at bottom
    QScrollBar *sb = ui->messages->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    // Add to messages table
    ui->messages->setSortingEnabled(false);
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);

    QTableWidgetItem *dateItem = new QTableWidgetItem();
    QTableWidgetItem *timeItem = new QTableWidgetItem();
    QTableWidgetItem *mmsiItem = new QTableWidgetItem();
    QTableWidgetItem *countryItem = new QTableWidgetItem();
    QTableWidgetItem *typeItem = new QTableWidgetItem();
    QTableWidgetItem *idItem = new QTableWidgetItem();
    QTableWidgetItem *dataItem = new QTableWidgetItem();
    QTableWidgetItem *nmeaItem = new QTableWidgetItem();
    QTableWidgetItem *hexItem = new QTableWidgetItem();
    QTableWidgetItem *slotItem = new QTableWidgetItem();
    ui->messages->setItem(row, MESSAGE_COL_DATE, dateItem);
    ui->messages->setItem(row, MESSAGE_COL_TIME, timeItem);
    ui->messages->setItem(row, MESSAGE_COL_MMSI, mmsiItem);
    ui->messages->setItem(row, MESSAGE_COL_COUNTRY, countryItem);
    ui->messages->setItem(row, MESSAGE_COL_TYPE, typeItem);
    ui->messages->setItem(row, MESSAGE_COL_ID, idItem);
    ui->messages->setItem(row, MESSAGE_COL_DATA, dataItem);
    ui->messages->setItem(row, MESSAGE_COL_NMEA, nmeaItem);
    ui->messages->setItem(row, MESSAGE_COL_HEX, hexItem);
    ui->messages->setItem(row, MESSAGE_COL_SLOT, slotItem);
    dateItem->setText(dateTime.date().toString());
    timeItem->setText(dateTime.time().toString());
    QString mmsi = QString("%1").arg(ais->m_mmsi, 9, 10, QChar('0'));
    mmsiItem->setText(mmsi);
    QIcon *flag = MMSI::getFlagIcon(mmsi);
    if (flag)
    {
        countryItem->setSizeHint(QSize(40, 20));
        countryItem->setIcon(*flag);
    }
    typeItem->setText(ais->getType());
    idItem->setData(Qt::DisplayRole, ais->m_id);
    dataItem->setText(ais->toString());
    nmeaItem->setText(ais->toNMEA());
    hexItem->setText(ais->toHex());
    slotItem->setData(Qt::DisplayRole, slot);
    if (!m_loadingData)
    {
        filterRow(row);
        ui->messages->setSortingEnabled(true);
        if (scrollToBottom) {
            ui->messages->scrollToBottom();
        }
    }

    updateCategory(mmsi, ais);

    // Update slot map
    updateSlotMap();
    QColor color = getColor(mmsi);
    m_painter.setPen(color);
    for (int i = 0; i < totalSlots; i++)
    {
        int y = (slot + i) / m_slotMapWidth;
        int x = (slot + i) % m_slotMapWidth;
        m_painter.fillRect(x * 5 + 1, y * 5 + 1, 4, 4, color);
    }
    m_slotsUsed += totalSlots;

    delete ais;
}

bool AISDemodGUI::handleMessage(const Message& message)
{
    if (AISDemod::MsgConfigureAISDemod::match(message))
    {
        qDebug("AISDemodGUI::handleMessage: AISDemod::MsgConfigureAISDemod");
        const AISDemod::MsgConfigureAISDemod& cfg = (AISDemod::MsgConfigureAISDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (AISDemod::MsgMessage::match(message))
    {
        AISDemod::MsgMessage& report = (AISDemod::MsgMessage&) message;
        messageReceived(report.getMessage(), report.getDateTime(), report.getSlot(), report.getSlots());
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

    return false;
}

void AISDemodGUI::handleInputMessages()
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

void AISDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void AISDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void AISDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void AISDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void AISDemodGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void AISDemodGUI::on_threshold_valueChanged(int value)
{
    ui->thresholdText->setText(QString("%1").arg(value));
    m_settings.m_correlationThreshold = value;
    applySettings();
}

void AISDemodGUI::on_filterMMSI_editingFinished()
{
    m_settings.m_filterMMSI = ui->filterMMSI->text();
    filter();
    applySettings();
}

void AISDemodGUI::on_clearTable_clicked()
{
    ui->messages->setRowCount(0);
}

void AISDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void AISDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void AISDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void AISDemodGUI::on_udpFormat_currentIndexChanged(int value)
{
    m_settings.m_udpFormat = (AISDemodSettings::UDPFormat)value;
    applySettings();
}

void AISDemodGUI::on_messages_cellDoubleClicked(int row, int column)
{
    // Get MMSI of message in row double clicked
    QString mmsi = ui->messages->item(row, MESSAGE_COL_MMSI)->text();
    if (column == MESSAGE_COL_MMSI)
    {
        // Search for MMSI on www.vesselfinder.com
        QDesktopServices::openUrl(QUrl(QString("https://www.vesselfinder.com/vessels?name=%1").arg(mmsi)));
    }
}

void AISDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterMMSI != "")
    {
        QRegExp re(m_settings.m_filterMMSI);
        QTableWidgetItem *fromItem = ui->messages->item(row, MESSAGE_COL_MMSI);
        if (!re.exactMatch(fromItem->text()))
            hidden = true;
    }
    ui->messages->setRowHidden(row, hidden);
}

void AISDemodGUI::filter()
{
    for (int i = 0; i < ui->messages->rowCount(); i++)
    {
        filterRow(i);
    }
}

void AISDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void AISDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
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
            dialog.setNumberOfStreams(m_aisDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, true);
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

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

AISDemodGUI::AISDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::AISDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0),
    m_loadingData(false),
    m_slotsUsed(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodais/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_aisDemod = reinterpret_cast<AISDemod*>(rxChannel);
    m_aisDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_aisDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    m_scopeVis->setNbStreams(AISDemodSettings::m_scopeStreams);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setStreams(QStringList({"IQ", "MagSq", "FM demod", "Gaussian", "RX buf", "Correlation", "Threshold met", "DC offset", "CRC"}));

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

    m_scopeVis->setLiveRate(9600*6);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("AIS Demodulator");
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
    ui->messages->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->messages->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    messagesMenu = new QMenu(ui->messages);
    for (int i = 0; i < ui->messages->horizontalHeader()->count(); i++)
    {
        QString text = ui->messages->horizontalHeaderItem(i)->text();
        messagesMenu->addAction(createCheckableItem(text, i, true, SLOT(messagesColumnSelectMenuChecked())));
    }
    ui->messages->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(messagesColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(messages_sectionMoved(int, int, int)));
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(messages_sectionResized(int, int, int)));
    ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->messages);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &AISDemodGUI::customContextMenuRequested);

    ui->scopeContainer->setVisible(false);

    // Create slot map image
    m_image = QPixmap(m_slotMapWidth*5+1, m_slotMapHeight*5+1);
    m_image.fill(Qt::transparent);
    m_image.fill(Qt::black);
    m_painter.begin(&m_image);
    m_pen.setColor(Qt::white);
    m_painter.setPen(m_pen);
    ui->slotMap->setPixmap(m_image);
    updateSlotMap();

    displaySettings();
    makeUIConnections();
    applySettings(true);
    DialPopup::addPopupsToChildDials(this);
}

void AISDemodGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item =  ui->messages->itemAt(pos);
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
        tableContextMenu->popup(ui->messages->viewport()->mapToGlobal(pos));
    }
}

AISDemodGUI::~AISDemodGUI()
{
    delete ui;
}

void AISDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AISDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        AISDemod::MsgConfigureAISDemod* message = AISDemod::MsgConfigureAISDemod::create( m_settings, force);
        m_aisDemod->getInputMessageQueue()->push(message);
    }
}

void AISDemodGUI::displaySettings()
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

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold));
    ui->threshold->setValue(m_settings.m_correlationThreshold);

    updateIndexLabel();

    ui->filterMMSI->setText(m_settings.m_filterMMSI);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));
    ui->udpFormat->setCurrentIndex((int)m_settings.m_udpFormat);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->showSlotMap->setChecked(m_settings.m_showSlotMap);
    ui->slotMapWidget->setVisible(m_settings.m_showSlotMap);

    // Order and size columns
    QHeaderView *header = ui->messages->horizontalHeader();
    for (int i = 0; i < AISDEMOD_MESSAGE_COLUMNS; i++)
    {
        bool hidden = m_settings.m_messageColumnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        messagesMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_messageColumnSizes[i] > 0)
            ui->messages->setColumnWidth(i, m_settings.m_messageColumnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_messageColumnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void AISDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void AISDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void AISDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_aisDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0)
    {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
        updateSlotMap();
    }

    m_tickCount++;
}

void AISDemodGUI::on_showSlotMap_clicked(bool checked)
{
    ui->slotMapWidget->setVisible(checked);
    m_settings.m_showSlotMap = checked;
    applySettings();
}

void AISDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void AISDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received frames to", "", "*.csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySettings();
        }
    }
}

// Read .csv log and process as received frames
void AISDemodGUI::on_logOpen_clicked()
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
                QDateTime startTime = QDateTime::currentDateTime();
                m_loadingData = true;
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Date", "Time", "Data", "Slot"}, error);
                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int dataCol = colIndexes.value("Data");
                    int slotCol = colIndexes.value("Slot");
                    int slotsCol = colIndexes.contains("Slots") ? colIndexes.value("Slots") : -1;
                    int maxCol = std::max({dateCol, timeCol, dataCol, slotCol, slotsCol});

                    QMessageBox dialog(this);
                    dialog.setText("Reading messages");
                    dialog.addButton(QMessageBox::Cancel);
                    dialog.show();
                    QApplication::processEvents();
                    int count = 0;
                    bool cancelled = false;
                    QStringList cols;

                    QList<ObjectPipe*> aisPipes;
                    MainCore::instance()->getMessagePipes().getMessagePipes(m_aisDemod, "ais", aisPipes);

                    while (!cancelled && CSV::readRow(in, &cols))
                    {
                        if (cols.size() > maxCol)
                        {
                            QDate date = QDate::fromString(cols[dateCol]);
                            QTime time = QTime::fromString(cols[timeCol]);
                            QDateTime dateTime(date, time);
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());
                            int slot = cols[slotCol].toInt();
                            int totalSlots = slotsCol == -1 ? 1 : cols[slotsCol].toInt();

                            // Add to table
                            messageReceived(bytes, dateTime, slot, totalSlots);

                            // Forward to AIS feature
                            for (const auto& pipe : aisPipes)
                            {
                                MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                                MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_aisDemod, bytes, dateTime);
                                messageQueue->push(msg);
                            }

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
                }
                else
                {
                    QMessageBox::critical(this, "AIS Demod", error);
                }
                m_loadingData = false;
                ui->messages->setSortingEnabled(true);
                QDateTime finishTime = QDateTime::currentDateTime();
                qDebug() << "Read CSV in " << startTime.secsTo(finishTime);
            }
            else
            {
                QMessageBox::critical(this, "AIS Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void AISDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &AISDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &AISDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &AISDemodGUI::on_fmDev_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &AISDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->filterMMSI, &QLineEdit::editingFinished, this, &AISDemodGUI::on_filterMMSI_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &AISDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &AISDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &AISDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &AISDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->udpFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AISDemodGUI::on_udpFormat_currentIndexChanged);
    QObject::connect(ui->messages, &QTableWidget::cellDoubleClicked, this, &AISDemodGUI::on_messages_cellDoubleClicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &AISDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &AISDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &AISDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->showSlotMap, &ButtonSwitch::clicked, this, &AISDemodGUI::on_showSlotMap_clicked);
}

void AISDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
