///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

#include "droneiddemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspcommands.h"
#include "ui_droneiddemodgui.h"
#include "plugin/pluginapi.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/droneid.h"
#include "gui/messagedialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/datetimedelegate.h"
#include "gui/decimaldelegate.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/tabletapandhold.h"
#include "maincore.h"
#include "feature/featurewebapiutils.h"
#include "channel/channelwebapiutils.h"

#include "droneiddemod.h"

void DroneIDDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing chars are for sort arrow
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);
    ui->messages->setItem(row, COL_DATE_TIME, new QTableWidgetItem("2015/04/15 10:17:00-"));
    ui->messages->setItem(row, COL_SERIAL, new QTableWidgetItem("1581F5FKD229400T"));
    ui->messages->setItem(row, COL_MODEL, new QTableWidgetItem("Mavic 3 Enterprise"));
    ui->messages->setItem(row, COL_TYPE, new QTableWidgetItem("Purpose-"));
    ui->messages->setItem(row, COL_SEQUENCE, new QTableWidgetItem("65535-"));
    ui->messages->setItem(row, COL_LATITUDE, new QTableWidgetItem("-90.000000-"));
    ui->messages->setItem(row, COL_LONGITUDE, new QTableWidgetItem("-180.000000-"));
    ui->messages->setItem(row, COL_ALTITUDE, new QTableWidgetItem("10000-"));
    ui->messages->setItem(row, COL_HEIGHT, new QTableWidgetItem("1000.0-"));
    ui->messages->setItem(row, COL_SPEED, new QTableWidgetItem("100.0-"));
    ui->messages->setItem(row, COL_VERTICAL_SPEED, new QTableWidgetItem("-100.0-"));
    ui->messages->setItem(row, COL_COURSE, new QTableWidgetItem("359-"));
    ui->messages->setItem(row, COL_YAW, new QTableWidgetItem("-180.0-"));
    ui->messages->setItem(row, COL_PILOT_LATITUDE, new QTableWidgetItem("-90.000000-"));
    ui->messages->setItem(row, COL_PILOT_LONGITUDE, new QTableWidgetItem("-180.000000-"));
    ui->messages->setItem(row, COL_HOME_LATITUDE, new QTableWidgetItem("-90.000000-"));
    ui->messages->setItem(row, COL_HOME_LONGITUDE, new QTableWidgetItem("-180.000000-"));
    ui->messages->setItem(row, COL_UUID, new QTableWidgetItem("0123456789012345678"));
    ui->messages->setItem(row, COL_STATE, new QTableWidgetItem("Motors on, In air, GPS"));
    ui->messages->setItem(row, COL_GPS_TIME, new QTableWidgetItem("2015/04/15 10:17:00"));
    ui->messages->setItem(row, COL_SNR, new QTableWidgetItem("30.0-"));
    ui->messages->setItem(row, COL_CFO, new QTableWidgetItem("-100000-"));
    ui->messages->setItem(row, COL_FREQUENCY, new QTableWidgetItem("5831.5-"));
    ui->messages->setItem(row, COL_CRC, new QTableWidgetItem("CRC-"));
    ui->messages->resizeColumnsToContents();
    ui->messages->removeRow(row);
}

// Columns in table reordered
void DroneIDDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void DroneIDDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void DroneIDDemodGUI::messagesColumnSelectMenu(QPoint pos)
{
    m_messagesMenu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void DroneIDDemodGUI::messagesColumnSelectMenuChecked(bool checked)
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
QAction *DroneIDDemodGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

DroneIDDemodGUI* DroneIDDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    DroneIDDemodGUI* gui = new DroneIDDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void DroneIDDemodGUI::destroy()
{
    delete this;
}

void DroneIDDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray DroneIDDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DroneIDDemodGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

// Add row to table
// frequency is channel frequency in Hz the message was received on, or 0 if unknown
void DroneIDDemodGUI::messageReceived(const QByteArray& message, const QDateTime& dateTime, float snr, float cfo, qint64 frequency, bool live)
{
    DroneIDMessage *msg = DroneIDMessage::decode(message);

    if (!msg)
    {
        qDebug() << "DroneIDDemodGUI::messageReceived: Unsupported message: " << message.toHex();
        return;
    }

    // Is scroll bar at bottom
    QScrollBar *sb = ui->messages->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    // Add to messages table
    ui->messages->setSortingEnabled(false);

    // Limit table size, by removing oldest message. Table may be sorted, so search for it.
    if (ui->messages->rowCount() >= m_maxRows)
    {
        int oldestRow = 0;
        QDateTime oldest;
        for (int i = 0; i < ui->messages->rowCount(); i++)
        {
            QDateTime rowDateTime = ui->messages->item(i, COL_DATE_TIME)->data(Qt::DisplayRole).toDateTime();
            if (!oldest.isValid() || (rowDateTime < oldest))
            {
                oldest = rowDateTime;
                oldestRow = i;
            }
        }
        ui->messages->removeRow(oldestRow);
    }

    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);

    QTableWidgetItem *items[DRONEIDDEMOD_COLUMNS];
    for (int i = 0; i < DRONEIDDEMOD_COLUMNS; i++)
    {
        items[i] = new QTableWidgetItem();
        ui->messages->setItem(row, i, items[i]);
    }

    items[COL_DATE_TIME]->setData(Qt::DisplayRole, dateTime);
    items[COL_SERIAL]->setText(msg->m_serial);
    items[COL_CRC]->setText(msg->m_crcValid ? "OK" : "Bad");

    if (msg->isFlightInformation())
    {
        items[COL_MODEL]->setText(msg->getProductTypeName());
        items[COL_TYPE]->setText("Flight");
        items[COL_SEQUENCE]->setData(Qt::DisplayRole, msg->m_sequence);
        if (msg->hasPosition())
        {
            items[COL_LATITUDE]->setData(Qt::DisplayRole, msg->m_latitude);
            items[COL_LONGITUDE]->setData(Qt::DisplayRole, msg->m_longitude);
        }
        items[COL_ALTITUDE]->setData(Qt::DisplayRole, msg->m_altitude);
        items[COL_HEIGHT]->setData(Qt::DisplayRole, msg->m_height);
        items[COL_SPEED]->setData(Qt::DisplayRole, msg->getSpeed());
        items[COL_VERTICAL_SPEED]->setData(Qt::DisplayRole, msg->m_vUp);
        if (msg->getSpeed() > 0.0f) {
            items[COL_COURSE]->setData(Qt::DisplayRole, msg->getCourse());
        }
        items[COL_YAW]->setData(Qt::DisplayRole, msg->m_yaw);
        if (msg->hasPilotPosition())
        {
            items[COL_PILOT_LATITUDE]->setData(Qt::DisplayRole, msg->m_pilotLatitude);
            items[COL_PILOT_LONGITUDE]->setData(Qt::DisplayRole, msg->m_pilotLongitude);
        }
        if (msg->hasHomePosition())
        {
            items[COL_HOME_LATITUDE]->setData(Qt::DisplayRole, msg->m_homeLatitude);
            items[COL_HOME_LONGITUDE]->setData(Qt::DisplayRole, msg->m_homeLongitude);
        }
        items[COL_UUID]->setText(msg->m_uuid);
        items[COL_STATE]->setText(msg->getStateInfoString());
        items[COL_STATE]->setToolTip(QString("0x%1").arg(msg->m_stateInfo, 4, 16, QChar('0')));
        if (msg->m_gpsTime.isValid()) {
            items[COL_GPS_TIME]->setData(Qt::DisplayRole, msg->m_gpsTime);
        }
    }
    else
    {
        items[COL_TYPE]->setText("Purpose");
        items[COL_TYPE]->setToolTip(msg->m_purpose.toHex());
    }

    if (live)
    {
        items[COL_SNR]->setData(Qt::DisplayRole, snr);
        items[COL_CFO]->setData(Qt::DisplayRole, (int) std::round(cfo));
    }
    if (frequency != 0) {
        items[COL_FREQUENCY]->setData(Qt::DisplayRole, frequency / 1e6);
    }

    filterRow(row);
    ui->messages->setSortingEnabled(true);
    if (scrollToBottom) {
        ui->messages->scrollToBottom();
    }

    delete msg;
}

void DroneIDDemodGUI::burstResult(const DroneIDDemod::MsgBurstResult& result)
{
    if (result.getDetected())
    {
        ui->snr->setText(QString("%1 dB %2 Hz")
            .arg(result.getSNR(), 0, 'f', 1)
            .arg((int) std::round(result.getCFO())));
    }

    if (result.getDecoded() && !result.getMessage().isEmpty()) {
        messageReceived(result.getMessage(), result.getDateTime(), result.getSNR(), result.getCFO(), result.getFrequency(), true);
    }
}

bool DroneIDDemodGUI::handleMessage(const Message& message)
{
    if (DroneIDDemod::MsgConfigureDroneIDDemod::match(message))
    {
        qDebug("DroneIDDemodGUI::handleMessage: DroneIDDemod::MsgConfigureDroneIDDemod");
        const DroneIDDemod::MsgConfigureDroneIDDemod& cfg = (DroneIDDemod::MsgConfigureDroneIDDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DroneIDDemod::MsgBurstResult::match(message))
    {
        burstResult((const DroneIDDemod::MsgBurstResult&) message);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        updateChannelSelection();
        return true;
    }

    return false;
}

void DroneIDDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != nullptr)
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void DroneIDDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateChannelSelection();
    applySettings(QStringList("inputFrequencyOffset"));
}

void DroneIDDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DroneIDDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    updateChannelSelection();
    applySettings(QStringList("inputFrequencyOffset"));
}

// Select entry in channel combo box that matches current frequency, or Custom if none match
void DroneIDDemodGUI::updateChannelSelection()
{
    qint64 frequency = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;
    int index = 0;

    for (int i = 1; i < ui->channel->count(); i++)
    {
        qint64 channelFrequency = (qint64) std::round(ui->channel->itemText(i).toDouble() * 1e6);
        // Allow for small differences due to LO correction or tuning resolution
        if (std::abs(channelFrequency - frequency) <= 1000)
        {
            index = i;
            break;
        }
    }

    ui->channel->blockSignals(true);
    ui->channel->setCurrentIndex(index);
    ui->channel->blockSignals(false);
}

void DroneIDDemodGUI::on_channel_currentIndexChanged(int index)
{
    if (index <= 0) {
        return;
    }

    // Manually selecting a channel stops scanning, otherwise scan would immediately move away
    if (m_settings.m_scan)
    {
        m_settings.m_scan = false;
        ui->scan->blockSignals(true);
        ui->scan->setChecked(false);
        ui->scan->blockSignals(false);
        ui->scan->setText("Scan");
        applySettings(QStringList("scan"));
    }

    // Tune the device so the channel, at its current offset, is on the selected frequency
    qint64 frequency = (qint64) std::round(ui->channel->itemText(index).toDouble() * 1e6);
    qint64 centerFrequency = frequency - m_settings.m_inputFrequencyOffset;

    if (!ChannelWebAPIUtils::setCenterFrequency(m_droneIDDemod->getDeviceSetIndex(), centerFrequency))
    {
        MessageDialog::warning(this, "DroneID Demodulator",
            QString("Failed to set device center frequency to %1 MHz.").arg(centerFrequency / 1e6, 0, 'f', 3));
        updateChannelSelection();
    }
}

void DroneIDDemodGUI::on_scan_toggled(bool checked)
{
    m_settings.m_scan = checked;
    if (!checked) {
        ui->scan->setText("Scan");
    }
    applySettings(QStringList("scan"));
}

void DroneIDDemodGUI::on_scanBand_currentIndexChanged(int index)
{
    m_settings.m_scanBand = (DroneIDDemodSettings::ScanBand) index;
    applySettings(QStringList("scanBand"));
}

void DroneIDDemodGUI::on_scan_rightClicked(const QPoint& point)
{
    (void) point;

    QDialog dialog(this);
    dialog.setWindowTitle("Scan Settings");
    QFormLayout *layout = new QFormLayout(&dialog);

    QDoubleSpinBox *dwell = new QDoubleSpinBox();
    dwell->setRange(0.5, 60.0);
    dwell->setDecimals(1);
    dwell->setSingleStep(0.1);
    dwell->setSuffix(" s");
    dwell->setValue(m_settings.m_scanDwell);
    dwell->setToolTip("Time to wait on each channel for a burst. Bursts are transmitted every 640ms.");
    layout->addRow("Dwell", dwell);

    QDoubleSpinBox *hold = new QDoubleSpinBox();
    hold->setRange(0.5, 600.0);
    hold->setDecimals(1);
    hold->setSingleStep(0.5);
    hold->setSuffix(" s");
    hold->setValue(m_settings.m_scanHold);
    hold->setToolTip("Time to remain on a channel after the last burst was detected.");
    layout->addRow("Hold", hold);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);

    new DialogPositioner(&dialog, true);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_scanDwell = (float) dwell->value();
        m_settings.m_scanHold = (float) hold->value();
        applySettings({"scanDwell", "scanHold"});
    }
}

void DroneIDDemodGUI::on_threshold_valueChanged(int value)
{
    m_settings.m_detectionThreshold = value / 100.0f;
    ui->thresholdText->setText(QString::number(m_settings.m_detectionThreshold, 'f', 2));
    applySettings(QStringList("detectionThreshold"));
}

void DroneIDDemodGUI::on_turboIterations_valueChanged(int value)
{
    m_settings.m_turboIterations = value;
    applySettings(QStringList("turboIterations"));
}

void DroneIDDemodGUI::on_mapTimeout_valueChanged(int value)
{
    m_settings.m_mapTimeout = value;
    applySettings(QStringList("mapTimeout"));
}

void DroneIDDemodGUI::on_mapAltitude_currentIndexChanged(int index)
{
    m_settings.m_mapAltitude = (DroneIDDemodSettings::MapAltitude) index;
    applySettings(QStringList("mapAltitude"));
}

void DroneIDDemodGUI::on_filterSerial_editingFinished()
{
    m_settings.m_filterSerial = ui->filterSerial->text();
    filter();
    applySettings(QStringList("filterSerial"));
}

void DroneIDDemodGUI::on_clearTable_clicked()
{
    ui->messages->setRowCount(0);
}

void DroneIDDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings(QStringList("udpEnabled"));
}

void DroneIDDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings(QStringList("udpAddress"));
}

void DroneIDDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings(QStringList("udpPort"));
}

void DroneIDDemodGUI::on_messages_cellDoubleClicked(int row, int column)
{
    QString serial = ui->messages->item(row, COL_SERIAL)->text();

    if ((column == COL_LATITUDE) || (column == COL_LONGITUDE)) {
        FeatureWebAPIUtils::mapFind(serial);
    } else if ((column == COL_PILOT_LATITUDE) || (column == COL_PILOT_LONGITUDE)) {
        FeatureWebAPIUtils::mapFind(serial + " Pilot");
    } else if ((column == COL_HOME_LATITUDE) || (column == COL_HOME_LONGITUDE)) {
        FeatureWebAPIUtils::mapFind(serial + " Home");
    }
}

void DroneIDDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterSerial != "")
    {
        QRegularExpression re(QRegularExpression::anchoredPattern(m_settings.m_filterSerial));
        QTableWidgetItem *serialItem = ui->messages->item(row, COL_SERIAL);
        QRegularExpressionMatch match = re.match(serialItem->text());
        if (!match.hasMatch()) {
            hidden = true;
        }
    }
    ui->messages->setRowHidden(row, hidden);
}

void DroneIDDemodGUI::filter()
{
    for (int i = 0; i < ui->messages->rowCount(); i++) {
        filterRow(i);
    }
}

void DroneIDDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList("rollupState"));
}

void DroneIDDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_droneIDDemod->getNumberOfDeviceStreams());
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

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(QStringList({"title", "rgbColor", "useReverseAPI", "reverseAPIAddress",
                                    "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex", "streamIndex"}));
    }

    resetContextMenuType();
}

DroneIDDemodGUI::DroneIDDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::DroneIDDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0),
    m_burstsDetected(-1),
    m_burstsDecoded(-1)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddroneid/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_droneIDDemod = reinterpret_cast<DroneIDDemod*>(rxChannel);
    m_droneIDDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);
    for (auto frequency : DroneIDDemodSettings::getScanFrequencies(DroneIDDemodSettings::BAND_2_4_AND_5_8_GHZ)) {
        ui->channel->addItem(QString::number(frequency / 1e6, 'f', 1));
    }
    ui->scan->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_occupiedBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("DroneID Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
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
    m_messagesMenu = new QMenu(ui->messages);
    for (int i = 0; i < ui->messages->horizontalHeader()->count(); i++)
    {
        QString text = ui->messages->horizontalHeaderItem(i)->text();
        m_messagesMenu->addAction(createCheckableItem(text, i, true, SLOT(messagesColumnSelectMenuChecked())));
    }
    ui->messages->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(messagesColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(messages_sectionMoved(int, int, int)));
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(messages_sectionResized(int, int, int)));
    ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->messages);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &DroneIDDemodGUI::customContextMenuRequested);

    ui->messages->setItemDelegateForColumn(COL_DATE_TIME, new DateTimeDelegate("yyyy/MM/dd hh:mm:ss", ui->messages));
    ui->messages->setItemDelegateForColumn(COL_LATITUDE, new DecimalDelegate(6, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_LONGITUDE, new DecimalDelegate(6, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_HEIGHT, new DecimalDelegate(1, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_SPEED, new DecimalDelegate(1, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_VERTICAL_SPEED, new DecimalDelegate(1, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_COURSE, new DecimalDelegate(0, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_YAW, new DecimalDelegate(1, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_PILOT_LATITUDE, new DecimalDelegate(6, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_PILOT_LONGITUDE, new DecimalDelegate(6, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_HOME_LATITUDE, new DecimalDelegate(6, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_HOME_LONGITUDE, new DecimalDelegate(6, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_GPS_TIME, new DateTimeDelegate("yyyy/MM/dd hh:mm:ss", ui->messages));
    ui->messages->setItemDelegateForColumn(COL_SNR, new DecimalDelegate(1, ui->messages));
    ui->messages->setItemDelegateForColumn(COL_FREQUENCY, new DecimalDelegate(1, ui->messages));

    displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

void DroneIDDemodGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->messages->itemAt(pos);
    if (item)
    {
        int row = item->row();
        QString serial = ui->messages->item(row, COL_SERIAL)->text();

        QMenu* tableContextMenu = new QMenu(ui->messages);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell
        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);

        // Find on Map
        tableContextMenu->addSeparator();
        QAction* findDroneAction = new QAction(QString("Find drone %1 on map").arg(serial), tableContextMenu);
        connect(findDroneAction, &QAction::triggered, this, [serial]()->void {
            FeatureWebAPIUtils::mapFind(serial);
        });
        tableContextMenu->addAction(findDroneAction);
        QAction* findPilotAction = new QAction(QString("Find pilot of %1 on map").arg(serial), tableContextMenu);
        connect(findPilotAction, &QAction::triggered, this, [serial]()->void {
            FeatureWebAPIUtils::mapFind(serial + " Pilot");
        });
        tableContextMenu->addAction(findPilotAction);
        QAction* findHomeAction = new QAction(QString("Find home of %1 on map").arg(serial), tableContextMenu);
        connect(findHomeAction, &QAction::triggered, this, [serial]()->void {
            FeatureWebAPIUtils::mapFind(serial + " Home");
        });
        tableContextMenu->addAction(findHomeAction);

        tableContextMenu->popup(ui->messages->viewport()->mapToGlobal(pos));
    }
}

DroneIDDemodGUI::~DroneIDDemodGUI()
{
    delete ui;
}

void DroneIDDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DroneIDDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        DroneIDDemod::MsgConfigureDroneIDDemod* message = DroneIDDemod::MsgConfigureDroneIDDemod::create(settingsKeys, m_settings, force);
        m_droneIDDemod->getInputMessageQueue()->push(message);
    }
}

void DroneIDDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_occupiedBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    updateChannelSelection();

    // Block signals, so any rounding or clamping by the widgets doesn't modify the settings
    ui->threshold->blockSignals(true);
    ui->threshold->setValue((int) std::round(m_settings.m_detectionThreshold * 100.0f));
    ui->threshold->blockSignals(false);
    ui->thresholdText->setText(QString::number(m_settings.m_detectionThreshold, 'f', 2));
    ui->turboIterations->blockSignals(true);
    ui->turboIterations->setValue(m_settings.m_turboIterations);
    ui->turboIterations->blockSignals(false);
    ui->mapTimeout->blockSignals(true);
    ui->mapTimeout->setValue(m_settings.m_mapTimeout);
    ui->mapTimeout->blockSignals(false);
    ui->mapAltitude->blockSignals(true);
    ui->mapAltitude->setCurrentIndex((int) m_settings.m_mapAltitude);
    ui->mapAltitude->blockSignals(false);
    ui->scan->blockSignals(true);
    ui->scan->setChecked(m_settings.m_scan);
    ui->scan->blockSignals(false);
    ui->scanBand->blockSignals(true);
    ui->scanBand->setCurrentIndex((int) m_settings.m_scanBand);
    ui->scanBand->blockSignals(false);

    updateIndexLabel();

    ui->filterSerial->setText(m_settings.m_filterSerial);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->useFileTime->setChecked(m_settings.m_useFileTime);

    // Order and size columns
    QHeaderView *header = ui->messages->horizontalHeader();
    for (int i = 0; i < DRONEIDDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        m_messagesMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0) {
            ui->messages->setColumnWidth(i, m_settings.m_columnSizes[i]);
        }
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void DroneIDDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void DroneIDDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void DroneIDDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_droneIDDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0)
    {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
        if (m_settings.m_scan) {
            ui->scan->setText(m_droneIDDemod->isScanLocked() ? "Lock" : "Scan");
        }
    }

    if (m_tickCount % 20 == 0)
    {
        int sampleRate = m_droneIDDemod->getChannelSampleRate();
        ui->sampleRate->setText(QString("%1 MS/s").arg(sampleRate / 1e6, 0, 'f', 2));
        if (sampleRate < DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE)
        {
            ui->sampleRate->setStyleSheet("QLabel { color: red }");
            ui->sampleRate->setToolTip("Channel sample rate is too low. Device sample rate must be at least 15.36 MS/s.");
        }
        else
        {
            ui->sampleRate->setStyleSheet("");
            ui->sampleRate->setToolTip("Channel sample rate");
        }

        int detected = m_droneIDDemod->getBurstsDetected();
        int decoded = m_droneIDDemod->getBurstsDecoded();
        if ((detected != m_burstsDetected) || (decoded != m_burstsDecoded))
        {
            ui->bursts->setText(QString("%1 / %2").arg(detected).arg(decoded));
            m_burstsDetected = detected;
            m_burstsDecoded = decoded;
        }
        int dropped = m_droneIDDemod->getBurstsDropped();
        ui->bursts->setToolTip(QString("Bursts detected / decoded (%1 dropped due to decoder being busy)").arg(dropped));
    }

    m_tickCount++;
}

void DroneIDDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings(QStringList("logEnabled"));
}

void DroneIDDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received messages to", "", "*.csv");
    fileDialog.setDefaultSuffix("csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
            applySettings(QStringList("logFilename"));
        }
    }
}

// Read .csv log and process as received messages
void DroneIDDemodGUI::on_logOpen_clicked()
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
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int dataCol = colIndexes.value("Data");
                    int frequencyCol = colIndexes.value("Frequency (MHz)", -1); // Not in older logs
                    int maxCol = std::max({dateCol, timeCol, dataCol});

                    QMessageBox dialog(this);
                    dialog.setText("Reading messages");
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

                            qint64 frequency = 0;
                            if ((frequencyCol >= 0) && (frequencyCol < cols.size())) {
                                frequency = (qint64) std::round(cols[frequencyCol].toDouble() * 1e6);
                            }

                            messageReceived(bytes, dateTime, 0.0f, 0.0f, frequency, false);

                            if (count % 100 == 0)
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
                    MessageDialog::critical(this, "DroneID Demodulator", error);
                }
            }
            else
            {
                MessageDialog::critical(this, "DroneID Demodulator", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void DroneIDDemodGUI::on_useFileTime_toggled(bool checked)
{
    m_settings.m_useFileTime = checked;
    applySettings(QStringList("useFileTime"));
}

void DroneIDDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DroneIDDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->channel, qOverload<int>(&QComboBox::currentIndexChanged), this, &DroneIDDemodGUI::on_channel_currentIndexChanged);
    QObject::connect(ui->scan, &ButtonSwitch::toggled, this, &DroneIDDemodGUI::on_scan_toggled);
    QObject::connect(ui->scan, &ButtonSwitch::customContextMenuRequested, this, &DroneIDDemodGUI::on_scan_rightClicked);
    QObject::connect(ui->scanBand, qOverload<int>(&QComboBox::currentIndexChanged), this, &DroneIDDemodGUI::on_scanBand_currentIndexChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &DroneIDDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->turboIterations, qOverload<int>(&QSpinBox::valueChanged), this, &DroneIDDemodGUI::on_turboIterations_valueChanged);
    QObject::connect(ui->mapTimeout, qOverload<int>(&QSpinBox::valueChanged), this, &DroneIDDemodGUI::on_mapTimeout_valueChanged);
    QObject::connect(ui->mapAltitude, qOverload<int>(&QComboBox::currentIndexChanged), this, &DroneIDDemodGUI::on_mapAltitude_currentIndexChanged);
    QObject::connect(ui->filterSerial, &QLineEdit::editingFinished, this, &DroneIDDemodGUI::on_filterSerial_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &DroneIDDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &DroneIDDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &DroneIDDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &DroneIDDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->messages, &QTableWidget::cellDoubleClicked, this, &DroneIDDemodGUI::on_messages_cellDoubleClicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &DroneIDDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &DroneIDDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &DroneIDDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->useFileTime, &ButtonSwitch::toggled, this, &DroneIDDemodGUI::on_useFileTime_toggled);
}

void DroneIDDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
