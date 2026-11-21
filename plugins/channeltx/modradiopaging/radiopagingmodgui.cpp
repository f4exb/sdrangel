///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include <QDateTime>
#include <QHeaderView>
#include <QTimeZone>
#include <QDateTimeEdit>
#include <QDebug>
#include <QFileDialog>
#include <QAction>
#include <QDesktopServices>
#include <QDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>
#include <QStyle>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenu>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>

#include <algorithm>
#include <cstdlib>

#include "dsp/dspcommands.h"
#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/audioselectdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/pagingmessagetable.h"
#include "gui/pagingvoiceplayer.h"
#include "radiopagingmodmessagemodel.h"
#include "dsp/dspengine.h"
#include "audio/audiodevicemanager.h"
#include "radiopagingwav.h"
#include "radiopagingvoicestore.h"
#include "maincore.h"

#include "ui_radiopagingmodgui.h"
#include "radiopagingmodgui.h"

namespace {

int pocsagSpeedIndex(int baud)
{
    for (int i = 0; i < RadioPagingModSettings::m_nbPocsagSpeeds; i++)
    {
        if (RadioPagingModSettings::m_pocsagSpeeds[i] == baud) {
            return i;
        }
    }

    return 1;
}

QString stateName(RadioPagingModSource::State state)
{
    switch (state)
    {
    case RadioPagingModSource::Idle:
        return "Idle";
    case RadioPagingModSource::RampUp:
        return "Ramp up";
    case RadioPagingModSource::Tx:
        return "Transmitting";
    case RadioPagingModSource::RampDown:
        return "Ramp down";
    }

    return "";
}

// Status chips, as in the receiver: dark text on a light colour
enum Chip { ChipActive, ChipBusy, ChipBad, ChipQuiet, ChipWaiting };

// The saved columns of the table of the messages start with it (RadioPagingModMessageModel columns)
const QByteArray messageColumnsTag("RPMM1:");

// A chip; with a link, a click on it is the link (a filter of the messages). The filter shown is underlined
QString chipHtml(Chip chip, const QString& text, const QString& link = QString(), bool active = false)
{
    static const char *colours[] = {"#a8e0a8", "#f0dc8c", "#f4aca4", "#d4d4d4", "#a8c8ec"};
    const QString span = QString("<span style=\"background-color:%1; color:#141414;%2\">&nbsp;%3&nbsp;</span>")
        .arg(colours[chip]).arg(active ? " text-decoration:underline; font-weight:bold;" : "").arg(text.toHtmlEscaped());

    return link.isEmpty() ? span : QString("<a href=\"%1\" style=\"text-decoration:none;\">%2</a>").arg(link, span);
}

// A combo box item that cannot be selected
void setItemEnabled(QComboBox *combo, int index, bool enabled)
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(combo->model());

    if (model && model->item(index)) {
        model->item(index)->setEnabled(enabled);
    }
}

} // namespace

RadioPagingModGUI* RadioPagingModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    RadioPagingModGUI* gui = new RadioPagingModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void RadioPagingModGUI::destroy()
{
    delete this;
}

void RadioPagingModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray RadioPagingModGUI::serialize() const
{
    return m_settings.serialize();
}

bool RadioPagingModGUI::deserialize(const QByteArray& data)
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

bool RadioPagingModGUI::handleMessage(const Message& message)
{
    if (RadioPagingMod::MsgConfigureRadioPagingMod::match(message))
    {
        const RadioPagingMod::MsgConfigureRadioPagingMod& cfg = (RadioPagingMod::MsgConfigureRadioPagingMod&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RadioPagingMod::MsgReportTx::match(message))
    {
        const RadioPagingMod::MsgReportTx& report = (RadioPagingMod::MsgReportTx&) message;

        if (m_liveWaiting)
        {
            // The live voice page is queued: the button becomes End, or the request was refused
            m_liveWaiting = false;
            ui->composer->setSendEnabled(true);

            if (report.getId() != 0)
            {
                m_liveId = report.getId();
                m_liveSeen = false;
                ui->composer->setSendText("End (alert time)");
            }
            else
            {
                ui->composer->setSendText(QString());
            }
        }

        // The message is in the tables; the log has what they do not show
        if (report.getId() != 0)
        {
            if (!report.getWarning().isEmpty()) {
                log(QString("Warning: %1").arg(report.getWarning()));
            }
        }
        else {
            refused(report.getError());
        }
        return true;
    }
    else if (RadioPagingMod::MsgTxLog::match(message))
    {
        const RadioPagingMod::MsgTxLog& entry = (const RadioPagingMod::MsgTxLog&) message;
        showTx(entry.getRecord(), entry.getStatus());

        // A live voice page that is dropped (aborted, cleared) is over
        if ((entry.getStatus() == "dropped") && (m_liveId != 0)
         && (entry.getRecord().m_protocolData.value("tx_id").toULongLong() == m_liveId))
        {
            log("Voice page dropped");
            m_liveId = 0;
            stopMic();
            m_liveSeen = false;
            ui->composer->setSendEnabled(true);
            ui->composer->setSendText(QString());
        }

        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        // The dial clamps its value to a new range and signals it as a change: the range of a sample rate that is
        // not yet the device's (a configuration being loaded, a file not yet opened) would replace the offset. The
        // offset stays in the settings; the dial shows it within the range
        ui->deltaFrequency->blockSignals(true);
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
        ui->deltaFrequency->blockSignals(false);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else
    {
        return false;
    }
}

void RadioPagingModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void RadioPagingModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void RadioPagingModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void RadioPagingModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
    applySettings(QStringList("channelMute"));
}

// A change by the user: the deviation goes to the usual one of the protocol. While the
// settings are displayed (preset, WebAPI) they are kept as they are
void RadioPagingModGUI::on_protocol_currentIndexChanged(int index)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_protocol = (RadioPagingModSettings::Protocol) index;
    m_settings.m_deviation = RadioPagingModSettings::defaultDeviation(m_settings.m_protocol);
    blockApplySettings(true);
    ui->deviation->setValue((int) m_settings.m_deviation);
    displayProtocol();
    blockApplySettings(false);
    m_channelMarker.setBandwidth(channelBandwidth());
    applySettings(QStringList({"protocol", "deviation"}));
}

void RadioPagingModGUI::on_mode_currentIndexChanged(int index)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_mode = (RadioPagingModSettings::Mode) index;
    blockApplySettings(true);
    displayProtocol();
    blockApplySettings(false);
    applySettings(QStringList("mode"));
}

void RadioPagingModGUI::on_deviation_valueChanged(int value)
{
    m_settings.m_deviation = value;
    m_channelMarker.setBandwidth(channelBandwidth());
    applySettings(QStringList("deviation"));
}

void RadioPagingModGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(QString("%1dB").arg(value));
    m_settings.m_gain = value;
    applySettings(QStringList("gain"));
}

// POCSAG speed or FLEX mode
void RadioPagingModGUI::on_speed_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    if (m_settings.m_protocol == RadioPagingModSettings::Gsc)
    {
        if (index <= (int) RadioPagingGsc::NonBatterySaver)
        {
            m_settings.m_gscMode = index;
            applySettings(QStringList("gscMode"));
        }
    }
    else if (m_settings.m_protocol == RadioPagingModSettings::Flex)
    {
        if (index <= (int) RadioPagingFlex::Mode6400_4)
        {
            m_settings.m_flexMode = index;
            m_channelMarker.setBandwidth(channelBandwidth());
            applySettings(QStringList("flexMode"));
        }
    }
    else if (index < RadioPagingModSettings::m_nbPocsagSpeeds)
    {
        m_settings.m_pocsagSpeed = RadioPagingModSettings::m_pocsagSpeeds[index];
        m_channelMarker.setBandwidth(channelBandwidth());
        applySettings(QStringList("pocsagSpeed"));
    }
}

void RadioPagingModGUI::on_polarity_currentIndexChanged(int index)
{
    if (m_settings.m_protocol == RadioPagingModSettings::Gsc)
    {
        m_settings.m_gscInverted = (index == 1);
        applySettings(QStringList("gscInverted"));
    }
    else if (m_settings.m_protocol == RadioPagingModSettings::Flex)
    {
        m_settings.m_flexInverted = (index == 1);
        applySettings(QStringList("flexInverted"));
    }
    else
    {
        m_settings.m_pocsagInverted = (index == 1);
        applySettings(QStringList("pocsagInverted"));
    }
}

void RadioPagingModGUI::on_collapse_valueChanged(int value)
{
    m_settings.m_flexCollapse = value;
    applySettings(QStringList("flexCollapse"));
}

void RadioPagingModGUI::on_roaming_toggled(bool checked)
{
    m_settings.m_flexRoaming = checked;
    applySettings(QStringList("flexRoaming"));
    displaySettings();
}

void RadioPagingModGUI::on_ssidZone_valueChanged(int value)
{
    m_settings.m_ssidCoverageZone = value;
    applySettings(QStringList("ssidCoverageZone"));
}

void RadioPagingModGUI::on_ssidLocalId_valueChanged(int value)
{
    m_settings.m_ssidLocalId = value;
    applySettings(QStringList("ssidLocalId"));
}

void RadioPagingModGUI::on_ssidCountry_valueChanged(int value)
{
    m_settings.m_ssidCountryCode = value;
    applySettings(QStringList("ssidCountryCode"));
}

void RadioPagingModGUI::on_ssidFlags_valueChanged(int value)
{
    m_settings.m_ssidTrafficFlags = value;
    applySettings(QStringList("ssidTrafficFlags"));
}

// FLEX date and time: the zones of STD-43A Table 3.7.2-3, west to east
void RadioPagingModGUI::fillTimeZones()
{
    QList<int> zones;

    for (int code = 0; code < 32; code++)
    {
        int minutes;

        if (RadioPagingFlex::timeZoneMinutes((unsigned) code, minutes)) {
            zones.append(minutes);
        }
    }

    std::sort(zones.begin(), zones.end());
    QSignalBlocker blocker(ui->flexTimeZone);
    ui->flexTimeZone->clear();

    for (int minutes : zones)
    {
        ui->flexTimeZone->addItem(QString("UTC%1%2:%3").arg(minutes < 0 ? "-" : "+").arg(std::abs(minutes) / 60, 2, 10, QChar('0'))
            .arg(std::abs(minutes) % 60, 2, 10, QChar('0')), minutes);
    }
}

// Time sent now (local, as set by the settings): the grid time plus the offset, the zone and daylight saving
QDateTime RadioPagingModGUI::flexTimeNow() const
{
    qint64 seconds = QDateTime::currentSecsSinceEpoch() + m_settings.m_flexTimeOffsetSeconds
        + (qint64) (m_settings.m_flexTimeZoneMinutes + (m_settings.m_flexTimeDaylightSaving ? 60 : 0)) * 60;
    return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc());
}

void RadioPagingModGUI::displayTime(bool flex)
{
    QSignalBlocker timeBlocker(ui->flexTime);
    QSignalBlocker referenceBlocker(ui->flexTimeReference);
    QSignalBlocker dstBlocker(ui->flexTimeDaylightSaving);
    QSignalBlocker sendZoneBlocker(ui->flexTimeSendZone);
    QSignalBlocker zoneBlocker(ui->flexTimeZone);
    QSignalBlocker clockBlocker(ui->flexTimeClock);
    QSignalBlocker setBlocker(ui->flexTimeSet);
    QSignalBlocker yearBlocker(ui->flexTimeEquivalentYear);
    QSignalBlocker extendedBlocker(ui->flexTimeExtendedSeconds);

    for (QWidget *w : QList<QWidget*>({ui->flexTime, ui->flexTimeReference, ui->flexTimeDaylightSaving, ui->flexTimeSendZone,
            ui->flexTimeZone, ui->flexTimeClock, ui->flexTimeSet, ui->flexTimeEquivalentYear, ui->flexTimeExtendedSeconds})) {
        w->setVisible(flex);
    }

    ui->flexTime->setChecked(m_settings.m_flexTime);
    ui->flexTimeReference->setCurrentIndex(m_settings.m_flexTimeFrameZero ? 0 : 1);
    ui->flexTimeDaylightSaving->setChecked(m_settings.m_flexTimeDaylightSaving);
    ui->flexTimeSendZone->setChecked(m_settings.m_flexTimeSendZone);
    ui->flexTimeEquivalentYear->setChecked(m_settings.m_flexTimeEquivalentYear);
    ui->flexTimeExtendedSeconds->setValue(m_settings.m_flexTimeExtendedSeconds);
    int zoneIndex = ui->flexTimeZone->findData(m_settings.m_flexTimeZoneMinutes);
    ui->flexTimeZone->setCurrentIndex((zoneIndex >= 0) ? zoneIndex : ui->flexTimeZone->findData(0));
    bool clock = (m_settings.m_flexTimeOffsetSeconds == 0);
    ui->flexTimeClock->setChecked(clock);
    ui->flexTimeSet->setDateTime(flexTimeNow());

    for (QWidget *w : QList<QWidget*>({ui->flexTimeReference, ui->flexTimeDaylightSaving, ui->flexTimeSendZone, ui->flexTimeZone,
            ui->flexTimeClock, ui->flexTimeEquivalentYear})) {
        w->setEnabled(m_settings.m_flexTime);
    }

    // The extended second is in the zone word
    ui->flexTimeExtendedSeconds->setEnabled(m_settings.m_flexTime && m_settings.m_flexTimeSendZone);
    ui->flexTimeSet->setEnabled(m_settings.m_flexTime && !clock);
}

void RadioPagingModGUI::displayNumbering(bool flex)
{
    QSignalBlocker numberingBlocker(ui->flexNumbering);
    QSignalBlocker maxBlocker(ui->flexMaxMessageNumber);

    ui->flexNumbering->setVisible(flex);
    ui->flexMaxMessageNumber->setVisible(flex);
    ui->flexNumbering->setChecked(m_settings.m_flexNumbering);
    ui->flexMaxMessageNumber->setValue(m_settings.m_flexMaxMessageNumber);
    ui->flexMaxMessageNumber->setEnabled(m_settings.m_flexNumbering);
}

void RadioPagingModGUI::on_flexNumbering_toggled(bool checked)
{
    m_settings.m_flexNumbering = checked;
    applySettings(QStringList("flexNumbering"));
    displayNumbering(true);
    displayMessage();           // FLEX copies need the numbering
}

void RadioPagingModGUI::on_flexMaxMessageNumber_valueChanged(int value)
{
    m_settings.m_flexMaxMessageNumber = value;
    applySettings(QStringList("flexMaxMessageNumber"));
}

void RadioPagingModGUI::displaySystemEvent(bool flex, bool network)
{
    // The events of STD-43A Table 3.9.6-1, e0 to e4 (e5-e10 are reserved)
    if (!ui->flexEventFlags->menu())
    {
        static const char *events[] = {
            "e0 Traffic split by SSID TMF",
            "e1 Traffic split by NID TMF",
            "e2 Change of the Channel Set Up Instruction",
            "e3 New frequency of the NID",
            "e4 New frequency of the SSID coverage zone"
        };
        QMenu *menu = new QMenu(ui->flexEventFlags);

        for (int i = 0; i < 5; i++)
        {
            QAction *action = menu->addAction(events[i]);
            action->setCheckable(true);
            action->setData(1 << i);
            connect(action, &QAction::toggled, this, [this]() {
                unsigned flags = systemEventFlags();
                ui->flexEventFlags->setText(flags ? QString("Events %1").arg(QString::number(flags, 16).toUpper().rightJustified(3, QChar('0')))
                    : QString("Events"));
            });
        }

        ui->flexEventFlags->setMenu(menu);
    }

    const bool shown = flex && network;
    ui->flexEventFlags->setVisible(shown);
    ui->flexEventFrames->setVisible(shown);
    ui->flexEventSend->setVisible(shown);
}

unsigned RadioPagingModGUI::systemEventFlags() const
{
    unsigned flags = 0;

    if (ui->flexEventFlags->menu())
    {
        for (QAction *action : ui->flexEventFlags->menu()->actions())
        {
            if (action->isChecked()) {
                flags |= action->data().toUInt();
            }
        }
    }

    return flags;
}

// Notify starts the notification of the events checked; while it is sent the button stops it
void RadioPagingModGUI::on_flexEventSend_clicked()
{
    RadioPagingModSource::Status status = m_radioPagingMod->getStatus();
    unsigned events = (status.m_systemEventFrames > 0) ? 0 : systemEventFlags();
    QString error;

    if ((status.m_systemEventFrames == 0) && (events == 0))
    {
        log(tr("System Event Notification: check the events to announce first"));
        return;
    }

    if (!m_radioPagingMod->notifySystemEvent(events, ui->flexEventFrames->value(), error)) {
        log(tr("System Event Notification refused: %1").arg(error));
    } else if (events == 0) {
        log(tr("System Event Notification stopped"));
    } else {
        log(tr("System Event Notification of events %1 for %2 frames").arg(events, 3, 16, QChar('0')).arg(ui->flexEventFrames->value()));
    }

    updateSystemEvent(m_radioPagingMod->getStatus());
}

void RadioPagingModGUI::updateSystemEvent(const RadioPagingModSource::Status& status)
{
    if (status.m_systemEventFrames > 0)
    {
        ui->flexEventSend->setText(tr("Stop %1").arg(status.m_systemEventFrames));
        ui->flexEventSend->setToolTip(tr("The System Event Notification of events %1 is being sent: %2 frames left. Click to stop it")
            .arg(status.m_systemEvents, 3, 16, QChar('0')).arg(status.m_systemEventFrames));
    }
    else
    {
        ui->flexEventSend->setText(tr("Notify"));
        ui->flexEventSend->setToolTip(tr("Start the System Event Notification of the events checked (FLEX network mode):\n"
            "it goes in every phase of each frame for the frames set"));
    }
}

void RadioPagingModGUI::on_flexTime_toggled(bool checked)
{
    m_settings.m_flexTime = checked;
    applySettings(QStringList("flexTime"));
    displayTime(true);
}

void RadioPagingModGUI::on_flexTimeReference_currentIndexChanged(int index)
{
    m_settings.m_flexTimeFrameZero = (index == 0);
    applySettings(QStringList("flexTimeFrameZero"));
}

void RadioPagingModGUI::on_flexTimeDaylightSaving_toggled(bool checked)
{
    // The time shown and sent keeps its value when Clock is off: the offset takes back the hour
    if (m_settings.m_flexTimeOffsetSeconds != 0) {
        m_settings.m_flexTimeOffsetSeconds += checked ? -3600 : 3600;
    }

    m_settings.m_flexTimeDaylightSaving = checked;
    applySettings(QStringList({"flexTimeDaylightSaving", "flexTimeOffsetSeconds"}));
    displayTime(true);
}

void RadioPagingModGUI::on_flexTimeSendZone_toggled(bool checked)
{
    m_settings.m_flexTimeSendZone = checked;
    applySettings(QStringList("flexTimeSendZone"));
    displayTime(true);
}

void RadioPagingModGUI::on_flexTimeEquivalentYear_toggled(bool checked)
{
    m_settings.m_flexTimeEquivalentYear = checked;
    applySettings(QStringList("flexTimeEquivalentYear"));
}

void RadioPagingModGUI::on_flexTimeExtendedSeconds_valueChanged(int value)
{
    m_settings.m_flexTimeExtendedSeconds = value;
    applySettings(QStringList("flexTimeExtendedSeconds"));
}

void RadioPagingModGUI::on_flexTimeZone_currentIndexChanged(int index)
{
    int minutes = ui->flexTimeZone->itemData(index).toInt();

    // The time shown and sent keeps its value when Clock is off: the offset takes back the change of zone
    if (m_settings.m_flexTimeOffsetSeconds != 0) {
        m_settings.m_flexTimeOffsetSeconds -= (qint64) (minutes - m_settings.m_flexTimeZoneMinutes) * 60;
    }

    m_settings.m_flexTimeZoneMinutes = minutes;
    applySettings(QStringList({"flexTimeZoneMinutes", "flexTimeOffsetSeconds"}));
    displayTime(true);
}

void RadioPagingModGUI::on_flexTimeClock_toggled(bool checked)
{
    if (checked) {
        m_settings.m_flexTimeOffsetSeconds = 0;
    } else {
        // From the time of the clock, set it then; 1 s keeps it apart from the clock (offset 0)
        m_settings.m_flexTimeOffsetSeconds = 1;
    }

    applySettings(QStringList("flexTimeOffsetSeconds"));
    displayTime(true);
}

void RadioPagingModGUI::on_flexTimeSet_editingFinished()
{
    // Local time wanted now (the editor shows the local time as such): the offset is what it lacks from the
    // clock with the zone and daylight saving
    const QDateTime edited = ui->flexTimeSet->dateTime();
    const QDateTime shown(edited.date(), edited.time(), QTimeZone::utc());
    qint64 wanted = shown.toSecsSinceEpoch();
    qint64 offset = wanted - QDateTime::currentSecsSinceEpoch()
        - (qint64) (m_settings.m_flexTimeZoneMinutes + (m_settings.m_flexTimeDaylightSaving ? 60 : 0)) * 60;

    // Offset 0 is the clock: a time set equal to the clock is kept 1 s apart
    m_settings.m_flexTimeOffsetSeconds = (offset == 0) ? 1 : offset;
    applySettings(QStringList("flexTimeOffsetSeconds"));
}

void RadioPagingModGUI::on_voiceSeconds_valueChanged(int value)
{
    m_settings.m_gscVoiceSeconds = value;
    applySettings(QStringList("gscVoiceSeconds"));
}

// The message of the composer, for the protocol it chose; the channel checks it and reports the result
void RadioPagingModGUI::send()
{
    // The button is End while a live voice page is going on
    if (m_liveId != 0)
    {
        log("Voice: ended");
        endLive();
        return;
    }
    if (m_liveWaiting) {
        return;
    }

    RadioPagingPage page = ui->composer->page();
    const bool microphone = ui->composer->microphoneSelected();
    QString error = page.check();

    // The microphone takes the place of the file: the page is checked without it
    if (microphone && (page.m_type == "voice") && page.m_voiceFile.isEmpty()) {
        page.m_voiceFile = "microphone";
        error = page.check();
    }
    if (!error.isEmpty())
    {
        refused(error);
        return;
    }

    RadioPagingMod::TxRequest request = RadioPagingMod::requestFromPage(page);
    request.m_live = microphone;

    if (microphone)
    {
        // The page goes out, the microphone is taken after the alert time
        request.m_voiceFile.clear();
        m_liveWaiting = true;
        ui->composer->setSendEnabled(false);
    }

    m_radioPagingMod->getInputMessageQueue()->push(RadioPagingMod::MsgTx::create(request));
}

// The composer for the channel: the protocols it sends (a FLEX channel with the mixing option also sends POCSAG),
// copies in the network mode (not GSC), the numbering of FLEX, the microphone for voice
void RadioPagingModGUI::displayMessage()
{
    const bool network = (m_settings.m_mode == RadioPagingModSettings::Network);
    QStringList protocols;

    if (m_settings.m_protocol == RadioPagingModSettings::Pocsag) {
        protocols << "pocsag";
    } else if (m_settings.m_protocol == RadioPagingModSettings::Gsc) {
        protocols << "gsc";
    } else {
        protocols << "flex";

        if (network && m_settings.m_flexMixPocsag) {
            protocols << "pocsag";
        }
    }

    ui->composer->setProtocols(protocols);
    ui->composer->setNetworkMode(network);
    ui->composer->setFlexNumbering(m_settings.m_flexNumbering);
    ui->composer->setMicrophone(true);

    // The button is End while a live voice page goes on
    if (m_liveId != 0) {
        ui->composer->setSendEnabled(true);
    }
}

void RadioPagingModGUI::micAudioSelect(const QPoint& p)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioInputDeviceName, true); // true for input
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioInputDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QStringList("audioInputDeviceName"));
    }
}

// The microphone is taken when the alert time of the live voice page is over (tick), and
// released when the page ends: End, or the voice length
void RadioPagingModGUI::startMic()
{
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    int index = audioDeviceManager->getInputDeviceIndex(m_settings.m_audioInputDeviceName);
    m_micRate = audioDeviceManager->getInputSampleRate(index);

    if (!m_micResampler.setup(m_micRate))
    {
        log(QString("Microphone: the audio input rate %1 Hz is not supported (1 to 76.8 kHz)").arg(m_micRate));
        endLive();
        return;
    }

    m_micSent = 0;
    m_micFifo.setSize(m_micRate);
    audioDeviceManager->addAudioSource(&m_micFifo, m_radioPagingMod->getInputMessageQueue(), index);
    m_micRecording = true;
    log("Microphone: on the air");
}

void RadioPagingModGUI::stopMic()
{
    if (!m_micRecording) {
        return;
    }

    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(&m_micFifo);
    readMic();              // What the fifo still holds
    m_micRecording = false;
}

// The voice from the audio input, the channels averaged, resampled to the rate of the modulator
// as it comes and passed to the transmitter. It stops at the voice length
void RadioPagingModGUI::readMic()
{
    AudioVector buffer(4096);
    std::vector<float> mono;
    std::vector<float> out;
    const quint64 limit = (quint64) m_settings.m_gscVoiceSeconds * RadioPagingWav::m_sampleRate;
    uint32_t n;

    while ((n = m_micFifo.read((quint8*) &buffer[0], buffer.size())) > 0)
    {
        mono.clear();

        for (uint32_t i = 0; i < n; i++) {
            mono.push_back((buffer[i].l + buffer[i].r) / (2.0f * 32768.0f));
        }

        out.clear();
        m_micResampler.process(mono.data(), mono.size(), out);

        if (m_micSent + out.size() > limit) {
            out.resize((size_t) (limit - std::min(limit, m_micSent)));
        }

        m_micSent += out.size();

        if (!out.empty()) {
            m_radioPagingMod->getInputMessageQueue()->push(RadioPagingMod::MsgLiveVoice::create(m_liveId, out));
        }
    }
}

void RadioPagingModGUI::micDataReady()
{
    if (m_micRecording) {
        readMic();
    }
}

// End of the live voice page: End, or the page ended by itself
void RadioPagingModGUI::endLive()
{
    stopMic();

    if (m_liveId != 0)
    {
        if (m_liveSeen) {
            m_radioPagingMod->getInputMessageQueue()->push(RadioPagingMod::MsgLiveEnd::create(m_liveId));
        } else {
            m_radioPagingMod->getInputMessageQueue()->push(RadioPagingMod::MsgClear::create(true));     // Not started: dropped
        }
    }

    m_liveId = 0;
    m_liveSeen = false;
    m_liveWaiting = false;
    ui->composer->setSendEnabled(true);
    ui->composer->setSendText(QString());
}

// The live voice page: the request, the End button and the counter while it goes on
void RadioPagingModGUI::updateLive(const RadioPagingModSource::Status& status)
{
    if (m_liveId == 0) {
        return;
    }

    bool mine = status.m_liveActive && (status.m_liveId == m_liveId);

    if (mine) {
        m_liveSeen = true;
    }

    if (mine && status.m_liveOpen && !m_micRecording) {
        startMic();
    }

    if (m_liveSeen && !mine)
    {
        // Ended by the voice length (or the page was cut)
        log("Voice page ended");
        endLive();
        return;
    }

    if (m_micRecording)
    {
        ui->composer->setSendText(QString("End %1 / %2 s").arg((double) m_micSent / RadioPagingWav::m_sampleRate, 0, 'f', 1)
            .arg(m_settings.m_gscVoiceSeconds));
    }
    else
    {
        ui->composer->setSendText("End (alert time)");
    }
}

// The last messages transmitted: a row per message, its state updated as it is sent or dropped; a copy
// (retransmission) has a row of its own
void RadioPagingModGUI::showTx(const MessageRecord& record, const QString& status)
{
    m_messageModel->update(record, status);
}

// The files of the voice sent: the statistics of the directory, the files kept, the deletion of the old ones
void RadioPagingModGUI::on_voiceFiles_clicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Files of the voice sent");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout();
    QSpinBox *days = new QSpinBox(&dialog);
    QSpinBox *minimum = new QSpinBox(&dialog);
    QLabel *stats = new QLabel(&dialog);
    QPushButton *clean = new QPushButton("Delete the old files...", &dialog);
    QPushButton *close = new QPushButton("Close", &dialog);

    days->setRange(0, 3650);
    days->setValue(m_settings.m_gscVoiceKeepDays);
    days->setSpecialValueText("Keep all");
    days->setSuffix(" days");
    days->setToolTip("Files older than this are deleted, a few at a time, when a new file of the voice sent is written.\n"
        "Keep all: no file is deleted for its age");
    minimum->setRange(0, 100000);
    minimum->setValue(m_settings.m_gscVoiceKeepMin);
    minimum->setToolTip("The newest files kept whatever their age");
    form->addRow("Delete after", days);
    form->addRow("Always keep the newest", minimum);
    layout->addLayout(form);
    layout->addWidget(stats);
    layout->addWidget(clean);
    layout->addWidget(close);

    auto refresh = [this, days, minimum, stats, clean]()
    {
        RadioPagingVoiceStore::Stats s = RadioPagingVoiceStore::stats(m_settings.voiceDirectory(), days->value(), minimum->value());
        stats->setText(QString("Directory: %1\nFiles: %2, %3\nOldest: %4\nNewest: %5\nTo delete now: %6 files, %7\nFree space on the disk: %8")
            .arg(m_settings.voiceDirectory())
            .arg(s.m_files).arg(RadioPagingVoiceStore::sizeText(s.m_bytes))
            .arg(s.m_oldest.isValid() ? s.m_oldest.toString("yyyy-MM-dd HH:mm") : QString("-"))
            .arg(s.m_newest.isValid() ? s.m_newest.toString("yyyy-MM-dd HH:mm") : QString("-"))
            .arg(s.m_expired).arg(RadioPagingVoiceStore::sizeText(s.m_expiredBytes))
            .arg(RadioPagingVoiceStore::sizeText(s.m_freeBytes)));
        clean->setEnabled(s.m_expired > 0);
    };

    QObject::connect(days, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [this, refresh](int value) {
        m_settings.m_gscVoiceKeepDays = value;
        applySettings(QStringList("gscVoiceKeepDays"));
        refresh();
    });
    QObject::connect(minimum, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [this, refresh](int value) {
        m_settings.m_gscVoiceKeepMin = value;
        applySettings(QStringList("gscVoiceKeepMin"));
        refresh();
    });
    QObject::connect(clean, &QPushButton::clicked, &dialog, [this, &dialog, refresh]()
    {
        RadioPagingVoiceStore::Stats s = RadioPagingVoiceStore::stats(m_settings.voiceDirectory(),
            m_settings.m_gscVoiceKeepDays, m_settings.m_gscVoiceKeepMin);

        if (QMessageBox::question(&dialog, "Delete the old files",
            QString("Delete %1 files of the voice sent (%2) from\n%3 ?\nThis cannot be undone.").arg(s.m_expired)
            .arg(RadioPagingVoiceStore::sizeText(s.m_expiredBytes)).arg(m_settings.voiceDirectory())) == QMessageBox::Yes)
        {
            RadioPagingVoiceStore::cleanup(m_settings.voiceDirectory(), m_settings.m_gscVoiceKeepDays, m_settings.m_gscVoiceKeepMin, -1);
        }

        refresh();
    });
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);

    refresh();
    dialog.exec();
}

void RadioPagingModGUI::on_clear_clicked()
{
    m_radioPagingMod->getInputMessageQueue()->push(RadioPagingMod::MsgClear::create(false));
    log("Queue cleared");
}

void RadioPagingModGUI::on_abort_clicked()
{
    m_radioPagingMod->getInputMessageQueue()->push(RadioPagingMod::MsgClear::create(true));
    log("Aborted");
}

void RadioPagingModGUI::log(const QString& text)
{
    ui->log->appendPlainText(QString("%1 %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(text));
}

// Refused here, before it was posted to the channel (the channel counts the ones it refuses)
void RadioPagingModGUI::refused(const QString& error)
{
    m_localRefused++;
    m_lastLocalRefusal = error;
    log(QString("Refused: %1").arg(error));
}

// The state of the transmitter and of its queue, as chips: the numbers, and colours only where something happens
void RadioPagingModGUI::updateQueueStatus(const RadioPagingModSource::Status& status)
{
    QString state = stateName(status.m_state);

    if (status.m_ers) {
        state += QString(" ERS %1 s").arg(status.m_ersMs / 1000.0, 0, 'f', 3);
    }

    // Refused by the channel (the GUI, the Message Center, the WebAPI) and here before posting
    QString lastRefusal;
    const quint64 refusedCount = m_radioPagingMod->getRefused(&lastRefusal) + m_localRefused;

    if (m_localRefused > 0 && lastRefusal.isEmpty()) {
        lastRefusal = m_lastLocalRefusal;
    }

    // Waiting, Sent and Dropped show their messages in the table, again for all; Sending shows the message on air
    // and the table follows it again
    const int filter = m_messageFilter->filterRegularExpression().pattern().isEmpty() ? -1
        : m_messageFilter->filterRegularExpression().pattern().mid(1, 1).toInt();
    QStringList chips;
    chips.append(chipHtml(status.m_state == RadioPagingModSource::Idle ? ChipQuiet : ChipActive, state));
    chips.append(chipHtml(status.m_queued > 0 ? ChipWaiting : ChipQuiet, tr("Waiting %1").arg(status.m_queued),
        "filter:0", filter == RadioPagingModMessageModel::Waiting));

    if (status.m_currentId != 0) {
        chips.append(chipHtml(ChipBusy, tr("Sending #%1").arg(status.m_currentId), "follow", m_follow));
    }

    chips.append(chipHtml(ChipQuiet, tr("Sent %1").arg(status.m_sent), "filter:2", filter == RadioPagingModMessageModel::Sent));

    if (status.m_dropped > 0) {
        chips.append(chipHtml(ChipQuiet, tr("Dropped %1").arg(status.m_dropped), "filter:3", filter == RadioPagingModMessageModel::Dropped));
    }
    if (refusedCount > 0) {
        chips.append(chipHtml(ChipBad, tr("Refused %1").arg(refusedCount)));
    }

    const QString text = chips.join(" ");

    if (text != ui->status->text())
    {
        ui->status->setText(text);
        ui->status->setToolTip(tr(
            "The transmitter and its queue:\n"
            "- the state: Idle (the carrier off), Ramp up, Transmitting, Ramp down; ERS while a FLEX emergency\n"
            "  re-synchronization is sent, with its length;\n"
            "- Waiting: messages queued and not sent yet (at most %1, Max);\n"
            "- Sending: the message going out now;\n"
            "- Sent: messages sent since the channel started;\n"
            "- Dropped: messages cleared, aborted or refused by the stream after they were queued;\n"
            "- Refused: requests refused before they were queued, from this window, the Message Center or the WebAPI.\n"
            "Click Waiting, Sent or Dropped to show only those messages in the table, again to show all.\n"
            "Click Sending to show the message on air: the table follows it (scrolling or selecting stops that).%2")
            .arg(m_settings.m_maxQueued)
            .arg(refusedCount > 0 ? tr("\n\nThe last refusal: %1").arg(lastRefusal) : QString()));
    }
}

// A click on a chip: a filter of the table (the same one again: all), or follow the message on air
void RadioPagingModGUI::statusLinkActivated(const QString& link)
{
    if (link == "follow")
    {
        m_messageFilter->setFilterRegularExpression(QString());
        m_follow = true;
        m_onAirShown = -1;      // Shown at the next tick
        return;
    }

    const QString pattern = QString("^%1$").arg(link.mid(7));
    m_messageFilter->setFilterRegularExpression(m_messageFilter->filterRegularExpression().pattern() == pattern ? QString() : pattern);
    m_follow = false;
}

// The table shows the message on air while it follows it
void RadioPagingModGUI::followOnAir()
{
    const int row = m_messageModel->onAirRow();

    if (!m_follow || (row < 0) || (row == m_onAirShown)) {
        return;
    }

    const QModelIndex index = m_messageFilter->mapFromSource(m_messageModel->index(row, 0));

    if (index.isValid())
    {
        m_scrolling = true;
        ui->messages->scrollTo(index, QAbstractItemView::PositionAtCenter);
        m_scrolling = false;
    }

    m_onAirShown = row;
}

// The record of a row, its voice played, the audio output, the columns
void RadioPagingModGUI::messagesContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->messages->indexAt(pos);
    QMenu menu(this);

    if (index.isValid())
    {
        connect(menu.addAction(tr("Details...")), &QAction::triggered, this, [this, index]() { showMessage(index); });
        menu.addSeparator();
    }

    connect(menu.addAction(tr("Audio output of the voice...")), &QAction::triggered, this, [this, pos]()
    {
        AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_voicePlaybackDeviceName, false);
        audioSelect.move(ui->messages->viewport()->mapToGlobal(pos));
        new DialogPositioner(&audioSelect, false);
        audioSelect.exec();

        if (audioSelect.m_selected)
        {
            m_settings.m_voicePlaybackDeviceName = audioSelect.m_audioDeviceName;
            m_voicePlayer->setAudioDevice(m_settings.m_voicePlaybackDeviceName);
            applySettings(QStringList("voicePlaybackDeviceName"));
        }
    });

    menu.exec(ui->messages->viewport()->mapToGlobal(pos));
}

void RadioPagingModGUI::showMessage(const QModelIndex& index)
{
    const QModelIndex source = m_messageFilter->mapToSource(index);

    if (source.isValid())
    {
        const MessageRecord& record = m_messageModel->record(source.row());
        PagingMessageTable::showRecord(QString::fromUtf8(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact)), this);
    }
}

// A click on the message of a voice message plays its voice, or stops it
void RadioPagingModGUI::messageClicked(const QModelIndex& index)
{
    const QModelIndex source = m_messageFilter->mapToSource(index);

    if (!source.isValid() || (source.column() != RadioPagingModMessageModel::ColMessage)) {
        return;
    }

    const QString file = m_messageModel->record(source.row()).m_protocolData.value("voice_file").toString();

    if (file.isEmpty()) {
        return;
    }
    if (m_voicePlayer->isPlaying() && (m_voicePlayer->file() == file))
    {
        m_voicePlayer->stop();
        return;
    }

    QString error;

    if (!m_voicePlayer->play(file, error)) {
        QMessageBox::information(this, tr("Voice message"), error);
    }
}

// The header: a menu of the columns shown
void RadioPagingModGUI::messagesHeaderMenu(const QPoint& pos)
{
    QHeaderView *header = ui->messages->horizontalHeader();
    QMenu menu(this);

    for (int column = 0; column < RadioPagingModMessageModel::ColCount; column++)
    {
        QAction *action = menu.addAction(m_messageModel->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
        action->setCheckable(true);
        action->setChecked(!header->isSectionHidden(column));
        connect(action, &QAction::toggled, this, [header, column](bool shown) { header->setSectionHidden(column, !shown); });
    }

    menu.exec(header->mapToGlobal(pos));
    m_columnsTimer.start();
}

// The table of the messages: on a model, filtered by state (the chips); rows of one line and one height, so that
// tens of thousands of them cost no more than the ones in view
void RadioPagingModGUI::setupMessages()
{
    QTableView *view = ui->messages;

    m_messageModel = new RadioPagingModMessageModel(this);
    m_messageModel->setRetention(1000);     // Finished messages kept, as the received messages of the demodulator
    m_messageFilter = new QSortFilterProxyModel(this);
    m_messageFilter->setSourceModel(m_messageModel);
    m_messageFilter->setFilterRole(RadioPagingModMessageModel::m_stateRole);
    m_messageFilter->setFilterKeyColumn(0);
    view->setModel(m_messageFilter);
    view->setItemDelegate(PagingMessageTable::createHtmlDelegate(view));
    view->setWordWrap(false);
    view->setTextElideMode(Qt::ElideRight);
    view->verticalHeader()->hide();
    view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view->verticalHeader()->setDefaultSectionSize(view->fontMetrics().height() + 6);
    view->horizontalHeader()->setStretchLastSection(true);
    view->horizontalHeader()->setSectionsMovable(true);
    view->setMinimumHeight(view->horizontalHeader()->sizeHint().height()
        + 8 * view->verticalHeader()->defaultSectionSize() + 2 * view->frameWidth());
    view->setToolTip(QString());

    const QFontMetrics fm = view->fontMetrics();
    const QStringList widest = {"000000", "dropped", "2026-11-21 00:00:00", "00:00:00", "Events", "POCSAG 1200",
        "0000000000A", "ALPHA", "QUEUE TEST 000 THE QUICK BROWN FOX"};

    for (int column = 0; column < widest.size(); column++) {
        view->setColumnWidth(column, fm.horizontalAdvance(widest[column]) + 16);
    }

    // Following the message on air stops when the user scrolls or selects a row
    connect(view->verticalScrollBar(), &QScrollBar::actionTriggered, this, [this](int) { m_follow = false; });
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
        if (!m_scrolling && ui->messages->underMouse()) {
            m_follow = false;
        }
    });
    connect(view, &QTableView::pressed, this, [this](const QModelIndex&) { m_follow = false; });
    connect(view, &QTableView::clicked, this, &RadioPagingModGUI::messageClicked);
    connect(view, &QTableView::doubleClicked, this, &RadioPagingModGUI::showMessage);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view, &QWidget::customContextMenuRequested, this, &RadioPagingModGUI::messagesContextMenu);
    view->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view->horizontalHeader(), &QWidget::customContextMenuRequested, this, &RadioPagingModGUI::messagesHeaderMenu);

    // The order, the sizes and the columns shown are saved once they stop changing
    m_columnsTimer.setSingleShot(true);
    m_columnsTimer.setInterval(500);
    connect(&m_columnsTimer, &QTimer::timeout, this, [this]() {
        m_settings.m_messageColumns = messageColumnsTag + ui->messages->horizontalHeader()->saveState();
        applySettings(QStringList("messageColumns"));
    });
    connect(view->horizontalHeader(), &QHeaderView::sectionMoved, this, [this]() { m_columnsTimer.start(); });
    connect(view->horizontalHeader(), &QHeaderView::sectionResized, this, [this]() { m_columnsTimer.start(); });

    m_voicePlayer = new PagingVoicePlayer(this);
}

void RadioPagingModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList("rollupState"));
}

void RadioPagingModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_radioPagingMod->getNumberOfDeviceStreams());
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

        applySettings(QStringList({"useReverseAPI", "reverseAPIAddress", "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex", "title", "rgbColor", "streamIndex"}));
    }

    resetContextMenuType();
}

RadioPagingModGUI::RadioPagingModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::RadioPagingModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_messageModel(nullptr),
    m_messageFilter(nullptr),
    m_voicePlayer(nullptr),
    m_follow(true),
    m_scrolling(false),
    m_onAirShown(-1),
    m_localRefused(0)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modradiopaging/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());

    // The fields of the network settings keep their size, an empty last column takes the width left (as the settings
    // lines of the other channels), instead of fields stretched across the window
    for (QComboBox *combo : ui->networkContainer->findChildren<QComboBox*>()) {
        combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    for (QAbstractSpinBox *spin : ui->networkContainer->findChildren<QAbstractSpinBox*>()) {
        spin->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    ui->networkLayout->setColumnStretch(ui->networkLayout->columnCount(), 1);

    // The log of the refusals, warnings and events: rare, two lines under the table of the messages (it scrolls)
    ui->log->setMaximumHeight(ui->log->fontMetrics().lineSpacing() * 2 + 2 * ui->log->frameWidth() + 8);
    // The state and the queue as chips; Waiting, Sent, Dropped and Sending are links (the filters of the table)
    ui->status->setTextFormat(Qt::RichText);
    ui->status->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(ui->status, &QLabel::linkActivated, this, &RadioPagingModGUI::statusLinkActivated);
    setupMessages();
    // The switch times of POCSAG mixed into FLEX, one per speed: named by their speed, as the roaming fields by their
    // letter
    ui->mixSwitch512->setPrefix("512: ");
    ui->mixSwitch1200->setPrefix("1200: ");
    ui->mixSwitch2400->setPrefix("2400: ");
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_radioPagingMod = (RadioPagingMod*) channelTx;
    m_radioPagingMod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.setBandwidth(channelBandwidth());
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Radio Paging Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_radioPagingMod->setLevelMeter(ui->volumeMeter);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    // The log keeps its last lines
    ui->log->setMaximumBlockCount(1000);
    connect(ui->maxQueued, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_settings.m_maxQueued = value;
        applySettings(QStringList("maxQueued"));
    });

    displayProtocolAvailability();
    fillTimeZones();
    displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

RadioPagingModGUI::~RadioPagingModGUI()
{
    if (m_micRecording) {
        DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(&m_micFifo);
    }

    delete ui;
}

// GSC has no network mode (it is sent one-shot): the network item is not selectable for it
void RadioPagingModGUI::displayProtocolAvailability()
{
    setItemEnabled(ui->mode, (int) RadioPagingModSettings::Network, m_settings.m_protocol != RadioPagingModSettings::Gsc);
}

// Marker bandwidth: twice the deviation plus the symbol rate
int RadioPagingModGUI::channelBandwidth() const
{
    int rate = m_settings.m_pocsagSpeed;

    int bandwidth;

    if (m_settings.m_protocol == RadioPagingModSettings::Gsc) {
        rate = RadioPagingGscTxStream::m_baud;
    }

    if (m_settings.m_protocol == RadioPagingModSettings::Flex)
    {
        rate = RadioPagingFlex::baud((RadioPagingFlex::Mode) m_settings.m_flexMode);
        bandwidth = 2 * ((int) m_settings.m_deviation + rate);

        // Mixed with POCSAG: the wider of the two
        if (m_settings.m_flexMixPocsag && (m_settings.m_mode == RadioPagingModSettings::Network)) {
            bandwidth = std::max(bandwidth, 2 * ((int) m_settings.m_pocsagDeviation + m_settings.m_pocsagSpeed));
        }

        return bandwidth;
    }

    return 2 * ((int) m_settings.m_deviation + rate);
}

// The widgets of the protocol: speeds or modes, polarity of the protocol, the address of its
// messages. Signals are blocked while the combos are refilled
void RadioPagingModGUI::displayProtocol()
{
    bool flex = (m_settings.m_protocol == RadioPagingModSettings::Flex);
    QSignalBlocker speedBlocker(ui->speed);
    QSignalBlocker polarityBlocker(ui->polarity);
    QSignalBlocker collapseBlocker(ui->collapse);
    QSignalBlocker roamingBlocker(ui->roaming);
    QSignalBlocker ssidZoneBlocker(ui->ssidZone);
    QSignalBlocker ssidLocalIdBlocker(ui->ssidLocalId);
    QSignalBlocker ssidCountryBlocker(ui->ssidCountry);
    QSignalBlocker ssidFlagsBlocker(ui->ssidFlags);
    QSignalBlocker voiceSecondsBlocker(ui->voiceSeconds);
    QSignalBlocker mixPocsagBlocker(ui->mixPocsag);
    QSignalBlocker mixSharedBlocker(ui->mixShared);
    QSignalBlocker mixSpeedBlocker(ui->mixSpeed);
    QSignalBlocker mixPolarityBlocker(ui->mixPolarity);
    QSignalBlocker mixDeviationBlocker(ui->mixDeviation);
    QSignalBlocker mixFramesBlocker(ui->mixFrames);
    QSignalBlocker mixSwitch512Blocker(ui->mixSwitch512);
    QSignalBlocker mixSwitch1200Blocker(ui->mixSwitch1200);
    QSignalBlocker mixSwitch2400Blocker(ui->mixSwitch2400);

    // The codepage of the protocol of the channel
    const char *protocolNames[3] = {"pocsag", "flex", "gsc"};
    PagingComposer::fillCodepages(ui->charset, protocolNames[(int) m_settings.m_protocol], false);
    PagingComposer::selectCodepage(ui->charset, m_settings.charset(m_settings.m_protocol));
    ui->charset->setVisible(RadioPagingCharset::codepages(protocolNames[(int) m_settings.m_protocol]).size() > 1);
    ui->charsetLabel->setVisible(ui->charset->isVisibleTo(this));
    // The codepages Default stands for in the composer
    ui->composer->setDefaultCodepage("pocsag", m_settings.m_pocsagCharset);
    ui->composer->setDefaultCodepage("flex", m_settings.m_flexCharset);
    ui->composer->setDefaultCodepage("gsc", m_settings.m_gscCharset);

    ui->speed->clear();

    bool gsc = (m_settings.m_protocol == RadioPagingModSettings::Gsc);

    if (gsc)
    {
        ui->speed->addItems(QStringList({"Auto", "Single", "Batch", "Non battery saver"}));
        ui->speedLabel->setText("Mode");
        ui->speed->setToolTip("GSC transmissions.\n"
            "Auto: a batch of the messages of the same preamble value, a message alone after its own preamble.\n"
            "Single: one message per transmission. Batch: a batch even for one message.\n"
            "Non battery saver: all queued messages of a polarity after one 1100 pattern");
        ui->speed->setCurrentIndex(m_settings.m_gscMode);
        ui->polarity->setCurrentIndex(m_settings.m_gscInverted ? 1 : 0);
        ui->polarity->setToolTip("Default polarity of the messages sent. Normal: binary 1 is a positive deviation");
    }
    else if (flex)
    {
        for (int mode = (int) RadioPagingFlex::Mode1600_2; mode <= (int) RadioPagingFlex::Mode6400_4; mode++) {
            ui->speed->addItem(RadioPagingFlex::modeName((RadioPagingFlex::Mode) mode));
        }

        ui->speedLabel->setText("Mode");
        ui->speed->setToolTip("FLEX mode (bit rate / levels) of the network: every frame is sent in it");
        ui->speed->setCurrentIndex(m_settings.m_flexMode);
        ui->polarity->setCurrentIndex(m_settings.m_flexInverted ? 1 : 0);
        ui->polarity->setToolTip("Polarity of the network. Normal: symbol 3 is the highest frequency.\n"
            "Messages of the other polarity are sent in the frames the network has nothing mandatory in");
    }
    else
    {
        for (int i = 0; i < RadioPagingModSettings::m_nbPocsagSpeeds; i++) {
            ui->speed->addItem(QString::number(RadioPagingModSettings::m_pocsagSpeeds[i]));
        }

        ui->speedLabel->setText("Speed");
        ui->speed->setToolTip("Default POCSAG speed (baud) of the messages sent");
        ui->speed->setCurrentIndex(pocsagSpeedIndex(m_settings.m_pocsagSpeed));
        ui->polarity->setCurrentIndex(m_settings.m_pocsagInverted ? 1 : 0);
        ui->polarity->setToolTip("Default polarity of the messages sent. Normal: binary 1 is a negative deviation");
    }

    ui->voiceSeconds->setValue(m_settings.m_gscVoiceSeconds);
    ui->voiceSecondsLabel->setVisible(gsc);
    ui->voiceSeconds->setVisible(gsc);

    ui->roaming->setVisible(flex);
    ui->ssidZone->setVisible(flex);
    ui->ssidLocalId->setVisible(flex);
    ui->ssidCountry->setVisible(flex);
    ui->ssidFlags->setVisible(flex);
    ui->roaming->setChecked(m_settings.m_flexRoaming);
    ui->ssidZone->setValue(m_settings.m_ssidCoverageZone);
    ui->ssidLocalId->setValue(m_settings.m_ssidLocalId);
    ui->ssidCountry->setValue(m_settings.m_ssidCountryCode);
    ui->ssidFlags->setValue(m_settings.m_ssidTrafficFlags);

    for (QWidget *w : QList<QWidget*>({ui->ssidZone, ui->ssidLocalId, ui->ssidCountry, ui->ssidFlags})) {
        w->setEnabled(m_settings.m_flexRoaming);
    }

    displayTime(flex);
    displayNumbering(flex);

    ui->collapse->setValue(m_settings.m_flexCollapse);
    // Collapse: the re-synchronization length; network: also the schedule of the frames
    bool network = (m_settings.m_mode == RadioPagingModSettings::Network);
    displaySystemEvent(flex, network);
    ui->collapseLabel->setVisible(flex);
    ui->collapse->setVisible(flex);
    ui->collapse->setToolTip(network
        ? "FLEX collapse: pagers listen to one frame in 2^collapse; a message waits for a frame of its capcode.\n"
          "The re-synchronization at the start of the network lasts at least 2^collapse frames"
        : "FLEX collapse of the pagers to reach: the re-synchronization lasts at least 2^collapse frames.\n"
          "The frames after it are sent with collapse 0: the message goes in the first frame");
    // Mixing with POCSAG: a FLEX network option
    bool mixOptions = flex && network;
    ui->mixPocsag->setVisible(mixOptions);
    ui->mixShared->setVisible(mixOptions);
    ui->mixSpeedLabel->setVisible(mixOptions);
    ui->mixSpeed->setVisible(mixOptions);
    ui->mixPolarity->setVisible(mixOptions);
    ui->mixDeviation->setVisible(mixOptions);
    ui->mixFramesLabel->setVisible(mixOptions);
    ui->mixFrames->setVisible(mixOptions);
    ui->mixSwitchLabel->setVisible(mixOptions);
    ui->mixSwitch512->setVisible(mixOptions);
    ui->mixSwitch1200->setVisible(mixOptions);
    ui->mixSwitch2400->setVisible(mixOptions);
    ui->mixPocsag->setChecked(m_settings.m_flexMixPocsag);
    ui->mixShared->setChecked(m_settings.m_mixShared);
    ui->mixSpeed->setCurrentIndex(pocsagSpeedIndex(m_settings.m_pocsagSpeed));
    ui->mixPolarity->setCurrentIndex(m_settings.m_pocsagInverted ? 1 : 0);
    ui->mixDeviation->setValue((int) m_settings.m_pocsagDeviation);
    ui->mixFrames->setMaximum(m_settings.m_mixShared ? 128 : 32);
    ui->mixFrames->setValue(m_settings.m_mixFramesPerMinute);
    ui->mixSwitch512->setValue(m_settings.m_mixSwitchMs[0]);
    ui->mixSwitch1200->setValue(m_settings.m_mixSwitchMs[1]);
    ui->mixSwitch2400->setValue(m_settings.m_mixSwitchMs[2]);
    // The codepage of the POCSAG messages mixed in
    {
        QSignalBlocker mixCharsetBlocker(ui->mixCharset);
        PagingComposer::fillCodepages(ui->mixCharset, "pocsag", false);
        PagingComposer::selectCodepage(ui->mixCharset, m_settings.m_pocsagCharset);
        ui->mixCharset->setVisible(mixOptions && (RadioPagingCharset::codepages("pocsag").size() > 1));
    }

    for (QWidget *w : QList<QWidget*>({ui->mixShared, ui->mixSpeed, ui->mixPolarity, ui->mixDeviation, ui->mixFrames,
        ui->mixSwitch512, ui->mixSwitch1200, ui->mixSwitch2400, ui->mixCharset})) {
        w->setEnabled(m_settings.m_flexMixPocsag);
    }

    // GSC has no network mode: the combo shows one-shot, the setting is kept for the other protocols
    {
        QSignalBlocker modeBlocker(ui->mode);
        ui->mode->setCurrentIndex(gsc ? (int) RadioPagingModSettings::OneShot : (int) m_settings.m_mode);
    }

    displayProtocolAvailability();
    displayMessage();

    // Another protocol shows other fields: the sections are arranged again, and a window too short for them grows (its
    // minimum size follows its contents), else its last sections would be under its bottom bar
    getRollupContents()->arrangeRollups();

    if (!isMaximized() && (getRollupContents()->minimumHeight() + getAdditionalHeight() > height())) {
        sizeToContents();
    }
}
void RadioPagingModGUI::on_mixPocsag_toggled(bool checked)
{
    m_settings.m_flexMixPocsag = checked;
    m_channelMarker.setBandwidth(channelBandwidth());
    blockApplySettings(true);
    displayProtocol();
    blockApplySettings(false);
    applySettings(QStringList("mixPocsag"));
}

void RadioPagingModGUI::on_mixShared_toggled(bool checked)
{
    m_settings.m_mixShared = checked;
    m_settings.m_mixFramesPerMinute = std::min(m_settings.m_mixFramesPerMinute, checked ? 128 : 32);
    blockApplySettings(true);
    displayProtocol();
    blockApplySettings(false);
    applySettings(QStringList({"mixShared", "mixFramesPerMinute"}));
}

void RadioPagingModGUI::on_mixSpeed_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < RadioPagingModSettings::m_nbPocsagSpeeds))
    {
        m_settings.m_pocsagSpeed = RadioPagingModSettings::m_pocsagSpeeds[index];
        m_channelMarker.setBandwidth(channelBandwidth());
        applySettings(QStringList("pocsagSpeed"));
    }
}

void RadioPagingModGUI::on_mixPolarity_currentIndexChanged(int index)
{
    m_settings.m_pocsagInverted = (index == 1);
    applySettings(QStringList("pocsagInverted"));
}

void RadioPagingModGUI::on_mixDeviation_valueChanged(int value)
{
    m_settings.m_pocsagDeviation = value;
    m_channelMarker.setBandwidth(channelBandwidth());
    applySettings(QStringList("pocsagDeviation"));
}

void RadioPagingModGUI::on_mixFrames_valueChanged(int value)
{
    m_settings.m_mixFramesPerMinute = value;
    applySettings(QStringList("mixFramesPerMinute"));
}

void RadioPagingModGUI::on_mixSwitch512_valueChanged(int value)
{
    m_settings.m_mixSwitchMs[0] = value;
    applySettings(QStringList("mixSwitchMs512"));
}

void RadioPagingModGUI::on_mixSwitch1200_valueChanged(int value)
{
    m_settings.m_mixSwitchMs[1] = value;
    applySettings(QStringList("mixSwitchMs1200"));
}

void RadioPagingModGUI::on_mixSwitch2400_valueChanged(int value)
{
    m_settings.m_mixSwitchMs[2] = value;
    applySettings(QStringList("mixSwitchMs2400"));
}

void RadioPagingModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RadioPagingModGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        RadioPagingMod::MsgConfigureRadioPagingMod *msg = RadioPagingMod::MsgConfigureRadioPagingMod::create(settingsKeys, m_settings, force);
        m_radioPagingMod->getInputMessageQueue()->push(msg);
    }
}

void RadioPagingModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(channelBandwidth());
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    // Shown without signalling a change. Before the device has given its sample rate (a configuration being loaded)
    // the range may be narrower than the offset: it is widened to show the offset, and set by the next sample rate
    ui->deltaFrequency->blockSignals(true);
    const qint64 halfRange = std::max<qint64>(m_basebandSampleRate / 2, std::abs((qint64) m_settings.m_inputFrequencyOffset));
    ui->deltaFrequency->setValueRange(false, 7, -halfRange, halfRange);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->deltaFrequency->blockSignals(false);
    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->protocol->setCurrentIndex((int) m_settings.m_protocol);
    ui->mode->setCurrentIndex((int) m_settings.m_mode);
    ui->deviation->setValue((int) m_settings.m_deviation);
    ui->gain->setValue((int) m_settings.m_gain);
    ui->gainText->setText(QString("%1dB").arg((int) m_settings.m_gain));
    displayProtocol();

    // The columns the user chose. Saved with the tag of this table: a state of the table of before is not one of
    // this table, it gives the usual columns
    if (m_settings.m_messageColumns.startsWith(messageColumnsTag))
    {
        ui->messages->horizontalHeader()->restoreState(m_settings.m_messageColumns.mid(messageColumnsTag.size()));
        m_columnsTimer.stop();      // Restored, not changed
    }

    ui->maxQueued->setValue(m_settings.m_maxQueued);
    m_voicePlayer->setAudioDevice(m_settings.m_voicePlaybackDeviceName);
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void RadioPagingModGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void RadioPagingModGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void RadioPagingModGUI::tick()
{
    double powDb = CalcDb::dbPower(m_radioPagingMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    RadioPagingModSource::Status status = m_radioPagingMod->getStatus();
    updateQueueStatus(status);
    followOnAir();
    updateLive(status);
    updateSystemEvent(status);
}

void RadioPagingModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RadioPagingModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &RadioPagingModGUI::on_channelMute_toggled);
    QObject::connect(ui->protocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_protocol_currentIndexChanged);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->charset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        const RadioPagingCharset::Key key = PagingComposer::codepageKey(ui->charset);
        const char *keys[3] = {"pocsagCharset", "flexCharset", "gscCharset"};

        if (m_settings.m_protocol == RadioPagingModSettings::Pocsag) {
            m_settings.m_pocsagCharset = key;
        } else if (m_settings.m_protocol == RadioPagingModSettings::Flex) {
            m_settings.m_flexCharset = key;
        } else {
            m_settings.m_gscCharset = key;
        }

        applySettings(QStringList(keys[(int) m_settings.m_protocol]));
        const char *protocols[3] = {"pocsag", "flex", "gsc"};
        ui->composer->setDefaultCodepage(protocols[(int) m_settings.m_protocol], key);
    });
    QObject::connect(ui->deviation, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_deviation_valueChanged);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &RadioPagingModGUI::on_gain_valueChanged);
    QObject::connect(ui->speed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_speed_currentIndexChanged);
    QObject::connect(ui->polarity, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_polarity_currentIndexChanged);
    QObject::connect(ui->collapse, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_collapse_valueChanged);
    QObject::connect(ui->roaming, &QCheckBox::toggled, this, &RadioPagingModGUI::on_roaming_toggled);
    QObject::connect(ui->ssidZone, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_ssidZone_valueChanged);
    QObject::connect(ui->ssidLocalId, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_ssidLocalId_valueChanged);
    QObject::connect(ui->ssidCountry, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_ssidCountry_valueChanged);
    QObject::connect(ui->ssidFlags, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_ssidFlags_valueChanged);
    QObject::connect(ui->flexTime, &QCheckBox::toggled, this, &RadioPagingModGUI::on_flexTime_toggled);
    QObject::connect(ui->flexTimeReference, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_flexTimeReference_currentIndexChanged);
    QObject::connect(ui->flexTimeDaylightSaving, &QCheckBox::toggled, this, &RadioPagingModGUI::on_flexTimeDaylightSaving_toggled);
    QObject::connect(ui->flexTimeSendZone, &QCheckBox::toggled, this, &RadioPagingModGUI::on_flexTimeSendZone_toggled);
    QObject::connect(ui->flexTimeEquivalentYear, &QCheckBox::toggled, this, &RadioPagingModGUI::on_flexTimeEquivalentYear_toggled);
    QObject::connect(ui->flexTimeExtendedSeconds, QOverload<int>::of(&QSpinBox::valueChanged), this,
        &RadioPagingModGUI::on_flexTimeExtendedSeconds_valueChanged);
    QObject::connect(ui->flexNumbering, &QCheckBox::toggled, this, &RadioPagingModGUI::on_flexNumbering_toggled);
    QObject::connect(ui->flexMaxMessageNumber, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_flexMaxMessageNumber_valueChanged);
    QObject::connect(ui->flexEventSend, &QPushButton::clicked, this, &RadioPagingModGUI::on_flexEventSend_clicked);
    QObject::connect(ui->flexTimeZone, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_flexTimeZone_currentIndexChanged);
    QObject::connect(ui->flexTimeClock, &QCheckBox::toggled, this, &RadioPagingModGUI::on_flexTimeClock_toggled);
    QObject::connect(ui->flexTimeSet, &QDateTimeEdit::editingFinished, this, &RadioPagingModGUI::on_flexTimeSet_editingFinished);
    QObject::connect(ui->voiceSeconds, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_voiceSeconds_valueChanged);
    QObject::connect(ui->mixPocsag, &QCheckBox::toggled, this, &RadioPagingModGUI::on_mixPocsag_toggled);
    QObject::connect(ui->mixShared, &QCheckBox::toggled, this, &RadioPagingModGUI::on_mixShared_toggled);
    QObject::connect(ui->mixSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_mixSpeed_currentIndexChanged);
    QObject::connect(ui->mixPolarity, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RadioPagingModGUI::on_mixPolarity_currentIndexChanged);
    QObject::connect(ui->mixCharset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_settings.m_pocsagCharset = PagingComposer::codepageKey(ui->mixCharset);
        applySettings(QStringList("pocsagCharset"));
        ui->composer->setDefaultCodepage("pocsag", m_settings.m_pocsagCharset);
    });
    QObject::connect(ui->mixDeviation, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_mixDeviation_valueChanged);
    QObject::connect(ui->mixFrames, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_mixFrames_valueChanged);
    QObject::connect(ui->mixSwitch512, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_mixSwitch512_valueChanged);
    QObject::connect(ui->mixSwitch1200, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_mixSwitch1200_valueChanged);
    QObject::connect(ui->mixSwitch2400, QOverload<int>::of(&QSpinBox::valueChanged), this, &RadioPagingModGUI::on_mixSwitch2400_valueChanged);
    QObject::connect(ui->composer, &PagingComposer::sendRequested, this, &RadioPagingModGUI::send);
    QObject::connect(ui->composer, &PagingComposer::microphoneInputRequested, this, [this]() { micAudioSelect(QCursor::pos()); });
    QObject::connect(&m_micFifo, &AudioFifo::dataReady, this, &RadioPagingModGUI::micDataReady);
    QObject::connect(ui->voiceFiles, &QToolButton::clicked, this, &RadioPagingModGUI::on_voiceFiles_clicked);
    QObject::connect(ui->clear, &QPushButton::clicked, this, &RadioPagingModGUI::on_clear_clicked);
    PagingMessageTable::setupMessageCenterButton(ui->openMessageCenter);
    QObject::connect(ui->abort, &QPushButton::clicked, this, &RadioPagingModGUI::on_abort_clicked);
}

void RadioPagingModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
