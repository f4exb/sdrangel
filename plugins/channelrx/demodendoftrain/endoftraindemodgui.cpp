///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QMainWindow>
#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QRegularExpression>
#include <QFileDialog>
#include <QScrollBar>

#include "endoftraindemodgui.h"
#include "endoftrainpacket.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_endoftraindemodgui.h"
#include "plugin/pluginapi.h"
#include "util/csv.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "endoftraindemod.h"

void EndOfTrainDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->packets->rowCount();
    ui->packets->setRowCount(row + 1);
    ui->packets->setItem(row, PACKETS_COL_DATE, new QTableWidgetItem("Frid Apr 15 2016-"));
    ui->packets->setItem(row, PACKETS_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->packets->setItem(row, PACKETS_COL_BATTERY_CONDITION, new QTableWidgetItem("Very low"));
    ui->packets->setItem(row, PACKETS_COL_TYPE, new QTableWidgetItem("7-"));
    ui->packets->setItem(row, PACKETS_COL_ADDRESS, new QTableWidgetItem("65535-"));
    ui->packets->setItem(row, PACKETS_COL_PRESSURE, new QTableWidgetItem("PID-"));
    ui->packets->setItem(row, PACKETS_COL_BATTERY_CHARGE, new QTableWidgetItem("100.0%"));
    ui->packets->setItem(row, PACKETS_COL_ARM_STATUS, new QTableWidgetItem("Normal"));
    ui->packets->setItem(row, PACKETS_COL_CRC, new QTableWidgetItem("123456-15-"));
    ui->packets->setItem(row, PACKETS_COL_DATA_HEX, new QTableWidgetItem("88888888888888888888"));
    ui->packets->resizeColumnsToContents();
    ui->packets->removeRow(row);
}

// Columns in table reordered
void EndOfTrainDemodGUI::endoftrains_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void EndOfTrainDemodGUI::endoftrains_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void EndOfTrainDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->packets->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void EndOfTrainDemodGUI::columnSelectMenuChecked(bool checked)
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
QAction *EndOfTrainDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

EndOfTrainDemodGUI* EndOfTrainDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    EndOfTrainDemodGUI* gui = new EndOfTrainDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void EndOfTrainDemodGUI::destroy()
{
    delete this;
}

void EndOfTrainDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
}

QByteArray EndOfTrainDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool EndOfTrainDemodGUI::deserialize(const QByteArray& data)
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

// Add row to table
void EndOfTrainDemodGUI::packetReceived(const QByteArray& packetData, const QDateTime& dateTime)
{
    EndOfTrainPacket packet;

    if (packet.decode(packetData))
    {
        // Is scroll bar at bottom
        QScrollBar *sb = ui->packets->verticalScrollBar();
        bool scrollToBottom = sb->value() == sb->maximum();

        ui->packets->setSortingEnabled(false);
        int row = ui->packets->rowCount();
        ui->packets->setRowCount(row + 1);

        QTableWidgetItem *dateItem = new QTableWidgetItem();
        QTableWidgetItem *timeItem = new QTableWidgetItem();
        QTableWidgetItem *chainingBitsItem = new QTableWidgetItem();
        QTableWidgetItem *batteryConditionItem = new QTableWidgetItem();
        QTableWidgetItem *typeItem = new QTableWidgetItem();
        QTableWidgetItem *addressItem = new QTableWidgetItem();
        QTableWidgetItem *pressureItem = new QTableWidgetItem();
        QTableWidgetItem *batteryChargeItem = new QTableWidgetItem();
        QTableWidgetItem *discretionaryItem = new QTableWidgetItem();
        QTableWidgetItem *valveCircuitStatusItem = new QTableWidgetItem();
        QTableWidgetItem *confIndItem = new QTableWidgetItem();
        QTableWidgetItem *turbineItem = new QTableWidgetItem();
        QTableWidgetItem *motionItem = new QTableWidgetItem();
        QTableWidgetItem *markerBatteryLightConditionItem = new QTableWidgetItem();
        QTableWidgetItem *markerLightStatusItem = new QTableWidgetItem();
        QTableWidgetItem *armStatusItem = new QTableWidgetItem();
        QTableWidgetItem *crcItem = new QTableWidgetItem();
        QTableWidgetItem *dataHexItem = new QTableWidgetItem();
        ui->packets->setItem(row, PACKETS_COL_DATE, dateItem);
        ui->packets->setItem(row, PACKETS_COL_TIME, timeItem);
        ui->packets->setItem(row, PACKETS_COL_CHAINING_BITS, chainingBitsItem);
        ui->packets->setItem(row, PACKETS_COL_BATTERY_CONDITION, batteryConditionItem);
        ui->packets->setItem(row, PACKETS_COL_TYPE, typeItem);
        ui->packets->setItem(row, PACKETS_COL_ADDRESS, addressItem);
        ui->packets->setItem(row, PACKETS_COL_PRESSURE, pressureItem);
        ui->packets->setItem(row, PACKETS_COL_BATTERY_CHARGE, batteryChargeItem);
        ui->packets->setItem(row, PACKETS_COL_DISCRETIONARY, discretionaryItem);
        ui->packets->setItem(row, PACKETS_COL_VALVE_CIRCUIT_STATUS, valveCircuitStatusItem);
        ui->packets->setItem(row, PACKETS_COL_CONF_IND, confIndItem);
        ui->packets->setItem(row, PACKETS_COL_TURBINE, turbineItem);
        ui->packets->setItem(row, PACKETS_COL_MOTION, motionItem);
        ui->packets->setItem(row, PACKETS_COL_MARKER_BATTERY, markerBatteryLightConditionItem);
        ui->packets->setItem(row, PACKETS_COL_MARKER_LIGHT, markerLightStatusItem);
        ui->packets->setItem(row, PACKETS_COL_ARM_STATUS, armStatusItem);
        ui->packets->setItem(row, PACKETS_COL_CRC, crcItem);
        ui->packets->setItem(row, PACKETS_COL_DATA_HEX, dataHexItem);
        dateItem->setText(dateTime.date().toString());
        timeItem->setText(dateTime.time().toString());
        chainingBitsItem->setText(QString::number(packet.m_chainingBits));
        batteryConditionItem->setText(packet.getBatteryCondition());
        typeItem->setText(packet.getMessageType());
        addressItem->setText(QString::number(packet.m_address));
        pressureItem->setText(QString::number(packet.m_pressure));
        batteryChargeItem->setText(QString("%1%").arg(packet.getBatteryChargePercent(), 0, 'f', 1));
        discretionaryItem->setCheckState(packet.m_discretionary ? Qt::Checked : Qt::Unchecked);
        valveCircuitStatusItem->setText(packet.getValveCircuitStatus());
        confIndItem->setCheckState(packet.m_confirmation ? Qt::Checked : Qt::Unchecked);
        turbineItem->setCheckState(packet.m_turbine ? Qt::Checked : Qt::Unchecked);
        motionItem->setCheckState(packet.m_motion ? Qt::Checked : Qt::Unchecked);
        markerBatteryLightConditionItem->setText(packet.getMarkerLightBatteryCondition());
        markerLightStatusItem->setText(packet.getMarkerLightStatus());
        armStatusItem->setText(packet.getArmStatus());
        crcItem->setCheckState(packet.m_crcValid ? Qt::Checked : Qt::Unchecked);
        dataHexItem->setText(packet.m_dataHex);
        filterRow(row);
        ui->packets->setSortingEnabled(true);
        if (scrollToBottom) {
            ui->packets->scrollToBottom();
        }
    }
    else
    {
        qDebug() << "EndOfTrainDemodGUI::packetReceived: Unsupported packet: " << packetData;
    }
}

bool EndOfTrainDemodGUI::handleMessage(const Message& message)
{
    if (EndOfTrainDemod::MsgConfigureEndOfTrainDemod::match(message))
    {
        qDebug("EndOfTrainDemodGUI::handleMessage: EndOfTrainDemod::MsgConfigureEndOfTrainDemod");
        const EndOfTrainDemod::MsgConfigureEndOfTrainDemod& cfg = (EndOfTrainDemod::MsgConfigureEndOfTrainDemod&) message;
        m_settings = cfg.getSettings();
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

void EndOfTrainDemodGUI::handleInputMessages()
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

void EndOfTrainDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void EndOfTrainDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void EndOfTrainDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void EndOfTrainDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySetting("rfBandwidth");
}

void EndOfTrainDemodGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySetting("fmDeviation");
}

void EndOfTrainDemodGUI::on_filterFrom_editingFinished()
{
    m_settings.m_filterFrom = ui->filterFrom->text();
    filter();
    applySetting("filterFrom");
}

void EndOfTrainDemodGUI::on_clearTable_clicked()
{
    ui->packets->setRowCount(0);
}

void EndOfTrainDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySetting("udpEnabled");
}

void EndOfTrainDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySetting("udpAddress");
}

void EndOfTrainDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySetting("udpPort");
}

void EndOfTrainDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterFrom != "")
    {
        QRegularExpression re(QRegularExpression::anchoredPattern(m_settings.m_filterFrom));
        QTableWidgetItem *fromItem = ui->packets->item(row, PACKETS_COL_ADDRESS);
        QRegularExpressionMatch match = re.match(fromItem->text());
        if (!match.hasMatch())
            hidden = true;
    }
    ui->packets->setRowHidden(row, hidden);
}

void EndOfTrainDemodGUI::filter()
{
    for (int i = 0; i < ui->packets->rowCount(); i++)
    {
        filterRow(i);
    }
}

void EndOfTrainDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

void EndOfTrainDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_endoftrainDemod->getNumberOfDeviceStreams());
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

        QStringList settingsKeys({
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

EndOfTrainDemodGUI::EndOfTrainDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::EndOfTrainDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodendoftrain/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_endoftrainDemod = reinterpret_cast<EndOfTrainDemod*>(rxChannel);
    m_endoftrainDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_endoftrainDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setStreams(QStringList({"IQ", "MagSq", "FM demod", "f0Filt", "f1Filt", "diff", "sample", "bit", "gotSOP"}));

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

    m_scopeVis->setLiveRate(EndOfTrainDemodSettings::CHANNEL_SAMPLE_RATE);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("End Of Train Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    ui->scopeContainer->setVisible(false);

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
    connect(ui->packets->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(endoftrains_sectionMoved(int, int, int)));
    connect(ui->packets->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(endoftrains_sectionResized(int, int, int)));

    displaySettings();
    makeUIConnections();
    applyAllSettings();
    m_resizer.enableChildMouseTracking();
}

EndOfTrainDemodGUI::~EndOfTrainDemodGUI()
{
    delete ui;
}

void EndOfTrainDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void EndOfTrainDemodGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void EndOfTrainDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        EndOfTrainDemod::MsgConfigureEndOfTrainDemod* message = EndOfTrainDemod::MsgConfigureEndOfTrainDemod::create(m_settings, m_settingsKeys, force);
        m_endoftrainDemod->getInputMessageQueue()->push(message);
        m_settingsKeys.clear();
    }
}

void EndOfTrainDemodGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void EndOfTrainDemodGUI::displaySettings()
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

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->useFileTime->setChecked(m_settings.m_useFileTime);

    // Order and size columns
    QHeaderView *header = ui->packets->horizontalHeader();
    for (int i = 0; i < ENDOFTRAINDEMOD_COLUMNS; i++)
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

void EndOfTrainDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void EndOfTrainDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void EndOfTrainDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_endoftrainDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
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

void EndOfTrainDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySetting("logEnabled");
}

void EndOfTrainDemodGUI::on_logFilename_clicked()
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
            applySetting("logFilename");
        }
    }
}

// Read .csv log and process as received frames
void EndOfTrainDemodGUI::on_logOpen_clicked()
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
                }
                else
                {
                    QMessageBox::critical(this, "End-Of-Train Demod", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "End-Of-Train Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void EndOfTrainDemodGUI::on_useFileTime_toggled(bool checked)
{
    m_settings.m_useFileTime = checked;
    applySetting("useFileTime");
}

void EndOfTrainDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &EndOfTrainDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &EndOfTrainDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &EndOfTrainDemodGUI::on_fmDev_valueChanged);
    QObject::connect(ui->filterFrom, &QLineEdit::editingFinished, this, &EndOfTrainDemodGUI::on_filterFrom_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &EndOfTrainDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &EndOfTrainDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &EndOfTrainDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &EndOfTrainDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &EndOfTrainDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &EndOfTrainDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &EndOfTrainDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->useFileTime, &ButtonSwitch::toggled, this, &EndOfTrainDemodGUI::on_useFileTime_toggled);
}

void EndOfTrainDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
