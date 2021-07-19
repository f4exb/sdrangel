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
#include <QDebug>
#include <QAction>
#include <QRegExp>
#include <QClipboard>

#include "pagerdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_pagerdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/crightclickenabler.h"
#include "maincore.h"

#include "pagerdemod.h"
#include "pagerdemodcharsetdialog.h"

void PagerDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);
    ui->messages->setItem(row, MESSAGE_COL_DATE, new QTableWidgetItem("Fri Apr 15 2016-"));
    ui->messages->setItem(row, MESSAGE_COL_TIME, new QTableWidgetItem("10:17:00"));
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS, new QTableWidgetItem("1000000"));
    ui->messages->setItem(row, MESSAGE_COL_MESSAGE, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->messages->setItem(row, MESSAGE_COL_FUNCTION, new QTableWidgetItem("0"));
    ui->messages->setItem(row, MESSAGE_COL_ALPHA, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZ"));
    ui->messages->setItem(row, MESSAGE_COL_NUMERIC, new QTableWidgetItem("123456789123456789123456789123456789123456789123456789"));
    ui->messages->setItem(row, MESSAGE_COL_EVEN_PE, new QTableWidgetItem("0"));
    ui->messages->setItem(row, MESSAGE_COL_BCH_PE, new QTableWidgetItem("0"));
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

// Add row to table
void PagerDemodGUI::messageReceived(const PagerDemod::MsgPagerMessage& message)
{
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
    ui->messages->setItem(row, MESSAGE_COL_DATE, dateItem);
    ui->messages->setItem(row, MESSAGE_COL_TIME, timeItem);
    ui->messages->setItem(row, MESSAGE_COL_ADDRESS, addressItem);
    ui->messages->setItem(row, MESSAGE_COL_MESSAGE, messageItem);
    ui->messages->setItem(row, MESSAGE_COL_FUNCTION, functionItem);
    ui->messages->setItem(row, MESSAGE_COL_ALPHA, alphaItem);
    ui->messages->setItem(row, MESSAGE_COL_NUMERIC, numericItem);
    ui->messages->setItem(row, MESSAGE_COL_EVEN_PE, evenPEItem);
    ui->messages->setItem(row, MESSAGE_COL_BCH_PE, bchPEItem);
    dateItem->setText(message.getDateTime().date().toString());
    timeItem->setText(message.getDateTime().time().toString());
    addressItem->setText(QString("%1").arg(message.getAddress(), 7, 10, QChar('0')));
    // Standard way of choosing numeric or alpha decode isn't followed widely
    if (m_settings.m_decode == PagerDemodSettings::Standard)
    {
        // Encoding is based on function bits
        if (message.getFunctionBits() == 0) {
            messageItem->setText(message.getNumericMessage());
        } else {
            messageItem->setText(message.getAlphaMessage());
        }
    }
    else if (m_settings.m_decode == PagerDemodSettings::Inverted)
    {
        // Encoding is based on function bits, but inverted from standard
        if (message.getFunctionBits() == 3) {
            messageItem->setText(message.getNumericMessage());
        } else {
            messageItem->setText(message.getAlphaMessage());
        }
    }
    else if (m_settings.m_decode == PagerDemodSettings::Numeric)
    {
        // Always display as numeric
        messageItem->setText(message.getNumericMessage());
    }
    else if (m_settings.m_decode == PagerDemodSettings::Alphanumeric)
    {
        // Always display as alphanumeric
        messageItem->setText(message.getAlphaMessage());
    }
    else
    {
        // Guess at what the encoding is
        QString numeric = message.getNumericMessage();
        QString alpha = message.getAlphaMessage();
        bool done = false;
        if (!done)
        {
            // If alpha contains control characters, possibly numeric
            for (int i = 0; i < alpha.size(); i++)
            {
                if (iscntrl(alpha[i].toLatin1()) && !isspace(alpha[i].toLatin1()))
                {
                    messageItem->setText(numeric);
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
                messageItem->setText(alpha);
            }
        }
        if (!done) {
            // Default to alpha
            messageItem->setText(alpha);
        }
    }
    functionItem->setText(QString("%1").arg(message.getFunctionBits()));
    alphaItem->setText(message.getAlphaMessage());
    numericItem->setText(message.getNumericMessage());
    evenPEItem->setText(QString("%1").arg(message.getEvenParityErrors()));
    bchPEItem->setText(QString("%1").arg(message.getBCHParityErrors()));
    ui->messages->setSortingEnabled(true);
    ui->messages->scrollToItem(dateItem); // Will only scroll if not hidden
    filterRow(row);
}

bool PagerDemodGUI::handleMessage(const Message& message)
{
    if (PagerDemod::MsgConfigurePagerDemod::match(message))
    {
        qDebug("PagerDemodGUI::handleMessage: PagerDemod::MsgConfigurePagerDemod");
        const PagerDemod::MsgConfigurePagerDemod& cfg = (PagerDemod::MsgConfigurePagerDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (PagerDemod::MsgPagerMessage::match(message))
    {
        PagerDemod::MsgPagerMessage& report = (PagerDemod::MsgPagerMessage&) message;
        messageReceived(report);
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
        QTableWidgetItem *fromItem = ui->messages->item(row, MESSAGE_COL_ADDRESS);
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
    if (widget == ui->scopeContainer)
    {
        if (rollDown)
        {
            // Make wide enough for scope controls
            setMinimumWidth(716);
        }
        else
        {
            setMinimumWidth(352);
        }
    }
}

void PagerDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_pagerDemod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
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
    m_doApplySettings(true),
    m_tickCount(0)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
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

    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_deviceUISet->addRollupWidget(this);

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

    ui->scopeContainer->setVisible(false);

    displaySettings();
    applySettings(true);
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

    displayStreamIndex();

    ui->filterAddress->setText(m_settings.m_filterAddress);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);

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

    blockApplySettings(false);
}

void PagerDemodGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void PagerDemodGUI::leaveEvent(QEvent*)
{
    m_channelMarker.setHighlighted(false);
}

void PagerDemodGUI::enterEvent(QEvent*)
{
    m_channelMarker.setHighlighted(true);
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
    if (dialog.exec() == QDialog::Accepted)
    {
        applySettings();
    }
}
