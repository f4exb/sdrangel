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
#include <QMessageBox>
#include <QAction>
#include <QRegExp>
#include <QFileDialog>
#include <QScrollBar>

#include "packetdemodgui.h"
#include "util/ax25.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_packetdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/morse.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "gui/crightclickenabler.h"
#include "gui/dialogpositioner.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "packetdemod.h"
#include "packetdemodsink.h"

#define PACKET_COL_FROM            0
#define PACKET_COL_TO              1
#define PACKET_COL_VIA             2
#define PACKET_COL_TYPE            3
#define PACKET_COL_PID             4
#define PACKET_COL_DATA_ASCII      5
#define PACKET_COL_DATA_HEX        6

void PacketDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->packets->rowCount();
    ui->packets->setRowCount(row + 1);
    ui->packets->setItem(row, PACKET_COL_FROM, new QTableWidgetItem("123456-15-"));
    ui->packets->setItem(row, PACKET_COL_TO, new QTableWidgetItem("123456-15-"));
    ui->packets->setItem(row, PACKET_COL_VIA, new QTableWidgetItem("123456-15-"));
    ui->packets->setItem(row, PACKET_COL_TYPE, new QTableWidgetItem("Type-"));
    ui->packets->setItem(row, PACKET_COL_PID, new QTableWidgetItem("PID-"));
    ui->packets->setItem(row, PACKET_COL_DATA_ASCII, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->packets->setItem(row, PACKET_COL_DATA_HEX, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->packets->resizeColumnsToContents();
    ui->packets->removeRow(row);
}

// Columns in table reordered
void PacketDemodGUI::packets_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void PacketDemodGUI::packets_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void PacketDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->packets->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void PacketDemodGUI::columnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->packets->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *PacketDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

PacketDemodGUI* PacketDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    PacketDemodGUI* gui = new PacketDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void PacketDemodGUI::destroy()
{
    delete this;
}

void PacketDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray PacketDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool PacketDemodGUI::deserialize(const QByteArray& data)
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

// Add row to table
void PacketDemodGUI::packetReceived(QByteArray packet)
{
    AX25Packet ax25;

    if (ax25.decode(packet))
    {
        // Is scroll bar at bottom
        QScrollBar *sb = ui->packets->verticalScrollBar();
        bool scrollToBottom = sb->value() == sb->maximum();

        ui->packets->setSortingEnabled(false);
        int row = ui->packets->rowCount();
        ui->packets->setRowCount(row + 1);

        QTableWidgetItem *fromItem = new QTableWidgetItem();
        QTableWidgetItem *toItem = new QTableWidgetItem();
        QTableWidgetItem *viaItem = new QTableWidgetItem();
        QTableWidgetItem *typeItem = new QTableWidgetItem();
        QTableWidgetItem *pidItem = new QTableWidgetItem();
        QTableWidgetItem *dataASCIIItem = new QTableWidgetItem();
        QTableWidgetItem *dataHexItem = new QTableWidgetItem();
        ui->packets->setItem(row, PACKET_COL_FROM, fromItem);
        ui->packets->setItem(row, PACKET_COL_TO, toItem);
        ui->packets->setItem(row, PACKET_COL_VIA, viaItem);
        ui->packets->setItem(row, PACKET_COL_TYPE, typeItem);
        ui->packets->setItem(row, PACKET_COL_PID, pidItem);
        ui->packets->setItem(row, PACKET_COL_DATA_ASCII, dataASCIIItem);
        ui->packets->setItem(row, PACKET_COL_DATA_HEX, dataHexItem);
        fromItem->setText(ax25.m_from);
        toItem->setText(ax25.m_to);
        viaItem->setText(ax25.m_via);
        typeItem->setText(ax25.m_type);
        pidItem->setText(ax25.m_pid);
        dataASCIIItem->setText(ax25.m_dataASCII);
        dataHexItem->setText(ax25.m_dataHex);
        ui->packets->setSortingEnabled(true);
        if (scrollToBottom) {
            ui->packets->scrollToBottom();
        }
        filterRow(row);
    }
    else
        qDebug() << "Unsupported AX.25 packet: " << packet;
}

bool PacketDemodGUI::handleMessage(const Message& message)
{
    if (PacketDemod::MsgConfigurePacketDemod::match(message))
    {
        qDebug("PacketDemodGUI::handleMessage: PacketDemod::MsgConfigurePacketDemod");
        const PacketDemod::MsgConfigurePacketDemod& cfg = (PacketDemod::MsgConfigurePacketDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
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
        packetReceived(report.getPacket());
        return true;
    }

    return false;
}

void PacketDemodGUI::handleInputMessages()
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

void PacketDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void PacketDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void PacketDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void PacketDemodGUI::on_mode_currentIndexChanged(int value)
{
    (void) value;
    QString mode = ui->mode->currentText();
    // TODO: Support 9600 FSK
}

void PacketDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void PacketDemodGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void PacketDemodGUI::on_filterFrom_editingFinished()
{
    m_settings.m_filterFrom = ui->filterFrom->text();
    filter();
    applySettings();
}

void PacketDemodGUI::on_filterTo_editingFinished()
{
    m_settings.m_filterTo = ui->filterTo->text();
    filter();
    applySettings();
}

void PacketDemodGUI::on_filterPID_stateChanged(int state)
{
    m_settings.m_filterPID = state==Qt::Checked ? "f0" : "";
    filter();
    applySettings();
}

void PacketDemodGUI::on_clearTable_clicked()
{
    ui->packets->setRowCount(0);
}

void PacketDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void PacketDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void PacketDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void PacketDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterFrom != "")
    {
        QRegExp re(m_settings.m_filterFrom);
        QTableWidgetItem *fromItem = ui->packets->item(row, PACKET_COL_FROM);
        if (!re.exactMatch(fromItem->text()))
            hidden = true;
    }
    if (m_settings.m_filterTo != "")
    {
        QRegExp re(m_settings.m_filterTo);
        QTableWidgetItem *toItem = ui->packets->item(row, PACKET_COL_TO);
        if (!re.exactMatch(toItem->text()))
            hidden = true;
    }
    if (m_settings.m_filterPID != "")
    {
        QTableWidgetItem *pidItem = ui->packets->item(row, PACKET_COL_PID);
        if (pidItem->text() != m_settings.m_filterPID)
            hidden = true;
    }
    ui->packets->setRowHidden(row, hidden);
}

void PacketDemodGUI::filter()
{
    for (int i = 0; i < ui->packets->rowCount(); i++)
    {
        filterRow(i);
    }
}

void PacketDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void PacketDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_packetDemod->getNumberOfDeviceStreams());
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

        applySettings();
    }

    resetContextMenuType();
}

PacketDemodGUI::PacketDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::PacketDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodpacket/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_packetDemod = reinterpret_cast<PacketDemod*>(rxChannel);
    m_packetDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Packet Demodulator");
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
    ui->packets->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->packets->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    menu = new QMenu(ui->packets);
    for (int i = 0; i < ui->packets->horizontalHeader()->count(); i++)
    {
        QString text = ui->packets->horizontalHeaderItem(i)->text();
        menu->addAction(createCheckableItem(text, i, true));
    }
    ui->packets->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->packets->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->packets->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(packets_sectionMoved(int, int, int)));
    connect(ui->packets->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(packets_sectionResized(int, int, int)));

    displaySettings();
    makeUIConnections();
    applySettings(true);
}

PacketDemodGUI::~PacketDemodGUI()
{
    delete ui;
}

void PacketDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PacketDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        PacketDemod::MsgConfigurePacketDemod* message = PacketDemod::MsgConfigurePacketDemod::create( m_settings, force);
        m_packetDemod->getInputMessageQueue()->push(message);
    }
}

void PacketDemodGUI::displaySettings()
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

    updateIndexLabel();

    ui->filterFrom->setText(m_settings.m_filterFrom);
    ui->filterTo->setText(m_settings.m_filterTo);
    ui->filterPID->setChecked(m_settings.m_filterPID == "f0");

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    // Order and size columns
    QHeaderView *header = ui->packets->horizontalHeader();
    for (int i = 0; i < PACKETDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->packets->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void PacketDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void PacketDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void PacketDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_packetDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    m_tickCount++;
}

void PacketDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void PacketDemodGUI::on_logFilename_clicked()
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
void PacketDemodGUI::on_logOpen_clicked()
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
                            packetReceived(bytes);
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
                    QMessageBox::critical(this, "Packet Demod", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "Packet Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void PacketDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &PacketDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PacketDemodGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &PacketDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &PacketDemodGUI::on_fmDev_valueChanged);
    QObject::connect(ui->filterFrom, &QLineEdit::editingFinished, this, &PacketDemodGUI::on_filterFrom_editingFinished);
    QObject::connect(ui->filterTo, &QLineEdit::editingFinished, this, &PacketDemodGUI::on_filterTo_editingFinished);
    QObject::connect(ui->filterPID, &QCheckBox::stateChanged, this, &PacketDemodGUI::on_filterPID_stateChanged);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &PacketDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &PacketDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &PacketDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &PacketDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &PacketDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &PacketDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &PacketDemodGUI::on_logOpen_clicked);
}

void PacketDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
