///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QScrollBar>
#include <QMenu>
#include <QDesktopServices>

#include "dscdemodgui.h"
#include "coaststations.h"

#include "device/deviceuiset.h"
#include "device/deviceset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspdevicesourceengine.h"
#include "ui_dscdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/mmsi.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/decimaldelegate.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/crightclickenabler.h"
#include "gui/tabletapandhold.h"
#include "gui/dialogpositioner.h"
#include "gui/frequencydelegate.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "maincore.h"

#include "dscdemod.h"

#include "SWGMapItem.h"

void DSCDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);
    ui->messages->setItem(row, MESSAGE_COL_RX_DATE, new QTableWidgetItem("15/04/2016-"));
    ui->messages->setItem(row, MESSAGE_COL_RX_TIME, new QTableWidgetItem("10:17"));
    ui->messages->setItem(row, MESSAGE_COL_FORMAT, new QTableWidgetItem("Selective call"));
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS, new QTableWidgetItem("123456789"));
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS_COUNTRY, new QTableWidgetItem("flag"));
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS_TYPE, new QTableWidgetItem("Coast"));
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS_NAME, new QTableWidgetItem("A ships name"));
    ui->messages->setItem(row, MESSAGE_COL_CATEGORY, new QTableWidgetItem("Distress"));
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID, new QTableWidgetItem("123456789"));
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_COUNTRY, new QTableWidgetItem("flag"));
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_TYPE, new QTableWidgetItem("Coast"));
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_NAME, new QTableWidgetItem("A ships name"));
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_RANGE, new QTableWidgetItem("3000.0"));
    ui->messages->setItem(row, MESSAGE_COL_TELECOMMAND_1, new QTableWidgetItem("No information"));
    ui->messages->setItem(row, MESSAGE_COL_TELECOMMAND_2, new QTableWidgetItem("No information"));
    ui->messages->setItem(row, MESSAGE_COL_RX, new QTableWidgetItem("30,000.0 kHz"));
    ui->messages->setItem(row, MESSAGE_COL_TX, new QTableWidgetItem("30,000.0 kHz"));
    ui->messages->setItem(row, MESSAGE_COL_POSITION, new QTableWidgetItem("-90d60N -180d60W"));
    ui->messages->setItem(row, MESSAGE_COL_NUMBER, new QTableWidgetItem("0898123456"));
    ui->messages->setItem(row, MESSAGE_COL_TIME, new QTableWidgetItem("12:00"));
    ui->messages->setItem(row, MESSAGE_COL_COMMS, new QTableWidgetItem("FSK"));
    ui->messages->setItem(row, MESSAGE_COL_EOS, new QTableWidgetItem("Req Ack"));
    ui->messages->setItem(row, MESSAGE_COL_ECC, new QTableWidgetItem("Fail"));
    ui->messages->setItem(row, MESSAGE_COL_ERRORS, new QTableWidgetItem("9"));
    ui->messages->setItem(row, MESSAGE_COL_VALID, new QTableWidgetItem("Invalid"));
    ui->messages->setItem(row, MESSAGE_COL_RSSI, new QTableWidgetItem("-50"));
    ui->messages->resizeColumnsToContents();
    ui->messages->removeRow(row);
}

// Columns in table reordered
void DSCDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void DSCDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void DSCDemodGUI::columnSelectMenu(QPoint pos)
{
    m_menu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void DSCDemodGUI::columnSelectMenuChecked(bool checked)
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
QAction *DSCDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

DSCDemodGUI* DSCDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    DSCDemodGUI* gui = new DSCDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void DSCDemodGUI::destroy()
{
    delete this;
}

void DSCDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray DSCDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DSCDemodGUI::deserialize(const QByteArray& data)
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
void DSCDemodGUI::messageReceived(const DSCMessage& message, int errors, float rssi)
{
    // Is scroll bar at bottom
    QScrollBar *sb = ui->messages->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    ui->messages->setSortingEnabled(false);
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);

    QTableWidgetItem *rxDateItem = new QTableWidgetItem();
    QTableWidgetItem *rxTimeItem = new QTableWidgetItem();
    QTableWidgetItem *formatItem = new QTableWidgetItem();
    QTableWidgetItem *addressItem = new QTableWidgetItem();
    QTableWidgetItem *addressCountryItem = new QTableWidgetItem();
    QTableWidgetItem *addressTypeItem = new QTableWidgetItem();
    QTableWidgetItem *addressNameItem = new QTableWidgetItem();
    QTableWidgetItem *categoryItem = new QTableWidgetItem();
    QTableWidgetItem *selfIdItem = new QTableWidgetItem();
    QTableWidgetItem *selfIdCountryItem = new QTableWidgetItem();
    QTableWidgetItem *selfIdTypeItem = new QTableWidgetItem();
    QTableWidgetItem *selfIdNameItem = new QTableWidgetItem();
    QTableWidgetItem *selfIdRangeItem = new QTableWidgetItem();
    QTableWidgetItem *telecommand1Item = new QTableWidgetItem();
    QTableWidgetItem *telecommand2Item = new QTableWidgetItem();
    QTableWidgetItem *rxItem = new QTableWidgetItem();
    QTableWidgetItem *txItem = new QTableWidgetItem();
    QTableWidgetItem *positionItem = new QTableWidgetItem();
    QTableWidgetItem *distressIdItem = new QTableWidgetItem();
    QTableWidgetItem *distressItem = new QTableWidgetItem();
    QTableWidgetItem *numberItem = new QTableWidgetItem();
    QTableWidgetItem *timeItem = new QTableWidgetItem();
    QTableWidgetItem *commsItem = new QTableWidgetItem();
    QTableWidgetItem *eosItem = new QTableWidgetItem();
    QTableWidgetItem *eccItem = new QTableWidgetItem();
    QTableWidgetItem *errorsItem = new QTableWidgetItem();
    QTableWidgetItem *validItem = new QTableWidgetItem();
    QTableWidgetItem *rssiItem = new QTableWidgetItem();
    ui->messages->setItem(row, MESSAGE_COL_RX_DATE, rxDateItem);
    ui->messages->setItem(row, MESSAGE_COL_RX_TIME, rxTimeItem);
    ui->messages->setItem(row, MESSAGE_COL_FORMAT, formatItem);
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS, addressItem);
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS_COUNTRY, addressCountryItem);
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS_TYPE, addressTypeItem);
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS_NAME, addressNameItem);
    ui->messages->setItem(row, MESSAGE_COL_CATEGORY, categoryItem);
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID, selfIdItem);
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_COUNTRY, selfIdCountryItem);
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_TYPE, selfIdTypeItem);
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_NAME, selfIdNameItem);
    ui->messages->setItem(row, MESSAGE_COL_SELF_ID_RANGE, selfIdRangeItem);
    ui->messages->setItem(row, MESSAGE_COL_TELECOMMAND_1, telecommand1Item);
    ui->messages->setItem(row, MESSAGE_COL_TELECOMMAND_2, telecommand2Item);
    ui->messages->setItem(row, MESSAGE_COL_RX, rxItem);
    ui->messages->setItem(row, MESSAGE_COL_TX, txItem);
    ui->messages->setItem(row, MESSAGE_COL_POSITION, positionItem);
    ui->messages->setItem(row, MESSAGE_COL_DISTRESS_ID, distressIdItem);
    ui->messages->setItem(row, MESSAGE_COL_DISTRESS, distressItem);
    ui->messages->setItem(row, MESSAGE_COL_NUMBER, numberItem);
    ui->messages->setItem(row, MESSAGE_COL_TIME, timeItem);
    ui->messages->setItem(row, MESSAGE_COL_COMMS, commsItem);
    ui->messages->setItem(row, MESSAGE_COL_EOS, eosItem);
    ui->messages->setItem(row, MESSAGE_COL_ECC, eccItem);
    ui->messages->setItem(row, MESSAGE_COL_ERRORS, errorsItem);
    ui->messages->setItem(row, MESSAGE_COL_VALID, validItem);
    ui->messages->setItem(row, MESSAGE_COL_RSSI, rssiItem);

    rxDateItem->setData(Qt::DisplayRole, message.m_dateTime.date());
    rxTimeItem->setData(Qt::DisplayRole, message.m_dateTime.time());

    formatItem->setText(message.formatSpecifier());
    if (message.m_hasCategory) {
        categoryItem->setText(message.category());
    }
    if (message.m_hasAddress)
    {
        addressItem->setText(message.m_address);
        if (message.m_formatSpecifier != DSCMessage::GEOGRAPHIC_CALL)
        {
            QIcon *addressFlag = MMSI::getFlagIcon(message.m_address);
            if (addressFlag)
            {
                addressCountryItem->setSizeHint(QSize(40, 20));
                addressCountryItem->setIcon(*addressFlag);
            }
            addressTypeItem->setText(MMSI::getCategory(message.m_address));
        }
    }
    selfIdItem->setText(message.m_selfId);
    QIcon *selfIdFlag = MMSI::getFlagIcon(message.m_selfId);
    if (selfIdFlag)
    {
        selfIdCountryItem->setSizeHint(QSize(40, 20));
        selfIdCountryItem->setIcon(*selfIdFlag);
    }
    selfIdTypeItem->setText(MMSI::getCategory(message.m_selfId));
    if (message.m_hasTelecommand1) {
        telecommand1Item->setText(DSCMessage::telecommand1(message.m_telecommand1));
    }
    if (message.m_hasTelecommand2) {
        telecommand2Item->setText(DSCMessage::telecommand2(message.m_telecommand2));
    }
    if (message.m_hasFrequency1) {
        rxItem->setData(Qt::DisplayRole, message.m_frequency1);
    } else if (message.m_hasChannel1) {
        rxItem->setText(message.m_channel1);
    }
    if (message.m_hasFrequency2) {
        txItem->setData(Qt::DisplayRole, message.m_frequency2);
    } else if (message.m_hasChannel2) {
        txItem->setText(message.m_channel2);
    }
    if (message.m_hasPosition) {
        positionItem->setText(message.m_position);
    }
    if (message.m_hasDistressId) {
        distressIdItem->setText(message.m_distressId);
    }
    if (message.m_hasDistressNature) {
        distressItem->setText(DSCMessage::distressNature(message.m_distressNature));
    }
    if (message.m_hasNumber) {
        numberItem->setText(message.m_number);
    }
    if (message.m_hasTime) {
        timeItem->setData(Qt::DisplayRole, message.m_time);
    }
    if (message.m_hasSubsequenceComms) {
        commsItem->setText(DSCMessage::telecommand1(message.m_subsequenceComms));
    }
    eosItem->setText(DSCMessage::endOfSignal(message.m_eos));
    if (message.m_eccOk) {
        eccItem->setText("OK");
    } else {
        eccItem->setText(QString("Fail (%1 != %2)").arg(message.m_ecc).arg(message.m_calculatedECC));
    }
    if (message.m_valid) {
        validItem->setText("Valid");
    } else {
        validItem->setText("Invalid");
    }

    errorsItem->setData(Qt::DisplayRole, errors);
    rssiItem->setData(Qt::DisplayRole, rssi);

    filterRow(row);
    ui->messages->setSortingEnabled(true);
    ui->messages->resizeRowToContents(row);
    if (scrollToBottom) {
        ui->messages->scrollToBottom();
    }

    if (CoastStations.contains(message.m_address)) {
        addressNameItem->setText(CoastStations.value(message.m_address));
    }

    // Get latest APRS.fi data to calculate distance
    if (m_aprsFi && message.m_valid)
    {
        QStringList addresses;
        addresses.append(message.m_selfId);
        if (message.m_hasAddress
             && (message.m_formatSpecifier != DSCMessage::GEOGRAPHIC_CALL)
             && (message.m_formatSpecifier != DSCMessage::GROUP_CALL)
             && (message.m_formatSpecifier != DSCMessage::ALL_SHIPS)
             && (message.m_selfId != message.m_address)
           ) {
            addresses.append(message.m_address);
        }
        m_aprsFi->getData(addresses);
    }
}

bool DSCDemodGUI::handleMessage(const Message& message)
{
    if (DSCDemod::MsgConfigureDSCDemod::match(message))
    {
        qDebug("DSCDemodGUI::handleMessage: DSCDemod::MsgConfigureDSCDemod");
        const DSCDemod::MsgConfigureDSCDemod& cfg = (DSCDemod::MsgConfigureDSCDemod&) message;
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
    else if (DSCDemod::MsgMessage::match(message))
    {
        DSCDemod::MsgMessage& textMsg = (DSCDemod::MsgMessage&) message;
        messageReceived(textMsg.getMessage(), textMsg.getErrors(), textMsg.getRSSI());
        return true;
    }

    return false;
}

void DSCDemodGUI::handleInputMessages()
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

void DSCDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void DSCDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DSCDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void DSCDemodGUI::on_filterInvalid_clicked(bool checked)
{
    m_settings.m_filterInvalid = checked;
    filter();
    applySettings();
}

void DSCDemodGUI::on_filterColumn_currentIndexChanged(int index)
{
    m_settings.m_filterColumn = index;
    filter();
    applySettings();
}

void DSCDemodGUI::on_filter_editingFinished()
{
    m_settings.m_filter = ui->filter->text();
    filter();
    applySettings();
}

void DSCDemodGUI::on_clearTable_clicked()
{
    ui->messages->setRowCount(0);
}

void DSCDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void DSCDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void DSCDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void DSCDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterInvalid)
    {
        QTableWidgetItem *validItem = ui->messages->item(row, MESSAGE_COL_VALID);
        if (validItem->text() != "Valid") {
            hidden = true;
        }
    }
    if (m_settings.m_filter != "")
    {
        QTableWidgetItem *item = ui->messages->item(row, m_settings.m_filterColumn);
        QRegularExpression re(m_settings.m_filter);
        QRegularExpressionMatch match = re.match(item->text());
        if (!match.hasMatch()) {
            hidden = true;
        }
    }
    ui->messages->setRowHidden(row, hidden);
}

void DSCDemodGUI::filter()
{
    for (int i = 0; i < ui->messages->rowCount(); i++) {
        filterRow(i);
    }
}

void DSCDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void DSCDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_dscDemod->getNumberOfDeviceStreams());
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

DSCDemodGUI::DSCDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::DSCDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddsc/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_dscDemod = reinterpret_cast<DSCDemod*>(rxChannel);
    m_dscDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    ui->messages->setItemDelegateForColumn(MESSAGE_COL_RSSI, new DecimalDelegate(1, ui->messages));

    m_scopeVis = m_dscDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setStreams(QStringList({"IQ", "MagSq", "abs1", "abs2", "Unbiased", "Biased", "Data", "Clock", "Bit", "GotSOP"}));

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

    m_scopeVis->setLiveRate(DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE);
    m_scopeVis->configure(500, 1, 0, 0, true);   // not working!
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("DSC Demodulator");
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
    m_menu = new QMenu(ui->messages);
    for (int i = 0; i < ui->messages->horizontalHeader()->count(); i++)
    {
        QString text = ui->messages->horizontalHeaderItem(i)->text();
        m_menu->addAction(createCheckableItem(text, i, true));
    }
    ui->messages->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(messages_sectionMoved(int, int, int)));
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(messages_sectionResized(int, int, int)));

    ui->messages->setItemDelegateForColumn(MESSAGE_COL_RX, new FrequencyDelegate());
    ui->messages->setItemDelegateForColumn(MESSAGE_COL_TX, new FrequencyDelegate());

    ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->messages);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &DSCDemodGUI::customContextMenuRequested);

    ui->scopeContainer->setVisible(false);

    m_aprsFi = APRSFi::create();
    if (m_aprsFi) {
        connect(m_aprsFi, &APRSFi::dataUpdated, this, &DSCDemodGUI::aprsFiDataUpdated);
    }

    CRightClickEnabler *feedRightClickEnabler = new CRightClickEnabler(ui->feed);
    connect(feedRightClickEnabler, &CRightClickEnabler::rightClick, this, &DSCDemodGUI::on_feed_rightClicked);

    displaySettings();
    makeUIConnections();
    applySettings(true);
    m_resizer.enableChildMouseTracking();
}

void DSCDemodGUI::createMenuOpenURLAction(QMenu* tableContextMenu, const QString& text, const QString& url, const QString& arg)
{
    QAction* action = new QAction(QString(text).arg(arg), tableContextMenu);
    connect(action, &QAction::triggered, this, [url, arg]()->void {
        QDesktopServices::openUrl(QUrl(QString(url).arg(arg)));
    });
    tableContextMenu->addAction(action);
}

void DSCDemodGUI::createMenuFindOnMapAction(QMenu* tableContextMenu, const QString& text, const QString& target)
{
    QAction* findOnMapAction = new QAction(QString(text).arg(target), tableContextMenu);
    connect(findOnMapAction, &QAction::triggered, this, [target]()->void {
        FeatureWebAPIUtils::mapFind(target);
    });    tableContextMenu->addAction(findOnMapAction);
    tableContextMenu->addAction(findOnMapAction);
}

void DSCDemodGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item =  ui->messages->itemAt(pos);
    if (item)
    {
        int row = item->row();
        QString time = ui->messages->item(row, MESSAGE_COL_RX_TIME)->data(Qt::DisplayRole).toTime().toString("hh:mm:ss");
        QString selfId = ui->messages->item(row, MESSAGE_COL_SELF_ID)->text();
        QString address = ui->messages->item(row, MESSAGE_COL_ADDRESS)->text();
        QString position = ui->messages->item(row, MESSAGE_COL_POSITION)->text();
        QString format = ui->messages->item(row, MESSAGE_COL_FORMAT)->text();
        FrequencyDelegate fd;
        QString rx = ui->messages->item(row, MESSAGE_COL_RX)->text();
        QString rxFormatted = fd.displayText(rx, QLocale::system());
        QString tx = ui->messages->item(row, MESSAGE_COL_TX)->text();
        QString txFormatted = fd.displayText(tx, QLocale::system());

        QMenu* tableContextMenu = new QMenu(ui->messages);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);

        // View MMSIs on various websites

        createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on aishub.net...", "https://www.aishub.net/vessels?Ship%5Bmmsi%5D=%1&mmsi=%1", selfId);
        createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on vesselfinder.com...", "https://www.vesselfinder.com/vessels?name=%1", selfId);
        createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on aprs.fi...", "https://aprs.fi/#!mt=roadmap&z=11&call=i/%1", selfId);
        createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on yaddnet.org...", "http://yaddnet.org/pages/php/band_today_messages.php?from_mmsi=%1", selfId);

        if (!address.isEmpty()
             && (format != "Geographic call")
             && (format != "Group call")
             && (format != "All ships"))
        {
            createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on aishub.net...", "https://www.aishub.net/vessels?Ship%5Bmmsi%5D=%1&mmsi=%1", address);
            createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on vesselfinder.com...", "https://www.vesselfinder.com/vessels?name=%1", address);
            createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on aprs.fi...", "https://aprs.fi/#!mt=roadmap&z=11&call=i/%1", address);
            createMenuOpenURLAction(tableContextMenu, "View MMSI %1 on yaddnet.fi...", "http://yaddnet.org/pages/php/band_today_messages.php?from_mmsi=%1", address);
        }

        // Find on Map
        if (!selfId.isEmpty() || !address.isEmpty() || !position.isEmpty())
        {
            tableContextMenu->addSeparator();
            if (!selfId.isEmpty()) {
                createMenuFindOnMapAction(tableContextMenu, "Find MMSI %1 on map", selfId);
            }
            if (!address.isEmpty() && (format != "Geographic call")) {
                createMenuFindOnMapAction(tableContextMenu, "Find MMSI %1 on map", address);
            }
            if (!position.isEmpty()) {
                createMenuFindOnMapAction(tableContextMenu, "Center map at %1", position);
            }
            if (!address.isEmpty() && (format == "Geographic call"))
            {
                QString name = QString("DSC Call %1 %2").arg(selfId).arg(time);
                if (!m_mapItems.contains(name))
                {
                    QString flag = MMSI::getFlagIconURL(selfId);
                    QStringList s;
                    s.append("Geographic call");
                    s.append(QString("From: %1 <img src=%3 width=20 height=10>").arg(selfId).arg(flag));
                    s.append(QString("To: %1").arg(address));
                    s.append(QString("Category: %1").arg(ui->messages->item(row, MESSAGE_COL_CATEGORY)->text()));
                    QString tc1 = ui->messages->item(row, MESSAGE_COL_TELECOMMAND_1)->text();
                    if (!tc1.isEmpty() && (tc1 != "No information")) {
                        s.append(QString("Telecommand 1: %1").arg(tc1));
                    }
                    FrequencyDelegate fd;
                    if (!rx.isEmpty()) {
                        s.append(QString("RX: %1").arg(rxFormatted));
                    }
                    if (!tx.isEmpty()) {
                        s.append(QString("TX: %1").arg(txFormatted));
                    }
                    QString info = s.join("<br>");

                    QAction* sendAreaToMapAction = new QAction(QString("Display %1 on map").arg(address), tableContextMenu);
                    connect(sendAreaToMapAction, &QAction::triggered, this, [this, name, address, info]()->void {
                        sendAreaToMapFeature(name, address, info);
                        QTimer::singleShot(500, [ name] {
                            FeatureWebAPIUtils::mapFind(name);
                        });
                    });
                    tableContextMenu->addAction(sendAreaToMapAction);
                }
                else
                {
                    QAction* findAreaOnMapAction = new QAction(QString("Center map on %1").arg(address), tableContextMenu);
                    connect(findAreaOnMapAction, &QAction::triggered, this, [ name]()->void {
                        FeatureWebAPIUtils::mapFind(name);
                    });
                    tableContextMenu->addAction(findAreaOnMapAction);

                    QAction* clearAreaFromMapAction = new QAction(QString("Remove %1 from map").arg(address), tableContextMenu);
                    connect(clearAreaFromMapAction, &QAction::triggered, this, [this, name]()->void {
                        clearAreaFromMapFeature(name);
                    });
                    tableContextMenu->addAction(clearAreaFromMapAction);
                }
            }
        }

        // Menu to tune SSB demods
        bool ok;
        qint64 rxFreq = rx.toLongLong(&ok);
        if (ok)
        {
            std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();
            int deviceSetIndex = 0;

            for (const auto& deviceSet : deviceSets)
            {
                DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

                if (deviceSourceEngine)
                {
                    for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
                    {
                        ChannelAPI *channel = deviceSet->getChannelAt(chi);

                        if (channel->getURI() == "sdrangel.channel.ssbdemod")
                        {
                            DeviceSampleSource *sampleSource = deviceSourceEngine->getSource();
                            if (sampleSource)
                            {
                                QAction* tuneRxAction = new QAction(QString("Tune SSB Demod %1:%2 to %3").arg(deviceSetIndex).arg(chi).arg(rxFormatted), tableContextMenu);
                                connect(tuneRxAction, &QAction::triggered, this, [deviceSetIndex, chi, rxFreq, sampleSource]()->void {

                                    int bw = sampleSource->getSampleRate();
                                    quint64 cf = sampleSource->getCenterFrequency();
                                    qint64 low = (cf - bw/2 - 2000);
                                    qint64 high = (cf + bw/2 + 2000);

                                    if ((rxFreq >= low) && (rxFreq <= high))
                                    {
                                        int offset = rxFreq - cf;
                                        ChannelWebAPIUtils::setFrequencyOffset(deviceSetIndex, chi, offset);
                                    }
                                    else
                                    {
                                        ChannelWebAPIUtils::setCenterFrequency(deviceSetIndex, rxFreq);
                                        ChannelWebAPIUtils::setFrequencyOffset(deviceSetIndex, chi, 0);
                                    }
                                });
                                tableContextMenu->addAction(tuneRxAction);
                            }
                        }
                    }
                }
                deviceSetIndex++;
            }
        }

        tableContextMenu->popup(ui->messages->viewport()->mapToGlobal(pos));
    }
}

void DSCDemodGUI::sendAreaToMapFeature(const QString& name, const QString& address, const QString& text)
{
    QRegularExpression re(QString("(\\d+)%1([NS]) (\\d+)%1([EW]) - (\\d+)%1([NS]) (\\d+)%1([EW])").arg(QChar(0xb0)));
    QRegularExpressionMatch match = re.match(address);
    if (match.hasMatch())
    {
        int lat1 = match.captured(1).toInt();
        if (match.captured(2) == "S") {
            lat1 = -lat1;
        }
        int lon1 = match.captured(3).toInt();
        if (match.captured(4) == "W") {
            lon1 = -lon1;
        }
        int lat2 = match.captured(5).toInt();
        if (match.captured(6) == "S") {
            lat2 = -lat2;
        }
        int lon2 = match.captured(7).toInt();
        if (match.captured(8) == "W") {
            lon2 = -lon2;
        }

        // Send to Map feature
        QList<ObjectPipe*> mapPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_dscDemod, "mapitems", mapPipes);

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
                swgMapItem->setLatitude(lat1);
                swgMapItem->setLongitude(lon1);
                swgMapItem->setAltitude(0.0);
                QString image = QString("none");
                swgMapItem->setImage(new QString(image));
                swgMapItem->setImageRotation(0);
                swgMapItem->setText(new QString(text));                   // Not used - label is used instead for now
                swgMapItem->setLabel(new QString(text));
                swgMapItem->setAltitudeReference(0);
                QList<SWGSDRangel::SWGMapCoordinate *> *coords = new QList<SWGSDRangel::SWGMapCoordinate *>();

                SWGSDRangel::SWGMapCoordinate* c = new SWGSDRangel::SWGMapCoordinate();
                c->setLatitude(lat1);
                c->setLongitude(lon1);
                c->setAltitude(0.0);
                coords->append(c);

                c = new SWGSDRangel::SWGMapCoordinate();
                c->setLatitude(lat1);
                c->setLongitude(lon2);
                c->setAltitude(0.0);
                coords->append(c);

                c = new SWGSDRangel::SWGMapCoordinate();
                c->setLatitude(lat2);
                c->setLongitude(lon2);
                c->setAltitude(0.0);
                coords->append(c);

                c = new SWGSDRangel::SWGMapCoordinate();
                c->setLatitude(lat2);
                c->setLongitude(lon1);
                c->setAltitude(0.0);
                coords->append(c);

                c = new SWGSDRangel::SWGMapCoordinate();
                c->setLatitude(lat1);
                c->setLongitude(lon1);
                c->setAltitude(0.0);
                coords->append(c);

                swgMapItem->setCoordinates(coords);
                swgMapItem->setType(3);

                MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_dscDemod, swgMapItem);
                messageQueue->push(msg);
            }
        }
    }
    else
    {
        qDebug() << "DSCDemodGUI::sendAreaToMapFeature: Couldn't parse address " << address;
    }
}

void DSCDemodGUI::clearAreaFromMapFeature(const QString& name)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_dscDemod, "mapitems", mapPipes);
    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setImage(new QString(""));
        swgMapItem->setType(3);
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_dscDemod, swgMapItem);
        messageQueue->push(msg);
    }
    m_mapItems.removeAll(name);
}

DSCDemodGUI::~DSCDemodGUI()
{
    delete m_aprsFi;
    delete ui;
}

void DSCDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DSCDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        DSCDemod::MsgConfigureDSCDemod* message = DSCDemod::MsgConfigureDSCDemod::create( m_settings, force);
        m_dscDemod->getInputMessageQueue()->push(message);
    }
}

void DSCDemodGUI::displaySettings()
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

    updateIndexLabel();

    ui->filterInvalid->setChecked(m_settings.m_filterInvalid);
    ui->filterColumn->setCurrentIndex(m_settings.m_filterColumn);
    ui->filter->setText(m_settings.m_filter);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->useFileTime->setChecked(m_settings.m_useFileTime);
    ui->feed->setChecked(m_settings.m_feed);

    // Order and size columns
    QHeaderView *header = ui->messages->horizontalHeader();
    for (int i = 0; i < DSCDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        m_menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->messages->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void DSCDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void DSCDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void DSCDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_dscDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
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

void DSCDemodGUI::on_feed_clicked(bool checked)
{
    m_settings.m_feed = checked;
    applySettings();
}

void DSCDemodGUI::on_feed_rightClicked(const QPoint &point)
{
    (void) point;

    QString id = MainCore::instance()->getSettings().getStationName();
    QString url = QString("http://yaddnet.org/pages/php/live_rx.php?rxid=%1").arg(id);
    QDesktopServices::openUrl(QUrl(url));
}

void DSCDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void DSCDemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received messages to", "", "*.csv");
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

// Read .csv log and process as received messages
void DSCDemodGUI::on_logOpen_clicked()
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
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Date", "Time", "Message", "Errors", "RSSI"}, error);
                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int messageCol = colIndexes.value("Message");
                    int errorsCol = colIndexes.value("Errors");
                    int rssiCol = colIndexes.value("RSSI");
                    int maxCol = std::max({dateCol, timeCol, messageCol, errorsCol, rssiCol});

                    QMessageBox dialog(this);
                    dialog.setText("Reading message data");
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
                            QString messageHex = cols[messageCol];
                            QByteArray bytes = QByteArray::fromHex(messageHex.toLatin1());
                            DSCMessage message(bytes, dateTime);
                            int errors = cols[errorsCol].toInt();
                            float rssi = cols[rssiCol].toFloat();
                            messageReceived(message, errors, rssi);
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
                    QMessageBox::critical(this, "DSC Demod", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "DSC Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void DSCDemodGUI::on_useFileTime_toggled(bool checked)
{
    m_settings.m_useFileTime = checked;
    applySettings();
}

void DSCDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DSCDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->filterInvalid, &ButtonSwitch::clicked, this, &DSCDemodGUI::on_filterInvalid_clicked);
    QObject::connect(ui->filterColumn, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSCDemodGUI::on_filterColumn_currentIndexChanged);
    QObject::connect(ui->filter, &QLineEdit::editingFinished, this, &DSCDemodGUI::on_filter_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &DSCDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &DSCDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &DSCDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &DSCDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &DSCDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &DSCDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &DSCDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->feed, &ButtonSwitch::clicked, this, &DSCDemodGUI::on_feed_clicked);
    QObject::connect(ui->useFileTime, &ButtonSwitch::toggled, this, &DSCDemodGUI::on_useFileTime_toggled);
}

void DSCDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void DSCDemodGUI::aprsFiDataUpdated(const QList<APRSFi::AISData>& data)
{
    for (int i = ui->messages->rowCount() - 1; i >= 0; i--)
    {
        bool match = false;
        QString mmsi;

        for (const auto& item : data)
        {
            mmsi = ui->messages->item(i, MESSAGE_COL_ADDRESS)->text();
            if (mmsi == item.m_mmsi)
            {
                ui->messages->item(i, MESSAGE_COL_ADDRESS_NAME)->setText(item.m_name);
                match = true;
            }
            mmsi = ui->messages->item(i, MESSAGE_COL_SELF_ID)->text();
            if (mmsi == item.m_mmsi)
            {
                ui->messages->item(i, MESSAGE_COL_SELF_ID_NAME)->setText(item.m_name);

                if ((item.m_latitude != 0.0) || (item.m_longitude != 0.0))
                {
                    // Calculate distance from My Position to position of source of message
                    Real stationLatitude = MainCore::instance()->getSettings().getLatitude();
                    Real stationLongitude = MainCore::instance()->getSettings().getLongitude();
                    Real stationAltitude = MainCore::instance()->getSettings().getAltitude();
                    QGeoCoordinate stationPosition(stationLatitude, stationLongitude, stationAltitude);
                    QGeoCoordinate shipPosition(item.m_latitude, item.m_longitude, 0.0);

                    float distance = stationPosition.distanceTo(shipPosition);

                    ui->messages->item(i, MESSAGE_COL_SELF_ID_RANGE)->setData(Qt::DisplayRole, (int)std::round(distance / 1000.0));
                }

                match = true;
            }
        }
        if (match) {
            break;
        }
    }
}
