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
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include <QAction>
#include <QRegularExpression>
#include <QClipboard>
#include <QScrollBar>
#include <QDesktopServices>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>

#include "acarsdemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/glscopesettings.h"
#include "ui_acarsdemodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "util/units.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "gui/crightclickenabler.h"
#include "gui/udpsettingsdialog.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QHostInfo>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "maincore.h"

#include "SWGMapItem.h"

#include "acarsdemod.h"
#include "acarsdemodsink.h"
#include "util/acarstextformat.h"
#include "gui/fastheaderview.h"
#include "gui/fastitemdelegate.h"



// Decode text conventions, as acarsDecodeToColumn, acarsDecodeToPlain and
// acarsDecodeToHtml in util/acarstextformat.h apply them: the full decode is plain
// multi-line text with "Label: value" lines. The Message Decode COLUMN shows it
// flattened to one semicolon-separated line so more fits; the decode VIEW and Map
// popups show it multi-line, with the label before each colon bolded.


// ARINC 620-8 DATALINK GROUND SYSTEM STANDAR AND INTERFACE SPECIFICATION
// Section 5.0 Downlink message text formats and table C-2

// Indexed by AcarsRowEvent::m_frameType: 0 ACARS message, 1 VDL-2 link frame,
// 2 HFDL frame, 3 Aero signal unit
const char *AcarsDemodGUI::m_frameSeriesNames[AcarsDemodGUI::CHART_FRAME_SERIES] =
{
    "ACARS", "VDL-2 link", "HFDL", "Aero"
};

// Whether any of the per-protocol frame rate series is showing, which decides if the
// frames-per-second axis takes part in a zoom or pan
bool AcarsDemodGUI::anyFrameSeriesVisible() const
{
    for (int i = 0; i < CHART_FRAME_SERIES; i++)
    {
        if (m_frameRateSeries[i] && m_frameRateSeries[i]->isVisible()) {
            return true;
        }
    }
    return false;
}

void AcarsDemodGUI::resizeTable()
{
    // Size the columns from one row of dummy data, then take it away again. Trailing
    // hyphens leave room for the sort arrow.
    AcarsRowEvent e;
    e.m_received = QDateTime(QDate(2016, 4, 15), QTime(23, 59, 39));
    e.m_mode = "All";
    e.m_address = "1234567-";
    e.m_ack = "NAK";
    e.m_label = "H1/CF-";
    e.m_labelDecode = "Arrival information report";
    e.m_blockId = "1-";
    e.m_originator = "A";
    e.m_originatorDecode = "System Control";
    e.m_messageNumber = "88";
    e.m_blockSequence = "A";
    e.m_flight = "WW8888";
    e.m_text = "ABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZ";
    e.m_textDecode = "ABCEDGHIJKLMNOPQRSTUVWXYZABCEDGHIJKLMNOPQRSTUVWXYZ";
    e.m_atc = "CLIMB TO 3000";
    e.m_hasPosition = true;
    e.m_hasAltitude = true;
    e.m_latitude = -90.0;
    e.m_longitude = -180.0;
    e.m_altitudeFt = 40000.0;
    e.m_hex = "000000000000000000000000000000000000000000000000000000000000000";

    // Straight into the model: the filter would otherwise decide it is not worth showing
    // and there would be nothing to measure
    m_messageModel->addRow(e);
    ui->messages->resizeColumnsToContents();
    m_messageModel->clear();
}

// Columns in table reordered
void AcarsDemodGUI::messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
    (void) logicalIndex;
    (void) oldVisualIndex;
    (void) newVisualIndex;

    // Every column between the two positions moved too, so record them all. Recording
    // only the one dragged left the others stale, and replaying them in displaySettings()
    // gave a different order after more than one move.
    QHeaderView *header = ui->messages->horizontalHeader();
    for (int i = 0; i < ACARSDEMOD_COLUMNS; i++) {
        m_settings.m_columnIndexes[i] = header->visualIndex(i);
    }
}

// Column in table resized (when hidden size is 0)
void AcarsDemodGUI::messages_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    (void) oldSize;

    m_settings.m_columnSizes[logicalIndex] = newSize;
}

// Right click in table header - show column select menu
void AcarsDemodGUI::columnSelectMenu(QPoint pos)
{
    menu->popup(ui->messages->horizontalHeader()->viewport()->mapToGlobal(pos));
}

// Hide/show column when menu selected
void AcarsDemodGUI::columnSelectMenuChecked(bool checked)
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
QAction *AcarsDemodGUI::createCheckableItem(QString &text, int idx, bool checked)
{
    QAction *action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(checked);
    action->setData(QVariant(idx));
    connect(action, SIGNAL(triggered()), this, SLOT(columnSelectMenuChecked()));
    return action;
}

AcarsDemodGUI* AcarsDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    AcarsDemodGUI* gui = new AcarsDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void AcarsDemodGUI::destroy()
{
    delete this;
}

void AcarsDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray AcarsDemodGUI::serialize() const
{
    // QSplitter::saveState() is const, so the live position can be read here rather than
    // shadowed in a member and kept in step
    AcarsDemodSettings settings = m_settings;
    settings.m_splitterState = ui->splitter->saveState();
    return settings.serialize();
}

bool AcarsDemodGUI::deserialize(const QByteArray& data)
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

// Ground stations go straight to the Map as fixed antennas, from the ground stations
// table's context menu. Nothing else in this plugin draws on the Map: aircraft, and the
// routes a flight plan or an oceanic clearance names, are reported to the Aircraft
// feature, and that feature draws them.
void AcarsDemodGUI::sendGroundStationToMap(const QString& name, float latitude, float longitude, const QString& text, QDateTime dateTime)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_acarsDemod, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(latitude);
        swgMapItem->setLongitude(longitude);
        swgMapItem->setAltitude(0.0f);
        swgMapItem->setPositionDateTime(new QString(dateTime.toUTC().toString(Qt::ISODateWithMs)));
        swgMapItem->setFixedPosition(true);
        // The Map feature's antenna icon, as used for its own beacons
        swgMapItem->setImage(new QString(QString("qrc:///map/map/antenna.png")));
        swgMapItem->setText(new QString(text));
        swgMapItem->setLabel(new QString(name));
        swgMapItem->setOrientation(0);
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_acarsDemod, swgMapItem);
        messageQueue->push(msg);
    }
}

// Add a table row for a decoded frame. All decode happens in the channel's
// worker; this just displays the precomputed strings.
void AcarsDemodGUI::rowReady(const AcarsRowEvent& e)
{
    // The frame rate chart counts every frame, including hidden ones
    if ((e.m_frameType >= 0) && (e.m_frameType < CHART_FRAME_SERIES)) {
        m_frameRateCount[e.m_frameType]++;
    }
    if (!e.m_chartAircraftId.isEmpty()) {
        m_aircraftLastSeen.insert(e.m_chartAircraftId, e.m_received.toMSecsSinceEpoch());
    }

    // HFDL ground stations heard, for the ground stations dialog
    if (!e.m_gsHeardIds.isEmpty())
    {
        for (int i = 0; i < e.m_gsHeardIds.size(); i++)
        {
            GsHeard& gs = m_gsHeard[e.m_gsHeardIds[i]];
            for (int freq : e.m_gsHeardFreqs[i]) {
                gs.m_frequencies.insert(freq);
            }
            gs.m_lastHeard = e.m_received;
        }
        updateGsTable();
    }

    // Is scroll bar at bottom
    QScrollBar *sb = ui->messages->verticalScrollBar();
    bool scrollToBottom = sb->value() == sb->maximum();

    // Everything the table shows is derived from the frame in the model, so adding a row
    // is one append and no per-cell work at all. The proxy decides whether it is shown
    // and where it sorts to.
    m_messageModel->addRow(e);

    // The scroll is held back to the timer: it forces a synchronous relayout, and one
    // serves every message that arrived in the tick
    if (scrollToBottom)
    {
        m_scrollPending = true;
        if (!m_tableTimer->isActive()) {
            m_tableTimer->start(250);
        }
    }
}

// Scroll at most once a tick, however many messages arrived during it
void AcarsDemodGUI::updateMessagesTable()
{
    if (m_scrollPending)
    {
        m_scrollPending = false;
        ui->messages->scrollToBottom();
    }
}

// The model derives the text in data(), so changing the format is one call and one
// dataChanged rather than a walk over the table
void AcarsDemodGUI::setShowDate(bool showDate)
{
    if (showDate == m_settings.m_showDate) {
        return;
    }
    m_settings.m_showDate = showDate;
    m_messageModel->setShowDate(showDate);
    ui->messages->resizeColumnToContents(MESSAGE_COL_DATETIME);
    applySettings();
}

// Sorting is the proxy's, and it keeps itself sorted as rows arrive - a new message
// lands in its place rather than being appended and the table re-sorted. Measured flat
// at about 4 us a row over 22000 rows, where re-sorting a QTableWidget on every message
// reached 390 us and was still climbing.
int AcarsDemodGUI::sourceRow(int viewRow) const
{
    const QModelIndex index = m_messageFilter->mapToSource(m_messageFilter->index(viewRow, 0));
    return index.isValid() ? index.row() : -1;
}

void AcarsDemodGUI::assemblyUpdated(const AcarsAssemblyEvent& event)
{
    m_assemblies.insert(event.m_assemblyId, event);
}

void AcarsDemodGUI::messagesSelectionChanged()
{
    const QModelIndexList selected = ui->messages->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    const int row = sourceRow(selected.first().row());
    if (row < 0) {
        return;
    }
    const AcarsRowEvent& e = m_messageModel->event(row);
    QString decode;

    if (e.m_assemblyId && m_assemblies.contains(e.m_assemblyId)
        && m_assemblies.value(e.m_assemblyId).m_isMultipart)
    {
        // A part of a multipart message: show the assembly state and combined
        // message from the worker, then this row's own block
        const AcarsAssemblyEvent assembly = m_assemblies.value(e.m_assemblyId);
        const QString direction = assembly.m_uplink ? "Uplink" : "Downlink";
        decode = QString("<b>%1 multipart message - %2</b><br>")
            .arg(direction, assembly.m_statusText);
        decode.append(QString("Selected part: %1; received parts: %2")
            .arg(e.m_partNumber)
            .arg(assembly.m_uniquePartCount));

        if (assembly.m_duplicateCount > 0) {
            decode.append(QString("; retransmissions: %1").arg(assembly.m_duplicateCount));
        }
        if (!assembly.m_missingSequences.isEmpty())
        {
            decode.append(QString("<br>Missing downlink sequence: %1")
                .arg(assembly.m_missingSequences.join(", ")));
        }

        decode.append("<hr><b>Combined message</b><br>");
        decode.append(assembly.m_combinedHtmlBody);

        QString selectedText = e.m_text.toHtmlEscaped();
        selectedText.replace("\n", "<br>");
        decode.append("<hr><b>Selected block</b><br>");
        decode.append(selectedText);
    }
    else
    {
        // The decode view content is precomputed by the worker
        decode = e.m_viewDecodeHtml.isEmpty() ? e.m_text : e.m_viewDecodeHtml;
    }
    ui->decode->setText(decode);

    hideAircraftDetails();

    // The Address column has the registration, or for link frames possibly the
    // raw ICAO address in hex - either can fetch a photo. Ground station and
    // unidentified-aircraft rows have nothing to look up.
    if (m_settings.m_displayPhotos)
    {
        const QString address = e.m_address;
        static const QRegularExpression icaoHexRe("^[0-9A-Fa-f]{6}$");

        if (!address.isEmpty() && (address != "All") && (address != "Unidentified")
            && !address.startsWith("GS ") && !address.startsWith("AC "))
        {
            updatePhotoText(address);
            if (icaoHexRe.match(address).hasMatch()) {
                m_planeSpotters.getAircraftPhoto(address);
            } else {
                m_planeSpotters.getAircraftPhotoByRegistration(address);
            }
        }
    }
}

bool AcarsDemodGUI::handleMessage(const Message& message)
{
    if (AcarsDemod::MsgConfigureAcarsDemod::match(message))
    {
        qDebug("AcarsDemodGUI::handleMessage: AcarsDemod::MsgConfigureAcarsDemod");
        const AcarsDemod::MsgConfigureAcarsDemod& cfg = (AcarsDemod::MsgConfigureAcarsDemod&) message;
        // The table layout and the splitter are the GUI's own: changed here without being
        // applied, so the channel's copy of them is stale, and taking it moved columns,
        // widths and the splitter back whenever the channel echoed its settings (a REST
        // PATCH, a centre frequency change)
        AcarsDemodSettings settings = cfg.getSettings();
        std::copy(std::begin(m_settings.m_columnIndexes), std::end(m_settings.m_columnIndexes), std::begin(settings.m_columnIndexes));
        std::copy(std::begin(m_settings.m_columnSizes), std::end(m_settings.m_columnSizes), std::begin(settings.m_columnSizes));
        settings.m_splitterState = ui->splitter->saveState();
        m_settings = settings;
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
    return false;
}

void AcarsDemodGUI::handleInputMessages()
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

void AcarsDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void AcarsDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void AcarsDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void AcarsDemodGUI::on_mode_currentIndexChanged(int value)
{
    switch (value)
    {
    case 1: m_settings.m_mode = AcarsDemodSettings::VDL2; break;
    case 2: m_settings.m_mode = AcarsDemodSettings::HFDL; break;
    case 3: m_settings.m_mode = AcarsDemodSettings::Aero; break;
    case 4: m_settings.m_mode = AcarsDemodSettings::Iridium; break;
    default: m_settings.m_mode = AcarsDemodSettings::ACARS; break;
    }

    updateModeDependentWidgets();
    applyModeBandwidth();
}

void AcarsDemodGUI::on_iridiumBandwidth_currentIndexChanged(int value)
{
    m_settings.m_iridiumBandwidth = value;
    // The channel sample rate is the width scanned, so changing it restarts the channel
    applySettings();
}

void AcarsDemodGUI::on_aeroChannel_currentIndexChanged(int value)
{
    m_settings.m_aeroChannel = value;
    updateModeDependentWidgets();
    // Each Aero rate has its own occupied bandwidth, so changing rate should move the
    // filter with it
    applyModeBandwidth();
}

// A suitable default channel bandwidth for the mode, in hundreds of Hz: plain ACARS is
// 12.5 kHz channelised, VDL-2 is 25 kHz, an HFDL channel is 3 kHz of USB audio, and an
// Aero channel is about 1.5 times its bit rate (twice the symbol rate for the OQPSK
// rate). For HFDL the channel should be tuned to the published (carrier) frequency;
// the 1440 Hz subcarrier shift is applied inside the demodulator.
void AcarsDemodGUI::applyModeBandwidth()
{
    // Only in response to the user actually choosing a mode or rate. displaySettings()
    // populates the combos too, and setCurrentIndex() fires the same handlers - so
    // without this a preset's own bandwidth is overwritten by the default for its mode
    // the moment it is loaded, and only survives because displaySettings() happens to
    // restore rfBW a few lines further down.
    if (!m_doApplySettings) {
        return;
    }

    int bw;
    switch (m_settings.m_mode)
    {
    case AcarsDemodSettings::VDL2: bw = 250; break;
    case AcarsDemodSettings::HFDL: bw = 30; break;
    case AcarsDemodSettings::Aero:
        bw = AcarsAeroReceiver::submodeBandwidth(m_settings.m_aeroChannel) / 100;
        break;
    // Ignored: Iridium scans the whole tuned width, and the sink passes all of it
    // rather than filtering to a channel. The control is hidden in this mode
    case AcarsDemodSettings::Iridium: bw = ui->rfBW->maximum(); break;
    default: bw = 125; break;
    }
    if (ui->rfBW->value() != bw) {
        ui->rfBW->setValue(bw); // Calls on_rfBW_valueChanged, which calls applySettings
    } else {
        applySettings();
    }
}

void AcarsDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value * 100.0f;
    ui->rfBWText->setText(QString("%1k").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_rfBandwidth = bw;
    updateChannelMarker();
    applySettings();
}


void AcarsDemodGUI::on_threshold_valueChanged(int value)
{
    // The dial is in hundredths: the setting is the depth of 2400 Hz modulation the pre-key
    // detector demands, which is a ratio in 0 to 1 and not a raw correlation any more
    ui->thresholdText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_correlationThreshold = value / 100.0f;
    applySettings();
}



void AcarsDemodGUI::on_filter_currentIndexChanged(int index)
{
    m_settings.m_filterColumn = index;
    applyFilter();
    applySettings();
}

void AcarsDemodGUI::on_filterPattern_editingFinished()
{
    m_settings.m_filter = ui->filterPattern->text();
    applyFilter();
    applySettings();
}

// Everything that describes the selected message's aircraft. Called when the selection
// moves to another message, and when the table is emptied - the photograph of an
// aircraft whose messages have just been deleted is describing nothing.
void AcarsDemodGUI::hideAircraftDetails()
{
    ui->photoHeader->setVisible(false);
    ui->photoFlag->setVisible(false);
    ui->photo->setVisible(false);
    ui->flightDetails->setVisible(false);
    ui->aircraftDetails->setVisible(false);
}

void AcarsDemodGUI::on_clearTable_clicked()
{
    m_messageModel->clear();
    ui->decode->setText("");
    hideAircraftDetails();
    m_assemblies.clear();
}

void AcarsDemodGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

// Where the packets go is only interesting while it is being changed, so it hangs off
// the enable button's right click rather than taking a row of the panel - the same place
// the feed settings live. The button's tooltip carries the destination so it can still be
// read at a glance.
QString AcarsDemodGUI::udpToolTip() const
{
    return QString("Forward received packets over UDP to %1:%2. Right click to change")
            .arg(m_settings.m_udpAddress).arg(m_settings.m_udpPort);
}

void AcarsDemodGUI::udpSettings(const QPoint& p)
{
    UDPSettingsDialog dialog(m_settings.m_udpAddress, (uint16_t) m_settings.m_udpPort, this);
    dialog.move(p);
    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_udpAddress = dialog.address();
        m_settings.m_udpPort = dialog.port();
        ui->udpEnabled->setToolTip(udpToolTip());
        applySettings();
    }
}

// Both filters are the proxy's now. A row that fails them is not in the proxy's mapping
// at all, where hiding it left a header section behind that cost O(rows already there)
// to manage - which was the table's worst scaling problem.
// Put the table and the decode view back where they were. Done after the settings have
// arrived rather than in the constructor, which runs before deserialize().
void AcarsDemodGUI::restoreSplitter()
{
    if (!m_settings.m_splitterState.isEmpty()) {
        ui->splitter->restoreState(m_settings.m_splitterState);
    }
}

void AcarsDemodGUI::applyFilter()
{
    // Table columns corresponding to the filter combo entries
    static const int filterColumns[] = {
        MESSAGE_COL_ADDRESS, MESSAGE_COL_FLIGHT, MESSAGE_COL_LABEL, MESSAGE_COL_TEXT
    };
    int column = MESSAGE_COL_ADDRESS;
    if ((m_settings.m_filterColumn >= 0)
        && (m_settings.m_filterColumn < (int) (sizeof(filterColumns) / sizeof(filterColumns[0])))) {
        column = filterColumns[m_settings.m_filterColumn];
    }

    m_messageFilter->setHideNoInfo(m_settings.m_hideNoInfo);
    m_messageFilter->setFilter(column, m_settings.m_filter);
}


void AcarsDemodGUI::on_channel1_currentIndexChanged(int index)
{
    m_settings.m_scopeCh1 = index;
    applySettings();
}

void AcarsDemodGUI::on_channel2_currentIndexChanged(int index)
{
    m_settings.m_scopeCh2 = index;
    applySettings();
}

void AcarsDemodGUI::customContextMenuRequested(QPoint pos)
{
    const QModelIndex index = ui->messages->indexAt(pos);
    const int row = index.isValid() ? sourceRow(index.row()) : -1;
    if (row >= 0)
    {
        const AcarsRowEvent& e = m_messageModel->event(row);
        QString registration = e.m_address;
        QString flight = e.m_flight;
        // ACARS Flight will be of the form AA0123 - strip the 0 for use on websites.
        // Length checked: a VDL-2 link frame, an Aero signal unit or an HFDL squitter
        // carries no flight number at all, and QString::operator[] past the end is an
        // out of bounds read once Q_ASSERT is compiled out of a release build.
        if ((flight.size() > 2) && (flight[2] == '0')) {
            flight = flight.left(2) + flight.mid(3);
        }

        QMenu* tableContextMenu = new QMenu(ui->messages);
        connect(tableContextMenu, &QMenu::aboutToHide, tableContextMenu, &QMenu::deleteLater);

        // Copy current cell

        QAction* copyAction = new QAction("Copy", tableContextMenu);
        const QString text = index.data(Qt::DisplayRole).toString();
        connect(copyAction, &QAction::triggered, this, [text]()->void {
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(text);
        });
        tableContextMenu->addAction(copyAction);

        tableContextMenu->addSeparator();

        // How much of the Date/Time column to show. The date is only worth its width
        // once a session has run past midnight, but the column always sorts by the
        // full date and time whichever is displayed
        QActionGroup* dateTimeGroup = new QActionGroup(tableContextMenu);

        QAction* dateAndTimeAction = new QAction("Show date and time", tableContextMenu);
        dateAndTimeAction->setCheckable(true);
        dateAndTimeAction->setChecked(m_settings.m_showDate);
        dateAndTimeAction->setActionGroup(dateTimeGroup);
        connect(dateAndTimeAction, &QAction::triggered, this, [this]()->void {
            setShowDate(true);
        });
        tableContextMenu->addAction(dateAndTimeAction);

        QAction* timeOnlyAction = new QAction("Show time only", tableContextMenu);
        timeOnlyAction->setCheckable(true);
        timeOnlyAction->setChecked(!m_settings.m_showDate);
        timeOnlyAction->setActionGroup(dateTimeGroup);
        connect(timeOnlyAction, &QAction::triggered, this, [this]()->void {
            setShowDate(false);
        });
        tableContextMenu->addAction(timeOnlyAction);

        tableContextMenu->addSeparator();

        // Right clicking either ground station column offers plotting that station on
        // the Map. Only HFDL and VDL-2 stations are at a known place: an ACARS Mode
        // character names no station at all, and an Aero GES id resolves only to a
        // satellite and an ocean region.
        if (e.m_gsHasPosition
            && ((index.column() == MESSAGE_COL_GS) || (index.column() == MESSAGE_COL_GS_DECODE)))
        {
            // HFDL stations are numbered, and the ground stations table (13) already
            // draws them as "GS <id>". Using the same name means the same station is
            // not drawn twice under two names - the Map keys its items on the name.
            bool numeric = false;
            e.m_mode.toInt(&numeric);
            const QString gsName = QString("GS %1").arg(
                (numeric || e.m_modeDecode.isEmpty()) ? e.m_mode : e.m_modeDecode);
            const QString gsText = QString("%1 ground station%2").arg(e.m_protocol).arg(
                e.m_modeDecode.isEmpty() ? QString() : QString(" - %1").arg(e.m_modeDecode));
            const float gsLatitude = e.m_gsLatitude;
            const float gsLongitude = e.m_gsLongitude;

            QAction* findGsAction = new QAction(QString("Find %1 on map").arg(gsName), tableContextMenu);
            connect(findGsAction, &QAction::triggered, this,
                [this, gsName, gsText, gsLatitude, gsLongitude]()->void
            {
                sendGroundStationToMap(gsName, gsLatitude, gsLongitude, gsText,
                                       QDateTime::currentDateTime());
                // The item reaches the Map over a message queue, so give it a moment to
                // be ingested before centring on it - finding it straight away races
                QTimer::singleShot(250, this, [gsName]() {
                    FeatureWebAPIUtils::mapFind(gsName);
                });
            });
            tableContextMenu->addAction(findGsAction);

            tableContextMenu->addSeparator();
        }

        if (!registration.isEmpty())
        {
            // View aircraft on various websites

            QAction* planeSpottersAction = new QAction("View aircraft on planespotters.net...", tableContextMenu);
            connect(planeSpottersAction, &QAction::triggered, this, [registration]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.planespotters.net/search?q=%1").arg(registration)));
            });
            tableContextMenu->addAction(planeSpottersAction);

            QAction* flightRadarAction = new QAction("View aircraft on flightradar24.net...", tableContextMenu);
            connect(flightRadarAction, &QAction::triggered, this, [registration]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/data/aircraft/%1").arg(registration)));
            });
            tableContextMenu->addAction(flightRadarAction);

            tableContextMenu->addSeparator();
        }

        if (!flight.isEmpty())
        {
            // View flight on various websites

            QAction* flightRadarFlightAction = new QAction("View flight on flightradar24.net...", tableContextMenu);
            connect(flightRadarFlightAction, &QAction::triggered, this, [flight]()->void {
                QDesktopServices::openUrl(QUrl(QString("https://www.flightradar24.com/%1").arg(flight)));
            });
            tableContextMenu->addAction(flightRadarFlightAction);

            tableContextMenu->addSeparator();
        }

        // Find on Map
        QAction* findMapFeatureAction = new QAction("Find on map", tableContextMenu);
        connect(findMapFeatureAction, &QAction::triggered, this, [registration]()->void {
            FeatureWebAPIUtils::mapFind(registration);
        });
        tableContextMenu->addAction(findMapFeatureAction);

        tableContextMenu->popup(ui->messages->viewport()->mapToGlobal(pos));
    }
}

void AcarsDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void AcarsDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_acarsDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

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

// Every demodulator's tick() hangs off the one master timer, so work done on a fixed tick
// count - the chart update once a second, the averaging once a minute - landed in the same
// timer event for all of them, and with 25 demodulators that added up to a 40-65 ms GUI
// stall once a second. Starting each count somewhere different puts each demodulator's
// share of that work in a different timer event
static uint32_t staggeredTickStart()
{
    static uint32_t next = 0;   // Only ever used on the GUI thread
    return next++;
}

AcarsDemodGUI::AcarsDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::AcarsDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_doApplySettings(true),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_tickCount(staggeredTickStart()),
    m_progressDialog(nullptr),
    m_tableTimer(nullptr),
    m_scrollPending(false),
    m_chart(nullptr),
    m_aircraftSeries(nullptr),
    m_xAxis(nullptr),
    m_fpsYAxis(nullptr),
    m_aircraftYAxis(nullptr),

    m_frameRateTime(QDateTime::currentDateTimeUtc()),
    m_chartPanning(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodacars/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);

    ui->messages->setItemDelegate(new FastItemDelegate(ui->messages));
    ui->channelPower->setFixedSize(
        ui->channelPower->fontMetrics().horizontalAdvance("-100.0") + 2,
        ui->channelPower->sizeHint().height());
    ui->channelPower->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->quality->setFixedSize(
        ui->quality->fontMetrics().horizontalAdvance("100.0%  SU 100%") + 2,
        ui->quality->sizeHint().height());

    // Before any header configuration below: setHorizontalHeader() resets column state
    ui->messages->setHorizontalHeader(new FastHeaderView(Qt::Horizontal, ui->messages));

    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_acarsDemod = reinterpret_cast<AcarsDemod*>(rxChannel);
    m_acarsDemod->setMessageQueueToGUI(getInputMessageQueue());


    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    m_scopeVis = m_acarsDemod->getScopeSink();
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

    // Corrected per mode in updateModeDependentWidgets once the settings are known
    m_scopeVis->setLiveRate(ACARSDEMOD_CHANNEL_SAMPLE_RATE);
    //m_scopeVis->setFreeRun(false); // FIXME: add method rather than call m_scopeVis->configure()

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    updateChannelMarker();
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("ACARS Demodulator");
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

    // The messages are a model behind a filtering proxy, not a QTableWidget - see the
    // note on AcarsMessageModel for the measurements that forced that.
    m_messageModel = new AcarsMessageModel(this);
    m_messageFilter = new AcarsMessageFilter(this);
    m_messageFilter->setSourceModel(m_messageModel);
    ui->messages->setModel(m_messageFilter);
    ui->messages->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->messages->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->messages->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Row numbers, so the count of messages can be read off the bottom of the table.
    // Cheap now, but do NOT give this header ResizeToContents: recalculating every
    // section on each insert is the trap that used to make adding a row O(rows).
    ui->messages->verticalHeader()->setVisible(true);

    // Resize the table using dummy data
    resizeTable();
    // Allow user to reorder columns
    ui->messages->horizontalHeader()->setSectionsMovable(true);
    // Safe to leave on now: the proxy keeps itself sorted as rows arrive, so a new
    // message goes into its place rather than the whole table being re-sorted
    ui->messages->setSortingEnabled(true);
    ui->messages->sortByColumn(MESSAGE_COL_DATETIME, Qt::AscendingOrder);

    m_tableTimer = new QTimer(this);
    m_tableTimer->setSingleShot(true);
    connect(m_tableTimer, &QTimer::timeout, this, &AcarsDemodGUI::updateMessagesTable);
    // Add context menu to allow hiding/showing of columns
    menu = new QMenu(ui->messages);
    for (int i = 0; i < ui->messages->horizontalHeader()->count(); i++)
    {
        QString text = m_messageModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        menu->addAction(createCheckableItem(text, i, true));
    }
    ui->messages->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages->horizontalHeader(), SIGNAL(customContextMenuRequested(QPoint)), SLOT(columnSelectMenu(QPoint)));
    // Get signals when columns change
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionMoved(int, int, int)), SLOT(messages_sectionMoved(int, int, int)));
    connect(ui->messages->horizontalHeader(), SIGNAL(sectionResized(int, int, int)), SLOT(messages_sectionResized(int, int, int)));
    ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->messages, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customContextMenuRequested(QPoint)));

    ui->scopeContainer->setVisible(false);

    // Airport idents in the decode get their names as tooltips from AcarsMessageView itself

    AircraftInformation::init();
    connect(&m_osnDB, &OsnDB::downloadingURL, this, &AcarsDemodGUI::downloadingURL);
    connect(&m_osnDB, &OsnDB::downloadError, this, &AcarsDemodGUI::downloadError);
    connect(&m_osnDB, &OsnDB::downloadProgress, this, &AcarsDemodGUI::downloadProgress);
    connect(&m_osnDB, &OsnDB::downloadAircraftInformationFinished, this, &AcarsDemodGUI::downloadAircraftInformationFinished);
    m_aircraftInfo = OsnDB::getAircraftInformationByReg();

    connect(&m_planeSpotters, &PlaneSpotters::aircraftPhoto, this, &AcarsDemodGUI::aircraftPhoto);
    connect(ui->photo, &ClickableLabel::clicked, this, &AcarsDemodGUI::photoClicked);

    // Hide photo
    ui->photoHeader->setVisible(false);
    ui->photoFlag->setVisible(false);
    ui->photo->setVisible(false);
    ui->flightDetails->setVisible(false);
    ui->aircraftDetails->setVisible(false);

    // BEFORE plotChart(), which creates the series and stores them here. This loop used
    // to run afterwards and threw those pointers away, so the frame rate chart drew its
    // axes and legend and then never plotted a point - the append is guarded by a null
    // check, so it failed silently rather than crashing.
    for (int i = 0; i < CHART_FRAME_SERIES; i++)
    {
        m_frameRateSeries[i] = nullptr;
        m_frameRateCount[i] = 0;
    }

    plotChart();
    // The splitter saves a hidden pane as size 0, and restoreState() collapses a
    // collapsible pane restored at 0, so the chart button showed a chart with no height.
    // Not collapsible, a 0 is raised to the chart's minimum height instead.
    ui->splitter->setCollapsible(ui->splitter->indexOf(ui->chart), false);
    CRightClickEnabler *displayChartClickEnabler = new CRightClickEnabler(ui->displayChart);
    connect(displayChartClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(clearChart(const QPoint &)));

    displaySettings();

    makeUIConnections();
    applySettings(true);
    m_resizer.enableChildMouseTracking();

    // Display events from the channel's worker, which does all the decode on
    // its own thread
    AcarsDemodWorker *worker = m_acarsDemod->getWorker();
    connect(worker, &AcarsDemodWorker::rowReady, this, &AcarsDemodGUI::rowReady);
    connect(worker, &AcarsDemodWorker::assemblyUpdated, this, &AcarsDemodGUI::assemblyUpdated);
}

AcarsDemodGUI::~AcarsDemodGUI()
{
    delete ui;
}

void AcarsDemodGUI::on_getOSNDB_clicked()
{
    // Don't try to download while already in progress
    if (m_progressDialog == nullptr)
    {
        m_progressDialog = new QProgressDialog(this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_osnDB.downloadAircraftInformation();
    }
}

void AcarsDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AcarsDemodGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        AcarsDemod::MsgConfigureAcarsDemod* message = AcarsDemod::MsgConfigureAcarsDemod::create( m_settings, force);
        m_acarsDemod->getInputMessageQueue()->push(message);
    }
}

// Ground stations heard in HFDL squitters, one row per station and announced
// frequency, with a context menu to find the station on the Map or tune to it
void AcarsDemodGUI::on_gsTable_clicked()
{
    if (!m_gsDialog)
    {
        m_gsDialog = new QDialog(this);
        m_gsDialog->setWindowTitle("HFDL Ground Stations");
        m_gsDialog->resize(420, 300);

        m_gsTableWidget = new QTableWidget(m_gsDialog);
        m_gsTableWidget->setColumnCount(4);
        m_gsTableWidget->setHorizontalHeaderLabels({"GS", "Name", "Frequency (kHz)", "Last heard"});
        m_gsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_gsTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_gsTableWidget->verticalHeader()->setVisible(false);
        m_gsTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_gsTableWidget, &QTableWidget::customContextMenuRequested,
                this, &AcarsDemodGUI::gsTableContextMenu);

        QVBoxLayout *layout = new QVBoxLayout(m_gsDialog);
        layout->addWidget(m_gsTableWidget);
    }
    updateGsTable();
    m_gsDialog->show();
    m_gsDialog->raise();
    m_gsDialog->activateWindow();
}

void AcarsDemodGUI::updateGsTable()
{
    if (!m_gsTableWidget) {
        return;
    }
    m_gsTableWidget->setSortingEnabled(false);
    m_gsTableWidget->setRowCount(0);

    QList<int> ids = m_gsHeard.keys();
    std::sort(ids.begin(), ids.end());
    for (int id : ids)
    {
        const GsHeard& heard = m_gsHeard[id];
        QList<int> freqs = heard.m_frequencies.values();
        std::sort(freqs.begin(), freqs.end());
        const char *name = AcarsHfdlReceiver::gsName(id);

        for (int freq : freqs)
        {
            int row = m_gsTableWidget->rowCount();
            m_gsTableWidget->setRowCount(row + 1);
            QTableWidgetItem *idItem = new QTableWidgetItem();
            idItem->setData(Qt::DisplayRole, id);
            m_gsTableWidget->setItem(row, 0, idItem);
            m_gsTableWidget->setItem(row, 1, new QTableWidgetItem(name ? name : ""));
            QTableWidgetItem *freqItem = new QTableWidgetItem();
            freqItem->setData(Qt::DisplayRole, freq);
            m_gsTableWidget->setItem(row, 2, freqItem);
            m_gsTableWidget->setItem(row, 3, new QTableWidgetItem(heard.m_lastHeard.time().toString()));
        }
    }
    m_gsTableWidget->resizeColumnsToContents();
    m_gsTableWidget->setSortingEnabled(true);
}

void AcarsDemodGUI::gsTableContextMenu(QPoint pos)
{
    QTableWidgetItem *item = m_gsTableWidget->itemAt(pos);
    if (!item) {
        return;
    }
    int row = item->row();
    int gsId = m_gsTableWidget->item(row, 0)->data(Qt::DisplayRole).toInt();
    int freqKhz = m_gsTableWidget->item(row, 2)->data(Qt::DisplayRole).toInt();
    QString gsLabel = QString("GS %1").arg(gsId);

    QMenu *menu = new QMenu(m_gsTableWidget);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QAction *findAction = new QAction(QString("Find %1 on map").arg(gsLabel), menu);
    connect(findAction, &QAction::triggered, this, [this, gsId, gsLabel]()
    {
        double latitude, longitude;
        if (AcarsHfdlReceiver::gsPosition(gsId, latitude, longitude))
        {
            // Make sure the station exists on the map before centring on it. The map
            // item arrives via a message queue, so give the Map a moment to ingest it
            // before the find - calling immediately races and finds nothing.
            const char *name = AcarsHfdlReceiver::gsName(gsId);
            sendGroundStationToMap(gsLabel, (float) latitude, (float) longitude,
                QString("HFDL ground station%1").arg(name ? QString(" - %1").arg(name) : ""),
                QDateTime::currentDateTime());
            QTimer::singleShot(250, this, [gsLabel]() {
                FeatureWebAPIUtils::mapFind(gsLabel);
            });
        }
    });
    menu->addAction(findAction);

    QAction *tuneAction = new QAction(QString("Tune to %1 kHz").arg(freqKhz), menu);
    connect(tuneAction, &QAction::triggered, this, [this, freqKhz]()
    {
        // Centre the device so this channel receives the published frequency at its
        // current offset
        ChannelWebAPIUtils::setCenterFrequency(m_acarsDemod->getDeviceSetIndex(),
            (qint64) freqKhz * 1000 - m_settings.m_inputFrequencyOffset);
    });
    menu->addAction(tuneAction);

    menu->popup(m_gsTableWidget->viewport()->mapToGlobal(pos));
}

// The pre-key detection threshold only drives the plain ACARS (MSK on AM) receiver;
// VDL-2 and HFDL acquire by preamble correlation with their own fixed thresholds, so
// the dial is greyed out in those modes
void AcarsDemodGUI::updateModeDependentWidgets()
{
    bool acars = m_settings.m_mode == AcarsDemodSettings::ACARS;
    bool aero = m_settings.m_mode == AcarsDemodSettings::Aero;
    ui->threshold->setEnabled(acars);
    ui->thresholdLabel->setEnabled(acars);
    ui->thresholdText->setEnabled(acars);
    ui->threshold->setToolTip(acars
        ? "Pre-key detection threshold, as depth of 2400 Hz modulation (hundredths)"
        : "Pre-key detection threshold (only used in ACARS mode)");

    // The rate and channel selector only means anything in Aero mode, and so does the
    // link quality readout - it needs a continuous carrier to average over
    ui->aeroChannel->setVisible(aero);
    // Iridium scans the whole tuned width rather than one channel
    ui->iridiumBandwidth->setVisible(m_settings.m_mode == AcarsDemodSettings::Iridium);
    const bool iridium = (m_settings.m_mode == AcarsDemodSettings::Iridium);
    ui->rfBW->setVisible(!iridium);
    ui->rfBWLabel->setVisible(!iridium);
    ui->rfBWText->setVisible(!iridium);
    ui->qualityLabel->setVisible(aero);
    ui->quality->setVisible(aero);
    if (!aero) {
        ui->quality->clear();
    }

    // The scope's time axis is calibrated in the demodulator's own sample rate, which
    // differs per mode - and, in Aero, per rate. It used to be hardcoded to the plain
    // ACARS 48 kHz, so the axis was already wrong for VDL-2 and HFDL.
    if (m_scopeVis) {
        m_scopeVis->setLiveRate(AcarsDemodSink::channelSampleRate(m_settings));
    }

    updateChannelMarker();
}

// The published HFDL channel frequency is the suppressed SSB carrier, with the whole
// emission above it on the 1440 Hz subcarrier, so the marker stays anchored on the
// dialled (published) frequency but shades the upper sideband only. The other modes
// are carrier centred. Note the renderer draws bandwidth/2 above the marker for usb,
// hence the doubling - the same convention as the SSB demodulator.
void AcarsDemodGUI::updateChannelMarker()
{
    if (m_settings.m_mode == AcarsDemodSettings::HFDL)
    {
        m_channelMarker.setSidebands(ChannelMarker::usb);
        m_channelMarker.setLowCutoff(0);
        m_channelMarker.setBandwidth((int) m_settings.m_rfBandwidth * 2);
    }
    else
    {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
        m_channelMarker.setBandwidth((int) m_settings.m_rfBandwidth);
    }
}

void AcarsDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    updateChannelMarker();
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->mode->setCurrentIndex(m_settings.m_mode);
    ui->aeroChannel->setCurrentIndex(m_settings.m_aeroChannel);
    ui->iridiumBandwidth->setCurrentIndex(m_settings.m_iridiumBandwidth);
    updateModeDependentWidgets();

    ui->rfBWText->setText(QString("%1k").arg(m_settings.m_rfBandwidth / 1000.0, 0, 'f', 1));
    ui->rfBW->setValue(m_settings.m_rfBandwidth / 100.0);


    ui->thresholdText->setText(QString("%1").arg(m_settings.m_correlationThreshold, 0, 'f', 2));
    ui->threshold->setValue((int) (m_settings.m_correlationThreshold * 100.0f));

    updateIndexLabel();


    ui->filter->setCurrentIndex(m_settings.m_filterColumn);
    ui->filterPattern->setText(m_settings.m_filter);

    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpEnabled->setToolTip(udpToolTip());


    ui->channel1->setCurrentIndex(m_settings.m_scopeCh1);
    ui->channel2->setCurrentIndex(m_settings.m_scopeCh2);

    ui->logFilename->setToolTip(QString(".csv log filename: %1").arg(m_settings.m_logFilename));
    ui->logEnable->setChecked(m_settings.m_logEnabled);

    ui->noInfo->setChecked(m_settings.m_hideNoInfo);
    ui->displayChart->setChecked(m_settings.m_displayChart);
    ui->chart->setVisible(m_settings.m_displayChart);

    // Order and size columns
    QHeaderView *header = ui->messages->horizontalHeader();
    for (int i = 0; i < ACARSDEMOD_COLUMNS; i++)
    {
        bool hidden = m_settings.m_columnSizes[i] == 0;
        header->setSectionHidden(i, hidden);
        menu->actions().at(i)->setChecked(!hidden);
        if (m_settings.m_columnSizes[i] > 0)
            ui->messages->setColumnWidth(i, m_settings.m_columnSizes[i]);
        header->moveSection(header->visualIndex(i), m_settings.m_columnIndexes[i]);
    }

    // Both are display options the model applies to every row, so they have to be
    // pushed in once the saved settings have arrived
    m_messageModel->setShowDate(m_settings.m_showDate);
    restoreSplitter();
    applyFilter();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void AcarsDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void AcarsDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void AcarsDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_acarsDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    // Only update labels that can be seen, to avoid unncessary compositing
    if (m_tickCount % 4 == 0) 
    {
        if (!ui->channelPower->visibleRegion().isEmpty()) {
            ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
        }

        // Aero is the one protocol with a continuous carrier to measure, so it is the only
        // one where a live quality figure means anything. A burst mode would be showing
        // whatever the last burst happened to look like.
        if ((m_settings.m_mode == AcarsDemodSettings::Aero) && !ui->quality->visibleRegion().isEmpty())
        {
            double evm, suRate;
            m_acarsDemod->getAeroQuality(evm, suRate);
            QStringList parts;
            if (evm >= 0.0) {
                parts.append(QString("%1%").arg(evm * 100.0, 0, 'f', 1));
            }
            if (suRate >= 0.0) {
                parts.append(QString("SU %1%").arg(suRate * 100.0, 0, 'f', 0));
            }
            ui->quality->setText(parts.join("  "));
        }
    }

    // Plot RX frame rate every second (master timer ticks every 50 ms)
    const int ticksPerSecond = 20;
    if ((m_tickCount % ticksPerSecond) == 0)
    {
        // UTC, as m_frameRateTime is: a local QDateTime works its offset out again for
        // every difference and conversion below, a time zone lookup under a lock Qt shares
        // between all threads - about half of what this cost, in every demodulator
        QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
        qint64 ms = m_frameRateTime.msecsTo(currentDateTime);

        if (ms > 0)
        {
            double s = ms / 1000.0;
            double maxRate = 0.0;
            bool xAtMax = true;

            // Every series gets a point every tick, whether or not that protocol is the one
            // being received, so they all share an x axis and any of them can be taken as
            // the reference for the axis extent below.
            for (int i = 0; i < CHART_FRAME_SERIES; i++)
            {
                if (!m_frameRateSeries[i]) {
                    continue;
                }
                double rate = m_frameRateCount[i] / s;
                if ((i == 0) && (m_frameRateSeries[i]->count() > 0)) {
                    xAtMax = m_frameRateSeries[i]->at(m_frameRateSeries[i]->count() - 1).x()
                           == m_xAxis->max().toMSecsSinceEpoch();
                }
                m_frameRateSeries[i]->append(currentDateTime.toMSecsSinceEpoch(), rate);
                maxRate = std::max(maxRate, rate);
            }

            if (m_xAxis && xAtMax) {
                m_xAxis->setMax(currentDateTime);
            }
            if (m_fpsYAxis && (m_fpsYAxis->max() < maxRate)) {
                m_fpsYAxis->setMax(std::ceil(maxRate * 1.25));
            }

            m_frameRateTime = currentDateTime;
            for (int i = 0; i < CHART_FRAME_SERIES; i++) {
                m_frameRateCount[i] = 0;
            }
        }

        if (m_aircraftSeries)
        {
            // Aircraft heard from in the last 5 minutes. ADS-B uses a 10 second window,
            // but ACARS and VDL-2 aircraft only transmit every so often.
            const qint64 activeSeconds = 5 * 60;
            const qint64 nowMs = currentDateTime.toMSecsSinceEpoch();
            QMutableHashIterator<QString, qint64> itr(m_aircraftLastSeen);
            while (itr.hasNext())
            {
                itr.next();
                if ((nowMs - itr.value()) / 1000 > activeSeconds) {
                    itr.remove();
                }
            }
            int active = m_aircraftLastSeen.size();

            m_aircraftSeries->append(currentDateTime.toMSecsSinceEpoch(), active);

            if (m_aircraftYAxis->max() < active + 1) {
                m_aircraftYAxis->setMax(active + 1);
            }
        }

        // Average data 10 minutes old over 1 minute, so long sessions don't accumulate
        // too many points
        const int ageMins = 10;
        const int averagePeriodMins = 1;
        if ((m_tickCount % (ticksPerSecond*60*averagePeriodMins)) == 0)
        {
            QDateTime endTime, startTime;

            if (m_averageTime.isValid())
            {
                startTime = m_averageTime;
                endTime = startTime.addSecs(averagePeriodMins*60);
            }
            else
            {
                endTime = QDateTime::currentDateTimeUtc().addSecs(-ageMins*60);
                startTime = endTime.addSecs(-averagePeriodMins*60);
            }

            for (int i = 0; i < CHART_FRAME_SERIES; i++)
            {
                if (m_frameRateSeries[i]) {
                    averageSeries(m_frameRateSeries[i], startTime, endTime);
                }
            }
            if (m_aircraftSeries) {
                averageSeries(m_aircraftSeries, startTime, endTime);
            }

            m_averageTime = endTime;
        }
    }

    m_tickCount++;
}

void AcarsDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &AcarsDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AcarsDemodGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->aeroChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AcarsDemodGUI::on_aeroChannel_currentIndexChanged);
    QObject::connect(ui->iridiumBandwidth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AcarsDemodGUI::on_iridiumBandwidth_currentIndexChanged);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &AcarsDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->threshold, &QDial::valueChanged, this, &AcarsDemodGUI::on_threshold_valueChanged);
    QObject::connect(ui->filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AcarsDemodGUI::on_filter_currentIndexChanged);
    QObject::connect(ui->filterPattern, &QLineEdit::editingFinished, this, &AcarsDemodGUI::on_filterPattern_editingFinished);
    QObject::connect(ui->clearTable, &QPushButton::clicked, this, &AcarsDemodGUI::on_clearTable_clicked);
    QObject::connect(ui->udpEnabled, &ButtonSwitch::clicked, this, &AcarsDemodGUI::on_udpEnabled_clicked);
    CRightClickEnabler *udpRightClickEnabler = new CRightClickEnabler(ui->udpEnabled);
    connect(udpRightClickEnabler, &CRightClickEnabler::rightClick, this, &AcarsDemodGUI::udpSettings);
    QObject::connect(ui->channel1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AcarsDemodGUI::on_channel1_currentIndexChanged);
    QObject::connect(ui->channel2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AcarsDemodGUI::on_channel2_currentIndexChanged);
    QObject::connect(ui->gsTable, &QToolButton::clicked, this, &AcarsDemodGUI::on_gsTable_clicked);
    QObject::connect(ui->messages->selectionModel(), &QItemSelectionModel::selectionChanged,
                     this, &AcarsDemodGUI::messagesSelectionChanged);
    QObject::connect(ui->logEnable, &ButtonSwitch::clicked, this, &AcarsDemodGUI::on_logEnable_clicked);
    QObject::connect(ui->logFilename, &QToolButton::clicked, this, &AcarsDemodGUI::on_logFilename_clicked);
    QObject::connect(ui->logOpen, &QToolButton::clicked, this, &AcarsDemodGUI::on_logOpen_clicked);
    QObject::connect(ui->getOSNDB, &QToolButton::clicked, this, &AcarsDemodGUI::on_getOSNDB_clicked);
    QObject::connect(ui->displayChart, &ButtonSwitch::clicked, this, &AcarsDemodGUI::on_displayChart_clicked);
    QObject::connect(ui->noInfo, &ButtonSwitch::clicked, this, &AcarsDemodGUI::on_noInfo_clicked);
}

// Every frame is in the table whether or not it is shown, so this re-runs over the rows
// already there and takes effect on the whole history, not just on what arrives next
void AcarsDemodGUI::on_noInfo_clicked(bool checked)
{
    m_settings.m_hideNoInfo = checked;
    applyFilter();
    applySettings();
}

void AcarsDemodGUI::on_displayChart_clicked(bool checked)
{
    m_settings.m_displayChart = checked;
    ui->chart->setVisible(m_settings.m_displayChart);
    applySettings();
}

void AcarsDemodGUI::clearChart(const QPoint& p)
{
    (void) p;

    for (int i = 0; i < CHART_FRAME_SERIES; i++)
    {
        if (m_frameRateSeries[i]) {
            m_frameRateSeries[i]->clear();
        }
    }
    if (m_aircraftSeries) {
        m_aircraftSeries->clear();
    }
    m_aircraftLastSeen.clear();
    m_averageTime = QDateTime();
    resetChartAxes();
}

// Replace the data in a series between the specified times with its average
void AcarsDemodGUI::averageSeries(QLineSeries *series, const QDateTime& startTime, const QDateTime& endTime)
{
    int startIdx = 0;
    int endIdx = -1;

    // The points are epoch milliseconds, so compare them as such. Making a QDateTime of
    // each meant a local time zone lookup per point, under a lock Qt shares between all
    // threads - and this walks back over 10 minutes of points in every series, in every
    // ACARS demodulator at once, since they share the master timer: a ~450 ms GUI stall
    // every minute
    const qint64 startMs = startTime.toMSecsSinceEpoch();
    const qint64 endMs = endTime.toMSecsSinceEpoch();

    for (int i = series->count() - 1; i >= 0; i--)
    {
        const qint64 ms = (qint64) series->at(i).x();

        if ((endIdx == -1) && (ms <= endMs))
        {
            endIdx = i;
        }
        else if (ms < startMs)
        {
            startIdx = i + 1;
            break;
        }
    }
    int count = (endIdx - startIdx) + 1;

    if ((endIdx != -1) && (count > 1))
    {
        double sum = 0.0;
        for (int i = startIdx; i <= endIdx; i++) {
            sum += series->at(i).y();
        }
        double avg = sum / count;
        series->removePoints(startIdx, count);
        qint64 midPoint = startTime.toMSecsSinceEpoch() + (endTime.toMSecsSinceEpoch() - startTime.toMSecsSinceEpoch()) / 2;
        series->insert(startIdx, QPointF(midPoint, avg));
    }
}

void AcarsDemodGUI::resetChartAxes()
{
    m_xAxis->setMin(QDateTime::currentDateTime());
    m_xAxis->setMax(QDateTime::currentDateTime().addSecs(60*60));
    m_fpsYAxis->setMin(0);
    m_fpsYAxis->setMax(5);
    m_aircraftYAxis->setMin(0);
    m_aircraftYAxis->setMax(10);
}

void AcarsDemodGUI::plotChart()
{
    QChart *oldChart = m_chart;

    m_chart = new QChart();

    m_chart->layout()->setContentsMargins(0, 0, 0, 0);
    m_chart->setMargins(QMargins(1, 1, 1, 1));
    m_chart->setTheme(QChart::ChartThemeDark);
    m_chart->legend()->setAlignment(Qt::AlignRight);

    for (int i = 0; i < CHART_FRAME_SERIES; i++)
    {
        m_frameRateSeries[i] = new QLineSeries();
        m_frameRateSeries[i]->setName(m_frameSeriesNames[i]);
    }

    m_aircraftSeries = new QLineSeries();
    m_aircraftSeries->setName("Aircraft");

    m_xAxis = new QDateTimeAxis();
    m_fpsYAxis = new QValueAxis();
    m_aircraftYAxis = new QValueAxis();
    resetChartAxes();

    m_chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_chart->addAxis(m_fpsYAxis, Qt::AlignLeft);
    m_chart->addAxis(m_aircraftYAxis, Qt::AlignRight);

    m_fpsYAxis->setTitleText("FPS");
    m_aircraftYAxis->setTitleText("Aircraft");

    for (int i = 0; i < CHART_FRAME_SERIES; i++) {
        m_chart->addSeries(m_frameRateSeries[i]);
    }
    m_chart->addSeries(m_aircraftSeries);

    for (int i = 0; i < CHART_FRAME_SERIES; i++)
    {
        m_frameRateSeries[i]->attachAxis(m_xAxis);
        m_frameRateSeries[i]->attachAxis(m_fpsYAxis);
    }

    m_aircraftSeries->attachAxis(m_xAxis);
    m_aircraftSeries->attachAxis(m_aircraftYAxis);

    ui->chart->setChart(m_chart);
    // Wheel events arrive at the view, mouse drags at its viewport
    ui->chart->installEventFilter(this);
    ui->chart->viewport()->installEventFilter(this);

    const auto markers = m_chart->legend()->markers();
    for (QLegendMarker *marker : markers) {
        connect(marker, &QLegendMarker::clicked, this, &AcarsDemodGUI::legendMarkerClicked);
    }

    delete oldChart;
}

static void scaleRange(qint64& start, qint64& end, qint64 min, int delta, qreal centre)
{
    qint64 diff = end - start;
    double scale = pow(0.50, abs(delta) / 120.0);
    qint64 newRange;

    if (delta < 0) {
        newRange = diff / scale;
    } else {
        newRange = diff * scale;
    }

    diff = std::max(min/2, diff);
    newRange = std::max(min, newRange);
    if (delta < 0) {
        start = start - centre * diff;
    } else {
        start = start + centre * newRange;
    }
    end = start + newRange;
}

// Pan and zoom of the frames-per-second chart: mouse wheel zooms the time axis (or the
// rate axis with shift held), centred on the cursor, and dragging with the left button
// pans. Panning or zooming away from the latest data stops the time axis auto-following;
// it resumes when the right edge is back at the newest point.
bool AcarsDemodGUI::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == ui->chart) || (obj == ui->chart->viewport()))
    {
        if (event->type() == QEvent::Wheel)
        {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
            int delta = wheelEvent->angleDelta().y(); // Typically 120 for one click of the wheel

            if (m_chart && m_frameRateSeries[0])
            {
                QPointF point = wheelEvent->position();
                QRectF plotArea = m_chart->plotArea();

                if (wheelEvent->modifiers() & Qt::ShiftModifier)
                {
                    // Centre scaling on the cursor location
                    qreal y = (point.y() - plotArea.y()) / plotArea.height();
                    y = 1.0 - std::min(1.0, std::max(0.0, y));

                    if (anyFrameSeriesVisible())
                    {
                        qint64 min = (qint64) m_fpsYAxis->min();
                        qint64 max = (qint64) m_fpsYAxis->max();

                        scaleRange(min, max, 2LL, delta / 2, y);

                        m_fpsYAxis->setMin((qreal) std::max(0LL, min));
                        m_fpsYAxis->setMax((qreal) std::min(5000LL, max));
                    }

                    if (m_aircraftSeries->isVisible())
                    {
                        qint64 min = (qint64) m_aircraftYAxis->min();
                        qint64 max = (qint64) m_aircraftYAxis->max();

                        scaleRange(min, max, 2LL, delta / 2, y);

                        m_aircraftYAxis->setMin((qreal) std::max(0LL, min));
                        m_aircraftYAxis->setMax((qreal) std::min(5000LL, max));
                    }
                }
                else if (m_frameRateSeries[0]->count() > 1)
                {
                    qreal x = (point.x() - plotArea.x()) / plotArea.width();
                    x = std::min(1.0, std::max(0.0, x));

                    qint64 startMS = m_xAxis->min().toMSecsSinceEpoch();
                    qint64 endMS = m_xAxis->max().toMSecsSinceEpoch();

                    scaleRange(startMS, endMS, 10000LL, delta, x);

                    // Don't let the range exceed the available data
                    startMS = std::max((qint64) m_frameRateSeries[0]->at(0).x(), startMS);
                    endMS = std::min((qint64) m_frameRateSeries[0]->at(m_frameRateSeries[0]->count() - 1).x(), endMS);
                    m_xAxis->setMin(QDateTime::fromMSecsSinceEpoch(startMS));
                    m_xAxis->setMax(QDateTime::fromMSecsSinceEpoch(endMS));
                }
            }
            wheelEvent->accept();
            return true;
        }
        else if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->buttons() & Qt::LeftButton)
            {
                // Don't consume the press, so legend clicks still work
                m_chartPanPos = mouseEvent->pos();
                m_chartPanning = true;
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (m_chartPanning && (mouseEvent->buttons() & Qt::LeftButton) && m_chart)
            {
                QPoint delta = mouseEvent->pos() - m_chartPanPos;
                m_chartPanPos = mouseEvent->pos();
                QRectF plotArea = m_chart->plotArea();

                if ((plotArea.width() > 0) && m_frameRateSeries[0] && (m_frameRateSeries[0]->count() > 1))
                {
                    // The content follows the cursor
                    qint64 startMS = m_xAxis->min().toMSecsSinceEpoch();
                    qint64 endMS = m_xAxis->max().toMSecsSinceEpoch();
                    qint64 shift = (qint64) (-delta.x() * (endMS - startMS) / plotArea.width());

                    // Don't pan beyond the available data
                    qint64 dataStart = (qint64) m_frameRateSeries[0]->at(0).x();
                    qint64 dataEnd = (qint64) m_frameRateSeries[0]->at(m_frameRateSeries[0]->count() - 1).x();
                    if (shift > 0) {
                        shift = std::max(0LL, std::min(shift, dataEnd - endMS));
                    } else {
                        shift = std::min(0LL, std::max(shift, dataStart - startMS));
                    }

                    if (shift != 0)
                    {
                        m_xAxis->setMin(QDateTime::fromMSecsSinceEpoch(startMS + shift));
                        m_xAxis->setMax(QDateTime::fromMSecsSinceEpoch(endMS + shift));
                    }
                }

                if (plotArea.height() > 0)
                {
                    if (anyFrameSeriesVisible())
                    {
                        qreal range = m_fpsYAxis->max() - m_fpsYAxis->min();
                        qreal shift = delta.y() * range / plotArea.height();
                        qreal newMin = std::max(0.0, m_fpsYAxis->min() + shift);
                        m_fpsYAxis->setMin(newMin);
                        m_fpsYAxis->setMax(newMin + range);
                    }
                    if (m_aircraftSeries->isVisible())
                    {
                        qreal range = m_aircraftYAxis->max() - m_aircraftYAxis->min();
                        qreal shift = delta.y() * range / plotArea.height();
                        qreal newMin = std::max(0.0, m_aircraftYAxis->min() + shift);
                        m_aircraftYAxis->setMin(newMin);
                        m_aircraftYAxis->setMax(newMin + range);
                    }
                }
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            m_chartPanning = false;
        }
    }
    return ChannelGUI::eventFilter(obj, event);
}

// Click on a legend marker shows/hides the corresponding series
void AcarsDemodGUI::legendMarkerClicked()
{
    QLegendMarker* marker = qobject_cast<QLegendMarker*>(sender());
    marker->series()->setVisible(!marker->series()->isVisible());
    marker->setVisible(true);

    // Dim the marker, if series is not visible
    qreal alpha = marker->series()->isVisible() ? 1.0 : 0.5;

    QBrush labelBrush = marker->labelBrush();
    QColor color = labelBrush.color();
    color.setAlphaF(alpha);
    labelBrush.setColor(color);
    marker->setLabelBrush(labelBrush);

    QBrush brush = marker->brush();
    color = brush.color();
    color.setAlphaF(alpha);
    brush.setColor(color);
    marker->setBrush(brush);

    QPen pen = marker->pen();
    color = pen.color();
    color.setAlphaF(alpha);
    pen.setColor(color);
    marker->setPen(pen);
}

void AcarsDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void AcarsDemodGUI::on_logEnable_clicked(bool checked)
{
    m_settings.m_logEnabled = checked;
    applySettings();
}

void AcarsDemodGUI::on_logFilename_clicked()
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
void AcarsDemodGUI::on_logOpen_clicked()
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
                // Sorting stays on: the proxy places each row as it arrives
                //ui->messages->setUpdatesEnabled(false);
                QTextStream in(&file);
                QString error;
                QHash<QString, int> colIndexes = CSV::readHeader(in, {"DateTime", "Data"}, error);
                if (error.isEmpty())
                {
                    int dateTimeCol = colIndexes.value("DateTime");
                    int dataCol = colIndexes.value("Data");
                    int maxCol = std::max(dateTimeCol, dataCol);

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
                            QDateTime dateTime = QDateTime::fromString(cols[dateTimeCol], Qt::ISODateWithMs);
                            QByteArray bytes = QByteArray::fromHex(cols[dataCol].toLatin1());

                            // Decoded by the worker, which echoes display events back here.
                            // As a replay, so it is not sent out again as live traffic.
                            if (!bytes.isEmpty()) {
                                m_acarsDemod->getWorker()->getInputMessageQueue()->push(
                                    AcarsDemodWorker::MsgReplayPacket::create(bytes, dateTime));
                            }

                            if (count % 50000 == 0)
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
                    //ui->messages->setUpdatesEnabled(true);
                    // Nothing to re-sort: the proxy kept the order as the rows arrived
                }
                else
                {
                    QMessageBox::critical(this, "ACARS Demod", error);
                }
                QDateTime finishTime = QDateTime::currentDateTime();
                qDebug() << "Read CSV in " << startTime.secsTo(finishTime);
            }
            else
            {
                QMessageBox::critical(this, "ACARS Demod", QString("Failed to open file %1").arg(fileNames[0]));
            }
        }
    }
}

void AcarsDemodGUI::aircraftPhoto(const PlaneSpottersPhoto *photo)
{
    // Make sure the photo is for the currently highlighted aircraft, as it may
    // have taken a while to download
    if (!photo->m_pixmap.isNull() /*&& (ui->messages->item(row, MESSAGE_COL_ADDRESS)->text() == photo->m_id)*/)
    {
        ui->photo->setPixmap(photo->m_pixmap);
        ui->photo->setToolTip(QString("Photographer: %1").arg(photo->m_photographer)); // Required by terms of use
        ui->photoHeader->setVisible(true);
        ui->photoFlag->setVisible(true);
        ui->photo->setVisible(true);
        ui->flightDetails->setVisible(true);
        ui->aircraftDetails->setVisible(true);
        m_photoLink = photo->m_link;
    }
}

void AcarsDemodGUI::photoClicked()
{
    // Photo needs to link back to PlaneSpotters, as per terms of use
    QDesktopServices::openUrl(QUrl(m_photoLink));
}

void AcarsDemodGUI::updatePhotoText(const QString& registration)
{
    if (m_settings.m_displayPhotos)
    {
        ui->photoHeader->setText(QString("%1").arg(registration));

        if (m_aircraftInfo && m_aircraftInfo->contains(registration))
        {
            AircraftInformation *aircraftInfo = m_aircraftInfo->value(registration);

            ui->photoFlag->setPixmap(QPixmap());
            QString flag = aircraftInfo->getFlag();
            if (flag != "")
            {
                QIcon *icon = AircraftInformation::getFlagIcon(flag);
                if (icon != nullptr)
                {
                    QList<QSize> sizes = icon->availableSizes();
                    if (sizes.size() > 0) {
                        ui->photoFlag->setPixmap(icon->pixmap(sizes[0]));
                    }
                }
            }

            ui->flightDetails->setPixmap(QPixmap());
            QIcon *airlineIcon = AircraftInformation::getAirlineIcon(aircraftInfo->m_operatorICAO);
            if (airlineIcon != nullptr)
            {
                QList<QSize> sizes = airlineIcon->availableSizes();
                if (sizes.size() > 0) {
                    ui->flightDetails->setPixmap(airlineIcon->pixmap(sizes[0]));
                }
            }

            QString aircraftDetails = "<table width=200>"; // Note, Qt seems to make the table bigger than this so text is cropped, not wrapped
            if (!aircraftInfo->m_manufacturerName.isEmpty()) {
                aircraftDetails.append(QString("<tr><th align=left>Manufacturer:<td>%1").arg(aircraftInfo->m_manufacturerName));
            }
            if (!aircraftInfo->m_model.isEmpty()) {
                aircraftDetails.append(QString("<tr><th align=left>Aircraft:<td>%1").arg(aircraftInfo->m_model));
            }
            if (!aircraftInfo->m_owner.isEmpty()) {
                aircraftDetails.append(QString("<tr><th align=left>Owner:<td>%1").arg(aircraftInfo->m_owner));
            }
            if (!aircraftInfo->m_operatorICAO.isEmpty()) {
                aircraftDetails.append(QString("<tr><th align=left>Operator:<td>%1").arg(aircraftInfo->m_operatorICAO));
            }
            if (!aircraftInfo->m_registered.isEmpty()) {
                aircraftDetails.append(QString("<tr><th align=left>Registered:<td>%1").arg(aircraftInfo->m_registered));
            }
            aircraftDetails.append("</table>");
            ui->aircraftDetails->setText(aircraftDetails);
        }
        else
        {
            qDebug() << "No info for " << registration;
        }
    }
}

void AcarsDemodGUI::downloadingURL(const QString& url)
{
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(QString("Downloading %1.").arg(url));
        m_progressDialog->setValue(m_progressDialog->value() + 1);
    }
}

void AcarsDemodGUI::downloadError(const QString& error)
{
    QMessageBox::critical(this, "ADS-B", error);
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void AcarsDemodGUI::downloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    if (m_progressDialog)
    {
        m_progressDialog->setMaximum(totalBytes);
        m_progressDialog->setValue(bytesRead);
    }
}

void AcarsDemodGUI::downloadAircraftInformationFinished()
{
    if (m_progressDialog)
    {
        delete m_progressDialog;
        m_progressDialog = new QProgressDialog("Reading Aircraft Information.", "", 0, 1, this);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->show();
        QApplication::processEvents();
    }
    m_aircraftInfo = OsnDB::getAircraftInformationByReg();
    // The worker holds its own copy, for the registration lookups
    if (m_acarsDemod->getWorker()) {
        m_acarsDemod->getWorker()->getInputMessageQueue()->push(AcarsDemodWorker::MsgReloadAircraftDatabase::create());
    }
    if (m_progressDialog)
    {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}
