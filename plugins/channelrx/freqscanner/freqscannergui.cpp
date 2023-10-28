///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include <QAction>
#include <QClipboard>
#include <QMenu>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QRegExp>

#include "device/deviceset.h"
#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_freqscannergui.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/tabletapandhold.h"
#include "gui/dialogpositioner.h"
#include "gui/decimaldelegate.h"
#include "gui/frequencydelegate.h"
#include "gui/glspectrum.h"
#include "channel/channelwebapiutils.h"

#include "freqscannergui.h"
#include "freqscanneraddrangedialog.h"
#include "freqscanner.h"
#include "freqscannersink.h"

FreqScannerGUI* FreqScannerGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    FreqScannerGUI* gui = new FreqScannerGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void FreqScannerGUI::destroy()
{
    delete this;
}

void FreqScannerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
}

QByteArray FreqScannerGUI::serialize() const
{
    return m_settings.serialize();
}

bool FreqScannerGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applyAllSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool FreqScannerGUI::handleMessage(const Message& message)
{
    if (FreqScanner::MsgConfigureFreqScanner::match(message))
    {
        qDebug("FreqScannerGUI::handleMessage: FreqScanner::MsgConfigureFreqScanner");
        const FreqScanner::MsgConfigureFreqScanner& cfg = (FreqScanner::MsgConfigureFreqScanner&) message;
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
        if (m_basebandSampleRate != 0)
        {
            ui->deltaFrequency->setValueRange(true, 7, 0, m_basebandSampleRate/2);
            ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
            ui->channelBandwidth->setValueRange(true, 7, 0, m_basebandSampleRate);
        }
        if (m_channelMarker.getBandwidth() == 0) {
            m_channelMarker.setBandwidth(m_basebandSampleRate);
        }
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (FreqScanner::MsgReportChannels::match(message))
    {
        FreqScanner::MsgReportChannels& report = (FreqScanner::MsgReportChannels&)message;
        updateChannelsList(report.getChannels());
        return true;
    }
    else if (FreqScanner::MsgStatus::match(message))
    {
        FreqScanner::MsgStatus& report = (FreqScanner::MsgStatus&)message;
        ui->status->setText(report.getText());
        return true;
    }
    else if (FreqScanner::MsgReportScanning::match(message))
    {
        ui->status->setText("Scanning");
        ui->table->clearSelection();
        ui->channelPower->setText("-");
        return true;
    }
    else if (FreqScanner::MsgScanComplete::match(message))
    {
        ui->startStop->setChecked(false);
        return true;
    }
    else if (FreqScanner::MsgReportActiveFrequency::match(message))
    {
        FreqScanner::MsgReportActiveFrequency& report = (FreqScanner::MsgReportActiveFrequency&)message;
        qint64 f = report.getCenterFrequency();
        QString frequency;
        QString annotation;
        QList<QTableWidgetItem*> items = ui->table->findItems(QString::number(f), Qt::MatchExactly);
        if (items.size() > 0)
        {
            ui->table->selectRow(items[0]->row());
            frequency = ui->table->item(items[0]->row(), COL_FREQUENCY)->text();
            annotation = ui->table->item(items[0]->row(), COL_ANNOTATION)->text();
        }
        FrequencyDelegate freqDelegate("Auto", 3);
        QString formattedFrequency = freqDelegate.displayText(frequency, QLocale::system());
        ui->status->setText(QString("Active: %1 %2").arg(formattedFrequency).arg(annotation));
        return true;
    }
    else if (FreqScanner::MsgReportActivePower::match(message))
    {
        FreqScanner::MsgReportActivePower& report = (FreqScanner::MsgReportActivePower&)message;
        float power = report.getPower();
        ui->channelPower->setText(QString::number(power, 'f', 1));
        return true;
    }
    else if (FreqScanner::MsgReportScanRange::match(message))
    {
        FreqScanner::MsgReportScanRange& report = (FreqScanner::MsgReportScanRange&)message;
        m_channelMarker.setCenterFrequency(report.getCenterFrequency());
        m_channelMarker.setBandwidth(report.getTotalBandwidth());
        m_channelMarker.setVisible(report.getTotalBandwidth() < m_basebandSampleRate); // Hide marker if full bandwidth
        return true;
    }
    else if (FreqScanner::MsgScanResult::match(message))
    {
        FreqScanner::MsgScanResult& report = (FreqScanner::MsgScanResult&)message;
        QList<FreqScanner::MsgScanResult::ScanResult> results = report.getScanResults();

        // Clear column
        for (int i = 0; i < ui->table->rowCount(); i++)
        {
            QTableWidgetItem* item = ui->table->item(i, COL_POWER);
            item->setText("");
            item->setBackground(QBrush());
        }
        // Add results
        for (int i = 0; i < results.size(); i++)
        {
            qint64 freq = results[i].m_frequency;
            QList<QTableWidgetItem *> items = ui->table->findItems(QString::number(freq), Qt::MatchExactly);
            for (auto item : items) {
                int row = item->row();
                QTableWidgetItem* powerItem = ui->table->item(row, COL_POWER);
                powerItem->setData(Qt::DisplayRole, results[i].m_power);
                bool active = results[i].m_power >= m_settings.m_threshold;
                if (active)
                {
                    powerItem->setBackground(Qt::darkGreen);
                    QTableWidgetItem* activeCountItem = ui->table->item(row, COL_ACTIVE_COUNT);
                    activeCountItem->setData(Qt::DisplayRole, activeCountItem->data(Qt::DisplayRole).toInt() + 1);
                }
            }
        }

        return true;
    }
    return false;
}

void FreqScannerGUI::updateChannelsList(const QList<FreqScannerSettings::AvailableChannel>& channels)
{
    ui->channels->blockSignals(true);
    ui->channels->clear();

    for (const auto& channel : channels)
    {
        // Add channels in this device set, other than ourself (Don't use ChannelGUI::getDeviceSetIndex()/getIndex() as not valid when this is first called)
        if ((channel.m_deviceSetIndex == m_freqScanner->getDeviceSetIndex()) && (channel.m_channelIndex != m_freqScanner->getIndexInDeviceSet()))
        {
            QString name = QString("R%1:%2").arg(channel.m_deviceSetIndex).arg(channel.m_channelIndex);
            ui->channels->addItem(name);
        }
    }

    // Channel can be created after this plugin, so select it
    // if the chosen channel appears
    int channelIndex = ui->channels->findText(m_settings.m_channel);

    if (channelIndex >= 0) {
        ui->channels->setCurrentIndex(channelIndex);
    } else {
        ui->channels->setCurrentIndex(-1); // return to nothing selected
    }

    ui->channels->blockSignals(false);
}

void FreqScannerGUI::on_channels_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        m_settings.m_channel = ui->channels->currentText();
        applySetting("channel");
    }
}

void FreqScannerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void FreqScannerGUI::channelMarkerChangedByCursor()
{
}

void FreqScannerGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void FreqScannerGUI::on_deltaFrequency_changed(qint64 value)
{
    m_settings.m_channelFrequencyOffset = value;
    applySetting("channelFrequencyOffset");
}

void FreqScannerGUI::on_channelBandwidth_changed(qint64 value)
{
    m_settings.m_channelBandwidth = value;
    applySetting("channelBandwidth");
}

void FreqScannerGUI::on_scanTime_valueChanged(int value)
{
    ui->scanTimeText->setText(QString("%1 s").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_scanTime = value / 10.0;
    applySetting("scanTime");
}

void FreqScannerGUI::on_retransmitTime_valueChanged(int value)
{
    ui->retransmitTimeText->setText(QString("%1 s").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_retransmitTime = value / 10.0;
    applySetting("retransmitTime");
}

void FreqScannerGUI::on_tuneTime_valueChanged(int value)
{
    ui->tuneTimeText->setText(QString("%1 ms").arg(value));
    m_settings.m_tuneTime = value;
    applySetting("tuneTime");
}

void FreqScannerGUI::on_thresh_valueChanged(int value)
{
    ui->threshText->setText(QString("%1 dB").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_threshold = value / 10.0;
    applySetting("threshold");
}

void FreqScannerGUI::on_priority_currentIndexChanged(int index)
{
    m_settings.m_priority = (FreqScannerSettings::Priority)index;
    applySetting("priority");
}

void FreqScannerGUI::on_measurement_currentIndexChanged(int index)
{
    m_settings.m_measurement = (FreqScannerSettings::Measurement)index;
    applySetting("measurement");
}

void FreqScannerGUI::on_mode_currentIndexChanged(int index)
{
    m_settings.m_mode = (FreqScannerSettings::Mode)index;
    applySetting("mode");
}

void FreqScannerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

void FreqScannerGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_freqScanner->getNumberOfDeviceStreams());
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

FreqScannerGUI::FreqScannerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::FreqScannerGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/freqscanner/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_freqScanner = reinterpret_cast<FreqScanner*>(rxChannel);
    m_freqScanner->setMessageQueueToGUI(getInputMessageQueue());

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(true, 7, 0, 9999999);

    ui->channelBandwidth->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->channelBandwidth->setValueRange(true, 7, 0, 9999999);

    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Frequency Scanner");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true);

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
    ui->table->horizontalHeader()->setSectionsMovable(true);
    // Add context menu to allow hiding/showing of columns
    m_menu = new QMenu(ui->table);
    for (int i = 0; i < ui->table->horizontalHeader()->count(); i++)
    {
        QString text = ui->table->horizontalHeaderItem(i)->text();
        m_menu->addAction(createCheckableItem(text, i, true));
    }
    ui->table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->table->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->table->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(table_sectionMoved(int, int, int)));
    connect(ui->table->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(table_sectionResized(int, int, int)));
    // Context menu
    ui->table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->table, &QTableWidget::customContextMenuRequested, this, &FreqScannerGUI::table_customContextMenuRequested);
    TableTapAndHold* tableTapAndHold = new TableTapAndHold(ui->table);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &FreqScannerGUI::table_customContextMenuRequested);

    ui->startStop->setStyleSheet(QString("QToolButton{ background-color: blue; } QToolButton:checked{ background-color: green; }"));

    displaySettings();
    makeUIConnections();
    applyAllSettings();

    ui->table->setItemDelegateForColumn(COL_FREQUENCY, new FrequencyDelegate("Auto", 3));
    ui->table->setItemDelegateForColumn(COL_POWER, new DecimalDelegate(1));

    connect(m_deviceUISet->m_spectrum->getSpectrumView(), &GLSpectrumView::updateAnnotations, this, &FreqScannerGUI::updateAnnotations);
}

FreqScannerGUI::~FreqScannerGUI()
{
    delete ui;
}

void FreqScannerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void FreqScannerGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void FreqScannerGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        FreqScanner::MsgConfigureFreqScanner* message = FreqScanner::MsgConfigureFreqScanner::create(m_settings, settingsKeys, force);
        m_freqScanner->getInputMessageQueue()->push(message);
    }
}

void FreqScannerGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void FreqScannerGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_basebandSampleRate);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);
    int channelIndex = ui->channels->findText(m_settings.m_channel);
    if (channelIndex >= 0) {
        ui->channels->setCurrentIndex(channelIndex);
    }
    ui->deltaFrequency->setValue(m_settings.m_channelFrequencyOffset);
    ui->channelBandwidth->setValue(m_settings.m_channelBandwidth);
    ui->scanTime->setValue(m_settings.m_scanTime * 10.0);
    ui->scanTimeText->setText(QString("%1 s").arg(m_settings.m_scanTime, 0, 'f', 1));
    ui->retransmitTime->setValue(m_settings.m_retransmitTime * 10.0);
    ui->retransmitTimeText->setText(QString("%1 s").arg(m_settings.m_retransmitTime, 0, 'f', 1));
    ui->tuneTime->setValue(m_settings.m_tuneTime);
    ui->tuneTimeText->setText(QString("%1 ms").arg(m_settings.m_tuneTime));
    ui->thresh->setValue(m_settings.m_threshold * 10.0);
    ui->threshText->setText(QString("%1 dB").arg(m_settings.m_threshold, 0, 'f', 1));
    ui->priority->setCurrentIndex((int)m_settings.m_priority);
    ui->measurement->setCurrentIndex((int)m_settings.m_measurement);
    ui->mode->setCurrentIndex((int)m_settings.m_mode);

    ui->table->blockSignals(true);
    ui->table->setRowCount(0);
    for (int i = 0; i < m_settings.m_frequencies.size(); i++)
    {
        addRow(m_settings.m_frequencies[i], m_settings.m_enabled[i], m_settings.m_notes[i]);
        updateAnnotation(i);
    }
    ui->table->blockSignals(false);

    // Order and size columns
    QHeaderView* header = ui->table->horizontalHeader();
    for (int i = 0; i < m_settings.m_columnSizes.size(); i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        m_menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0) {
            ui->table->setColumnWidth(i, m_settings.m_columnSizes[i]);
        }
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void FreqScannerGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void FreqScannerGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void FreqScannerGUI::on_startStop_clicked(bool checked)
{
    if (checked)
    {
        FreqScanner::MsgStartScan* message = FreqScanner::MsgStartScan::create();
        m_freqScanner->getInputMessageQueue()->push(message);
    }
    else
    {
        FreqScanner::MsgStopScan* message = FreqScanner::MsgStopScan::create();
        m_freqScanner->getInputMessageQueue()->push(message);
    }
}

void FreqScannerGUI::addRow(qint64 frequency, bool enabled, const QString& notes)
{
    int row = ui->table->rowCount();
    ui->table->setRowCount(row + 1);

    // Must create before frequency so updateAnnotation can work
    QTableWidgetItem* annotationItem = new QTableWidgetItem();
    annotationItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->table->setItem(row, COL_ANNOTATION, annotationItem);

    ui->table->setItem(row, COL_FREQUENCY, new QTableWidgetItem(QString("%1").arg(frequency)));

    QTableWidgetItem *enableItem = new QTableWidgetItem();
    enableItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    enableItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    ui->table->setItem(row, COL_ENABLE, enableItem);

    QTableWidgetItem* powerItem = new QTableWidgetItem();
    powerItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->table->setItem(row, COL_POWER, powerItem);

    QTableWidgetItem *activeCountItem = new QTableWidgetItem();
    activeCountItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->table->setItem(row, COL_ACTIVE_COUNT, activeCountItem);
    activeCountItem->setData(Qt::DisplayRole, 0);

    QTableWidgetItem* notesItem = new QTableWidgetItem(notes);
    ui->table->setItem(row, COL_NOTES, notesItem);
}

void FreqScannerGUI::on_addSingle_clicked()
{
    addRow(0, true);
}

void FreqScannerGUI::on_addRange_clicked()
{
    FreqScannerAddRangeDialog dialog(m_settings.m_channelBandwidth, this);
    new DialogPositioner(&dialog, false);
    if (dialog.exec())
    {
        blockApplySettings(true);
        for (const auto f : dialog.m_frequencies) {
            addRow(f, true);
        }
        blockApplySettings(false);
        applySetting("frequencies");
    }
}

void FreqScannerGUI::on_remove_clicked()
{
    QList<QTableWidgetItem*> items = ui->table->selectedItems();

    for (auto item : items)
    {
        int row = ui->table->row(item);
        ui->table->removeRow(row);
        m_settings.m_frequencies.removeAt(row); // table_cellChanged isn't called for removeRow
        m_settings.m_enabled.removeAt(row);
        m_settings.m_notes.removeAt(row);
    }
    applySetting("frequencies");
}

void FreqScannerGUI::on_removeInactive_clicked()
{
    for (int i = ui->table->rowCount() - 1; i >= 0; i--)
    {
        if (ui->table->item(i, COL_ACTIVE_COUNT)->data(Qt::DisplayRole).toInt() == 0)
        {
            ui->table->removeRow(i);
            m_settings.m_frequencies.removeAt(i); // table_cellChanged isn't called for removeRow
            m_settings.m_enabled.removeAt(i);
            m_settings.m_notes.removeAt(i);
        }
    }
    applySetting("frequencies");
}

static QList<QTableWidgetItem*> takeRow(QTableWidget* table, int row)
{
    QList<QTableWidgetItem*> rowItems;

    for (int col = 0; col < table->columnCount(); col++) {
        rowItems.append(table->takeItem(row, col));
    }
    return rowItems;
}

static void setRow(QTableWidget* table, int row, const QList<QTableWidgetItem*>& rowItems)
{
    for (int col = 0; col < rowItems.size(); col++) {
        table->setItem(row, col, rowItems.at(col));
    }
}

void FreqScannerGUI::on_up_clicked()
{
    QList<QTableWidgetItem*> items = ui->table->selectedItems();
    for (auto item : items)
    {
        int row = ui->table->row(item);
        if (row > 0)
        {
            QList<QTableWidgetItem*> sourceItems = takeRow(ui->table, row);
            QList<QTableWidgetItem*> destItems = takeRow(ui->table, row - 1);
            setRow(ui->table, row - 1, sourceItems);
            setRow(ui->table, row, destItems);
            ui->table->setCurrentCell(row - 1, 0);
        }
    }
}

void FreqScannerGUI::on_down_clicked()
{
    QList<QTableWidgetItem*> items = ui->table->selectedItems();
    for (auto item : items)
    {
        int row = ui->table->row(item);
        if (row < ui->table->rowCount() - 1)
        {
            QList<QTableWidgetItem*> sourceItems = takeRow(ui->table, row);
            QList<QTableWidgetItem*> destItems = takeRow(ui->table, row + 1);
            setRow(ui->table, row + 1, sourceItems);
            setRow(ui->table, row, destItems);
            ui->table->setCurrentCell(row + 1, 0);
        }
    }
}

void FreqScannerGUI::on_clearActiveCount_clicked()
{
    for (int i = 0; i < ui->table->rowCount(); i++) {
        ui->table->item(i, COL_ACTIVE_COUNT)->setData(Qt::DisplayRole, 0);
    }
}

void FreqScannerGUI::on_table_cellChanged(int row, int column)
{
    QTableWidgetItem* item = ui->table->item(row, column);
    if (item)
    {
        if (column == COL_FREQUENCY)
        {
            qint64 value = item->text().toLongLong();
            while (m_settings.m_frequencies.size() <= row)
            {
                m_settings.m_frequencies.append(0);
                m_settings.m_enabled.append(true);
                m_settings.m_notes.append("");
            }
            m_settings.m_frequencies[row] = value;
            updateAnnotation(row);
            applySetting("frequencies");
        }
        else if (column == COL_ENABLE)
        {
            m_settings.m_enabled[row] = item->checkState() == Qt::Checked;
            applySetting("frequencies");
        }
        else if (column == COL_NOTES)
        {
            m_settings.m_notes[row] = item->text();
            applySetting("frequencies");
        }
    }
}

void FreqScannerGUI::updateAnnotation(int row)
{
    QTableWidgetItem* item = ui->table->item(row, COL_FREQUENCY);
    QTableWidgetItem* annotationItem = ui->table->item(row, COL_ANNOTATION);
    if (item && annotationItem)
    {
        qint64 frequency = item->text().toLongLong();
        const QList<SpectrumAnnotationMarker>& markers = m_deviceUISet->m_spectrum->getAnnotationMarkers();
        const SpectrumAnnotationMarker* closest = nullptr;
        for (const auto& marker : markers)
        {
            qint64 start1 = marker.m_startFrequency;
            qint64 stop1 = marker.m_startFrequency + marker.m_bandwidth;
            qint64 start2 = frequency - m_settings.m_channelBandwidth / 2;
            qint64 stop2 = frequency + m_settings.m_channelBandwidth / 2;
            if (   ((start2 >= start1) && (start2 <= stop1))
                || ((stop2 >= start1) && (stop2 <= stop1))
               )
            {
                if (marker.m_bandwidth == (unsigned)m_settings.m_channelBandwidth) {
                    // Exact match
                    annotationItem->setText(marker.m_text);
                    return;
                }
                else if (!closest)
                {
                    closest = &marker;
                }
                else
                {
                    if (marker.m_bandwidth < closest->m_bandwidth) {
                        closest = &marker;
                    }
                }
            }
        }
        if (closest) {
            annotationItem->setText(closest->m_text);
        }
    }
}

void FreqScannerGUI::updateAnnotations()
{
    for (int i = 0; i < ui->table->rowCount(); i++) {
        updateAnnotation(i);
    }
}

void FreqScannerGUI::setAllEnabled(bool enable)
{
    for (int i = 0; i < ui->table->rowCount(); i++) {
        ui->table->item(i, COL_ENABLE)->setCheckState(enable ? Qt::Checked : Qt::Unchecked);
    }
}

void FreqScannerGUI::table_customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem* item = ui->table->itemAt(pos);
    if (item)
    {
        int row = item->row();

        QMenu* tableContextMenu = new QMenu(ui->table);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard* clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
            });
        tableContextMenu->addAction(copyAction);

        tableContextMenu->addSeparator();

        // Enable all

        QAction* enableAllAction = new QAction("Enable all", tableContextMenu);
        connect(enableAllAction, &QAction::triggered, this, [this]()->void {
            setAllEnabled(true);
            });
        tableContextMenu->addAction(enableAllAction);

        // Disable all

        QAction* disableAllAction = new QAction("Disable all", tableContextMenu);
        connect(disableAllAction, &QAction::triggered, this, [this]()->void {
            setAllEnabled(false);
            });
        tableContextMenu->addAction(disableAllAction);

        // Remove selected rows

        QAction* removeAction = new QAction("Remove", tableContextMenu);
        connect(removeAction, &QAction::triggered, this, [this]()->void {
            on_remove_clicked();
            });
        tableContextMenu->addAction(removeAction);

        tableContextMenu->addSeparator();

        // Tune to frequency

        const QRegExp re("R([0-9]+):([0-9]+)");
        if (re.indexIn(m_settings.m_channel) >= 0)
        {
            int scanDeviceSetIndex = re.capturedTexts()[1].toInt();
            int scanChannelIndex = re.capturedTexts()[2].toInt();
            qDebug() << "scanDeviceSetIndex" << scanDeviceSetIndex << "scanChannelIndex" << scanChannelIndex;

            qint64 frequency = ui->table->item(row, COL_FREQUENCY)->text().toLongLong();

            QAction* findChannelMapAction = new QAction(QString("Tune R%1:%2 to %3").arg(scanDeviceSetIndex).arg(scanChannelIndex).arg(frequency), tableContextMenu);
            connect(findChannelMapAction, &QAction::triggered, this, [this, scanDeviceSetIndex, scanChannelIndex, frequency]()->void {

                if ((frequency - m_settings.m_channelBandwidth / 2 < m_deviceCenterFrequency - m_basebandSampleRate / 2)
                    || (frequency + m_settings.m_channelBandwidth / 2 >= m_deviceCenterFrequency + m_basebandSampleRate / 2))
                {
                    qint64 centerFrequency = frequency;
                    int offset = 0;
                    while (frequency - centerFrequency < m_settings.m_channelFrequencyOffset)
                    {
                        centerFrequency -= m_settings.m_channelBandwidth;
                        offset += m_settings.m_channelBandwidth;
                    }

                    if (!ChannelWebAPIUtils::setCenterFrequency(getDeviceSetIndex(), centerFrequency)) {
                        qWarning() << "Scanner failed to set frequency" << centerFrequency;
                    }

                    ChannelWebAPIUtils::setFrequencyOffset(scanDeviceSetIndex, scanChannelIndex, offset);
                }
                else
                {
                    int offset = frequency - m_deviceCenterFrequency;
                    ChannelWebAPIUtils::setFrequencyOffset(scanDeviceSetIndex, scanChannelIndex, offset);
                }

                });
            tableContextMenu->addAction(findChannelMapAction);
        }
        else
        {
            qDebug() << "Failed to parse channel" << m_settings.m_channel;
        }

        tableContextMenu->popup(ui->table->viewport()->mapToGlobal(pos));
    }
}

// Columns in table reordered
void FreqScannerGUI::table_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void)oldVisualIndex;
    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void FreqScannerGUI::table_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void)oldSize;
    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in ADSB table header - show column select menu
void FreqScannerGUI::columnSelectMenu(QPoint pos)
{
    m_menu->popup(ui->table->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void FreqScannerGUI::columnSelectMenuChecked(bool checked)
{
    (void)checked;
    QAction* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->table->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction* FreqScannerGUI::createCheckableItem(QString& text, int idx, bool checked)
{
    QAction* action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

void FreqScannerGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    int row = ui->table->rowCount();
    ui->table->setRowCount(row + 1);
    ui->table->setItem(row, COL_FREQUENCY, new QTableWidgetItem("800,000.5 MHz"));
    ui->table->setItem(row, COL_ANNOTATION, new QTableWidgetItem("London VOLMET"));
    ui->table->setItem(row, COL_ENABLE, new QTableWidgetItem("Enable"));
    ui->table->setItem(row, COL_POWER, new QTableWidgetItem("-100.0"));
    ui->table->setItem(row, COL_ACTIVE_COUNT, new QTableWidgetItem("10000"));
    ui->table->setItem(row, COL_NOTES, new QTableWidgetItem("Enter some notes"));
    ui->table->resizeColumnsToContents();
    ui->table->setRowCount(row);
}

void FreqScannerGUI::makeUIConnections()
{
    QObject::connect(ui->channels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreqScannerGUI::on_channels_currentIndexChanged);
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &FreqScannerGUI::on_deltaFrequency_changed);
    QObject::connect(ui->channelBandwidth, &ValueDialZ::changed, this, &FreqScannerGUI::on_channelBandwidth_changed);
    QObject::connect(ui->scanTime, &QDial::valueChanged, this, &FreqScannerGUI::on_scanTime_valueChanged);
    QObject::connect(ui->retransmitTime, &QDial::valueChanged, this, &FreqScannerGUI::on_retransmitTime_valueChanged);
    QObject::connect(ui->tuneTime, &QDial::valueChanged, this, &FreqScannerGUI::on_tuneTime_valueChanged);
    QObject::connect(ui->thresh, &QDial::valueChanged, this, &FreqScannerGUI::on_thresh_valueChanged);
    QObject::connect(ui->priority, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreqScannerGUI::on_priority_currentIndexChanged);
    QObject::connect(ui->measurement, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreqScannerGUI::on_measurement_currentIndexChanged);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FreqScannerGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::clicked, this, &FreqScannerGUI::on_startStop_clicked);
    QObject::connect(ui->table, &QTableWidget::cellChanged, this, &FreqScannerGUI::on_table_cellChanged);
    QObject::connect(ui->addSingle, &QToolButton::clicked, this, &FreqScannerGUI::on_addSingle_clicked);
    QObject::connect(ui->addRange, &QToolButton::clicked, this, &FreqScannerGUI::on_addRange_clicked);
    QObject::connect(ui->remove, &QToolButton::clicked, this, &FreqScannerGUI::on_remove_clicked);
    QObject::connect(ui->removeInactive, &QToolButton::clicked, this, &FreqScannerGUI::on_removeInactive_clicked);
    QObject::connect(ui->up, &QToolButton::clicked, this, &FreqScannerGUI::on_up_clicked);
    QObject::connect(ui->down, &QToolButton::clicked, this, &FreqScannerGUI::on_down_clicked);
    QObject::connect(ui->clearActiveCount, &QToolButton::clicked, this, &FreqScannerGUI::on_clearActiveCount_clicked);
}

void FreqScannerGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
