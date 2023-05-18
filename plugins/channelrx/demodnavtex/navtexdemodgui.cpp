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

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QClipboard>
#include <QFileDialog>
#include <QScrollBar>
#include <QMenu>

#include "navtexdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_navtexdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/csv.h"
#include "util/db.h"
#include "util/morse.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/decimaldelegate.h"
#include "dsp/dspengine.h"
#include "dsp/glscopesettings.h"
#include "gui/crightclickenabler.h"
#include "gui/tabletapandhold.h"
#include "gui/dialogpositioner.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "maincore.h"

#include "navtexdemod.h"
#include "navtexdemodsink.h"

void NavtexDemodGUI::resizeTable()
{
    // Fill table with a row of dummy data that will size the columns nicely
    // Trailing spaces are for sort arrow
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);
    ui->messages->setItem(row, MESSAGE_COL_DATE, new QTableWidgetItem("15/04/2016-"));
    ui->messages->setItem(row, MESSAGE_COL_TIME, new QTableWidgetItem("10:17"));
    ui->messages->setItem(row, MESSAGE_COL_STATION_ID, new QTableWidgetItem("A"));
    ui->messages->setItem(row, MESSAGE_COL_STATION, new QTableWidgetItem("Netherlands"));
    ui->messages->setItem(row, MESSAGE_COL_TYPE_ID, new QTableWidgetItem("B"));
    ui->messages->setItem(row, MESSAGE_COL_TYPE, new QTableWidgetItem("Meteorological\nwarning"));
    ui->messages->setItem(row, MESSAGE_COL_MESSAGE_ID, new QTableWidgetItem("12"));
    ui->messages->setItem(row, MESSAGE_COL_MESSAGE, new QTableWidgetItem("ABCEDGHIJKLMNOPQRSTUVWXYZ\n123456789"));
    ui->messages->setItem(row, MESSAGE_COL_ERRORS, new QTableWidgetItem("100"));
    ui->messages->setItem(row, MESSAGE_COL_ERROR_PERCENT, new QTableWidgetItem("10.0%"));
    ui->messages->resizeColumnsToContents();
    ui->messages->removeRow(row);
}

// Columns in table reordered
void NavtexDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) oldVisualIndex;

    m_settings.m_columnIndexes[logicalIndex] = newVisualIndex;
}

// Column in table resized (when hidden size is 0)
void NavtexDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void NavtexDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void NavtexDemodGUI::columnSelectMenuChecked(bool checked)
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
QAction *NavtexDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

NavtexDemodGUI* NavtexDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    NavtexDemodGUI* gui = new NavtexDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void NavtexDemodGUI::destroy()
{
    delete this;
}

void NavtexDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray NavtexDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool NavtexDemodGUI::deserialize(const QByteArray& data)
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

qint64 NavtexDemodGUI::getFrequency()
{
    qint64 frequency;

    // m_deviceCenterFrequency may sometimes be 0 if using file source
    frequency = (m_deviceCenterFrequency ? m_deviceCenterFrequency : 518000) + m_settings.m_inputFrequencyOffset;

    return ((frequency + 500) / 1000) * 1000; // Round to nearest kHz
}

// Add row to table
void NavtexDemodGUI::messageReceived(const NavtexMessage& message, int errors, float rssi)
{
    // Is scroll bar at bottom
    QScrollBar *sb = ui->messages->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    ui->messages->setSortingEnabled(false);
    int row = ui->messages->rowCount();
    ui->messages->setRowCount(row + 1);

    QTableWidgetItem *dateItem = new QTableWidgetItem();
    QTableWidgetItem *timeItem = new QTableWidgetItem();
    QTableWidgetItem *stationIdItem = new QTableWidgetItem();
    QTableWidgetItem *stationItem = new QTableWidgetItem();
    QTableWidgetItem *typeIdItem = new QTableWidgetItem();
    QTableWidgetItem *typeItem = new QTableWidgetItem();
    QTableWidgetItem *messageIdItem = new QTableWidgetItem();
    QTableWidgetItem *messageItem = new QTableWidgetItem();
    QTableWidgetItem *errorsItem = new QTableWidgetItem();
    QTableWidgetItem *errorPercentItem = new QTableWidgetItem();
    QTableWidgetItem *rssiItem = new QTableWidgetItem();
    ui->messages->setItem(row, MESSAGE_COL_DATE, dateItem);
    ui->messages->setItem(row, MESSAGE_COL_TIME, timeItem);
    ui->messages->setItem(row, MESSAGE_COL_STATION_ID, stationIdItem);
    ui->messages->setItem(row, MESSAGE_COL_STATION, stationItem);
    ui->messages->setItem(row, MESSAGE_COL_TYPE_ID, typeIdItem);
    ui->messages->setItem(row, MESSAGE_COL_TYPE, typeItem);
    ui->messages->setItem(row, MESSAGE_COL_MESSAGE_ID, messageIdItem);
    ui->messages->setItem(row, MESSAGE_COL_MESSAGE, messageItem);
    ui->messages->setItem(row, MESSAGE_COL_ERRORS, errorsItem);
    ui->messages->setItem(row, MESSAGE_COL_ERROR_PERCENT, errorPercentItem);
    ui->messages->setItem(row, MESSAGE_COL_RSSI, rssiItem);

    dateItem->setData(Qt::DisplayRole, message.m_dateTime.date());
    timeItem->setData(Qt::DisplayRole, message.m_dateTime.time());
    if (message.m_valid)
    {
        QString station = message.getStation(m_settings.m_navArea, getFrequency());
        QString type = message.getType();
        stationIdItem->setText(message.m_stationId);
        stationItem->setText(station);
        typeIdItem->setText(message.m_typeId);
        typeItem->setText(type);
        messageIdItem->setText(message.m_id);
        // Add to filter comboboxes
        if (!station.isEmpty() && (ui->filterStation->findText(station) == -1)) {
            ui->filterStation->addItem(station);
        }
        if (!type.isEmpty() && (ui->filterType->findText(type) == -1)) {
            ui->filterType->addItem(type);
        }
        errorsItem->setData(Qt::DisplayRole, errors);
        float errorPC = 100.0f * errors / (message.m_message.size() * 2.0f); // SITOR-B sends each character twice
        errorPercentItem->setData(Qt::DisplayRole, errorPC);
        rssiItem->setData(Qt::DisplayRole, rssi);
    }
    messageItem->setText(message.m_message);
    filterRow(row);
    ui->messages->setSortingEnabled(true);
    ui->messages->resizeRowToContents(row);
    if (scrollToBottom) {
        ui->messages->scrollToBottom();
    }
}

bool NavtexDemodGUI::handleMessage(const Message& message)
{
    if (NavtexDemod::MsgConfigureNavtexDemod::match(message))
    {
        qDebug("NavtexDemodGUI::handleMessage: NavtexDemod::MsgConfigureNavtexDemod");
        const NavtexDemod::MsgConfigureNavtexDemod& cfg = (NavtexDemod::MsgConfigureNavtexDemod&) message;
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
    else if (NavtexDemod::MsgCharacter::match(message))
    {
        NavtexDemod::MsgCharacter& report = (NavtexDemod::MsgCharacter&) message;
        QString c = report.getCharacter();

        // Is the scroll bar at the bottom?
        int scrollPos = ui->text->verticalScrollBar()->value();
        bool atBottom = scrollPos >= ui->text->verticalScrollBar()->maximum();

        // Move cursor to end where we want to append new text
        // (user may have moved it by clicking / highlighting text)
        ui->text->moveCursor(QTextCursor::End);

        // Restore scroll position
        ui->text->verticalScrollBar()->setValue(scrollPos);

        if (c == '\b') {
            ui->text->textCursor().deletePreviousChar();
        } else {
            ui->text->insertPlainText(c);
        }

        // Scroll to bottom, if we we're previously at the bottom
        if (atBottom) {
            ui->text->verticalScrollBar()->setValue(ui->text->verticalScrollBar()->maximum());
        }

        return true;
    }
    else if (NavtexDemod::MsgMessage::match(message))
    {
        NavtexDemod::MsgMessage& textMsg = (NavtexDemod::MsgMessage&) message;
        messageReceived(textMsg.getMessage(), textMsg.getErrors(), textMsg.getRSSI());
        return true;
    }

    return false;
}

void NavtexDemodGUI::handleInputMessages()
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

void NavtexDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void NavtexDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void NavtexDemodGUI::updateTxStation()
{
    const NavtexTransmitter *transmitter = NavtexTransmitter::getTransmitter(QDateTime::currentDateTimeUtc().time(),
                                                                             m_settings.m_navArea,
                                                                             getFrequency());
    if (transmitter)
    {
        ui->txStation->setText(transmitter->m_station);
    }
    else
    {
        ui->txStation->setText("");
    }
}

void NavtexDemodGUI::on_findOnMapFeature_clicked()
{
    QString station = ui->txStation->text();
    if (!station.isEmpty()) {
        FeatureWebAPIUtils::mapFind(station);
    }
}

void NavtexDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void NavtexDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value;
    ui->rfBWText->setText(QString("%1 Hz").arg((int)bw));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings();
}

void NavtexDemodGUI::on_filterStation_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_filterStation = ui->filterStation->currentText();
    filter();
    applySettings();
}

void NavtexDemodGUI::on_filterType_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_filterType = ui->filterType->currentText();
    filter();
    applySettings();
}

void NavtexDemodGUI::on_clearTable_clicked()
{
    ui->messages->setRowCount(0);
    ui->text->clear();
}

void NavtexDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void NavtexDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void NavtexDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void NavtexDemodGUI::filterRow(int row)
{
    bool hidden = false;
    if ((m_settings.m_filterStation != "") && (m_settings.m_filterStation != "All"))
    {
        QTableWidgetItem *stationItem = ui->messages->item(row, MESSAGE_COL_STATION);
        if (m_settings.m_filterStation != stationItem->text()) {
            hidden = true;
        }
    }
    if ((m_settings.m_filterType != "") && (m_settings.m_filterType != "All"))
    {
        QTableWidgetItem *typeItem = ui->messages->item(row, MESSAGE_COL_TYPE);
        if (m_settings.m_filterType != typeItem->text()) {
            hidden = true;
        }
    }
    ui->messages->setRowHidden(row, hidden);
}

void NavtexDemodGUI::filter()
{
    for (int i = 0; i < ui->messages->rowCount(); i++) {
        filterRow(i);
    }
}

void NavtexDemodGUI::on_navArea_currentIndexChanged(int index)
{
    m_settings.m_navArea = index + 1;
    updateTxStation();
    applySettings();
}

void NavtexDemodGUI::on_channel1_currentIndexChanged(int index)
{
    m_settings.m_scopeCh1 = index;
    applySettings();
}

void NavtexDemodGUI::on_channel2_currentIndexChanged(int index)
{
    m_settings.m_scopeCh2 = index;
    applySettings();
}

void NavtexDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void NavtexDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_navtexDemod->getNumberOfDeviceStreams());
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

NavtexDemodGUI::NavtexDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::NavtexDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_tickCount(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodnavtex/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_navtexDemod = reinterpret_cast<NavtexDemod*>(rxChannel);
    m_navtexDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    ui->messages->setItemDelegateForColumn(MESSAGE_COL_ERROR_PERCENT, new DecimalDelegate(1));
    ui->messages->setItemDelegateForColumn(MESSAGE_COL_RSSI, new DecimalDelegate(1));

    m_scopeVis = m_navtexDemod->getScopeSink();
    m_scopeVis->setGLScope(ui->glScope);
    ui->glScope->connectTimer(MainCore::instance()->getMasterTimer());
    ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

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

    m_scopeVis->setLiveRate(NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE);
    m_scopeVis->configure(500, 1, 0, 0, true);   // not working!
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("Navtex Demodulator");
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
    menu = new QMenu(ui->messages);
    for (int i = 0; i < ui->messages->horizontalHeader()->count(); i++)
    {
        QString text = ui->messages->horizontalHeaderItem(i)->text();
        menu->addAction(createCheckableItem(text, i, true));
    }
    ui->messages->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(messages_sectionMoved(int, int, int)));
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(messages_sectionResized(int, int, int)));

    ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));
    TableTapAndHold *tableTapAndHold = new TableTapAndHold(ui->messages);
    connect(tableTapAndHold, &TableTapAndHold::tapAndHold, this, &NavtexDemodGUI::customContextMenuRequested);

    ui->scopeContainer->setVisible(false);

    displaySettings();
    makeUIConnections();
    applySettings(true);
}

void NavtexDemodGUI::customContextMenuRequested(QPoint pos)
{
    QTableWidgetItem *item =  ui->messages->itemAt(pos);
    if (item)
    {
        int row = item->row();
        QString station = ui->messages->item(row, MESSAGE_COL_STATION)->text();

        QMenu* tableContextMenu = new QMenu(ui->messages);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = item->text();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);

        if (!station.isEmpty())
        {
            tableContextMenu->addSeparator();
            QAction* findOnMapAction = new QAction(QString("Find %1 on map").arg(station), tableContextMenu);
            connect(findOnMapAction, &QAction::triggered, this, [station]()->void {
                FeatureWebAPIUtils::mapFind(station);
            });
            tableContextMenu->addAction(findOnMapAction);
        }

        tableContextMenu->popup(ui->messages->viewport()->mapToGlobal(pos));
    }
}

NavtexDemodGUI::~NavtexDemodGUI()
{
    delete ui;
}

void NavtexDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void NavtexDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        NavtexDemod::MsgConfigureNavtexDemod* message = NavtexDemod::MsgConfigureNavtexDemod::create( m_settings, force);
        m_navtexDemod->getInputMessageQueue()->push(message);
    }
}

void NavtexDemodGUI::displaySettings()
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

    ui->rfBWText->setText(QString("%1 Hz").arg((int)m_settings.m_rfBandwidth));
    ui->rfBW->setValue(m_settings.m_rfBandwidth);

    ui->navArea->setCurrentIndex(m_settings.m_navArea - 1);

    updateIndexLabel();

    ui->filterStation->setCurrentText(m_settings.m_filterStation);
    ui->filterType->setCurrentText(m_settings.m_filterType);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));

    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    // Order and size columns
    QHeaderView *header = ui->messages->horizontalHeader();
    for (int i = 0; i < NAVTEXDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->messages->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    filter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void NavtexDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void NavtexDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void NavtexDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_navtexDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);
    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }
    if (m_tickCount % (50*10) == 0) {
        updateTxStation();
    }

    m_tickCount++;
}

void NavtexDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void NavtexDemodGUI::on_logFilename_clicked()
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
void NavtexDemodGUI::on_logOpen_clicked()
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
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"Date", "Time", "SID", "TID", "MID", "Message"}, error);
                if (error.isEmpty())
                {
                    int dateCol = colIndexes.value("Date");
                    int timeCol = colIndexes.value("Time");
                    int sidCol = colIndexes.value("SID");
                    int tidCol = colIndexes.value("TID");
                    int midCol = colIndexes.value("MID");
                    int messageCol = colIndexes.value("Message");
                    int errorsCol = colIndexes.value("Errors");
                    int rssiCol = colIndexes.value("RSSI");
                    int maxCol = std::max({dateCol, timeCol, sidCol, tidCol, midCol, messageCol});

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
                            NavtexMessage message(dateTime,
                                                  cols[sidCol],
                                                  cols[tidCol],
                                                  cols[midCol],
                                                  cols[messageCol]);
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
                    QMessageBox::critical(this, "Navtex Demod", error);
                }
            }
            else
            {
                QMessageBox::critical(this, "Navtex Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void NavtexDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &NavtexDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &NavtexDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->navArea, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NavtexDemodGUI::on_navArea_currentIndexChanged);
    QObject::connect(ui->findOnMapFeature, &QPushButton::clicked, this, &NavtexDemodGUI::on_findOnMapFeature_clicked);
    QObject::connect(ui->filterStation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NavtexDemodGUI::on_filterStation_currentIndexChanged);
    QObject::connect(ui->filterType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NavtexDemodGUI::on_filterType_currentIndexChanged);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &NavtexDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &NavtexDemodGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &NavtexDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &NavtexDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &NavtexDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &NavtexDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &NavtexDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->channel1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NavtexDemodGUI::on_channel1_currentIndexChanged);
    QObject::connect(ui->channel2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NavtexDemodGUI::on_channel2_currentIndexChanged);
}

void NavtexDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
    updateTxStation();
}

