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
#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>

#include "ieee_802_15_4_demodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspcommands.h"
#include "dsp/glscopesettings.h"
#include "dsp/scopevis.h"
#include "ui_ieee_802_15_4_demodgui.h"
#include "plugin/pluginapi.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/ieee_802_15_4.h"
#include "gui/messagedialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/datetimedelegate.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/tabletapandhold.h"
#include "maincore.h"

#include "ieee_802_15_4_demod.h"

void IEEE_802_15_4_DemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->frames->rowCount();
    ui->frames->setRowCount(row + 1);
    ui->frames->setItem(row, FRAME_COL_DATE_TIME, new QTableWidgetItem("2016/04/15 10:17:00-"));
    ui->frames->setItem(row, FRAME_COL_TYPE, new QTableWidgetItem("Command-"));
    ui->frames->setItem(row, FRAME_COL_SEQ, new QTableWidgetItem("255-"));
    ui->frames->setItem(row, FRAME_COL_DST_PAN, new QTableWidgetItem("0xffff-"));
    ui->frames->setItem(row, FRAME_COL_DST_ADDR, new QTableWidgetItem("00:11:22:33:44:55:66:77-"));
    ui->frames->setItem(row, FRAME_COL_SRC_PAN, new QTableWidgetItem("0xffff-"));
    ui->frames->setItem(row, FRAME_COL_SRC_ADDR, new QTableWidgetItem("00:11:22:33:44:55:66:77-"));
    ui->frames->setItem(row, FRAME_COL_INFO, new QTableWidgetItem("Association Request"));
    ui->frames->setItem(row, FRAME_COL_LENGTH, new QTableWidgetItem("127-"));
    ui->frames->setItem(row, FRAME_COL_PAYLOAD, new QTableWidgetItem("000102030405060708090a0b0c0d0e0f"));
    ui->frames->setItem(row, FRAME_COL_PSDU, new QTableWidgetItem("000102030405060708090a0b0c0d0e0f"));
    ui->frames->setItem(row, FRAME_COL_FCS, new QTableWidgetItem("Bad-"));
    ui->frames->setItem(row, FRAME_COL_POWER, new QTableWidgetItem("-100.0-"));
    ui->frames->setItem(row, FRAME_COL_SNR, new QTableWidgetItem("30.0-"));
    ui->frames->setItem(row, FRAME_COL_LQI, new QTableWidgetItem("0.00-"));
    ui->frames->setItem(row, FRAME_COL_FREQUENCY_OFFSET, new QTableWidgetItem("-200000-"));
    ui->frames->resizeColumnsToContents();
    ui->frames->removeRow(row);
}

// Columns in table reordered
void IEEE_802_15_4_DemodGUI::frames_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void IEEE_802_15_4_DemodGUI::frames_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void IEEE_802_15_4_DemodGUI::framesColumnSelectMenu(QPoint pos)
{
    m_framesMenu->popup(ui->frames->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void IEEE_802_15_4_DemodGUI::framesColumnSelectMenuChecked(bool checked)
{
    (void) checked;

    QAction* action = qobject_cast<QAction*>(sender());

    if (action != nullptr)
    {
        int idx = action->data().toInt(nullptr);
        ui->frames->setColumnHidden(idx, !action->isChecked());
    }
}

// Create column select menu item
QAction *IEEE_802_15_4_DemodGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

IEEE_802_15_4_DemodGUI* IEEE_802_15_4_DemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    IEEE_802_15_4_DemodGUI* gui = new IEEE_802_15_4_DemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void IEEE_802_15_4_DemodGUI::destroy()
{
    delete this;
}

void IEEE_802_15_4_DemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray IEEE_802_15_4_DemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool IEEE_802_15_4_DemodGUI::deserialize(const QByteArray& data)
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
void IEEE_802_15_4_DemodGUI::frameReceived(const QByteArray& psdu, const QDateTime& dateTime, bool fcsOK, float frequencyOffset,
                                           float lqi, float powerDB, float snr)
{
    IEEE_802_15_4_FrameDecoder mac;
    mac.decode(psdu);

    // Is scroll bar at bottom
    QScrollBar *sb = ui->frames->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    ui->frames->setSortingEnabled(false);
    int row = ui->frames->rowCount();
    ui->frames->setRowCount(row + 1);

    QTableWidgetItem *items[IEEE_802_15_4_DEMOD_COLUMNS];
    for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++)
    {
        items[i] = new QTableWidgetItem();
        ui->frames->setItem(row, i, items[i]);
    }

    items[FRAME_COL_DATE_TIME]->setData(Qt::DisplayRole, dateTime);
    items[FRAME_COL_TYPE]->setText(mac.m_valid ? mac.getFrameTypeString() : "Invalid");
    items[FRAME_COL_TYPE]->setData(Qt::UserRole, (int) mac.m_frameType);
    if (mac.m_hasSeqNo) {
        items[FRAME_COL_SEQ]->setData(Qt::DisplayRole, (int) mac.m_seqNo);
    }
    items[FRAME_COL_DST_PAN]->setText(mac.getDestPANIDString());
    items[FRAME_COL_DST_ADDR]->setText(mac.getDestAddressString());
    items[FRAME_COL_SRC_PAN]->setText(mac.getSrcPANIDString());
    items[FRAME_COL_SRC_ADDR]->setText(mac.getSrcAddressString());
    items[FRAME_COL_INFO]->setText(mac.toString());
    items[FRAME_COL_LENGTH]->setData(Qt::DisplayRole, (int) psdu.size());
    items[FRAME_COL_PAYLOAD]->setText(mac.m_payload.toHex());
    items[FRAME_COL_PSDU]->setText(psdu.toHex());
    items[FRAME_COL_FCS]->setText(fcsOK ? "OK" : "Bad");
    items[FRAME_COL_POWER]->setData(Qt::DisplayRole, std::round(powerDB * 10.0) / 10.0);
    items[FRAME_COL_SNR]->setData(Qt::DisplayRole, std::round(snr * 10.0) / 10.0);
    items[FRAME_COL_LQI]->setData(Qt::DisplayRole, std::round(lqi * 100.0) / 100.0);
    items[FRAME_COL_FREQUENCY_OFFSET]->setData(Qt::DisplayRole, (int) std::round(frequencyOffset));

    if (!fcsOK)
    {
        for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++) {
            items[i]->setForeground(QBrush(Qt::red));
        }
    }

    if (!m_loadingData)
    {
        filterRow(row);
        ui->frames->setSortingEnabled(true);

        if (scrollToBottom) {
            ui->frames->scrollToBottom();
        }
    }
}

bool IEEE_802_15_4_DemodGUI::handleMessage(const Message& message)
{
    if (IEEE_802_15_4_Demod::MsgConfigureIEEE_802_15_4_Demod::match(message))
    {
        qDebug("IEEE_802_15_4_DemodGUI::handleMessage: IEEE_802_15_4_Demod::MsgConfigureIEEE_802_15_4_Demod");
        const IEEE_802_15_4_Demod::MsgConfigureIEEE_802_15_4_Demod& cfg = (IEEE_802_15_4_Demod::MsgConfigureIEEE_802_15_4_Demod&) message;

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
    else if (IEEE_802_15_4_Demod::MsgFrame::match(message))
    {
        const IEEE_802_15_4_Demod::MsgFrame& frame = (const IEEE_802_15_4_Demod::MsgFrame&) message;
        frameReceived(frame.getPSDU(), frame.getDateTime(), frame.getFCSOK(), frame.getFrequencyOffset(),
                      frame.getLQI(), frame.getPowerDB(), frame.getSNR());
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
        checkSampleRate();
        return true;
    }

    return false;
}

void IEEE_802_15_4_DemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void IEEE_802_15_4_DemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void IEEE_802_15_4_DemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void IEEE_802_15_4_DemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void IEEE_802_15_4_DemodGUI::on_phy_currentIndexChanged(int index)
{
    (void) index;

    // Also sets RF bandwidth and maximum frequency offset to defaults for the PHY
    m_settings.setPHY(ui->phy->currentText());
    displayPHYDependent();
    applySettings(QStringList({"phy", "rfBandwidth", "maxFrequencyOffset"}));
}

void IEEE_802_15_4_DemodGUI::on_samplesPerChip_valueChanged(int value)
{
    m_settings.m_samplesPerChip = value;
    displayPHYDependent();
    applySettings(QStringList({"samplesPerChip"}));
}

void IEEE_802_15_4_DemodGUI::on_rfBW_valueChanged(int value)
{
    m_settings.m_rfBandwidth = value * 10000.0f;
    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1e6, 0, 'f', 2));
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    checkSampleRate();
    applySettings(QStringList({"rfBandwidth"}));
}

void IEEE_802_15_4_DemodGUI::on_maxFrequencyOffset_valueChanged(int value)
{
    m_settings.m_maxFrequencyOffset = value * 1000.0f;
    applySettings(QStringList({"maxFrequencyOffset"}));
}

void IEEE_802_15_4_DemodGUI::on_threshold_valueChanged(int value)
{
    m_settings.m_threshold = value / 100.0f;
    ui->thresholdText->setText(QString("%1").arg(m_settings.m_threshold, 0, 'f', 2));
    applySettings(QStringList({"threshold"}));
}

void IEEE_802_15_4_DemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings(QStringList({"udpEnabled"}));
}

void IEEE_802_15_4_DemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings(QStringList({"udpAddress"}));
}

void IEEE_802_15_4_DemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings(QStringList({"udpPort"}));
}

void IEEE_802_15_4_DemodGUI::on_udpFormat_currentIndexChanged(int index)
{
    m_settings.m_udpFormat = (IEEE_802_15_4_DemodSettings::UDPFormat) index;
    applySettings(QStringList({"udpFormat"}));
}

void IEEE_802_15_4_DemodGUI::on_filterFrameType_currentIndexChanged(int index)
{
    m_settings.m_filterFrameType = (IEEE_802_15_4_DemodSettings::FrameTypeFilter) index;
    filter();
    applySettings(QStringList({"filterFrameType"}));
}

void IEEE_802_15_4_DemodGUI::on_filterPANID_editingFinished()
{
    m_settings.m_filterPANID = ui->filterPANID->text();
    filter();
    applySettings(QStringList({"filterPANID"}));
}

void IEEE_802_15_4_DemodGUI::on_filterAddress_editingFinished()
{
    m_settings.m_filterAddress = ui->filterAddress->text();
    filter();
    applySettings(QStringList({"filterAddress"}));
}

void IEEE_802_15_4_DemodGUI::on_showBadFCS_toggled(bool checked)
{
    m_settings.m_showBadFCS = checked;
    filter();
    applySettings(QStringList({"showBadFCS"}));
}

void IEEE_802_15_4_DemodGUI::on_useFileTime_toggled(bool checked)
{
    m_settings.m_useFileTime = checked;
    applySettings(QStringList({"useFileTime"}));
}

void IEEE_802_15_4_DemodGUI::on_clearTable_clicked()
{
    ui->frames->setRowCount(0);
}

void IEEE_802_15_4_DemodGUI::filterRow(int row)
{
    bool hidden = false;

    if (m_settings.m_filterFrameType != IEEE_802_15_4_DemodSettings::FilterAll)
    {
        int type = ui->frames->item(row, FRAME_COL_TYPE)->data(Qt::UserRole).toInt();
        int filterType;

        switch (m_settings.m_filterFrameType)
        {
        case IEEE_802_15_4_DemodSettings::FilterBeacon:
            filterType = IEEE_802_15_4_FrameDecoder::Beacon;
            break;
        case IEEE_802_15_4_DemodSettings::FilterData:
            filterType = IEEE_802_15_4_FrameDecoder::Data;
            break;
        case IEEE_802_15_4_DemodSettings::FilterAck:
            filterType = IEEE_802_15_4_FrameDecoder::Ack;
            break;
        case IEEE_802_15_4_DemodSettings::FilterCommand:
            filterType = IEEE_802_15_4_FrameDecoder::Command;
            break;
        default:
            filterType = -1;
            break;
        }

        if (filterType >= 0) {
            hidden = type != filterType;
        } else {
            hidden = type <= IEEE_802_15_4_FrameDecoder::Command;
        }
    }

    if (!hidden && !m_settings.m_filterPANID.isEmpty())
    {
        QRegularExpression re(QRegularExpression::anchoredPattern(m_settings.m_filterPANID), QRegularExpression::CaseInsensitiveOption);
        bool match = re.match(ui->frames->item(row, FRAME_COL_DST_PAN)->text()).hasMatch()
            || re.match(ui->frames->item(row, FRAME_COL_SRC_PAN)->text()).hasMatch();
        hidden = !match;
    }

    if (!hidden && !m_settings.m_filterAddress.isEmpty())
    {
        QRegularExpression re(QRegularExpression::anchoredPattern(m_settings.m_filterAddress), QRegularExpression::CaseInsensitiveOption);
        bool match = re.match(ui->frames->item(row, FRAME_COL_DST_ADDR)->text()).hasMatch()
            || re.match(ui->frames->item(row, FRAME_COL_SRC_ADDR)->text()).hasMatch();
        hidden = !match;
    }

    if (!hidden && !m_settings.m_showBadFCS) {
        hidden = ui->frames->item(row, FRAME_COL_FCS)->text() != "OK";
    }

    ui->frames->setRowHidden(row, hidden);
}

void IEEE_802_15_4_DemodGUI::filter()
{
    for (int i = 0; i < ui->frames->rowCount(); i++) {
        filterRow(i);
    }
}

void IEEE_802_15_4_DemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList({"rollupState"}));
}

void IEEE_802_15_4_DemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_demod->getNumberOfDeviceStreams());
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

        applySettings(QStringList({"title", "rgbColor", "useReverseAPI", "reverseAPIAddress",
                                   "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex", "streamIndex"}));
    }

    resetContextMenuType();
}

IEEE_802_15_4_DemodGUI::IEEE_802_15_4_DemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::IEEE_802_15_4_DemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0),
    m_loadingData(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demod802.15.4/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_demod = reinterpret_cast<IEEE_802_15_4_Demod*>(rxChannel);
    m_demod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_demod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setStreams(QStringList({"IQ", "MagSq", "Detect", "Correlation", "Chip", "Phase", "State", "FCS"}));

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

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(QColor(255, 0, 255));
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("802.15.4 Demodulator");
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

    // Display date and time in a single column, which sorts chronologically
    ui->frames->setItemDelegateForColumn(FRAME_COL_DATE_TIME, new DateTimeDelegate("yyyy/MM/dd hh:mm:ss", ui->frames));

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->frames->horizontalHeader()->setSectionsMovable(true);
    // Allow user to sort table by clicking on headers
    ui->frames->setSortingEnabled(true);
    // Add context menu to allow hiding/showing of columns
    m_framesMenu = new QMenu(ui->frames);
    for (int i = 0; i < ui->frames->horizontalHeader()->count(); i++)
    {
        QString text = ui->frames->horizontalHeaderItem(i)->text();
        m_framesMenu->addAction(createCheckableItem(text, i, true, SLOT(framesColumnSelectMenuChecked())));
    }
    ui->frames->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->frames->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(framesColumnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->frames->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(frames_sectionMoved(int, int, int)));
    connect(ui->frames->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(frames_sectionResized(int, int, int)));
    ui->frames->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->frames, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->frames);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &IEEE_802_15_4_DemodGUI::customContextMenuRequested);

    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

void IEEE_802_15_4_DemodGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item = ui->frames->itemAt(pos);

    if (item)
    {
        QMenu* tableContextMenu = new QMenu(ui->frames);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);
        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);
        tableContextMenu->popup(ui->frames->viewport()->mapToGlobal(pos));
    }
}

IEEE_802_15_4_DemodGUI::~IEEE_802_15_4_DemodGUI()
{
    delete ui;
}

void IEEE_802_15_4_DemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void IEEE_802_15_4_DemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        IEEE_802_15_4_Demod::MsgConfigureIEEE_802_15_4_Demod* message = IEEE_802_15_4_Demod::MsgConfigureIEEE_802_15_4_Demod::create(settingsKeys, m_settings, force);
        m_demod->getInputMessageQueue()->push(message);
    }
}

// Display settings that depend on the PHY or samples per chip
void IEEE_802_15_4_DemodGUI::displayPHYDependent()
{
    bool block = !m_doApplySettings;
    blockApplySettings(true);

    // Don't let the widgets' slots round the settings to the widgets' resolution
    QSignalBlocker rfBWBlocker(ui->rfBW);
    QSignalBlocker maxFrequencyOffsetBlocker(ui->maxFrequencyOffset);

    IEEE_802_15_4_PHY phy = m_settings.getPHY();
    ui->chipRate->setText(QString("%1k").arg(phy.getChipRate() / 1000));
    ui->sampleRate->setText(QString("%1M").arg(m_settings.getChannelSampleRate() / 1e6, 0, 'f', 1));
    ui->rfBW->setValue((int) std::round(m_settings.m_rfBandwidth / 10000.0f));
    ui->rfBWText->setText(QString("%1M").arg(m_settings.m_rfBandwidth / 1e6, 0, 'f', 2));
    ui->maxFrequencyOffset->setValue((int) std::round(m_settings.m_maxFrequencyOffset / 1000.0f));
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_scopeVis->setLiveRate(m_settings.getChannelSampleRate());
    checkSampleRate();

    blockApplySettings(block);
}

// Warn if the device sample rate is too low for the RF bandwidth
void IEEE_802_15_4_DemodGUI::checkSampleRate()
{
    if (m_basebandSampleRate < m_settings.m_rfBandwidth)
    {
        ui->rfBWText->setStyleSheet("QLabel { background-color : red; }");
        ui->rfBWText->setToolTip(QString("Device sample rate (%1 MSa/s) is lower than the RF bandwidth").arg(m_basebandSampleRate / 1e6, 0, 'f', 2));
    }
    else
    {
        ui->rfBWText->setStyleSheet("");
        ui->rfBWText->setToolTip("");
    }
}

void IEEE_802_15_4_DemodGUI::displaySettings()
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

    // Widget slots modify m_settings (E.g. selecting a PHY sets the RF bandwidth to the
    // default for it), so must not be run when displaying settings
    QSignalBlocker phyBlocker(ui->phy);
    QSignalBlocker samplesPerChipBlocker(ui->samplesPerChip);
    QSignalBlocker thresholdBlocker(ui->threshold);
    QSignalBlocker udpFormatBlocker(ui->udpFormat);
    QSignalBlocker filterFrameTypeBlocker(ui->filterFrameType);
    QSignalBlocker showBadFCSBlocker(ui->showBadFCS);
    QSignalBlocker useFileTimeBlocker(ui->useFileTime);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    int phyIndex = ui->phy->findText(m_settings.m_phy);
    ui->phy->setCurrentIndex(phyIndex >= 0 ? phyIndex : ui->phy->count() - 1);
    ui->samplesPerChip->setValue(m_settings.m_samplesPerChip);
    displayPHYDependent();

    ui->thresholdText->setText(QString("%1").arg(m_settings.m_threshold, 0, 'f', 2));
    ui->threshold->setValue((int) std::round(m_settings.m_threshold * 100.0f));

    updateIndexLabel();

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));
    ui->udpFormat->setCurrentIndex((int) m_settings.m_udpFormat);

    ui->filterFrameType->setCurrentIndex((int) m_settings.m_filterFrameType);
    ui->filterPANID->setText(m_settings.m_filterPANID);
    ui->filterAddress->setText(m_settings.m_filterAddress);
    ui->showBadFCS->setChecked(m_settings.m_showBadFCS);

    ui->logFilename->setToolTip(QString(".csv/.pcap log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);
    ui->useFileTime->setChecked(m_settings.m_useFileTime);

    // Order and size columns
    QHeaderView *header = ui->frames->horizontalHeader();
    for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        m_framesMenu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0) {
            ui->frames->setColumnWidth(i, m_settings.m_columnSizes[i]);
        }
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void IEEE_802_15_4_DemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void IEEE_802_15_4_DemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void IEEE_802_15_4_DemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_demod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
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

void IEEE_802_15_4_DemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings(QStringList({"logEnabled"}));
}

void IEEE_802_15_4_DemodGUI::on_logFilename_clicked()
{
    // Get filename to save to
    QFileDialog fileDialog(nullptr, "Select file to log received frames to", "", "*.csv *.pcap");
    fileDialog.setDefaultSuffix("csv");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);

    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();

        if (fileNames.size() > 0)
        {
            m_settings.m_logFilename = fileNames[0];
            ui->logFilename->setToolTip(QString(".csv/.pcap log filename: %1").arg(m_settings.m_logFilename));
            applySettings(QStringList({"logFilename"}));
        }
    }
}

// Read .csv log and process as received frames
void IEEE_802_15_4_DemodGUI::on_logOpen_clicked()
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
                m_loadingData = true;
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Date", "Time", "Data", "FCS"}, error);

                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int dataCol = colIndexes.value("Data");
                    int fcsCol = colIndexes.value("FCS");
                    int powerCol = colIndexes.value("Power", -1);
                    int snrCol = colIndexes.value("SNR", -1);
                    int lqiCol = colIndexes.value("LQI", -1);
                    int freqCol = colIndexes.value("Freq Offset", -1);
                    int maxCol = std::max({dateCol, timeCol, dataCol, fcsCol});
                    QStringList cols;

                    while (CSV::readRow(in, &cols))
                    {
                        if (cols.size() > maxCol)
                        {
                            QDate date = QDate::fromString(cols[dateCol]);
                            QTime time = QTime::fromString(cols[timeCol]);
                            QDateTime dateTime(date, time);
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());
                            bool fcsOK = cols[fcsCol] == "OK";
                            auto value = [&cols](int col) -> float {
                                return (col >= 0) && (col < cols.size()) ? cols[col].toFloat() : 0.0f;
                            };

                            frameReceived(bytes, dateTime, fcsOK, value(freqCol), value(lqiCol), value(powerCol), value(snrCol));
                        }
                    }
                }
                else
                {
                    MessageDialog::critical(this, "802.15.4 Demod", error);
                }

                m_loadingData = false;
                ui->frames->setSortingEnabled(true);
                filter();
            }
            else
            {
                MessageDialog::critical(this, "802.15.4 Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void IEEE_802_15_4_DemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &IEEE_802_15_4_DemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->phy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &IEEE_802_15_4_DemodGUI::on_phy_currentIndexChanged);
    QObject::connect(ui->samplesPerChip, QOverload<int>::of(&QSpinBox::valueChanged), this, &IEEE_802_15_4_DemodGUI::on_samplesPerChip_valueChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &IEEE_802_15_4_DemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->maxFrequencyOffset, QOverload<int>::of(&QSpinBox::valueChanged), this, &IEEE_802_15_4_DemodGUI::on_maxFrequencyOffset_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &IEEE_802_15_4_DemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &IEEE_802_15_4_DemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &IEEE_802_15_4_DemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &IEEE_802_15_4_DemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->udpFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &IEEE_802_15_4_DemodGUI::on_udpFormat_currentIndexChanged);
    QObject::connect(ui->filterFrameType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &IEEE_802_15_4_DemodGUI::on_filterFrameType_currentIndexChanged);
    QObject::connect(ui->filterPANID, &QLineEdit::editingFinished, this, &IEEE_802_15_4_DemodGUI::on_filterPANID_editingFinished);
    QObject::connect(ui->filterAddress, &QLineEdit::editingFinished, this, &IEEE_802_15_4_DemodGUI::on_filterAddress_editingFinished);
    QObject::connect(ui->showBadFCS, &ButtonSwitch::toggled, this, &IEEE_802_15_4_DemodGUI::on_showBadFCS_toggled);
    QObject::connect(ui->useFileTime, &ButtonSwitch::toggled, this, &IEEE_802_15_4_DemodGUI::on_useFileTime_toggled);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &IEEE_802_15_4_DemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &IEEE_802_15_4_DemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &IEEE_802_15_4_DemodGUI::on_logOpen_clicked);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &IEEE_802_15_4_DemodGUI::on_clearTable_clicked);
}

void IEEE_802_15_4_DemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
