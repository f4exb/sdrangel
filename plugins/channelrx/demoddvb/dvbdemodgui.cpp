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

#include <algorithm>
#include <iterator>
#include <cmath>

#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDateTime>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QMenu>
#include <QInputDialog>
#include <QFileDialog>
#include <QPainter>
#include <QDebug>
#include <QIntValidator>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QLocale>
#include <QSignalBlocker>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

#include "device/deviceuiset.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "ui_dvbdemodgui.h"
#include "plugin/pluginapi.h"
#include "gui/basicchannelsettingsdialog.h"
#include "channel/channelwebapiutils.h"
#include "webapi/webapiutils.h"
#include "SWGDeviceSettings.h"
#include "gui/audioselectdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "dvbdemod.h"
#include "gui/buttonswitch.h"
#include "gui/glvideowidget.h"
#include "dvbdemodgui.h"
#ifdef DVBDEMOD_HBBTV
#include <QJsonObject>
#include "dtv/dtvstreamplayback.h"
#include "dvbdemodhbbtv.h"
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

const int DVBDemodGUI::m_bandwidths[6] = {10000000, 8000000, 7000000, 6000000, 5000000, 1700000}; // As the list

DVBDemodGUI* DVBDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    DVBDemodGUI* gui = new DVBDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void DVBDemodGUI::destroy()
{
    delete this;
}

void DVBDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applyAllSettings();
}

QByteArray DVBDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DVBDemodGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
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

bool DVBDemodGUI::handleMessage(const Message& message)
{
    if (DVBDemod::MsgConfigureDVBDemod::match(message))
    {
        qDebug("DVBDemodGUI::handleMessage: DVBDemod::MsgConfigureDVBDemod");
        const DVBDemod::MsgConfigureDVBDemod& cfg = (DVBDemod::MsgConfigureDVBDemod&) message;

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
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;

        if (notif.getCenterFrequency() != m_deviceCenterFrequency) {
            clearStreamList();
        }

        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        updateAbsoluteCenterFrequency();
        return true;
    }

    return false;
}

void DVBDemodGUI::handleInputMessages()
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

void DVBDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySetting("inputFrequencyOffset");
}

void DVBDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DVBDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    clearStreamList();
    applySetting("inputFrequencyOffset");
}

void DVBDemodGUI::on_bandwidth_currentIndexChanged(int index)
{
    if ((index < 0) || (index >= (int) std::size(m_bandwidths))) {
        return;
    }

    m_settings.m_bandwidth = m_bandwidths[index];
    m_channelMarker.setBandwidth(m_settings.occupiedBandwidth());
    applySetting("bandwidth");
}

void DVBDemodGUI::on_standard_currentIndexChanged(int index)
{
    m_settings.m_standard = index;
    clearStreamList();
    updateStandardWidgets();
    m_channelMarker.setBandwidth(m_settings.occupiedBandwidth());
    applySetting("standard");
}

void DVBDemodGUI::on_plp_currentIndexChanged(int index)
{
    if (m_listingISIs)
    {
        m_settings.m_isi = index <= 0 ? -1 : m_isiIds.value(index - 1, -1);
        applySetting("isi");
        return;
    }

    m_settings.m_plpId = index <= 0 ? -1 : m_plpIds.value(index - 1, -1);
    applySetting("plpId");
}

void DVBDemodGUI::on_symbolRate_editingFinished()
{
    bool ok;
    const QString text = ui->symbolRate->currentText().trimmed();
    const bool automatic = text.compare("Auto", Qt::CaseInsensitive) == 0;
    const int symbolRate = automatic ? 0 : text.toInt(&ok);

    if ((!automatic && !ok) || !DVBDemodSettings::isValidSymbolRate(symbolRate))
    {
        ui->symbolRate->setEditText(m_settings.m_symbolRate > 0 ? QString::number(m_settings.m_symbolRate) : QString("Auto"));
        return;
    }

    if (symbolRate != m_settings.m_symbolRate)
    {
        m_settings.m_symbolRate = symbolRate;
        m_channelMarker.setBandwidth(m_settings.occupiedBandwidth());
        applySetting("symbolRate");
    }
}

void DVBDemodGUI::on_rollOff_currentIndexChanged(int index)
{
    static const int rollOffs[] = {0, 35, 25, 20, 15, 10, 5};

    if ((index >= 0) && (index < 7))
    {
        m_settings.m_rollOff = rollOffs[index];
        m_channelMarker.setBandwidth(m_settings.occupiedBandwidth());
        applySetting("rollOff");
    }
}

void DVBDemodGUI::on_codeRate_currentIndexChanged(int index)
{
    m_settings.m_codeRate = index - 1;
    applySetting("codeRate");
}

void DVBDemodGUI::on_plScrambling_valueChanged(int value)
{
    m_settings.m_plScramblingCode = value;
    applySetting("plScramblingCode");
}

void DVBDemodGUI::on_equaliser_toggled(bool checked)
{
    m_settings.m_equaliser = checked;
    applySetting("equaliser");
}

// The terrestrial standards' settings, or the satellite ones'
void DVBDemodGUI::updateStandardWidgets()
{
    const int standard = m_settings.m_standard;
    const bool satellite = DVBDemodSettings::isSatellite(standard);
    ui->bandwidthLabel->setVisible(!satellite);
    ui->bandwidth->setVisible(!satellite);
    ui->priority->setVisible((standard == 0) || (standard == 1));
    ui->symbolRateLabel->setVisible(satellite);
    ui->symbolRate->setVisible(satellite);
    ui->rollOff->setVisible((standard == 4) || (standard == 6));
    ui->codeRate->setVisible((standard == 4) || (standard == 5));
    ui->plScrambling->setVisible((standard == 4) || (standard == 6));
    ui->equaliser->setVisible(satellite);
    const bool plp = (standard == 0) || (standard == 2) || (standard == 3);
    const bool isi = (standard == 4) || (standard == 6);
    ui->plpLabel->setVisible(plp || isi);
    ui->plp->setVisible(plp || isi);
    ui->plpLabel->setText(isi ? "ISI" : "PLP");
    ui->scan->setVisible(satellite);

    if (isi != m_listingISIs)
    {
        m_listingISIs = isi;
        fillStreamList();
    }

    ui->scope->setSpectrum(satellite);
}

void DVBDemodGUI::clearStreamList()
{
    m_plpIds.clear();
    m_isiIds.clear();
    fillStreamList(); // The setting's kept
}

// The PLP combo box: Auto, then the PLPs (or DVB-S2's input streams) known, the setting selected
void DVBDemodGUI::fillStreamList()
{
    const QList<int>& ids = m_listingISIs ? m_isiIds : m_plpIds;
    const int selected = m_listingISIs ? m_settings.m_isi : m_settings.m_plpId;
    ui->plp->blockSignals(true);
    ui->plp->clear();
    ui->plp->addItem("Auto");

    for (int id : ids) {
        ui->plp->addItem(QString::number(id));
    }

    if ((selected >= 0) && !ids.contains(selected))
    {
        (m_listingISIs ? m_isiIds : m_plpIds).append(selected);
        ui->plp->addItem(QString::number(selected));
    }

    const QList<int>& now = m_listingISIs ? m_isiIds : m_plpIds;
    ui->plp->setCurrentIndex(selected < 0 ? 0 : now.indexOf(selected) + 1);
    ui->plp->blockSignals(false);
}

void DVBDemodGUI::on_priority_currentIndexChanged(int index)
{
    m_settings.m_lowPriority = index == 1;
    applySetting("lowPriority");
}

void DVBDemodGUI::on_udpEnabled_toggled(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySetting("udpEnabled");
}

void DVBDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySetting("udpAddress");
}

void DVBDemodGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySetting("udpPort");
}

void DVBDemodGUI::on_videoEnabled_toggled(bool checked)
{
    m_settings.m_videoEnabled = checked;
    applySetting("videoEnabled");

    if (checked)
    {
        startPlayer();
    }
    else
    {
        m_streamPlaying = false;    // An application's media, and the broadcast, stopped
        stopStreamPlayback();
        stopPlayer();
    }
}

// What's on now and next on the service playing (or selected), from the programme guide, in the channel's status
// (at the bottom of the window), unless a search for a multiplex is saying what it's doing, or why it failed
void DVBDemodGUI::displayNowNext()
{
    const int serviceId = m_playingService != 0 ? m_playingService : m_settings.m_serviceId;
    DVBDemodEPG::Event now, next;
    const bool message = m_dvbDemod->isSearching()
        || (m_statusMessageTimer.isValid() && (m_statusMessageTimer.elapsed() < 5000));

    if ((serviceId == 0) || !m_dvbDemod->getEPG()->getNowNext(serviceId, now, next))
    {
        if (!message)
        {
            setStatusText(serviceId != 0 ? serviceText(serviceId) : QString());
            setStatusToolTip("The programme on now and next, from the programme guide");
        }

        return;
    }

    auto time = [](qint64 utc) { return QDateTime::fromSecsSinceEpoch(utc).toLocalTime().toString("hh:mm"); };
    QStringList parts;

    if (now.isValid()) {
        parts.append(QString("%1 - %2  %3").arg(time(now.m_start)).arg(time(now.end())).arg(now.m_title));
    }

    if (next.isValid()) {
        parts.append(QString("Next: %1  %2").arg(time(next.m_start)).arg(next.m_title));
    }

    if (!message)
    {
        setStatusText(parts.join("   "));
        setStatusToolTip(now.isValid() ? QString("%1\n\n%2").arg(now.m_title).arg(now.m_text) : next.m_text);
    }

    // A new programme: show it, as when the service is selected
    if (now.isValid() && (now.m_title != m_nowTitle) && m_playingService) {
        showServiceOverlay();
    }
}

void DVBDemodGUI::on_record_toggled(bool checked)
{
    if (!checked)
    {
        m_dvbDemod->stopRecording();
        displayRecordButton(!m_dvbDemod->getRecordings().empty());
        return;
    }

    QString error;

    if (!m_dvbDemod->startRecording(error))
    {
        ui->record->blockSignals(true);
        ui->record->setChecked(false);
        ui->record->blockSignals(false);
        QMessageBox::warning(this, "Record", QString("Can't record: %1").arg(error));
    }

    displayRecordButton(!m_dvbDemod->getRecordings().empty());
}

// Red while anything is being recorded (manually, or a programme from the guide), as in the File Sink
void DVBDemodGUI::displayRecordButton(bool recording)
{
    const QString styleSheet = recording ? QString("QToolButton { background-color : red; }") : QString();

    if (ui->record->styleSheet() != styleSheet) {
        ui->record->setStyleSheet(styleSheet);
    }
}

// What's recorded, where, and how much of the programmes from the guide either side
void DVBDemodGUI::recordMenu(const QPoint& position)
{
    QMenu menu(this);
    QAction *service = menu.addAction("Service selected");
    QAction *multiplex = menu.addAction("Whole multiplex");
    service->setCheckable(true);
    multiplex->setCheckable(true);
    service->setChecked(m_settings.m_recordMode == 1);
    multiplex->setChecked(m_settings.m_recordMode == 0);
    menu.addSeparator();
    QAction *directory = menu.addAction(QString("Directory: %1...").arg(m_settings.m_recordDirectory));
    QAction *padding = menu.addAction(QString("Programmes from the guide: start %1 min early, end %2 min late...")
        .arg(m_settings.m_recordStartPadding / 60).arg(m_settings.m_recordEndPadding / 60));
    QAction *follow = menu.addAction("Follow programmes that start late or over-run (by their running status)");
    follow->setCheckable(true);
    follow->setChecked(m_settings.m_recordFollowRunning);
    QAction *retune = menu.addAction("Retune for programmes on other multiplexes, and back afterwards");
    retune->setCheckable(true);
    retune->setChecked(m_settings.m_recordRetune);
    QAction *chosen = menu.exec(ui->record->mapToGlobal(position));

    if (chosen == follow)
    {
        m_settings.m_recordFollowRunning = follow->isChecked();
        applySetting("recordFollowRunning");
        return;
    }

    if (chosen == retune)
    {
        m_settings.m_recordRetune = retune->isChecked();
        applySetting("recordRetune");
        return;
    }

    if ((chosen == service) || (chosen == multiplex))
    {
        m_settings.m_recordMode = chosen == service ? 1 : 0;
        applySetting("recordMode");
    }
    else if (chosen == directory)
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Directory for recordings", m_settings.m_recordDirectory);

        if (!dir.isEmpty())
        {
            m_settings.m_recordDirectory = dir;
            applySetting("recordDirectory");
        }
    }
    else if (chosen == padding)
    {
        bool ok;
        int before = QInputDialog::getInt(this, "Recording programmes", "Minutes to start before the programme",
            m_settings.m_recordStartPadding / 60, 0, 60, 1, &ok);

        if (!ok) {
            return;
        }

        int after = QInputDialog::getInt(this, "Recording programmes", "Minutes to continue after the programme",
            m_settings.m_recordEndPadding / 60, 0, 120, 1, &ok);

        if (!ok) {
            return;
        }

        m_settings.m_recordStartPadding = before * 60;
        m_settings.m_recordEndPadding = after * 60;
        applySettings({"recordStartPadding", "recordEndPadding"});
    }
}

void DVBDemodGUI::displayRecordings()
{
    const std::vector<DVBDemodSink::Recording> recordings = m_dvbDemod->getRecordings();
    const int scheduled = m_dvbDemod->getScheduledRecordings().size();
    ui->record->blockSignals(true);
    ui->record->setChecked(m_dvbDemod->isRecording());
    ui->record->blockSignals(false);
    displayRecordButton(!recordings.empty());

    if (recordings.empty())
    {
        ui->recordStatus->setText(scheduled > 0 ? QString("%1 to record").arg(scheduled) : QString());
        ui->recordStatus->setToolTip(scheduled > 0 ? QString("Programmes from the guide to record") : QString());
        return;
    }

    qint64 bytes = 0;
    double seconds = 0.0;
    QStringList files;

    for (const auto& recording : recordings)
    {
        const DTVRecorder::Status& s = recording.m_status;
        bytes += s.m_bytes;
        seconds = std::max(seconds, s.m_seconds);
        QString line = QString("%1 (%2 MB)").arg(s.m_fileName).arg(s.m_bytes / 1e6, 0, 'f', 1);

        if (s.m_dropped > 0) {
            line += QString(" - %1 MB lost, as the disk didn't keep up").arg(s.m_dropped / 1e6, 0, 'f', 1);
        }

        if (!s.m_error.isEmpty()) {
            line += " - " + s.m_error;
        }

        files.append(line);
    }

    const int t = (int) seconds;
    QString text = QString("REC %1 %2:%3:%4").arg(bytes >= 1000000000 ? QString("%1 GB").arg(bytes / 1e9, 0, 'f', 2) : QString("%1 MB").arg(bytes / 1e6, 0, 'f', 0))
        .arg(t / 3600).arg((t / 60) % 60, 2, 10, QChar('0')).arg(t % 60, 2, 10, QChar('0'));

    if (recordings.size() > 1) {
        text += QString(" (%1)").arg(recordings.size());
    }

    ui->recordStatus->setText(text);
    ui->recordStatus->setToolTip(files.join("\n") + (scheduled > 0 ? QString("\n%1 programmes to record").arg(scheduled) : QString()));
}

// The range to scan, the device's band by default (the transponders found go into the guide); or stop scanning
void DVBDemodGUI::on_scan_clicked()
{
    if (m_dvbDemod->isScanning())
    {
        m_dvbDemod->cancelScan();
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Scan for transponders");
    QFormLayout *form = new QFormLayout(&dialog);
    double fromMHz, toMHz;
    defaultScanRange(fromMHz, toMHz);
    QDoubleSpinBox *from = new QDoubleSpinBox(&dialog);
    QDoubleSpinBox *to = new QDoubleSpinBox(&dialog);

    for (QDoubleSpinBox *box : {from, to})
    {
        box->setRange(0.0, 100000.0);
        box->setDecimals(3);
        box->setSuffix(" MHz");
    }

    from->setValue(fromMHz);
    to->setValue(toMHz);
    from->setToolTip("Downlink frequency the scan starts at (as the device's, with its transverter delta). The last scan's, or the LNB's band");
    to->setToolTip("Downlink frequency it ends at. A range wider than the device's band is scanned in steps");
    form->addRow("From", from);
    form->addRow("To", to);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString error;
    m_settings.m_scanFrom = (qint64) std::llround(from->value() * 1e6);
    m_settings.m_scanTo = (qint64) std::llround(to->value() * 1e6);
    applySettings({"scanFrom", "scanTo"});

    if (!m_dvbDemod->startScan(m_settings.m_scanFrom, m_settings.m_scanTo, error)) {
        QMessageBox::warning(this, "Scan", error);
    }
}

// The scan's default range (MHz, downlink, as the device's frequency): the last scan's, else what the LNB gives, from
// its local oscillator (the device's transverter delta). A universal LNB's band (10.7 to 11.7 GHz with its 9.75 GHz
// oscillator, 11.7 to 12.75 with its 10.6), else the Ku band's within a wideband IF (all of it with a Sky Q LNB's
// 10.41). Without a transverter, below 2.4 GHz, a satellite receiver's IF, 950 to 2150 MHz. Else the device's band
void DVBDemodGUI::defaultScanRange(double& from, double& to)
{
    if ((m_settings.m_scanFrom > 0) && (m_settings.m_scanTo > m_settings.m_scanFrom))
    {
        from = m_settings.m_scanFrom / 1e6;
        to = m_settings.m_scanTo / 1e6;
        return;
    }

    const double centre = m_deviceCenterFrequency / 1e6;
    // The delta's more than an int holds, so read as a double
    double transverterMode = 0.0, delta = 0.0;
    SWGSDRangel::SWGDeviceSettings deviceSettings;
    DeviceSet *deviceSet;

    if (ChannelWebAPIUtils::getDeviceSettings(m_dvbDemod->getDeviceSetIndex(), deviceSettings, deviceSet))
    {
        QJsonObject *json = deviceSettings.asJsonObject();

        if (WebAPIUtils::getSubObjectDouble(*json, "transverterMode", transverterMode) && (transverterMode != 0.0)) {
            WebAPIUtils::getSubObjectDouble(*json, "transverterDeltaFrequency", delta);
        }

        delete json;
    }

    const double lo = delta / 1e6;

    if ((centre >= 10700.0) && (centre <= 12750.0) && (lo > 9000.0))
    {
        if (std::abs(lo - 9750.0) < 50.0)
        {
            from = 10700.0;
            to = 11700.0;
        }
        else if (std::abs(lo - 10600.0) < 50.0)
        {
            from = 11700.0;
            to = 12750.0;
        }
        else
        {
            from = std::max(10700.0, lo + 250.0);
            to = std::min(12750.0, lo + 2350.0);
        }
    }
    else if ((transverterMode == 0.0) && (centre >= 900.0) && (centre <= 2400.0))
    {
        from = 950.0;
        to = 2150.0;
    }
    else
    {
        const double half = 0.4 * m_basebandSampleRate / 1e6;
        from = centre - half;
        to = centre + half;
    }
}

void DVBDemodGUI::on_epg_clicked()
{
    if (!m_epgDialog)
    {
        m_epgDialog = new DTVEPGDialog(m_dvbDemod->getEPG(), m_dvbDemod, this);
        m_epgDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_epgDialog, &DTVEPGDialog::serviceSelected, this, &DVBDemodGUI::epgServiceSelected);
    }

    m_epgDialog->setCurrentService(m_playingService != 0 ? m_playingService : m_settings.m_serviceId);
    m_epgDialog->show();
    m_epgDialog->raise();
    m_epgDialog->activateWindow();
}

void DVBDemodGUI::on_apps_clicked()
{
    if (!m_appsDialog)
    {
        m_appsDialog = new DTVAppsDialog(m_dvbDemod->getApps(), this);
        m_appsDialog->setAttribute(Qt::WA_DeleteOnClose);
    }

    updateAppsDialog();
    m_appsDialog->show();
    m_appsDialog->raise();
    m_appsDialog->activateWindow();
}

// Whether the service's HbbTV applications are run (over the video)
void DVBDemodGUI::appsMenu(const QPoint& position)
{
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    QAction *run = menu.addAction("Run HbbTV applications (red button)");
    run->setCheckable(true);
    run->setChecked(m_settings.m_hbbtv);
    run->setToolTip("Run the AUTOSTART application of the service played, and those it starts (as when the red button's pressed): from its "
        "carousels or the internet. The coloured buttons are R, G, Y and B (or F1 to F4), and Back is Backspace");
#ifndef DVBDEMOD_HBBTV
    run->setEnabled(false);
    run->setToolTip("Not built: Qt WebEngine is needed");
#endif

    QAction *clear = menu.addAction("Forget HbbTV sign-ins (cookies and cache)");
    clear->setToolTip("Applications' cookies (as iPlayer's sign-in) and cache are kept from one run to the next: forget them");
#ifndef DVBDEMOD_HBBTV
    clear->setEnabled(false);
#endif
    QAction *chosen = menu.exec(ui->apps->mapToGlobal(position));

    if (chosen == run)
    {
        m_settings.m_hbbtv = run->isChecked();
        applySetting("hbbtv");
        updateHbbTV();
    }
#ifdef DVBDEMOD_HBBTV
    else if (chosen == clear)
    {
        DVBDemodHbbTV::clearData();
    }
#endif
}

void DVBDemodGUI::updateHbbTV()
{
#ifdef DVBDEMOD_HBBTV
    const bool run = m_settings.m_hbbtv && (m_player || m_streamPlaying);

    if (run && !m_hbbtv)
    {
        m_hbbtv = new DVBDemodHbbTV(m_dvbDemod->getApps(), this);
        m_hbbtv->createView(m_videoScreen.load());
        connect(m_hbbtv, &DVBDemodHbbTV::videoAreaChanged, this, &DVBDemodGUI::hbbtvVideoArea);
        connect(m_hbbtv, &DVBDemodHbbTV::channelChangeRequested, this, &DVBDemodGUI::hbbtvChannelChange);
        connect(m_hbbtv, &DVBDemodHbbTV::mediaRequest, this, &DVBDemodGUI::hbbtvMedia);
        connect(m_hbbtv, &DVBDemodHbbTV::channelStepRequested, this, [this](int direction) {
            QKeyEvent key(QEvent::KeyPress, direction > 0 ? Qt::Key_ChannelUp : Qt::Key_ChannelDown, Qt::NoModifier);
            m_guideOverlay->handleKey(&key);
        });
        connect(m_hbbtv, &DVBDemodHbbTV::log, this, [](const QString& message) {
            qDebug("DVBDemodGUI: HbbTV: %s", qPrintable(message));
        });
        qDebug("DVBDemodGUI: HbbTV: user agent %s", qPrintable(m_hbbtv->getUserAgent()));
        m_hbbtvServiceState.clear();
        updateHbbTVGeometry();
        updateHbbTVService();
    }
    else if (!run && m_hbbtv)
    {
        delete m_hbbtv;
        m_hbbtv = nullptr;
        hbbtvVideoArea(QRectF());
        m_hbbtvChannelRequest = 0;
        stopStreamPlayback();
    }
#endif
}

// The service played, the multiplex's others, and what's on: given to the application when they change
void DVBDemodGUI::updateHbbTVService()
{
#ifdef DVBDEMOD_HBBTV
    if (!m_hbbtv) {
        return;
    }

    const int serviceId = m_playingService != 0 ? m_playingService : m_dvbDemod->currentServiceId();
    DVBDemodHbbTV::Channel channel;
    std::vector<DVBDemodHbbTV::Channel> channels;
    std::vector<DVBDemodHbbTV::Programme> programmes;
    QString state = QString::number(serviceId);

    for (const DVBDemodEPG::Service& service : m_dvbDemod->getEPG()->getServices())
    {
        if (!service.m_actual) {
            continue;
        }

        DVBDemodHbbTV::Channel c;
        c.m_networkId = service.m_networkId;
        c.m_transportStreamId = service.m_transportStreamId;
        c.m_serviceId = service.m_serviceId;
        c.m_lcn = service.m_channel;
        c.m_name = service.m_name;
        c.m_radio = (service.m_type == 0x02) || (service.m_type == 0x07) || (service.m_type == 0x0A);
        c.m_hd = (service.m_type == 0x11) || ((service.m_type >= 0x19) && (service.m_type <= 0x1F));
        channels.push_back(c);
        state += QString(";%1.%2.%3:%4:%5").arg(c.m_networkId).arg(c.m_transportStreamId).arg(c.m_serviceId).arg(c.m_lcn).arg(c.m_name);

        if (c.m_serviceId == serviceId) {
            channel = c;
        }
    }

    // Before the guide has the service, it's known by its id, of the multiplex received
    if ((channel.m_serviceId == 0) && (serviceId != 0)) {
        channel.m_serviceId = serviceId;
    }

    DVBDemodEPG::Event now, next;

    if ((serviceId != 0) && m_dvbDemod->getEPG()->getNowNext(serviceId, now, next))
    {
        for (const DVBDemodEPG::Event& event : {now, next})
        {
            if (event.isValid())
            {
                programmes.push_back(DVBDemodHbbTV::Programme{event.m_eventId, event.m_title, event.m_text, event.m_start, event.m_duration});
                state += QString(";%1").arg(event.m_eventId);
            }
        }
    }

    if (state == m_hbbtvServiceState) {
        return;
    }

    m_hbbtvServiceState = state;
    const int standard = m_settings.m_standard;
    m_hbbtv->setIdType(standard == DVBDemodSettings::StandardDVBS ? 11 : standard == DVBDemodSettings::StandardDVBS2 ? 15 : standard == DVBDemodSettings::StandardAutoSatellite ? 15
        : (standard == DVBDemodSettings::StandardDVBT2) || (standard == DVBDemodSettings::StandardDVBT2Lite) ? 16 : 12);
    m_hbbtv->setService(channel, channels, programmes);
#endif
}

// Applications are laid out for a 16:9 page, of 1280 by 720 pixels: the largest that fits on the screen, centred, as
// the picture is
QRect DVBDemodGUI::hbbtvPageRect(const QWidget *screen) const
{
    const int width = screen->width();
    const int height = screen->height();
    int w = width, h = (width * 9) / 16;

    if (h > height)
    {
        h = height;
        w = (height * 16) / 9;
    }

    return QRect((width - w) / 2, (height - h) / 2, w, h);
}

void DVBDemodGUI::updateHbbTVGeometry()
{
#ifdef DVBDEMOD_HBBTV
    if (!m_hbbtv) {
        return;
    }

    GLVideoWidget *screen = m_videoScreen.load();
    m_hbbtv->setViewParent(screen);
    m_hbbtv->setViewGeometry(hbbtvPageRect(screen));
    hbbtvVideoArea(m_hbbtvVideoArea);
#endif
}

// The video in the application's window: its area of the page, as an area of the screen
void DVBDemodGUI::hbbtvVideoArea(const QRectF& area)
{
    m_hbbtvVideoArea = area;
    GLVideoWidget *screen = m_videoScreen.load();
    GLVideoWidget *other = screen == ui->screenTV ? m_fullScreenTV : ui->screenTV;

    if (other) {
        other->setVideoArea(QRectF());
    }

    if (area.isNull() || (screen->width() <= 0) || (screen->height() <= 0))
    {
        screen->setVideoArea(QRectF());
        return;
    }

    const QRect page = hbbtvPageRect(screen);
    screen->setVideoArea(QRectF((page.x() + area.x() * page.width()) / screen->width(), (page.y() + area.y() * page.height()) / screen->height(),
        area.width() * page.width() / screen->width(), area.height() * page.height() / screen->height()));
}

// The application's video/broadcast setChannel(): a service of this multiplex, or another the guide knows of
void DVBDemodGUI::hbbtvChannelChange(int networkId, int transportStreamId, int serviceId)
{
#ifdef DVBDEMOD_HBBTV
    (void) transportStreamId;

    if (serviceId == m_playingService)
    {
        m_hbbtv->channelChangeResult(true);
        return;
    }

    m_hbbtvChannelRequest = serviceId;
    selectService(networkId, serviceId, this, [this](const QString& error) {
        qDebug("DVBDemodGUI::hbbtvChannelChange: %s", qPrintable(error));
        m_hbbtvChannelRequest = 0;

        if (m_hbbtv) {
            m_hbbtv->channelChangeResult(false);
        }
    });
#else
    (void) networkId;
    (void) transportStreamId;
    (void) serviceId;
#endif
}

// The red button: given to the application, or the service's started again for it. The video gets the keys after
void DVBDemodGUI::hbbtvRed()
{
#ifdef DVBDEMOD_HBBTV
    if (m_hbbtv)
    {
        m_hbbtv->pressRed();
        m_hbbtv->takeFocus();
    }
#endif
}

// Red buttons, on the ribbon, guide and here, while the service played has an HbbTV application to run
void DVBDemodGUI::displayRedButton()
{
    bool shown = false;
#ifdef DVBDEMOD_HBBTV
    shown = m_hbbtv && m_hbbtv->hasAutostartApp();
#endif
    m_guideOverlay->setRedButton(shown);
    ui->redButton->setVisible(shown);
}

// The application's media, as iPlayer's: played by a player of its own, in place of the broadcast's (stopped while
// it plays, and started again after), in the application's video window
void DVBDemodGUI::hbbtvMedia(const QString& command, int id, const QString& url, const QMap<QString, QString>& options, double position)
{
#ifdef DVBDEMOD_HBBTV
    if (command == "load")
    {
        if (!m_streamPlayback)
        {
            m_streamPlayback = new DTVStreamPlayback(m_dvbDemod->getAudioFifo(),
                [this](GLVideoFramePtr frame) { m_videoScreen.load()->setFrame(frame); },
                [this](int type) { return m_videoScreen.load()->canShow((GLVideoFrame::Type) type); },
                [this](DTVVideoPlayer *player) {
                    player->setAudioMute(m_settings.m_audioMute);
                    player->setAudioVolume(m_settings.m_audioVolume);
                    player->setDeinterlace(m_settings.m_deinterlace);
                    player->setHardwareDecoding(m_settings.m_hardwareDecoding);
                    player->setAVOffset(m_settings.m_avOffset);
                },
                this);
            connect(m_streamPlayback, &DTVStreamPlayback::metadata, this, [this](double duration, int width, int height) {
                if (m_hbbtv)
                {
                    QJsonObject data;
                    data.insert("duration", std::isfinite(duration) ? QJsonValue(duration) : QJsonValue());
                    data.insert("width", width);
                    data.insert("height", height);
                    m_hbbtv->mediaEvent(m_mediaId, "metadata", data);
                }
            });
            connect(m_streamPlayback, &DTVStreamPlayback::playing, this, [this]() {
                if (m_hbbtv) {
                    m_hbbtv->mediaEvent(m_mediaId, "playing");
                }
            });
            connect(m_streamPlayback, &DTVStreamPlayback::ended, this, [this]() {
                if (m_hbbtv) {
                    m_hbbtv->mediaEvent(m_mediaId, "ended");
                }
            });
            connect(m_streamPlayback, &DTVStreamPlayback::error, this, [this](const QString& message) {
                qDebug("DVBDemodGUI: HbbTV: media error: %s", qPrintable(message));

                if (m_hbbtv)
                {
                    QJsonObject data;
                    data.insert("code", 2);     // MEDIA_ERR_NETWORK
                    data.insert("message", message);
                    m_hbbtv->mediaEvent(m_mediaId, "error", data);
                }
            });
        }

        m_mediaId = id;

        if (!m_streamPlaying)
        {
            m_streamPlaying = true;
            stopPlayer();
            showVideoMessage("");
        }

        m_streamPlayback->load(url, options);
    }
    else if (m_streamPlayback && (id == m_mediaId))
    {
        if (command == "play") {
            m_streamPlayback->play();
        } else if (command == "pause") {
            m_streamPlayback->pause();
        } else if (command == "seek") {
            m_streamPlayback->seek(position);
        } else if (command == "stop") {
            stopStreamPlayback();
        }
    }
#else
    (void) command;
    (void) id;
    (void) url;
    (void) options;
    (void) position;
#endif
}

void DVBDemodGUI::stopStreamPlayback()
{
#ifdef DVBDEMOD_HBBTV
    if (m_streamPlayback) {
        m_streamPlayback->stop();
    }

    m_mediaId = 0;

    if (m_streamPlaying)
    {
        m_streamPlaying = false;
        m_videoScreen.load()->clear();

        if (m_settings.m_videoEnabled) {
            startPlayer();
        }
    }
#endif
}

// The applications dialog shows the service whose carousels are received: the one played
void DVBDemodGUI::updateAppsDialog()
{
    if (!m_appsDialog) {
        return;
    }

    const int serviceId = m_playingService != 0 ? m_playingService : m_dvbDemod->currentServiceId();
    const int index = ui->service->findData(serviceId);
    m_appsDialog->setService(serviceId, (serviceId != 0) && (index >= 0) ? ui->service->itemText(index) : QString());
}

// Play a service chosen in the programme guide, retuning to its multiplex if it's on another
void DVBDemodGUI::epgServiceSelected(int networkId, int serviceId)
{
    QPointer<DTVEPGDialog> dialog = m_epgDialog;

    selectService(networkId, serviceId, m_epgDialog, [this, dialog](const QString& error) {
        QMessageBox::warning(dialog ? (QWidget *) dialog : (QWidget *) this, "Programme guide", QString("Can't play the service: %1").arg(error));
    });
}

// Play a service chosen in the guide over the video, saying why not over full screen video, as a message box would
// be behind it (or in one, if full screen was left to ask about the polarisation)
void DVBDemodGUI::guideServiceSelected(int networkId, int serviceId)
{
    selectService(networkId, serviceId, m_videoFullScreen ? nullptr : this, [this](const QString& error) {
        const QString text = QString("Can't play the service: %1").arg(error);

        if (m_videoFullScreen && m_fullScreenTV) {
            m_fullScreenTV->setOverlay(text, 5.0);
        } else {
            QMessageBox::warning(this, "Programme guide", text);
        }
    });
}

// EN 300 468's polarisations
QString DVBDemodGUI::polarisationName(int polarisation)
{
    static const char *names[4] = {"horizontal", "vertical", "left hand circular", "right hand circular"};
    return names[polarisation & 3];
}

// Play a service, retuning to its multiplex if it's on another. On a satellite multiplex on another polarisation than
// the one received, the LNB may need switching (unless it gives both, as to a multiswitch): by the user's command, if
// they've set one, else they're asked whether to tune to it anyway, in a message box over parent (null over full
// screen video, which is left, as it would hide the box). failed: why it isn't played
void DVBDemodGUI::selectService(int networkId, int serviceId, QWidget *parent, const std::function<void(const QString&)>& failed)
{
    auto play = [this, networkId, serviceId, failed]() {
        QString error;

        if (!m_dvbDemod->playService(networkId, serviceId, error)) {
            failed(error);
        }
    };

    int needed, current;

    if (!m_dvbDemod->polarisationChange(networkId, serviceId, needed, current))
    {
        play();
        return;
    }

    const QString command = m_settings.m_polarisationCommand.trimmed();

    if (!command.isEmpty() && (command != m_polarisationCommandConfirmed))
    {
        if (!parent)
        {
            setVideoFullScreen(false);
            parent = this;
        }

        const QMessageBox::StandardButton answer = QMessageBox::question(parent, "Polarisation switching command",
            QString("Run this command to switch the LNB to %1 polarisation?\n\n%2\n\nIt's in this channel's settings (perhaps from a preset or "
                "configuration), and hasn't been run yet. It can be changed from the guide button's right-click menu.")
                .arg(polarisationName(needed)).arg(command),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (answer != QMessageBox::Yes) {
            return;
        }

        m_polarisationCommandConfirmed = command;
    }

    if (!command.isEmpty())
    {
        switchPolarisation(needed, [this, needed, play, failed](const QString& error) {
            if (!error.isEmpty())
            {
                failed(QString("Switching the LNB to %1 polarisation: %2").arg(polarisationName(needed)).arg(error));
                return;
            }

            m_dvbDemod->setPolarisation(needed);
            play();
        });

        return;
    }

    const QString text = QString("The service's multiplex is on %1 polarisation, and %2 polarisation is received now. "
        "The LNB may need switching to receive it.").arg(polarisationName(needed)).arg(polarisationName(current));

    // Over full screen video, which would hide the message box (and too much to say over the video)
    if (!parent)
    {
        setVideoFullScreen(false);
        parent = this;
    }

    QMessageBox box(QMessageBox::Question, "Polarisation", text, QMessageBox::NoButton, parent);
    box.setInformativeText(QString("Switch the LNB to %1 polarisation and continue, or cancel to stay on this multiplex. "
        "A command to switch it can be set from the guide button's right-click menu.").arg(polarisationName(needed)));
    QPushButton *proceed = box.addButton("Continue", QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(proceed);
    box.exec();

    if (box.clickedButton() == proceed)
    {
        m_dvbDemod->setPolarisation(needed);
        play();
    }
}

// The user's command run to switch the LNB's polarisation: %p in it replaced by H, V, L or R, else that's added as its
// last argument. done: with why if it failed (not run, a non-zero exit code, or crashed or didn't finish in time)
void DVBDemodGUI::switchPolarisation(int polarisation, const std::function<void(const QString&)>& done)
{
    QStringList args = QProcess::splitCommand(m_settings.m_polarisationCommand);

    if (args.isEmpty())
    {
        done("No command");
        return;
    }

    const QString letter = QString("HVLR").mid(polarisation & 3, 1);
    bool substituted = false;

    for (QString& arg : args)
    {
        if (arg.contains("%p"))
        {
            arg.replace("%p", letter);
            substituted = true;
        }
    }

    if (!substituted) {
        args.append(letter);
    }

    const QString program = args.takeFirst();

    // A service selected while the last's command was running: only the last's tuned to
    if (m_polarisationProcess)
    {
        m_polarisationProcess->disconnect(this);
        m_polarisationProcess->kill();
        m_polarisationProcess->deleteLater();
    }

    QProcess *process = new QProcess(this);
    m_polarisationProcess = process;
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [process, done](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString output = QString::fromLocal8Bit(process->readAll()).trimmed().left(300);
            process->deleteLater();

            if (exitStatus != QProcess::NormalExit) {
                done("The command crashed, or didn't finish within 20 s");
            } else if (exitCode != 0) {
                done(QString("The command exited with %1%2").arg(exitCode).arg(output.isEmpty() ? QString() : ": " + output));
            } else {
                done(QString());
            }
        });

    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            done(QString("The command couldn't be run: %1").arg(process->errorString()));
            process->deleteLater();
        }
    });

    QTimer::singleShot(20000, process, [process]() { process->kill(); });
    qDebug() << "DVBDemodGUI::switchPolarisation:" << program << args;
    process->start(program, args);
}

// The command switching the LNB's polarisation
void DVBDemodGUI::epgMenu(const QPoint& position)
{
    QMenu menu(this);
    const QString command = m_settings.m_polarisationCommand.isEmpty() ? QString("none, ask") : m_settings.m_polarisationCommand;
    QAction *polarisation = menu.addAction(QString("Polarisation switching command: %1...").arg(command));

    if (menu.exec(ui->epg->mapToGlobal(position)) != polarisation) {
        return;
    }

    bool ok;
    const QString text = QInputDialog::getText(this, "Polarisation switching command",
        "Command run to switch the LNB's polarisation, before tuning from the guide to a satellite multiplex on another.\n"
        "%p in it is replaced by H, V, L or R (horizontal, vertical, left or right hand circular), else that's added\n"
        "as its last argument. It should exit with 0 once switched. Empty to be asked whether to tune instead",
        QLineEdit::Normal, m_settings.m_polarisationCommand, &ok);

    if (ok)
    {
        m_settings.m_polarisationCommand = text.trimmed();
        m_polarisationCommandConfirmed = m_settings.m_polarisationCommand;    // Typed here, so it's the user's
        applySetting("polarisationCommand");
    }
}

// The same programme as on the service playing, on another, as in HD, from the guide's event links
void DVBDemodGUI::on_simulcast_clicked()
{
    DVBDemodEPG *epg = m_dvbDemod->getEPG();
    const int serviceId = m_playingService != 0 ? m_playingService : m_dvbDemod->currentServiceId();
    DVBDemodEPG::Service simulcast;

    if (!epg->getSimulcast(epg->getNetworkId(), serviceId, simulcast)) {
        QMessageBox::warning(this, "HD", "The programme isn't on another service");
    } else {
        const QString name = simulcast.m_name;
        selectService(simulcast.m_networkId, simulcast.m_serviceId, this, [this, name](const QString& error) {
            QMessageBox::warning(this, "HD", QString("Can't play %1: %2").arg(name).arg(error));
        });
    }
}

void DVBDemodGUI::displaySimulcast()
{
    DVBDemodEPG *epg = m_dvbDemod->getEPG();
    const int serviceId = m_playingService != 0 ? m_playingService : m_dvbDemod->currentServiceId();
    DVBDemodEPG::Service simulcast;
    const bool available = (serviceId != 0) && epg->getSimulcast(epg->getNetworkId(), serviceId, simulcast)
        && !(simulcast.m_actual && (simulcast.m_serviceId == serviceId));
    QString toolTip = "Watch the programme in HD, when the programme guide says it's also on another service";

    if (available)
    {
        toolTip = QString("Watch the programme on %1").arg(simulcast.m_name);

        if (!simulcast.m_actual)
        {
            toolTip += simulcast.m_frequency > 0 ? QString(", tuning to %1 MHz").arg(simulcast.m_frequency / 1e6, 0, 'f', 3)
                : QString(", searching the UHF channels for its multiplex, as its frequency isn't known yet");
        }
    }

    ui->simulcast->setEnabled(available && !m_dvbDemod->isSearching());

    if (ui->simulcast->toolTip() != toolTip) {
        ui->simulcast->setToolTip(toolTip);
    }
}

// The demodulator's search for a service's multiplex, over the video, and why it failed. The Scan button stops one
void DVBDemodGUI::displaySearch()
{
    const QString text = m_dvbDemod->getSearchText();
    const QString scanText = m_dvbDemod->isScanning() ? "Stop" : "Scan";

    if (ui->scan->text() != scanText)
    {
        ui->scan->setText(scanText);
        ui->scan->setToolTip(m_dvbDemod->isScanning()
            ? "Stop the scan: the device returns to where it was, and the transponders found so far stay in the programme guide"
            : "Scan for satellite transponders: each signal in the device's band (or in a range, the device stepped across it) is found, "
              "locked and added to the programme guide. Click again to stop");
    }

    if (text == m_searchText) {
        return;
    }

    m_searchText = text;

    if (!text.isEmpty())
    {
        setStatusText(text);
        setStatusToolTip(text);
        m_statusMessageTimer.start();
        m_videoScreen.load()->setOverlay(text, m_dvbDemod->isSearching() ? 15.0 : 5.0);
    }
}

void DVBDemodGUI::on_service_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    m_settings.m_serviceId = ui->service->itemData(index).toInt();

    if (m_player) {
        m_player->setService(m_settings.m_serviceId);
    }

    applySetting("serviceId");
}

void DVBDemodGUI::on_avOffset_valueChanged(int value)
{
    m_settings.m_avOffset = value;

    if (m_player) {
        m_player->setAVOffset(value);
    }

    applySetting("avOffset");
}

void DVBDemodGUI::on_deinterlace_toggled(bool checked)
{
    m_settings.m_deinterlace = checked;

    if (m_player) {
        m_player->setDeinterlace(checked);
    }

    ui->screenTV->setDeinterlace(checked);

    if (m_fullScreenTV) {
        m_fullScreenTV->setDeinterlace(checked);
    }

    applySetting("deinterlace");
}

// The name of a language from its ISO 639-2 code
QString DVBDemodGUI::languageName(const QString& code)
{
    if (code.isEmpty() || (code == "und")) {
        return "Unknown language";
    }
    if (code == "qaa") {
        return "Original language";
    }
    if (code == "mul") {
        return "Multiple languages";
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    const QLocale::Language language = QLocale::codeToLanguage(code);

    if ((language != QLocale::AnyLanguage) && (language != QLocale::C)) {
        return QLocale::languageToString(language);
    }
#endif

    return code;
}

// Fill the audio and subtitle lists from the service's tracks, selecting those playing. Choosing one sets the
// language and kind (main or clean audio, audio description and its mix, normal or hard of hearing subtitles) and
// whether to show subtitles, which the player applies to later services too (the language alone where the kind's not
// there)
void DVBDemodGUI::tracksChanged(const DTVVideoPlayer::Tracks& tracks)
{
    if (!fromPlayer()) {
        return;
    }

    QSignalBlocker audioBlocker(ui->audioTrack);
    QSignalBlocker subtitleBlocker(ui->subtitleTrack);
    ui->audioTrack->clear();
    ui->subtitleTrack->clear();
    int audioIndex = -1;
    bool playingDescription = tracks.m_adPid >= 0;

    for (const auto& track : tracks.m_audio)
    {
        if ((track.m_pid == tracks.m_audioPid) && (track.m_type == DTVVideoPlayer::Track::AudioDescriptionMixed)) {
            playingDescription = true;
        }
    }

    for (const auto& track : tracks.m_audio)
    {
        const QString name = languageName(track.m_language);
        QString text;
        bool description = false;

        switch (track.m_type)
        {
        case DTVVideoPlayer::Track::AudioDescription:
            text = name + " + audio description";
            description = true;
            break;
        case DTVVideoPlayer::Track::AudioDescriptionMixed:
            text = name + " (audio description)";
            description = true;
            break;
        case DTVVideoPlayer::Track::CleanAudio:
            text = name + " (clean audio)";
            break;
        default:
            text = name;
            break;
        }

        if (ui->audioTrack->findText(text) >= 0) {
            continue; // As for a description that comes in both mixes
        }

        ui->audioTrack->addItem(text);
        const int index = ui->audioTrack->count() - 1;
        ui->audioTrack->setItemData(index, track.m_language, Qt::UserRole);
        ui->audioTrack->setItemData(index, description, Qt::UserRole + 1);
        ui->audioTrack->setItemData(index, (int) track.m_type, Qt::UserRole + 2);

        if ((description == playingDescription) && ((track.m_pid == tracks.m_audioPid) || (track.m_pid == tracks.m_adPid))) {
            audioIndex = index;
        }
    }

    ui->audioTrack->setCurrentIndex(audioIndex);
    ui->audioTrack->setEnabled(ui->audioTrack->count() > 1);

    ui->subtitleTrack->addItem("No subtitles");
    ui->subtitleTrack->setItemData(0, false, Qt::UserRole + 1);

    for (const auto& track : tracks.m_subtitles)
    {
        QString text = "Subtitles: " + languageName(track.m_language);

        if (track.m_type == DTVVideoPlayer::Track::SubtitlesHardOfHearing) {
            text += " (hard of hearing)";
        }

        ui->subtitleTrack->addItem(text);
        const int index = ui->subtitleTrack->count() - 1;
        ui->subtitleTrack->setItemData(index, track.m_language, Qt::UserRole);
        ui->subtitleTrack->setItemData(index, true, Qt::UserRole + 1);
        ui->subtitleTrack->setItemData(index, (int) track.m_type, Qt::UserRole + 2);

        if (track.m_pid == tracks.m_subtitlePid) {
            ui->subtitleTrack->setCurrentIndex(index);
        }
    }

    ui->subtitleTrack->setEnabled(!tracks.m_subtitles.isEmpty());
}

void DVBDemodGUI::clearTracks()
{
    QSignalBlocker audioBlocker(ui->audioTrack);
    QSignalBlocker subtitleBlocker(ui->subtitleTrack);
    ui->audioTrack->clear();
    ui->subtitleTrack->clear();
    ui->audioTrack->setEnabled(false);
    ui->subtitleTrack->setEnabled(false);
}

// The kinds as the settings have them (audio description and its mix, clean audio, hard of hearing subtitles)
void DVBDemodGUI::setTrackPreferences()
{
    if (m_player)
    {
        typedef DTVVideoPlayer::Track Track;
        const Track::Type audioType = m_settings.m_audioDescription
            ? (m_settings.m_audioDescriptionMixed ? Track::AudioDescriptionMixed : Track::AudioDescription)
            : m_settings.m_cleanAudio ? Track::CleanAudio : Track::Main;
        const Track::Type subtitleType = m_settings.m_subtitlesHardOfHearing ? Track::SubtitlesHardOfHearing : Track::Subtitles;
        m_player->setTrackPreferences(m_settings.m_audioLanguage, audioType, m_settings.m_subtitles, m_settings.m_subtitleLanguage,
            subtitleType);
    }
}

void DVBDemodGUI::on_audioTrack_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    typedef DTVVideoPlayer::Track Track;
    const int type = ui->audioTrack->itemData(index, Qt::UserRole + 2).toInt();
    m_settings.m_audioLanguage = ui->audioTrack->itemData(index, Qt::UserRole).toString();
    m_settings.m_audioDescription = ui->audioTrack->itemData(index, Qt::UserRole + 1).toBool();
    m_settings.m_audioDescriptionMixed = type == Track::AudioDescriptionMixed;
    m_settings.m_cleanAudio = type == Track::CleanAudio;
    setTrackPreferences();
    applySettings({"audioLanguage", "audioDescription", "audioDescriptionMixed", "cleanAudio"});
}

void DVBDemodGUI::on_subtitleTrack_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    m_settings.m_subtitles = ui->subtitleTrack->itemData(index, Qt::UserRole + 1).toBool();

    if (m_settings.m_subtitles)
    {
        m_settings.m_subtitleLanguage = ui->subtitleTrack->itemData(index, Qt::UserRole).toString();
        m_settings.m_subtitlesHardOfHearing = ui->subtitleTrack->itemData(index, Qt::UserRole + 2).toInt()
            == DTVVideoPlayer::Track::SubtitlesHardOfHearing;
    }

    setTrackPreferences();
    applySettings({"subtitles", "subtitleLanguage", "subtitlesHardOfHearing"});
}

void DVBDemodGUI::on_hardwareDecoding_toggled(bool checked)
{
    m_settings.m_hardwareDecoding = checked;

    if (m_player) {
        m_player->setHardwareDecoding(checked);
    }

    applySetting("hardwareDecoding");
}

void DVBDemodGUI::on_audioMute_toggled(bool checked)
{
    m_settings.m_audioMute = checked;

    if (m_player) {
        m_player->setAudioMute(checked);
    }

    applySetting("audioMute");
}

void DVBDemodGUI::on_audioVolume_valueChanged(int value)
{
    m_settings.m_audioVolume = value;
    ui->audioVolumeText->setText(QString::number(value));

    if (m_player) {
        m_player->setAudioVolume(value);
    }

    applySetting("audioVolume");
}

void DVBDemodGUI::audioSelect(const QPoint& p)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySetting("audioDeviceName");
    }
}

// Queued from the player: not from one stopped since (its signals already queued are still delivered)
bool DVBDemodGUI::fromPlayer() const
{
    return m_player && (sender() == m_player);
}

void DVBDemodGUI::servicesChanged(const QList<DTVVideoPlayer::Service>& services)
{
    if (!fromPlayer()) {
        return;
    }

    ui->service->blockSignals(true);
    ui->service->clear();
    ui->service->addItem("Automatic", 0);

    for (const auto& service : services)
    {
        QString text = service.m_channel >= 0
            ? QString("%1 %2 (%3)").arg(service.m_channel).arg(service.m_name).arg(service.m_id)
            : QString("%1 (%2)").arg(service.m_name).arg(service.m_id);
        ui->service->addItem(text, service.m_id);
    }

    ui->service->blockSignals(false);
    selectServiceItem();
    m_services = services;

    // The name or number of the service playing may have just arrived
    if ((m_playingService != 0) && (serviceText(m_playingService) != m_overlayText))
    {
        showServiceOverlay();

        if (!m_videoMessage.isEmpty()) {
            showVideoMessage(m_videoMessage);
        }
    }
}

// Logical channel number and name, as a TV shows them
QString DVBDemodGUI::serviceText(int id) const
{
    for (const auto& service : m_services)
    {
        if (service.m_id == id) {
            return service.m_channel >= 0 ? QString("%1  %2").arg(service.m_channel).arg(service.m_name) : service.m_name;
        }
    }

    return QString();
}

void DVBDemodGUI::showServiceOverlay()
{
    m_overlayText = serviceText(m_playingService);

    // And the programme on now, if known
    DVBDemodEPG::Event now, next;

    if (!m_overlayText.isEmpty() && m_dvbDemod->getEPG()->getNowNext(m_playingService, now, next) && now.isValid()) {
        m_overlayText += "\n" + now.m_title;
    }

    m_nowTitle = now.m_title;
    m_guideOverlay->setPlaying(m_playingService);

    // The programme guide's ribbon, unless the guide itself is shown, or just the name and programme if the guide
    // doesn't have the service yet
    if (m_player && m_guideOverlay->hasPlaying())
    {
        if (!m_guideOverlay->isGuideShown()) {
            m_guideOverlay->showRibbon();
        }
    }
    else if (m_player && !m_overlayText.isEmpty())
    {
        m_videoScreen.load()->setOverlay(m_overlayText, 5.0);
    }
}

// The service set selected in the list. One not listed (yet, as just after retuning to its multiplex) is added as a
// placeholder, so the list doesn't show Automatic for it, and choosing Automatic changes the setting
void DVBDemodGUI::selectServiceItem()
{
    QSignalBlocker blocker(ui->service);
    const int serviceId = m_settings.m_serviceId;

    if (ui->service->count() == 0) {
        ui->service->addItem("Automatic", 0);
    }

    // A placeholder for another service, no longer wanted
    for (int i = ui->service->count() - 1; i > 0; i--)
    {
        if (ui->service->itemData(i, Qt::UserRole + 1).toBool() && (ui->service->itemData(i).toInt() != serviceId)) {
            ui->service->removeItem(i);
        }
    }

    int index = ui->service->findData(serviceId);

    if ((index < 0) && (serviceId != 0))
    {
        ui->service->addItem(QString("Service %1").arg(serviceId), serviceId);
        index = ui->service->count() - 1;
        ui->service->setItemData(index, true, Qt::UserRole + 1);
    }

    ui->service->setCurrentIndex(index >= 0 ? index : 0);
}

void DVBDemodGUI::serviceSelected(int id, bool hasVideo, bool hasAudio)
{
    if (!fromPlayer()) {
        return;
    }

    // Clear the last service's picture, or say why there won't be one
    m_serviceHasVideo = hasVideo;
    m_serviceHasAudio = hasAudio;
    m_playingService = id;
    m_dvbDemod->setPlayingService(id);  // So recording and the reports, in Automatic, are of this one
    m_dvbDemod->getApps()->setCarouselService(m_dvbDemod->currentServiceId());   // Its carousels received straight away
    updateAppsDialog();
    updateHbbTVService();
#ifdef DVBDEMOD_HBBTV
    if (m_hbbtv && (m_hbbtvChannelRequest != 0))
    {
        m_hbbtv->channelChangeResult(id == m_hbbtvChannelRequest);
        m_hbbtvChannelRequest = 0;
    }
#endif
    m_lastFrames = 0;
    m_framesTimer.start();
    showServiceOverlay();

    if (hasVideo) {
        showVideoMessage("");
    } else if (hasAudio) {
        showVideoMessage("Audio only");
    } else {
        showVideoMessage("No video or audio: the service may not be broadcasting now");
    }

    displayNowNext();
}

void DVBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySetting("rollupState");
}

void DVBDemodGUI::onSplitterMoved(int pos, int index)
{
    (void) pos;
    (void) index;

    // Saved once it's stopped moving: each move of a drag would send the settings to the channel
    m_splitterTimer.start(500);
}

void DVBDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_dvbDemod->getNumberOfDeviceStreams());
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
            settingsKeys.append("streamIndex");
        }

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

DVBDemodGUI::DVBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::DVBDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_player(nullptr),
    m_tickCount(0),
    m_videoFullScreen(false),
    m_fullScreenTV(nullptr),
    m_guideOverlay(nullptr),
    m_serviceHasVideo(true),
    m_playingService(0),
    m_serviceHasAudio(true),
    m_lastFrames(0),
    m_lastAudioFrames(0),
    m_audioPlaying(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddvb/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(ui->splitter, &QSplitter::splitterMoved, this, &DVBDemodGUI::onSplitterMoved);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_dvbDemod = reinterpret_cast<DVBDemod*>(rxChannel);
    m_dvbDemod->setMessageQueueToGUI(getInputMessageQueue());
    m_guideOverlay = new DTVGuideOverlay(m_dvbDemod->getEPG(), this);
    m_guideOverlay->setLogo(QImage(":/demoddvb/images/angeltv.png"));
    m_splitterTimer.setSingleShot(true);
    connect(&m_splitterTimer, &QTimer::timeout, this, [this]() {
        m_settings.m_splitterSizes = ui->splitter->sizes();
        applySetting("splitterSizes");
    });
    m_pointerTimer.setSingleShot(true);
    connect(&m_pointerTimer, &QTimer::timeout, this, [this]() {
        if (m_fullScreenTV) {
            m_fullScreenTV->setCursor(Qt::BlankCursor);
        }
    });
    m_guideOverlay->setRecordings(
        [this](int networkId, int serviceId, int eventId) { return m_dvbDemod->isScheduled(networkId, serviceId, eventId); },
        [this](int networkId, int serviceId, int eventId) {
            if (m_dvbDemod->isScheduled(networkId, serviceId, eventId)) {
                m_dvbDemod->cancelScheduledRecording(networkId, serviceId, eventId);
            } else {
                m_dvbDemod->scheduleRecording(networkId, serviceId, eventId);
            }
        });
    connect(m_guideOverlay, &DTVGuideOverlay::playService, this, &DVBDemodGUI::guideServiceSelected);
    connect(m_guideOverlay, &DTVGuideOverlay::redButton, this, &DVBDemodGUI::hbbtvRed);
    ui->redButton->setVisible(false);

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);
    ui->udpPort->setValidator(new QIntValidator(1, 65535, ui->udpPort));
    ui->screenTV->setKeepAspectRatio(true);
    ui->screenTV->setDeinterlace(m_settings.m_deinterlace);
    m_videoScreen = ui->screenTV;
    ui->screenTV->setToolTip("Double-click for full screen, and double-click or press Esc to return.\n"
        "Right-click (or click here and press G) for the programme guide, and I for what's on now and next");
    ui->screenTV->setFocusPolicy(Qt::StrongFocus); // For the guide's keys
    ui->screenTV->setMouseTracking(true);          // Keeps the guide open while the mouse is used
    ui->screenTV->installEventFilter(this);
    m_guideOverlay->setWidget(ui->screenTV);

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.occupiedBandwidth());
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("DVB Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    if (!DTVVideoPlayer::canDeinterlace())
    {
        ui->deinterlace->setEnabled(false);
        ui->deinterlace->setToolTip("Deinterlacing needs FFmpeg's libavfilter, which this build doesn't have");
    }

    displaySettings();
    makeUIConnections();
    setupStats();
    applyAllSettings();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();

    if (m_settings.m_videoEnabled) {
        startPlayer();
    }
}

DVBDemodGUI::~DVBDemodGUI()
{
    // The channel outlives the GUI (DeviceUISet deletes the GUI first), and its destructor can still report
    m_dvbDemod->setMessageQueueToGUI(nullptr);
    disconnect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));
    stopPlayer();
#ifdef DVBDEMOD_HBBTV
    delete m_streamPlayback;
    m_streamPlayback = nullptr;
    m_streamPlaying = false;
    delete m_hbbtv;
    m_hbbtv = nullptr;
#endif
    delete m_fullScreenTV;
    delete ui;
}

void DVBDemodGUI::startPlayer()
{
    if (m_player || m_streamPlaying) {
        return;
    }

    m_dvbDemod->getTSFifo()->attachReader();
    m_player = new DTVVideoPlayer(m_dvbDemod->getTSFifo(), m_dvbDemod->getAudioFifo());
    // From the player's video thread
    m_player->setFrameCallback([this](GLVideoFramePtr frame) {
        m_videoScreen.load()->setFrame(frame);
    });
    m_player->setCanShowCallback([this](int type) {
        return m_videoScreen.load()->canShow((GLVideoFrame::Type) type);
    });
    m_player->setService(m_settings.m_serviceId);
    m_player->setAudioMute(m_settings.m_audioMute);
    m_player->setAudioVolume(m_settings.m_audioVolume);
    m_player->setDeinterlace(m_settings.m_deinterlace);
    m_player->setHardwareDecoding(m_settings.m_hardwareDecoding);
    m_player->setAVOffset(m_settings.m_avOffset);
    setTrackPreferences();
    connect(m_player, &DTVVideoPlayer::servicesChanged, this, &DVBDemodGUI::servicesChanged, Qt::QueuedConnection);
    connect(m_player, &DTVVideoPlayer::serviceSelected, this, &DVBDemodGUI::serviceSelected, Qt::QueuedConnection);
    connect(m_player, &DTVVideoPlayer::tracksChanged, this, &DVBDemodGUI::tracksChanged, Qt::QueuedConnection);
    m_player->start();
    updateHbbTV();
}

void DVBDemodGUI::stopPlayer()
{
    if (m_player)
    {
        m_dvbDemod->setPlayingService(0);
        disconnect(m_player, nullptr, this, nullptr);
        m_player->stopPlayer();
        delete m_player;
        m_player = nullptr;
        updateHbbTV();
        m_dvbDemod->getTSFifo()->detachReader(); // After the player's stopped, as its reads would attach it again
        ui->screenTV->clear();
        clearTracks();

        if (m_fullScreenTV) {
            m_fullScreenTV->clear();
        }

        showVideoMessage("");
    }
}

void DVBDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void DVBDemodGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void DVBDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    // Keys from the widgets' handlers while the settings are displayed aren't kept: the channel has those settings
    // already, and sent with the next change, they'd apply again (as "standard" resetting the guide's settings)
    if (!m_doApplySettings) {
        return;
    }

    m_settingsKeys.append(settingsKeys);
    DVBDemod::MsgConfigureDVBDemod* message = DVBDemod::MsgConfigureDVBDemod::create(m_settings, m_settingsKeys, force);
    m_dvbDemod->getInputMessageQueue()->push(message);
    m_settingsKeys.clear();
}

void DVBDemodGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void DVBDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.occupiedBandwidth());
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    int bandwidthIndex = 0;

    for (int i = 0; i < (int) std::size(m_bandwidths); i++)
    {
        if (m_bandwidths[i] == m_settings.m_bandwidth) {
            bandwidthIndex = i;
        }
    }

    ui->bandwidth->setCurrentIndex(bandwidthIndex);
    ui->standard->setCurrentIndex(m_settings.m_standard);
    ui->symbolRate->setEditText(m_settings.m_symbolRate > 0 ? QString::number(m_settings.m_symbolRate) : QString("Auto"));
    ui->rollOff->setCurrentIndex(m_settings.m_rollOff == 35 ? 1 : m_settings.m_rollOff == 25 ? 2 : m_settings.m_rollOff == 20 ? 3
        : m_settings.m_rollOff == 15 ? 4 : m_settings.m_rollOff == 10 ? 5 : m_settings.m_rollOff == 5 ? 6 : 0);
    ui->codeRate->setCurrentIndex(m_settings.m_codeRate + 1);
    ui->plScrambling->setValue(m_settings.m_plScramblingCode);
    ui->equaliser->setChecked(m_settings.m_equaliser);
    m_listingISIs = (m_settings.m_standard == 4) || (m_settings.m_standard == 6);
    updateStandardWidgets();
    fillStreamList();
    ui->priority->setCurrentIndex(m_settings.m_lowPriority ? 1 : 0);
    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));
    ui->videoEnabled->setChecked(m_settings.m_videoEnabled);
    ui->deinterlace->setChecked(m_settings.m_deinterlace);
    ui->hardwareDecoding->setChecked(m_settings.m_hardwareDecoding);
    setTrackPreferences(); // The lists show what the player chooses
    ui->avOffset->setValue(m_settings.m_avOffset);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->audioVolume->setValue(m_settings.m_audioVolume);
    ui->audioVolumeText->setText(QString::number(m_settings.m_audioVolume));

    selectServiceItem();

    // As when retuned to another multiplex, whose services aren't listed yet
    if (m_player) {
        m_player->setService(m_settings.m_serviceId);
    }

    updateIndexLabel();
    getRollupContents()->restoreState(m_rollupState);

    if (m_settings.m_splitterSizes.size() == ui->splitter->count()) {
        ui->splitter->setSizes(m_settings.m_splitterSizes);
    }

    updateAbsoluteCenterFrequency();
    updateHbbTV();
    blockApplySettings(false);
}

void DVBDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void DVBDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void DVBDemodGUI::tick()
{
    if (m_tickCount % 20 == 0) // 1 s
    {
        displayNowNext();
        displayRecordings();
        displaySimulcast();
        m_guideOverlay->refresh();
        updateHbbTVService();
        displayRedButton();
    }

#ifdef DVBDEMOD_HBBTV
    if (m_hbbtv)
    {
        m_hbbtv->setSuppressed(m_guideOverlay->isShown());

        // What the red button's waiting for, at the bottom right of the video, until there's nothing to wait for
        const QString status = m_hbbtv->isStatusInPage() ? QString() : m_hbbtv->getStatus();

        if (status != m_hbbtvStatus)
        {
            m_videoScreen.load()->setOverlay(status, status.isEmpty() ? 0.0 : 60.0);
            m_hbbtvStatus = status;
        }
    }
#endif

    if (++m_tickCount % 4 == 0) // 200 ms
    {
        displayStatus();
        displaySearch();
        checkVideoReceived();

        if (m_player)
        {
            int sampleRate;
            double latency;
            m_dvbDemod->getAudioOutput(sampleRate, latency);
            m_player->setAudioOutput(sampleRate, latency);
        }

#ifdef DVBDEMOD_HBBTV
        if (m_streamPlayback && m_streamPlaying)
        {
            int sampleRate;
            double latency;
            m_dvbDemod->getAudioOutput(sampleRate, latency);
            m_streamPlayback->setAudioOutput(sampleRate, latency);

            if (m_hbbtv && m_streamPlayback->isPlaying())
            {
                QJsonObject data;
                data.insert("position", m_streamPlayback->getPosition());
                m_hbbtv->mediaEvent(m_mediaId, "time", data);
            }
        }
#endif
    }
}

// A message, or blank if empty, on the screen, in place of video
void DVBDemodGUI::showVideoMessage(const QString& message)
{
    m_videoMessage = message;
    QString text;

    if (!message.isEmpty())
    {
        // With the service's name, as the overlay on video would show it
        const QString name = serviceText(m_playingService);
        text = name.isEmpty() ? message : QString("%1\n\n%2").arg(name).arg(message);
    }

    m_videoScreen.load()->setMessage(text);
}

// Say if nothing has arrived for a while, as when the service isn't broadcasting, though it has streams
void DVBDemodGUI::checkVideoReceived()
{
    if (!m_player || !m_framesTimer.isValid() || (!m_serviceHasVideo && !m_serviceHasAudio)) {
        return;
    }

    DTVVideoPlayer::Info info = m_player->getInfo();
    uint64_t frames = m_serviceHasVideo ? info.m_videoFrames : info.m_audioFrames;

    if (frames != m_lastFrames)
    {
        m_lastFrames = frames;
        m_framesTimer.start();

        // Video is showing, as after the signal returns: the screen shows a message instead of the video, so it's
        // cleared there too
        if (m_serviceHasVideo)
        {
            if (!m_videoMessage.isEmpty()) {
                showVideoMessage("");
            }
        }
        else if (m_videoMessage != "Audio only")
        {
            showVideoMessage("Audio only");
        }
    }
    else if (info.m_scrambled)
    {
        const QString scrambled("Scrambled: the service is encrypted (pay TV), so it can't be played");

        if (m_videoMessage != scrambled) {
            showVideoMessage(scrambled);
        }
    }
    else if ((m_framesTimer.elapsed() > 5000) && (m_videoMessage.isEmpty() || (m_videoMessage == "Audio only")))
    {
        showVideoMessage(m_serviceHasVideo ? "No video received: the service may not be broadcasting now"
            : "No audio received: the service may not be broadcasting now");
    }
}

bool DVBDemodGUI::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == ui->screenTV) || (m_fullScreenTV && (obj == m_fullScreenTV)))
    {
        const QEvent::Type type = event->type();

        // The screen showing the video, full screen or not, has the guide over it, which uses the mouse and keys
        // first (Esc to close itself)
        if (obj == m_videoScreen.load())
        {
            // Full screen, the pointer shows while the mouse is used
            if (m_videoFullScreen && ((type == QEvent::MouseMove) || (type == QEvent::MouseButtonPress) || (type == QEvent::Wheel)))
            {
                m_fullScreenTV->setCursor(Qt::ArrowCursor);
                m_pointerTimer.start(3000);
            }

#ifdef DVBDEMOD_HBBTV
            // With an application showing, a click gives the video the keys (for the application), rather than
            // opening the ribbon, which would hide it. Double-clicking still goes full screen, and right-clicking
            // opens the guide
            if (m_hbbtv && m_hbbtv->isRunning() && m_hbbtv->isVisible() && !m_guideOverlay->isShown() && (type == QEvent::MouseButtonPress)
                && (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton))
            {
                m_hbbtv->takeFocus();
                return true;
            }
#endif

            if (((type == QEvent::MouseMove) || (type == QEvent::MouseButtonPress) || (type == QEvent::MouseButtonDblClick))
                && m_guideOverlay->handleMouse(static_cast<QMouseEvent*>(event))) {
                return true;
            }

            if ((type == QEvent::Wheel) && m_guideOverlay->handleWheel(static_cast<QWheelEvent*>(event))) {
                return true;
            }

#ifdef DVBDEMOD_HBBTV
            if (m_hbbtv && !m_guideOverlay->isShown())
            {
                if ((type == QEvent::KeyPress) && m_hbbtv->keyPress(static_cast<QKeyEvent*>(event))) {
                    return true;
                }

                if ((type == QEvent::KeyRelease) && m_hbbtv->keyRelease(static_cast<QKeyEvent*>(event))) {
                    return true;
                }
            }
#endif

            if ((type == QEvent::KeyPress) && m_guideOverlay->handleKey(static_cast<QKeyEvent*>(event))) {
                return true;
            }

            if (type == QEvent::Resize)
            {
                m_guideOverlay->refresh(true);
                updateHbbTVGeometry();
            }
        }

        if (type == QEvent::MouseButtonDblClick)
        {
            setVideoFullScreen(!m_videoFullScreen);
            return true;
        }

        if (m_videoFullScreen && (type == QEvent::KeyPress) && (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape))
        {
            setVideoFullScreen(false);
            return true;
        }

        // The full screen window closed (as with Alt+F4): it's only hidden, as when leaving full screen
        if ((obj == m_fullScreenTV) && (type == QEvent::Close))
        {
            event->ignore();
            setVideoFullScreen(false);
            return true;
        }
    }

    return ChannelGUI::eventFilter(obj, event);
}

// Full screen video is shown in a window of its own, made the first time and then kept, as making an OpenGL
// window takes a while, as would turning the screen in the GUI into a window
void DVBDemodGUI::setVideoFullScreen(bool fullScreen)
{
    if (fullScreen == m_videoFullScreen) {
        return;
    }

    m_videoFullScreen = fullScreen;

    if (fullScreen)
    {
        if (!m_fullScreenTV)
        {
            m_fullScreenTV = new GLVideoWidget(nullptr);
            m_fullScreenTV->setWindowTitle(m_settings.m_title);
            m_fullScreenTV->setKeepAspectRatio(true);
            m_fullScreenTV->setDeinterlace(m_settings.m_deinterlace);
            m_fullScreenTV->setFocusPolicy(Qt::StrongFocus);
            m_fullScreenTV->setCursor(Qt::BlankCursor);
            m_fullScreenTV->setMouseTracking(true); // For the pointer, which shows while the mouse moves
            m_fullScreenTV->installEventFilter(this);
        }

        ui->screenTV->setMessage("");
        m_videoScreen = m_fullScreenTV;
        // The decoder's GPU surfaces it holds let go of (with both screens' frames held, the decoder's pool could run
        // out). Now, and again a little later, as the video thread may be giving it a frame, having read m_videoScreen
        // before it was changed
        ui->screenTV->clear();
        QTimer::singleShot(100, ui->screenTV, [this]() {
            if (m_videoFullScreen) {
                ui->screenTV->clear();
            }
        });
        m_guideOverlay->setWidget(m_fullScreenTV);
        m_guideOverlay->setPlaying(m_playingService);
        m_fullScreenTV->showFullScreen();
#ifdef _WIN32
        // An OpenGL window exactly covering the screen is switched to exclusive full screen mode, which takes
        // seconds to enter and leave. A border avoids that, as Qt's QWindowsWindow::setHasBorderInFullScreen() does
        HWND hwnd = reinterpret_cast<HWND>(m_fullScreenTV->winId());
        SetWindowLongPtr(hwnd, GWL_STYLE, GetWindowLongPtr(hwnd, GWL_STYLE) | WS_BORDER);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
        m_fullScreenTV->activateWindow();
        m_fullScreenTV->setFocus();
        updateHbbTVGeometry();

        if (!m_videoMessage.isEmpty()) {
            showVideoMessage(m_videoMessage);
        }
    }
    else
    {
        m_videoScreen = ui->screenTV;
        m_guideOverlay->setWidget(ui->screenTV);
        updateHbbTVGeometry();
        m_fullScreenTV->hide();

        // Its frames may hold the decoder's GPU surfaces. Cleared now, and again a little later, as the video thread
        // may be giving it a frame, having read m_videoScreen before it was changed
        m_fullScreenTV->clear();
        QTimer::singleShot(100, m_fullScreenTV, [this]() {
            if (!m_videoFullScreen) {
                m_fullScreenTV->clear();
            }
        });

        if (!m_videoMessage.isEmpty()) {
            showVideoMessage(m_videoMessage);
        }
    }
}

// The measurements table, beside the constellation: sized to show every row, but scrolling when the splitter above the
// video's moved up, to give the video more room
void DVBDemodGUI::setupStats()
{
    ui->stats->setColumnCount(2);
    ui->stats->setRowCount(NbStats);
    const QFontMetrics metrics(ui->stats->font());
    const int rowHeight = metrics.height() + 4;
    ui->stats->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->stats->verticalHeader()->setDefaultSectionSize(rowHeight);
    ui->stats->setColumnWidth(0, metrics.horizontalAdvance("BER after Viterbi") + 12);
    ui->stats->setColumnWidth(1, metrics.horizontalAdvance("MPEG-2 video (D3D11VA)") + 12);
    ui->stats->horizontalHeader()->setStretchLastSection(true);

    for (int row = 0; row < NbStats; row++)
    {
        for (int column = 0; column < 2; column++) {
            ui->stats->setItem(row, column, new QTableWidgetItem());
        }
    }

    // Room for the scroll bar, so the values aren't cut off when it's shown (the last column stretches when it isn't)
    const int frame = 2 * ui->stats->frameWidth();
    const int scrollBar = ui->stats->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    ui->stats->setFixedWidth(ui->stats->columnWidth(0) + ui->stats->columnWidth(1) + frame + scrollBar);
    ui->stats->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    ui->stats->setMinimumHeight(3 * rowHeight + frame);
}

// The player's A/V problems in the last 10 s (what's happening now, rather than since it started), from its counts then and
// now: frames late or with errors, the frame rate and stalls, A/V sync and the clock resynchronising, audio muted or
// missing, the audio FIFO running dry or overflowing, and TS the player didn't take. Orange while there are any
void DVBDemodGUI::displayAVStats(const DTVVideoPlayer::Info& info, uint64_t tsOverflows)
{
    const bool video = m_player && !info.m_videoCodec.isEmpty();
    const bool audio = m_player && !info.m_audioCodec.isEmpty();
    const Stat rows[] = {StatFrameRate, StatVideoDrops, StatAVSync, StatAudioDrops, StatAudioBuffer, StatTSDropped};
    const char *names[] = {"Frame rate", "Video drops", "A/V sync", "Audio drops", "Audio buffer", "TS dropped"};

    if (!m_player)
    {
        m_avHistory.clear();
        m_avToolTipInputs.clear();

        for (int i = 0; i < 6; i++)
        {
            setStat(rows[i], names[i], "-", "Shown when a service is playing");
            setStatWarning(rows[i], false);
        }

        return;
    }

    if (!m_avTimer.isValid()) {
        m_avTimer.start();
    }

    const AVCounts now{m_avTimer.elapsed(), info.m_videoFrames, info.m_lateFrames, info.m_decodeErrors + info.m_videoDropped,
        info.m_audioConcealed, info.m_audioGaps, info.m_audioUnderruns, info.m_audioOverflows, info.m_clockRestarts, tsOverflows};

    // Counted again for another service, or player
    if (!m_avHistory.empty() && ((info.m_serviceId != m_avServiceId) || (now.m_videoFrames < m_avHistory.back().m_videoFrames)
        || (now.m_tsOverflows < m_avHistory.back().m_tsOverflows))) {
        m_avHistory.clear();
    }

    m_avServiceId = info.m_serviceId;
    m_avHistory.push_back(now);

    while ((m_avHistory.size() > 1) && (now.m_time - m_avHistory[1].m_time >= 10000)) {
        m_avHistory.pop_front();
    }

    const AVCounts& then = m_avHistory.front();
    const double span = (now.m_time - then.m_time) / 1000.0;
    const double recentSeconds = std::max(1.0, std::round(span));
    const QString recent = QString("In the last %1 s").arg(recentSeconds, 0, 'f', 0);

    // The tooltips have the totals: made again only when they change, rather than every update
    const std::vector<uint64_t> toolTipInputs{(uint64_t) recentSeconds, info.m_lateFrames, info.m_decodeErrors, info.m_videoDropped,
        info.m_softwareGOPs, info.m_clockRestarts, info.m_audioConcealed, info.m_audioGaps, info.m_audioUnderruns, info.m_audioOverflows,
        tsOverflows};
    const bool toolTips = toolTipInputs != m_avToolTipInputs;
    m_avToolTipInputs = toolTipInputs;
    auto statToolTip = [this, toolTips](Stat row, const std::function<QString()>& toolTip) {
        if (toolTips) {
            setStatToolTip(row, toolTip());
        }
    };

    // Frame rate, and stalls
    const double fps = span >= 1.0 ? (now.m_videoFrames - then.m_videoFrames) / span : 0.0;
    const int maxInterval = (int) std::lround(info.m_maxFrameInterval * 1e3);
    setStatValue(StatFrameRate, "Frame rate", !video || (span < 1.0) ? QString("-")
        : QString("%1 fps, max %2 ms").arg(fps, 0, 'f', 1).arg(maxInterval));
    statToolTip(StatFrameRate, []() {
        return QString("Video frames shown a second, and the longest interval between them in the last 5 to 10 s. Over 100 ms (orange) is a stall");
    });
    setStatWarning(StatFrameRate, video && (maxInterval > 100));

    // Video frames late or with errors
    const uint64_t late = now.m_lateFrames - then.m_lateFrames;
    const uint64_t videoErrors = now.m_videoErrors - then.m_videoErrors;
    setStatValue(StatVideoDrops, "Video drops", !video ? QString("-") : QString("%1 late, %2 errors").arg(late).arg(videoErrors));
    statToolTip(StatVideoDrops, [&]() {
        return QString("%1: video frames not shown, as they were too late (the decoder or display falling behind), and errors: frames "
            "that failed to decode, and packets dropped: damaged ones (with the GPU decoding, if FFmpeg can't decode them in software), "
            "or those up to the next keyframe when the decoder fell far behind. In all, %2 late, %3 decode errors, %4 packets dropped, "
            "and %5 damaged GOPs decoded in software rather than by the GPU")
            .arg(recent).arg(info.m_lateFrames).arg(info.m_decodeErrors).arg(info.m_videoDropped).arg(info.m_softwareGOPs);
    });
    setStatWarning(StatVideoDrops, video && ((late > 0) || (videoErrors > 0)));

    // A/V sync, and the video clock resynchronising
    const uint64_t restarts = now.m_clockRestarts - then.m_clockRestarts;
    const bool synced = video && audio && !std::isnan(info.m_avSync);
    const int syncMs = synced ? (int) std::lround(info.m_avSync * 1e3) : 0;
    setStatValue(StatAVSync, "A/V sync", !video ? QString("-")
        : synced ? QString("%1 ms, %2 resyncs").arg(syncMs >= 0 ? QString("+%1").arg(syncMs) : QString::number(syncMs)).arg(restarts)
        : QString("%1 resyncs").arg(restarts));
    statToolTip(StatAVSync, [&]() {
        return QString("How far the video is shown after the audio heard with it (negative: before), allowing for the A/V offset set, "
            "and the times the video lost sync with the audio (or its timestamps jumped) and was shown as decoded until resynchronised, %1. "
            "Over 60 ms either way, or any resyncs, is orange. In all, %2 resyncs").arg(recent.toLower()).arg(info.m_clockRestarts);
    });
    setStatWarning(StatAVSync, video && ((std::abs(syncMs) > 60) || (restarts > 0)));

    // Audio muted or missing
    const uint64_t muted = now.m_audioMuted - then.m_audioMuted;
    const uint64_t gaps = now.m_audioGaps - then.m_audioGaps;
    setStatValue(StatAudioDrops, "Audio drops", !audio ? QString("-") : QString("%1 muted, %2 gaps").arg(muted).arg(gaps));
    statToolTip(StatAudioDrops, [&]() {
        return QString("%1: audio frames muted as they had errors, and gaps where frames were missing (lost packets), filled with "
            "silence. In all, %2 muted, %3 gaps").arg(recent).arg(info.m_audioConcealed).arg(info.m_audioGaps);
    });
    setStatWarning(StatAudioDrops, audio && ((muted > 0) || (gaps > 0)));

    // The audio FIFO running dry or overflowing
    const uint64_t underruns = now.m_audioUnderruns - then.m_audioUnderruns;
    const uint64_t overflows = now.m_audioOverflows - then.m_audioOverflows;
    setStatValue(StatAudioBuffer, "Audio buffer", !audio ? QString("-") : QString("%1 under, %2 over").arg(underruns).arg(overflows));
    statToolTip(StatAudioBuffer, [&]() {
        return QString("%1: times the audio buffer ran dry while playing (audio came too late, and the device played silence), "
            "and times it was too full and its oldest audio was dropped (as audio comes all at once when a service starts). "
            "In all, %2 underruns, %3 overflows").arg(recent).arg(info.m_audioUnderruns).arg(info.m_audioOverflows);
    });
    setStatWarning(StatAudioBuffer, audio && ((underruns > 0) || (overflows > 0)));

    // TS the player didn't take
    const uint64_t tsDropped = now.m_tsOverflows - then.m_tsOverflows;
    setStatValue(StatTSDropped, "TS dropped", tsDropped == 0 ? QString("0") : QString("%1 kB").arg((tsDropped + 999) / 1000));
    statToolTip(StatTSDropped, [&]() {
        return QString("%1: transport stream the player didn't take, as it fell behind the receiver, which is seen and heard as errors. "
            "In all, %2 kB").arg(recent).arg((tsOverflows + 999) / 1000);
    });
    setStatWarning(StatTSDropped, tsDropped > 0);
}

// Narrowband interferers notched out, for the CFO measurement
QString DVBDemodGUI::notchText(const std::vector<double>& notches)
{
    QString text;

    for (double f : notches) {
        text += QString(", notch %1 kHz").arg(f / 1e3, 0, 'f', 1);
    }

    return text;
}

void DVBDemodGUI::setStat(Stat row, const QString& name, const QString& value, const QString& toolTip)
{
    setStatValue(row, name, value);
    setStatToolTip(row, toolTip);
}

void DVBDemodGUI::setStatValue(Stat row, const QString& name, const QString& value)
{
    QTableWidgetItem *nameItem = ui->stats->item(row, 0);
    QTableWidgetItem *valueItem = ui->stats->item(row, 1);

    if (nameItem->text() != name) {
        nameItem->setText(name);
    }

    if (valueItem->text() != value) {
        valueItem->setText(value);
    }
}

void DVBDemodGUI::setStatToolTip(Stat row, const QString& toolTip)
{
    QTableWidgetItem *nameItem = ui->stats->item(row, 0);

    if (nameItem->toolTip() != toolTip)
    {
        nameItem->setToolTip(toolTip);
        ui->stats->item(row, 1)->setToolTip(toolTip);
    }
}

// A value orange, as for a problem now
void DVBDemodGUI::setStatWarning(Stat row, bool warning)
{
    QTableWidgetItem *valueItem = ui->stats->item(row, 1);
    const QBrush brush = warning ? QBrush(QColor(255, 165, 0)) : QBrush();

    if (valueItem->foreground() != brush) {
        valueItem->setForeground(brush);
    }
}

void DVBDemodGUI::displayStatus()
{
    DVBDemodSink::Status status;
    m_dvbDemod->getStatus(status);
    const dvbt::OFDMDemod::Status& ofdm = status.m_receiver.m_ofdm;

    ui->channelPower->setText(QString("%1 dB").arg(status.m_power, 0, 'f', 1));

    // The channel marker shows the occupied bandwidth: 7.61 MHz in 8, or 7.77 MHz for DVB-T2's extended carrier mode, or
    // (1 + roll-off) times the symbol rate
    const bool extended = (status.m_standard == 2) && status.m_t2.m_l1Valid && status.m_t2.m_pre.m_bwtExt;
    const int markerBandwidth = m_settings.occupiedBandwidth(std::max(0.0, status.m_autoSymbolRate), extended);

    if (markerBandwidth != m_channelMarker.getBandwidth()) {
        m_channelMarker.setBandwidth(markerBandwidth);
    }

    QString state = status.stateText(m_settings.m_bandwidth);
    QString colour = state == "Locked" ? "lime" : (state == "No TS") || (state == "Generic") ? "yellow"
        : (state == "Sync") || (state == "TPS") || (state == "L1") || (state == "Carrier") || (state == "FEC") ? "orange" : "red";

    const QString styleSheet = QString("QLabel { color: %1; }").arg(colour);

    if (ui->state->text() != state) {
        ui->state->setText(state);
    }

    // Set only when it changes, as setting a style sheet restyles the widget
    if (ui->state->styleSheet() != styleSheet) {
        ui->state->setStyleSheet(styleSheet);
    }
    displayPlayerInfo(status.m_fifoOverflows);
    setStat(StatTSRate, "TS rate", QString("%1 Mb/s").arg(status.m_tsBitRate / 1e6, 0, 'f', 2), "Transport stream bit rate");

    if (status.m_standard == 2)
    {
        displayT2Status(status);
        return;
    }

    if (status.isSatellite() || DVBDemodSettings::isSatellite(m_settings.m_standard))
    {
        displaySatelliteStatus(status);
        return;
    }

    const bool synced = ofdm.m_state != dvbt::OFDMDemod::Acquire;
    setStat(StatMode, "Mode", synced ? QString("%1 %2").arg(dvbt::toString(ofdm.m_mode)).arg(dvbt::toString(ofdm.m_guard)) : QString("-"),
        synced && (ofdm.m_cellId >= 0) ? QString("FFT size and guard interval. Cell id %1").arg(ofdm.m_cellId, 4, 16, QChar('0'))
        : QString("FFT size and guard interval"));
    setStat(StatCFO, "CFO", synced ? QString("%1 Hz").arg(ofdm.m_frequencyOffset, 0, 'f', 0) + notchText(status.m_receiver.m_notches) : QString("-"),
        "Carrier frequency offset, and any narrowband interferers notched out (in kHz from the centre)");
    setStat(StatClock, "Clock", synced ? QString("%1 ppm").arg(ofdm.m_clockOffset, 0, 'f', 2) : QString("-"), "Sample clock offset");

    const bool tps = ofdm.m_tpsValid;
    const bool hierarchical = tps && (ofdm.m_tps.m_hierarchy != dvbt::Hierarchy::None);
    const int alpha = !hierarchical ? 1 : ofdm.m_tps.m_hierarchy == dvbt::Hierarchy::Alpha1 ? 1 : ofdm.m_tps.m_hierarchy == dvbt::Hierarchy::Alpha2 ? 2 : 4;

    if (hierarchical)
    {
        // The stream being decoded first
        const bool lp = status.m_receiver.m_lowPriority;
        setStat(StatModulation, "Modulation", QString("%1 %2 %3, %4 %5, a=%6")
            .arg(dvbt::toString(ofdm.m_tps.m_constellation))
            .arg(lp ? "LP" : "HP").arg(dvbt::toString(lp ? ofdm.m_tps.m_lpCodeRate : ofdm.m_tps.m_hpCodeRate))
            .arg(lp ? "HP" : "LP").arg(dvbt::toString(lp ? ofdm.m_tps.m_hpCodeRate : ofdm.m_tps.m_lpCodeRate))
            .arg(alpha),
            "Constellation, and the code rates of the stream being decoded and the other, of a hierarchical mode with constellation ratio a, from TPS");
    }
    else
    {
        setStat(StatModulation, "Modulation", tps ? QString("%1 %2").arg(dvbt::toString(ofdm.m_tps.m_constellation)).arg(dvbt::toString(ofdm.m_tps.m_hpCodeRate))
            : QString("-"), "Constellation and code rate, from TPS");
    }

    setStat(StatMER, "MER", tps ? QString("%1 dB").arg(status.m_receiver.m_mer, 0, 'f', 1) : QString("-"), "Modulation error ratio");
    setStat(StatSNR, "SNR", tps && (ofdm.m_snr != 0.0) ? QString("%1 dB").arg(ofdm.m_snr, 0, 'f', 1) : QString("-"),
        "Signal to noise ratio of the equalised cells, from the continual pilots. Unlike the MER, it isn't overestimated when noise takes cells past the nearest constellation point, so it's the one to compare with the C/N a mode needs");
    setStat(StatFEC1, "BER before Viterbi", tps ? QString::number(status.m_receiver.m_preViterbiBER, 'e', 1) : QString("-"),
        "Bit error rate before Viterbi decoding");
    setStat(StatFEC2, "BER after Viterbi", tps ? QString::number(status.m_receiver.m_postViterbiBER, 'e', 1) : QString("-"),
        "Bit error rate after Viterbi decoding. Below 2e-4 is error free");
    setStat(StatFEC3, "RS corrected", QString::number(status.m_receiver.m_outer.m_rsCorrected), "Packets with errors Reed-Solomon corrected");
    setStat(StatFEC4, "RS failed", QString::number(status.m_receiver.m_outer.m_rsFailed), "Packets with errors Reed-Solomon couldn't correct");

    int constellationSize = ofdm.m_tpsValid ? (1 << dvbt::bitsPerCell(ofdm.m_tps.m_constellation)) : 0;
    ui->scope->setPoints({}); // The grid of the constellation size, not a satellite standard's points
    ui->scope->setData(status.m_cells, status.m_responseDb, constellationSize, alpha);
    displayScopeExtras(status, tps ? status.m_receiver.m_mer : NAN, -1.0, status.m_receiver.m_outer.m_rsFailed);
}

// The impulse response, and the MER, LDPC iterations (negative for DVB-T, NaN if not known) and errors that couldn't
// be corrected since the last update for the history, every status update
void DVBDemodGUI::displayScopeExtras(const DVBDemodSink::Status& status, double mer, double iterations, uint64_t errors)
{
    ui->scope->setImpulseResponse(status.m_impulseDb, status.m_impulseResolution, status.m_impulseGuard,
        DVBDemodSettings::terrestrialSampleRate(m_settings.m_bandwidth));
    const bool sameCount = (status.m_standard == m_lastErrorStandard) && (errors >= m_lastErrorCount);
    const int newErrors = sameCount ? (int) std::min<uint64_t>(errors - m_lastErrorCount, 1000000) : 0;
    m_lastErrorCount = errors;
    m_lastErrorStandard = status.m_standard;
    ui->scope->addHistory(mer, iterations, newErrors, 0.2);
}

// A codec's name, short enough for the table: "H.264" of "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10", "MP2" of
// "MP2 (MPEG audio layer 2)"
QString DVBDemodGUI::shortCodecName(const QString& name)
{
    QString s = name.section(" / ", 0, 0);
    const int bracket = s.indexOf(" (");
    return bracket > 0 ? s.left(bracket) : s;
}

// The codecs and picture size in the table, the A/V problems, and the mute button green while audio is playing
void DVBDemodGUI::displayPlayerInfo(uint64_t tsOverflows)
{
    DTVVideoPlayer::Info info;

    if (m_player) {
        info = m_player->getInfo();
    }

    const bool video = m_player && !info.m_videoCodec.isEmpty();
    const bool audio = m_player && !info.m_audioCodec.isEmpty();
    setStat(StatVideo, "Video", !video ? QString("-")
            : shortCodecName(info.m_videoCodec) + (info.m_videoDecoder.isEmpty() ? QString() : QString(" (%1)").arg(info.m_videoDecoder)),
        !video ? QString("Video codec") : info.m_videoDecoder.isEmpty() ? QString("%1, decoded by the CPU").arg(info.m_videoCodec)
            : QString("%1, decoded by the GPU (%2)").arg(info.m_videoCodec).arg(info.m_videoDecoder));
    setStat(StatResolution, "Resolution", video && (info.m_width > 0) ? QString("%1x%2").arg(info.m_width).arg(info.m_height) : QString("-"),
        "Size of the decoded pictures (cropped as the stream says), before scaling for the display and the aspect ratio");
    setStat(StatAudio, "Audio", !audio ? QString("-") : shortCodecName(info.m_audioCodec) + (info.m_audioDescription ? " + AD" : ""),
        !audio ? QString("Audio codec") : info.m_audioCodec + (info.m_audioDescription ? ", with audio description mixed in" : ""));
    displayAVStats(info, tsOverflows);

    // Audio is decoded in bursts (a PES packet holds several frames), so some 200 ms have none, though it plays
    // continuously from the buffer: playing until there's been none for a second
    if (info.m_audioFrames != m_lastAudioFrames)
    {
        m_lastAudioFrames = info.m_audioFrames;
        m_audioFramesTimer.start();
    }

    const bool playing = audio && !m_settings.m_audioMute && m_audioFramesTimer.isValid() && (m_audioFramesTimer.elapsed() < 1000);

    if (playing != m_audioPlaying)
    {
        m_audioPlaying = playing;
        ui->audioMute->setStyleSheet(playing ? QString("QToolButton { background-color : green; }") : QString());
    }
}

void DVBDemodGUI::displayT2Status(const DVBDemodSink::Status& status)
{
    const dvbt2::Receiver::Status& t2 = status.m_t2;

    const bool synced = t2.m_ofdm.m_state != dvbt2::OFDMDemod::SearchP1;
    setStat(StatMode, "Mode", !synced ? QString("-") : QString("%1%2 %3%4%5")
            .arg(dvbt2::fftSizeString(t2.m_ofdm.m_fftSize))
            .arg(t2.m_l1Valid && t2.m_pre.m_bwtExt ? "E" : "")
            .arg(dvbt2::toString(t2.m_ofdm.m_guard))
            .arg(t2.m_l1Valid ? QString(" ") + dvbt2::toString(t2.m_pre.m_pilotPattern) : QString())
            .arg(QString(t2.m_ofdm.m_p1.isT2Lite() ? " Lite" : "") + (t2.m_ofdm.m_p1.isMISO() ? " MISO" : "")),
        synced && t2.m_l1Valid
            ? QString("FFT size (E for extended carriers), guard interval and pilot pattern, Lite for the T2-Lite profile, and MISO. Cell id %1, network id %2, T2 system id %3")
                .arg(t2.m_pre.m_cellId, 4, 16, QChar('0')).arg(t2.m_pre.m_networkId, 4, 16, QChar('0')).arg(t2.m_pre.m_t2SystemId, 4, 16, QChar('0'))
            : QString("FFT size and guard interval"));
    setStat(StatCFO, "CFO", synced ? QString("%1 Hz").arg(t2.m_ofdm.m_frequencyOffset, 0, 'f', 0) + notchText(t2.m_notches) : QString("-"),
        "Carrier frequency offset, and any narrowband interferers notched out (in kHz from the centre)");
    setStat(StatClock, "Clock", synced ? QString("%1 ppm").arg(t2.m_ofdm.m_clockOffset, 0, 'f', 2) : QString("-"), "Sample clock offset");

    int constellationSize = 0;
    QString modulation = "-";
    QString modulationTip = "Constellation, code rate and LDPC block length of the PLP";

    if (t2.m_l1Valid)
    {
        for (const dvbt2::PLP& plp : t2.m_post.m_plps)
        {
            if (plp.m_id == t2.m_plpId)
            {
                modulation = QString("%1 %2%3 %4")
                    .arg(dvbt2::toString(plp.m_constellation))
                    .arg(dvbt2::toString(plp.m_codeRate))
                    .arg(plp.m_rotation ? " R" : "")
                    .arg(plp.m_fecType == dvbt2::FECType::Normal ? "64K" : "16K");
                modulationTip = QString("PLP %1: constellation, code rate%2 and LDPC block length")
                    .arg(plp.m_id).arg(plp.m_rotation ? ", rotated (R)" : "");
                constellationSize = 1 << dvbt2::bitsPerCell(plp.m_constellation);
            }
        }
    }

    setStat(StatModulation, "Modulation", modulation, modulationTip);
    setStat(StatMER, "MER", t2.m_l1Valid ? QString("%1 dB").arg(t2.m_mer, 0, 'f', 1) : QString("-"), "Modulation error ratio, of the PLP's cells");
    setStat(StatSNR, "SNR", synced && (t2.m_ofdm.m_snr != 0.0) ? QString("%1 dB").arg(t2.m_ofdm.m_snr, 0, 'f', 1) : QString("-"),
        "Signal to noise ratio of the data symbols, from the pilots. Unlike the MER, it isn't overestimated when noise takes cells past the nearest constellation point, so it's the one to compare with the C/N a mode needs");
    setStat(StatFEC1, "LDPC iterations", t2.m_fecBlocks > 0 ? QString::number(t2.m_ldpcIterations, 'f', 1) : QString("-"),
        "Average LDPC decoder iterations, of the slowest of each group of blocks decoded together. Up to 50 are tried");
    setStat(StatFEC2, "FEC blocks", QString::number(t2.m_fecBlocks), "FEC blocks decoded");

    if (DVBDemodSink::Status::isGeneric(t2.m_bb)) {
        setStat(StatTSRate, "Stream", dvbt2::BBFrameDecoder::streamTypeName(t2.m_bb.m_streamType), "Not a transport stream, so not decoded or played: a generic stream (GSE carries IP). From the BBFRAMEs' headers");
    }
    setStat(StatFEC3, "BCH failed", QString::number(t2.m_bchFailures), "FEC blocks with errors BCH couldn't correct");
    setStat(StatFEC4, "L1 errors", QString::number(t2.m_l1Errors), "L1 signalling that failed its CRC");
    ui->scope->setPoints({});
    ui->scope->setData(status.m_cells, status.m_responseDb, constellationSize);
    displayScopeExtras(status, t2.m_l1Valid ? t2.m_mer : NAN, t2.m_fecBlocks > 0 ? t2.m_ldpcIterations : NAN, t2.m_bchFailures);
    displayPLPs(t2);
}

void DVBDemodGUI::displaySatelliteStatus(const DVBDemodSink::Status& status)
{
    const bool s2 = status.m_standard == 6;
    const dvbs::FrontEnd::Status& frontEnd = s2 ? status.m_s2.m_frontEnd : status.m_s.m_frontEnd;
    // The equaliser's correction of the receive chain's response, as the power gain half the symbol rate from the centre
    const QString equaliser = frontEnd.m_equalising ? QString(", EQ %1%2").arg(frontEnd.m_equaliserEdgeDb >= 0.0 ? "+" : "")
        .arg(frontEnd.m_equaliserEdgeDb, 0, 'f', 1) : QString();
    const QString equaliserTip = " EQ: the equaliser's correction of the receive chain's response (the device's filter, the transponder's), "
        "in dB of power half the symbol rate from the centre, learnt once locked. Without it, the signal's edges cut by this much "
        "would cost 1 to 2 dB of SNR";
    const bool measured = !frontEnd.m_estimating && (status.m_standard != 4);
    if (m_settings.m_symbolRate > 0)
    {
        setStat(StatMode, "Symbol rate", QString("%1 kS/s%2").arg(m_settings.m_symbolRate / 1e3, 0, 'f', 1)
                .arg(status.m_signalSymbolRate > 0.0 ? QString(" (signal %1)").arg(status.m_signalSymbolRate / 1e3, 0, 'f', 0)
                    : frontEnd.m_measuredSymbolRate > 0.0 ? QString(" (%1)").arg(frontEnd.m_measuredSymbolRate / 1e3, 0, 'f', 0) : QString()),
            "Symbol rate set, and in brackets as measured from the spectrum's width (which shows if it's set wrong), when the sample rate "
            "leaves room around the signal to see the noise floor (from about 1.7 times the symbol rate). If nothing locks for 5 s, the "
            "signal's rate is measured (as Auto does): if it's another, it's shown as the signal's, and the state is Wrong SR");
        setStatWarning(StatMode, status.m_signalSymbolRate > 0.0);
    }
    else
    {
        setStat(StatMode, "Symbol rate", status.m_autoSymbolRate > 0.0 ? QString("Auto: %1 kS/s").arg(status.m_autoSymbolRate / 1e3, 0, 'f', 1)
                : QString("Auto: searching"),
            "Symbol rate found from the signal (the strongest in the channel), which is searched for again if nothing locks for 10 s");
    }
    const double frequency = s2 ? status.m_s2.m_frequencyOffset : status.m_s.m_frequencyOffset;
    const double clock = s2 ? status.m_s2.m_clockOffset : status.m_s.m_clockOffset;
    setStat(StatCFO, "CFO", measured ? QString("%1 Hz").arg(frequency, 0, 'f', 0) : QString("-"), "Carrier frequency offset from the channel's centre");
    setStat(StatClock, "Clock", measured ? QString("%1 ppm").arg(clock, 0, 'f', 1) : QString("-"), "Symbol rate's offset from that set");
    std::vector<std::complex<float>> points;
    const float a = 0.70710678f;

    // The spectrum relative to its plateau (the 80th percentile), so the signal's at 0 dB and the floor below
    std::vector<float> spectrum = status.m_responseDb;

    if (!spectrum.empty())
    {
        std::vector<float> sorted(spectrum);
        std::sort(sorted.begin(), sorted.end());
        const float plateau = sorted[sorted.size() * 8 / 10];

        for (float& db : spectrum) {
            db -= plateau;
        }
    }

    if (!s2)
    {
        const dvbs::Receiver::Status& s = status.m_s;
        const bool fecLocked = s.m_fec.m_state == dvbs::FECDecoder::State::Locked;
        setStat(StatModulation, "Modulation", (s.m_fec.m_codeRateKnown ? QString("QPSK %1").arg(dvbt::toString(s.m_fec.m_codeRate)) : QString("QPSK"))
                + (fecLocked && s.m_fec.m_inverted ? " inv" : ""),
            "Constellation and code rate (found by trying each, if automatic), and inv if the spectrum's inverted (found as the code rate is)");
        setStat(StatMER, "MER", s.m_carrierLocked ? QString("%1 dB").arg(s.m_mer, 0, 'f', 1) : QString("-"),
            QString("Modulation error ratio, from the decisions. Carrier lock %1 (the coherence of the symbols' fourth power: 1 without noise, "
                "about 0.3 at the lowest Es/N0 DVB-S works at, 0 unlocked)").arg(s.m_carrierLock, 0, 'f', 2));
        setStat(StatSNR, "SNR", s.m_carrierLocked ? QString("%1 dB").arg(s.m_snr, 0, 'f', 1) + equaliser : QString("-"),
            "Signal to noise ratio (Es/N0), from the symbols' second and fourth moments. Unlike the MER, it isn't overestimated near the threshold, "
            "where noise takes symbols past the nearest constellation point." + equaliserTip);
        setStat(StatFEC1, "BER before Viterbi", s.m_fec.m_state == dvbs::FECDecoder::State::Locked ? QString::number(s.m_fec.m_preViterbiBER, 'e', 1) : QString("-"),
            "Bit error rate before Viterbi decoding");
        setStat(StatFEC2, "BER after Viterbi", s.m_fec.m_state == dvbs::FECDecoder::State::Locked ? QString::number(s.m_fec.m_postViterbiBER, 'e', 1) : QString("-"),
            "Bit error rate after Viterbi decoding. Below 2e-4 is error free");
        setStat(StatFEC3, "RS corrected", QString::number(s.m_fec.m_outer.m_rsCorrected), "Packets with errors Reed-Solomon corrected");
        setStat(StatFEC4, "RS failed", QString::number(s.m_fec.m_outer.m_rsFailed), "Packets with errors Reed-Solomon couldn't correct");
        points = {{a, a}, {a, -a}, {-a, a}, {-a, -a}};
        ui->scope->setPoints(points);
        ui->scope->setData(status.m_cells, spectrum, 0);
        displayScopeExtras(status, s.m_carrierLocked ? s.m_mer : NAN, -1.0, s.m_fec.m_outer.m_rsFailed);
        return;
    }

    const dvbs2::Receiver::Status& s = status.m_s2;
    const bool locked = s.m_pl.m_state == dvbs2::PLDecoder::State::Locked;
    dvbs2::Modulation modulation;
    dvbt2::CodeRate rate;
    const bool known = dvbs2::modcodInfo(s.m_pl.m_modcod, modulation, rate);
    QString modcod = "-";
    const QString stream = !s.m_multipleStreams ? QString() : s.m_pl.m_timeSlicing ? QString(" TSN %1").arg(s.m_isi) : QString(" ISI %1").arg(s.m_isi);

    if (s.m_pl.m_vlsnr >= 0)
    {
        // A VL-SNR frame: its MODCOD from its VL-SNR header, its cells pi/2 BPSK's turned to the real axis (or QPSK's)
        const dvbs2::VLSNRModcod& vl = dvbs2::vlsnrModcod(s.m_pl.m_vlsnr);
        modcod = QString("VL-SNR %1 %2%3%4").arg(vl.m_name)
            .arg(vl.m_type == dvbt2::FECType::Short ? "16K" : vl.m_type == dvbt2::FECType::Medium ? "32K" : "64K")
            .arg(s.m_acm ? " VCM" : "")
            .arg(stream);

        if (vl.m_modulation == dvbs2::Modulation::BPSK) {
            points = {{1.0f, 0.0f}, {-1.0f, 0.0f}};
        } else {
            dvbs2::constellation(vl.m_modulation, vl.m_rate, points, vl.m_type);
        }
    }
    else if (known)
    {
        modcod = QString("%1 %2%3%4%5").arg(dvbs2::modcodString(s.m_pl.m_modcod))
            .arg((s.m_pl.m_type & 2) ? "16K" : "64K")
            .arg((s.m_pl.m_type & 1) ? " P" : "")
            .arg(s.m_acm ? " VCM" : "")
            .arg(stream);
        dvbs2::constellation(modulation, rate, points, (s.m_pl.m_type & 2) ? dvbt2::FECType::Short : dvbt2::FECType::Normal);
    }

    setStat(StatModulation, "MODCOD", modcod,
        QString("Modulation, code rate, FECFRAME size (64K normal, 16K short), P with pilots, VCM (or ACM) and the input stream decoded "
            "(TSN: the time slice, in EN 302 307-1 Annex M's time slicing). DVB-S2X's MODCODs by their canonical names (-L: a low SNR variant; "
            "VL-SNR: a very low SNR frame's, 32K its medium FECFRAME, BPSK-S spread over two symbols). Roll-off %1")
            .arg(s.m_rollOff, 0, 'f', 2));
    setStat(StatMER, "MER", locked ? QString("%1 dB").arg(s.m_pl.m_mer, 0, 'f', 1) : QString("-"), "Modulation error ratio of the data symbols");
    setStat(StatSNR, "SNR", locked ? QString("%1 dB").arg(s.m_pl.m_esn0, 0, 'f', 1) + equaliser : QString("-"),
        "Es/N0 from the known symbols (headers and pilots). Unlike the MER, it isn't overestimated near the threshold, so it's the one to compare "
        "with what a MODCOD needs." + equaliserTip);
    setStat(StatFEC1, "LDPC iterations", s.m_fecFrames > 0 ? QString::number(s.m_ldpcIterations, 'f', 1) : QString("-"),
        "Average LDPC decoder iterations, of the slowest of each group of frames decoded together. Up to 50 are tried");
    setStat(StatFEC2, "Frames", QString("%1 (%2 dummy)").arg(s.m_pl.m_frames).arg(s.m_pl.m_dummyFrames), "Physical layer frames, and dummy frames among them");
    setStat(StatFEC3, "BCH failed", QString::number(s.m_bchFailures), "Frames with errors BCH couldn't correct");
    setStat(StatFEC4, "Header misses", QString::number(s.m_pl.m_headerMisses), "Frame headers expected but not found");

    if (DVBDemodSink::Status::isGeneric(s.m_bb)) {
        setStat(StatTSRate, "Stream", dvbt2::BBFrameDecoder::streamTypeName(s.m_bb.m_streamType), "Not a transport stream, so not decoded or played: a generic stream (GSE carries IP). From the BBFRAMEs' headers");
    }
    ui->scope->setPoints(points);
    ui->scope->setData(status.m_cells, spectrum, 0);
    displayScopeExtras(status, locked ? s.m_pl.m_mer : NAN, s.m_fecFrames > 0 ? s.m_ldpcIterations : NAN, s.m_bchFailures);
    displayISIs(s);
}

// Add the input streams seen to the combo box
void DVBDemodGUI::displayISIs(const dvbs2::Receiver::Status& status)
{
    if (!m_listingISIs) {
        return;
    }

    for (int isi : status.m_isis)
    {
        if (!m_isiIds.contains(isi))
        {
            m_isiIds.append(isi);
            ui->plp->blockSignals(true);
            ui->plp->addItem(QString::number(isi));
            ui->plp->blockSignals(false);
        }
    }
}

// Add the PLPs signalled to the combo box
void DVBDemodGUI::displayPLPs(const dvbt2::Receiver::Status& status)
{
    if (m_listingISIs) {
        return;
    }

    if (!status.m_l1Valid) {
        return;
    }

    for (const dvbt2::PLP& plp : status.m_post.m_plps)
    {
        if (!m_plpIds.contains(plp.m_id))
        {
            m_plpIds.append(plp.m_id);
            ui->plp->blockSignals(true);
            ui->plp->addItem(QString("%1%2").arg(plp.m_id).arg(plp.m_type == 0 ? " (common)" : ""));
            ui->plp->blockSignals(false);
        }
    }
}

void DVBDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DVBDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->bandwidth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_bandwidth_currentIndexChanged);
    QObject::connect(ui->standard, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_standard_currentIndexChanged);
    QObject::connect(ui->plp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_plp_currentIndexChanged);
    QObject::connect(ui->priority, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_priority_currentIndexChanged);
    QObject::connect(ui->symbolRate->lineEdit(), &QLineEdit::editingFinished, this, &DVBDemodGUI::on_symbolRate_editingFinished);
    QObject::connect(ui->symbolRate, QOverload<int>::of(&QComboBox::activated), this, [this](int) { on_symbolRate_editingFinished(); });
    QObject::connect(ui->rollOff, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_rollOff_currentIndexChanged);
    QObject::connect(ui->codeRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_codeRate_currentIndexChanged);
    QObject::connect(ui->plScrambling, QOverload<int>::of(&QSpinBox::valueChanged), this, &DVBDemodGUI::on_plScrambling_valueChanged);
    QObject::connect(ui->udpEnabled, &QCheckBox::toggled, this, &DVBDemodGUI::on_udpEnabled_toggled);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &DVBDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &DVBDemodGUI::on_udpPort_editingFinished);
    QObject::connect(ui->videoEnabled, &ButtonSwitch::toggled, this, &DVBDemodGUI::on_videoEnabled_toggled);
    QObject::connect(ui->service, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_service_currentIndexChanged);
    QObject::connect(ui->epg, &QToolButton::clicked, this, &DVBDemodGUI::on_epg_clicked);
    QObject::connect(ui->apps, &QToolButton::clicked, this, &DVBDemodGUI::on_apps_clicked);
    QObject::connect(ui->redButton, &QToolButton::clicked, this, &DVBDemodGUI::hbbtvRed);
    ui->apps->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->apps, &QToolButton::customContextMenuRequested, this, &DVBDemodGUI::appsMenu);
    ui->epg->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->epg, &QToolButton::customContextMenuRequested, this, &DVBDemodGUI::epgMenu);
    QObject::connect(ui->scan, &QToolButton::clicked, this, &DVBDemodGUI::on_scan_clicked);
    QObject::connect(ui->simulcast, &QToolButton::clicked, this, &DVBDemodGUI::on_simulcast_clicked);
    QObject::connect(ui->record, &QToolButton::toggled, this, &DVBDemodGUI::on_record_toggled);
    ui->record->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->record, &QToolButton::customContextMenuRequested, this, &DVBDemodGUI::recordMenu);
    QObject::connect(ui->deinterlace, &ButtonSwitch::toggled, this, &DVBDemodGUI::on_deinterlace_toggled);
    QObject::connect(ui->equaliser, &ButtonSwitch::toggled, this, &DVBDemodGUI::on_equaliser_toggled);
    QObject::connect(ui->hardwareDecoding, &ButtonSwitch::toggled, this, &DVBDemodGUI::on_hardwareDecoding_toggled);
    QObject::connect(ui->audioTrack, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_audioTrack_currentIndexChanged);
    QObject::connect(ui->subtitleTrack, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DVBDemodGUI::on_subtitleTrack_currentIndexChanged);
    QObject::connect(ui->avOffset, QOverload<int>::of(&QSpinBox::valueChanged), this, &DVBDemodGUI::on_avOffset_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &DVBDemodGUI::on_audioMute_toggled);
    QObject::connect(ui->audioVolume, &QDial::valueChanged, this, &DVBDemodGUI::on_audioVolume_valueChanged);
}

void DVBDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
