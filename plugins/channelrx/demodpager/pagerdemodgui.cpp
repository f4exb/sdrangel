///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QDockWidget>
#include <QDebug>
#include <QAction>
#include <QRegExp>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QProcess>

#include "pagerdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_pagerdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "util/csv.h"
#include "util/units.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "dsp/dspengine.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "pagerdemod.h"
#include "pagerdemodcharsetdialog.h"
#include "pagerdemodnotificationdialog.h"
#include "pagerdemodfilterdialog.h"

#include "SWGMapItem.h"

void PagerDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_DATE, new QTableWidgetItem("Fri Apr 15 2016--"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_ADDRESS, new QTableWidgetItem("1000000"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_MESSAGE, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_FUNCTION, new QTableWidgetItem("0"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_ALPHA, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_NUMERIC, new QTableWidgetItem("123456789123456789123456789123456789123456789123456789"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_EVEN_PE, new QTableWidgetItem("0"));
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_BCH_PE, new QTableWidgetItem("0"));
    ui->messages->resizeColumnsToContents();
    ui->messages->removeRow(row);
}

// Columns in table reordered
void PagerDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_messageColumnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void PagerDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_messageColumnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void PagerDemodGUI::messagesColumnSelectMenu(QPoint pos)
{
    messagesMenu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void PagerDemodGUI::messagesColumnSelectMenuChecked(bool checked)
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
QAction *PagerDemodGUI::createCheckableItem(QString &text, int idx, bool checked, const char *slot)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, slot);
    return action;
}

PagerDemodGUI* PagerDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    PagerDemodGUI* gui = new PagerDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void PagerDemodGUI::destroy()
{
    delete this;
}

void PagerDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray PagerDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool PagerDemodGUI::deserialize(const QByteArray& data)
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

QString PagerDemodGUI::selectMessage(int functionBits, const QString &numericMessage, const QString &alphaMessage) const
{
    QString message;

    // Standard way of choosing numeric or alpha decode isn't followed widely
    if (m_settings.m_decode == PagerDemodSettings::Standard)
    {
        // Encoding is based on function bits
        if (functionBits == 0) {
            message = numericMessage;
        } else {
            message = alphaMessage;
        }
    }
    else if (m_settings.m_decode == PagerDemodSettings::Inverted)
    {
        // Encoding is based on function bits, but inverted from standard
        if (functionBits == 3) {
            message = numericMessage;
        } else {
            message = alphaMessage;
        }
    }
    else if (m_settings.m_decode == PagerDemodSettings::Numeric)
    {
        // Always display as numeric
        message = numericMessage;
    }
    else if (m_settings.m_decode == PagerDemodSettings::Alphanumeric)
    {
        // Always display as alphanumeric
        message = alphaMessage;
    }
    else
    {
        // Guess at what the encoding is
        QString numeric = numericMessage;
        QString alpha = alphaMessage;
        bool done = false;
        if (!done)
        {
            // If alpha contains control characters, possibly numeric
            for (int i = 0; i < alpha.size(); i++)
            {
                if (iscntrl(alpha[i].toLatin1()) && !isspace(alpha[i].toLatin1()))
                {
                    message = numeric;
                    done = true;
                    break;
                }
            }
        }
        if (!done) {
            // Possibly not likely to get only longer than 15 digits
            if (numeric.size() > 15)
            {
                done = true;
                message = alpha;
            }
        }
        if (!done) {
            // Default to alpha
            message = alpha;
        }
    }

    return message;

}

// Add row to table
void PagerDemodGUI::messageReceived(const QDateTime dateTime, int address, int functionBits,
        const QString &numericMessage, const QString &alphaMessage,
        int evenParityErrors, int bchParityErrors)
{
    QString message = selectMessage(functionBits, numericMessage, alphaMessage);
    QString addressString = QString("%1").arg(address, 7, 10, QChar('0'));

    // Should we ignore the message if it is a duplicate?
    if (m_settings.m_filterDuplicates  && (ui->messages->rowCount() > 0))
    {
        int startRow = m_settings.m_duplicateMatchLastOnly ? ui->messages->rowCount() - 1 : 0;
        for (int row = startRow; row < ui->messages->rowCount(); row++)
        {
            QString prevAddress = ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_ADDRESS)->text();
            QString prevMessage = ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_MESSAGE)->text();

            if ((message == prevMessage) && (m_settings.m_duplicateMatchMessageOnly || (addressString == prevAddress)))
            {
                // Ignore this message
                return;
            }
        }
    }

    // Is scroll bar at bottom
    QScrollBar *sb = ui->messages->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    // Add to messages table
    ui->messages->setSortingEnabled(false);
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);

    QTableWidgetItem *dateItem = new QTableWidgetItem();
    QTableWidgetItem *timeItem = new QTableWidgetItem();
    QTableWidgetItem *addressItem = new QTableWidgetItem();
    QTableWidgetItem *messageItem = new QTableWidgetItem();
    QTableWidgetItem *functionItem = new QTableWidgetItem();
    QTableWidgetItem *alphaItem = new QTableWidgetItem();
    QTableWidgetItem *numericItem = new QTableWidgetItem();
    QTableWidgetItem *evenPEItem = new QTableWidgetItem();
    QTableWidgetItem *bchPEItem = new QTableWidgetItem();
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_DATE, dateItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_TIME, timeItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_ADDRESS, addressItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_MESSAGE, messageItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_FUNCTION, functionItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_ALPHA, alphaItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_NUMERIC, numericItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_EVEN_PE, evenPEItem);
    ui->messages->setItem(row, PagerDemodSettings::MESSAGE_COL_BCH_PE, bchPEItem);
    dateItem->setText(dateTime.date().toString());
    timeItem->setText(dateTime.time().toString());
    addressItem->setText(addressString);
    messageItem->setText(message);
    functionItem->setText(QString("%1").arg(functionBits));
    alphaItem->setText(alphaMessage);
    numericItem->setText(numericMessage);
    evenPEItem->setText(QString("%1").arg(evenParityErrors));
    bchPEItem->setText(QString("%1").arg(bchParityErrors));
    filterRow(row);
    ui->messages->setSortingEnabled(true);
    if (scrollToBottom) {
        ui->messages->scrollToBottom();
    }
    checkNotification(row);
}

bool PagerDemodGUI::handleMessage(const Message& message)
{
    if (PagerDemod::MsgConfigurePagerDemod::match(message))
    {
        qDebug("PagerDemodGUI::handleMessage: PagerDemod::MsgConfigurePagerDemod");
        const PagerDemod::MsgConfigurePagerDemod& cfg = (const PagerDemod::MsgConfigurePagerDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->scopeGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (PagerDemod::MsgPagerMessage::match(message))
    {
        const PagerDemod::MsgPagerMessage& report = (const PagerDemod::MsgPagerMessage&) message;
        messageReceived(report.getDateTime(), report.getAddress(), report.getFunctionBits(),
            report.getNumericMessage(), report.getAlphaMessage(),
            report.getEvenParityErrors(), report.getBCHParityErrors());
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void PagerDemodGUI::handleInputMessages()
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

void PagerDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void PagerDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void PagerDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void PagerDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void PagerDemodGUI::on_fmDev_valueChanged(int value)
{
    ui->fmDevText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_fmDeviation = value * 100.0;
    applySettings();
}

void PagerDemodGUI::on_baud_currentIndexChanged(int index)
{
    (void)index;
    m_settings.m_baud = ui->baud->currentText().toInt();
    applySettings();
}

void PagerDemodGUI::on_decode_currentIndexChanged(int index)
{
    m_settings.m_decode = (PagerDemodSettings::Decode)index;
    applySettings();
}

void PagerDemodGUI::on_filterAddress_editingFinished()
{
    m_settings.m_filterAddress = ui->filterAddress->text();
    filter();
    applySettings();
}

void PagerDemodGUI::on_clearTable_clicked()
{
    ui->messages->setRowCount(0);
}

void PagerDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void PagerDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void PagerDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void PagerDemodGUI::on_channel1_currentIndexChanged(int index)
{
    m_settings.m_scopeCh1 = index;
    applySettings();
}

void PagerDemodGUI::on_channel2_currentIndexChanged(int index)
{
    m_settings.m_scopeCh2 = index;
    applySettings();
}

void PagerDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if (m_settings.m_filterAddress != "")
    {
        QRegExp re(m_settings.m_filterAddress);
        QTableWidgetItem *fromItem = ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_ADDRESS);
        if (!re.exactMatch(fromItem->text())) {
            hidden = true;
        }
    }
    ui->messages->setRowHidden(row, hidden);
}

void PagerDemodGUI::filter()
{
    for (int i = 0; i < ui->messages->rowCount(); i++) {
        filterRow(i);
    }
}

void PagerDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void PagerDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_pagerDemod->getNumberOfDeviceStreams());
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

PagerDemodGUI::PagerDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::PagerDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_tickCount(0),
    m_speech(nullptr)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodpager/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_pagerDemod = reinterpret_cast<PagerDemod*>(rxChannel);
    m_pagerDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_pagerDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    m_scopeVis->setLiveRate(PagerDemodSettings::m_channelSampleRate);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);
    ui->scopeGUI->setSampleRate(PagerDemodSettings::m_channelSampleRate);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Pager Demodulator");
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

    CRightClickEnabler *filterDuplicatesRightClickEnabler = new CRightClickEnabler(ui->filterDuplicates);
    connect(filterDuplicatesRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(on_filterDuplicates_rightClicked(const QPoint &)));

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

    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(true);
    m_resizer.enableChildMouseTracking();
}

void PagerDemodGUI::customContextMenuRequested(QPoint pos)
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

PagerDemodGUI::~PagerDemodGUI()
{
    clearFromMap();
    delete ui;
}

void PagerDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PagerDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        PagerDemod::MsgConfigurePagerDemod* message = PagerDemod::MsgConfigurePagerDemod::create( m_settings, force);
        m_pagerDemod->getInputMessageQueue()->push(message);
    }
}

void PagerDemodGUI::displaySettings()
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

    if (m_settings.m_baud == 512) {
        ui->baud->setCurrentIndex(0);
    } else if (m_settings.m_baud == 1200) {
        ui->baud->setCurrentIndex(1);
    } else {
        ui->baud->setCurrentIndex(2);
    }
    ui->decode->setCurrentIndex((int)m_settings.m_decode);

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);

    ui->fmDevText->setText(QString("%1k").arg(m_settings.m_fmDeviation / 1000.0, 0, 'f', 1));
    ui->fmDev->setValue(m_settings.m_fmDeviation / 100.0);

    updateIndexLabel();

    ui->filterAddress->setText(m_settings.m_filterAddress);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->filterDuplicates->setChecked(m_settings.m_filterDuplicates);

    // Order and size columns
    QHeaderView *header = ui->messages->horizontalHeader();

    for (int i = 0; i < PAGERDEMOD_MESSAGE_COLUMNS; i++)
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
    enableSpeechIfNeeded();
}

void PagerDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void PagerDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void PagerDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_pagerDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
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

void PagerDemodGUI::on_charset_clicked()
{
    PagerDemodCharsetDialog dialog(&m_settings);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted) {
        applySettings();
    }
}

void PagerDemodGUI::on_notifications_clicked()
{
    PagerDemodNotificationDialog dialog(&m_settings);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        enableSpeechIfNeeded();
        applySettings();
    }
}

void PagerDemodGUI::on_filterDuplicates_clicked(bool checked)
{
    m_settings.m_filterDuplicates = checked;
    applySettings();
}

void PagerDemodGUI::on_filterDuplicates_rightClicked(const QPoint &p)
{
    (void) p;

    PagerDemodFilterDialog dialog(&m_settings);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted) {
        applySettings();
    }
}

void PagerDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void PagerDemodGUI::on_logFilename_clicked()
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
void PagerDemodGUI::on_logOpen_clicked()
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
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Date", "Time", "Address", "Function Bits", "Alpha", "Numeric", "Even Parity Errors", "BCH Parity Errors"}, error);
                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int addressCol = colIndexes.value("Address");
                    int functionCol = colIndexes.value("Function Bits");
                    int alphaCol = colIndexes.value("Alpha");
                    int numericCol = colIndexes.value("Numeric");
                    int evenCol = colIndexes.value("Even Parity Errors");
                    int bchCol = colIndexes.value("BCH Parity Errors");
                    int maxCol = std::max({dateCol, timeCol, addressCol, functionCol, alphaCol, numericCol, evenCol, bchCol});

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
                            int address = cols[addressCol].toInt();
                            int functionBits = cols[functionCol].toInt();
                            int evenErrors = cols[evenCol].toInt();
                            int bchErrors = cols[bchCol].toInt();

                            messageReceived(dateTime, address, functionBits,
                                cols[numericCol], cols[alphaCol],
                                evenErrors, bchErrors);

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
                    QMessageBox::critical(this, "Pager Demod", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "Pager Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void PagerDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &PagerDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &PagerDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->fmDev, &QSlider::valueChanged, this, &PagerDemodGUI::on_fmDev_valueChanged);
    QObject::connect(ui->baud, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PagerDemodGUI::on_baud_currentIndexChanged);
    QObject::connect(ui->decode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PagerDemodGUI::on_decode_currentIndexChanged);
    QObject::connect(ui->charset, &QToolButton::clicked, this, &PagerDemodGUI::on_charset_clicked);
    QObject::connect(ui->filterAddress, &QLineEdit::editingFinished, this, &PagerDemodGUI::on_filterAddress_editingFinished);
    QObject::connect(ui->clearTable, &QToolButton::clicked, this, &PagerDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &PagerDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &PagerDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &PagerDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->notifications, &QToolButton::clicked, this, &PagerDemodGUI::on_notifications_clicked);
    QObject::connect(ui->filterDuplicates, &ButtonSwitch::clicked, this, &PagerDemodGUI::on_filterDuplicates_clicked);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &PagerDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &PagerDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &PagerDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->channel1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PagerDemodGUI::on_channel1_currentIndexChanged);
    QObject::connect(ui->channel2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PagerDemodGUI::on_channel2_currentIndexChanged);
}

void PagerDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

// Initialise text to speech engine
// This takes 10 seconds on some versions of Linux, so only do it, if user actually
// has speech notifications configured
void PagerDemodGUI::enableSpeechIfNeeded()
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        return;
    }
    for (const auto& notification : m_settings.m_notificationSettings)
    {
        if (!notification->m_speech.isEmpty())
        {
            qDebug() << "PagerDemodGUI: Enabling text to speech";
            m_speech = new QTextToSpeech(this);
            return;
        }
    }
#endif
}

void PagerDemodGUI::checkNotification(int row)
{
    QString address = ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_ADDRESS)->text();
    QString message = ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_MESSAGE)->text();

    for (int i = 0; i < m_settings.m_notificationSettings.size(); i++)
    {
        QString match;
        switch (m_settings.m_notificationSettings[i]->m_matchColumn)
        {
        case PagerDemodSettings::MESSAGE_COL_ADDRESS:
            match = address;
            break;
        case PagerDemodSettings::MESSAGE_COL_MESSAGE:
            match = message;
            break;
        }
        if (!match.isEmpty())
        {
            if (m_settings.m_notificationSettings[i]->m_regularExpression.isValid())
            {
                QRegularExpressionMatch matchResult = m_settings.m_notificationSettings[i]->m_regularExpression.match(match);
                if (matchResult.hasMatch())
                {
                    if (m_settings.m_notificationSettings[i]->m_highlight) {
                        ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_MESSAGE)->setForeground(QBrush(m_settings.m_notificationSettings[i]->m_highlightColor));
                    }

                    if (!m_settings.m_notificationSettings[i]->m_speech.isEmpty())
                    {
                        QString speech = subStrings(address, message, matchResult, m_settings.m_notificationSettings[i]->m_speech);

                        speechNotification(speech);
                    }
                    if (!m_settings.m_notificationSettings[i]->m_command.isEmpty())
                    {
                        QString command = subStrings(address, message, matchResult, m_settings.m_notificationSettings[i]->m_command);

                        commandNotification(command);
                    }
                    if (m_settings.m_notificationSettings[i]->m_plotOnMap)
                    {
                        float latitude;
                        float longitude;

                        if (Units::stringToLatitudeAndLongitude(message, latitude, longitude, false))
                        {
                            QDateTime dateTime;

                            dateTime.setDate(QDate::fromString(ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_DATE)->text()));
                            dateTime.setTime(QTime::fromString(ui->messages->item(row, PagerDemodSettings::MESSAGE_COL_TIME)->text()));

                            sendToMap(address, message, latitude, longitude, dateTime);
                        }
                    }
                }
            }
        }
    }
}

QString PagerDemodGUI::subStrings(const QString& address, const QString& message, const QRegularExpressionMatch& match, const QString &string) const
{
    QString s = string;
    s = s.replace("${address}", address);
    s = s.replace("${message}", message);
    for (int i = 0; i < match.capturedTexts().size(); i++)
    {
        QString escape = QString("${%1}").arg(i);
        s = s.replace(escape, match.capturedTexts()[i]);
    }
    return s;
}

void PagerDemodGUI::speechNotification(const QString &speech)
{
#ifdef QT_TEXTTOSPEECH_FOUND
    if (m_speech) {
        m_speech->say(speech);
    } else {
        qWarning() << "PagerDemodGUI::speechNotification: Unable to say " << speech;
    }
#else
    qWarning() << "PagerDemodGUI::speechNotification: TextToSpeech not supported. Unable to say " << speech;
#endif
}

void PagerDemodGUI::commandNotification(const QString &command)
{
#if QT_CONFIG(process)
    QStringList allArgs = QProcess::splitCommand(command);

    if (allArgs.size() > 0)
    {
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }
#else
    qWarning() << "PagerDemodGUI::commandNotification: QProcess not supported. Can't run: " << command;
#endif
}

void PagerDemodGUI::sendToMap(const QString& address, const QString& message, float latitude, float longitude, QDateTime dateTime)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_pagerDemod, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(address));
        swgMapItem->setLatitude(latitude);
        swgMapItem->setLongitude(longitude);
        swgMapItem->setAltitude(0);
        swgMapItem->setAltitudeReference(1); // CLAMP_TO_GROUND
        swgMapItem->setFixedPosition(false);
        swgMapItem->setPositionDateTime(new QString(dateTime.toString(Qt::ISODateWithMs)));

        swgMapItem->setImageRotation(0);
        swgMapItem->setText(new QString(message));
        swgMapItem->setImage(new QString(QString("pager.png")));

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_pagerDemod, swgMapItem);
        messageQueue->push(msg);
    }

    m_mapItems.insert(address);
}

// Clear all items from map
void PagerDemodGUI::clearFromMap()
{
    for (const auto& address : m_mapItems)
    {
        QList<ObjectPipe*> mapPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(m_pagerDemod, "mapitems", mapPipes);

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(address));
            swgMapItem->setImage(new QString(QString("")));

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_pagerDemod, swgMapItem);
            messageQueue->push(msg);
        }
    }

    m_mapItems.clear();
}
