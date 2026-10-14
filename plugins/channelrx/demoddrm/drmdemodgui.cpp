///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
#include <cmath>
#include <limits>

#include <QSignalBlocker>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QFontMetrics>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTableWidgetItem>

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "gui/crightclickenabler.h"
#include "util/db.h"
#include "maincore.h"
#include "plugin/pluginapi.h"
#include "ui_drmdemodgui.h"

#include "drmdemodgui.h"
#include "drmwebsitebrowser.h"


// SDC entity 9 coding (ES 201 980 clause 6.4.3.10) as a short description.
QString DRMDemodGUI::audioFormatText(const DRMServiceStatus& service)
{
    if (service.audioCoding < 0) {
        return tr("no audio descriptor");
    }

    QString coding;
    QString rate;
    QString channels;
    if (service.audioCoding == 0)
    {
        coding = service.sbr ? QStringLiteral("AAC+SBR") : QStringLiteral("AAC");
        static constexpr const char* rates[8] = {nullptr, "12 kHz", nullptr, "24 kHz", nullptr, "48 kHz", nullptr, nullptr};
        if (service.samplingRateCode >= 0 && service.samplingRateCode < 8 && rates[service.samplingRateCode]) {
            rate = rates[service.samplingRateCode];
        }

        static constexpr const char* modes[4] = {"mono", "parametric stereo", "stereo", nullptr};
        if (service.audioMode >= 0 && service.audioMode < 4 && modes[service.audioMode]) {
            channels = modes[service.audioMode];
        }
    }
    else if (service.audioCoding == 3)
    {
        coding = QStringLiteral("xHE-AAC");
        static constexpr const char* rates[8] = {nullptr, nullptr, "16 kHz", "19.2 kHz", "24 kHz", "32 kHz", "38.4 kHz", "48 kHz"};
        if (service.samplingRateCode >= 0 && service.samplingRateCode < 8 && rates[service.samplingRateCode]) {
            rate = rates[service.samplingRateCode];
        }

        if (service.audioMode == 0) {
            channels = QStringLiteral("mono");
        } else if (service.audioMode == 2) {
            channels = QStringLiteral("stereo");
        }
    }
    else
    {
        return tr("reserved coding %1 (unsupported)").arg(service.audioCoding);
    }

    QStringList parts{coding};
    parts << (rate.isEmpty() ? tr("reserved rate") : rate);
    parts << (channels.isEmpty() ? tr("reserved mode") : channels);
    return parts.join(QStringLiteral(", "));
}

QString DRMDemodGUI::afsTypeName(int type)
{
    switch (type)
    {
    case 3:
        return tr("DRM frequencies");
    case 4:
        return tr("Schedule");
    case 7:
        return tr("Region");
    case 11:
        return tr("Other services");
    case 13:
        return tr("Detailed region");
    case 15:
        return tr("Service linking");
    default:
        return tr("Type %1").arg(type);
    }
}

// Entity 3 frequency codes: msb selects 10 kHz units (mode E) instead of 1 kHz.
QString DRMDemodGUI::afsFrequencies(const DRMAFSStatus& afs)
{
    if (afs.type != 3) {
        return afs.frequencyCodes.empty() ? QStringLiteral("-") : tr("%1 codes").arg(afs.frequencyCodes.size());
    }

    QStringList frequencies;
    for (std::uint16_t code : afs.frequencyCodes)
    {
        const int kHz = static_cast<int>(code & 0x7fffU) * ((code & 0x8000U) ? 10 : 1);
        frequencies << QStringLiteral("%1 kHz").arg(kHz);
    }

    return frequencies.isEmpty() ? QStringLiteral("-") : frequencies.join(QStringLiteral(", "));
}

QString DRMDemodGUI::afsDetails(const DRMAFSStatus& afs)
{
    QStringList details;
    details << tr("list %1").arg(afs.listVersion ? 1 : 0);
    if (afs.type == 3)
    {
        details << (afs.synchronousMultiplex ? tr("synchronous") : tr("non-synchronous"));
        details << (afs.baseLayer ? tr("base layer") : tr("enhancement layer"));
        if (afs.appliesToAllServices) {
            details << tr("all services");
        }
        else if (afs.serviceRestrictionField >= 0)
        {
            QStringList shortIds;
            for (int shortId = 0; shortId < 4; ++shortId)
            {
                if (afs.serviceRestrictionField & (0x10 << shortId)) {
                    shortIds << QString::number(shortId);
                }
            }

            details << tr("short IDs %1").arg(shortIds.join(QStringLiteral(",")));
        }

        if (afs.regionScheduleField >= 0)
        {
            details << tr("region %1, schedule %2")
                .arg(afs.regionScheduleField >> 4).arg(afs.regionScheduleField & 0x0f);
        }
    }

    if (afs.extension >= 0) {
        details << tr("extension %1").arg(afs.extension);
    }

    return details.join(QStringLiteral(", "));
}

DRMDemodGUI* DRMDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet* deviceUISet, BasebandSampleSink* rxChannel)
{
    return new DRMDemodGUI(pluginAPI, deviceUISet, rxChannel);
}

DRMDemodGUI::DRMDemodGUI(
    PluginAPI* pluginAPI,
    DeviceUISet* deviceUISet,
    BasebandSampleSink* rxChannel,
    QWidget* parent
) :
    ChannelGUI(parent),
    ui(new Ui::DRMDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_drmDemod(static_cast<DRMDemod*>(rxChannel)),
    m_channelMarker(this)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demoddrm/readme.md";
    RollupContents* contents = getRollupContents();
    ui->setupUi(contents);
    ui->frequencyOffset->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->frequencyOffset->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);
    ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    setupStatusTable();
    ui->afsTable->verticalHeader()->setVisible(false);
    ui->afsTable->horizontalHeader()->setStretchLastSection(true);
    ui->afsTable->setSelectionMode(QAbstractItemView::NoSelection);
    auto* audioRightClick = new CRightClickEnabler(ui->audioMute);
    connect(audioRightClick, &CRightClickEnabler::rightClick, this, &DRMDemodGUI::audioSelect);
    setSizePolicy(contents->sizePolicy());
    contents->arrangeRollups();

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true);
    m_deviceUISet->addChannelMarker(&m_channelMarker);
    m_drmDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(contents, SIGNAL(widgetRolled(QWidget*,bool)),
        this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
        this, SLOT(onMenuDialogCalled(const QPoint&)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()),
        this, SLOT(handleInputMessages()));
    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()),
        this, SLOT(tick()));
    connect(&m_channelMarker, SIGNAL(changedByCursor()),
        this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()),
        this, SLOT(channelMarkerHighlightedByCursor()));
    makeUIConnections();

    // A channel can be opened after the device's last signal notification.
    auto* deviceAPI = m_drmDemod->getDeviceAPI();
    const auto frequencies = deviceAPI->getCenterFrequency();
    if (!frequencies.isEmpty()) {
        m_deviceCenterFrequency = frequencies.front();
    }

    if (auto* source = deviceAPI->getSampleSource()) {
        m_basebandSampleRate = source->getSampleRate();
    } else if (auto* mimo = deviceAPI->getSampleMIMO()) {
        m_basebandSampleRate = mimo->getSourceSampleRate(m_settings.m_streamIndex);
    }

    if (m_basebandSampleRate > 0) {
        ui->frequencyOffset->setValueRange(false, 7, -m_basebandSampleRate / 2, m_basebandSampleRate / 2);
    }

    displaySettings();
    applySettings(QStringList(), true);
    m_resizer.enableChildMouseTracking();
}

DRMDemodGUI::~DRMDemodGUI()
{
    delete ui;
}

void DRMDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray DRMDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool DRMDemodGUI::deserialize(const QByteArray& data)
{
    if (!m_settings.deserialize(data))
    {
        resetToDefaults();
        return false;
    }

    displaySettings();
    applySettings(QStringList(), true);
    return true;
}

bool DRMDemodGUI::handleMessage(const Message& message)
{
    if (DRMDemod::MsgConfigureDRMDemod::match(message))
    {
        const auto& configure = static_cast<const DRMDemod::MsgConfigureDRMDemod&>(message);
        if (configure.getForce()) {
            m_settings = configure.getSettings();
        } else {
            m_settings.applySettings(configure.getSettingsKeys(), configure.getSettings());
        }

        m_settings.sanitize();
        displaySettings();
        return true;
    }

    if (DSPSignalNotification::match(message))
    {
        const auto& notification = static_cast<const DSPSignalNotification&>(message);
        m_deviceCenterFrequency = notification.getCenterFrequency();
        m_basebandSampleRate = notification.getSampleRate();
        ui->frequencyOffset->setValueRange(false, 7, -m_basebandSampleRate / 2, m_basebandSampleRate / 2);
        ui->frequencyOffsetLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate / 2));
        updateAbsoluteCenterFrequency();
        updateChannelMarker(m_drmDemod->getStatus());
        return true;
    }

    return false;
}

void DRMDemodGUI::handleInputMessages()
{
    while (Message* message = getInputMessageQueue()->pop())
    {
        handleMessage(*message);
        delete message;
    }
}

void DRMDemodGUI::applySettings(const QStringList& keys, bool force)
{
    if (!m_doApplySettings) {
        return;
    }

    m_drmDemod->getInputMessageQueue()->push(
        DRMDemod::MsgConfigureDRMDemod::create(m_settings, keys, force)
    );
}

// A change can change another value (a forced occupancy the forced mode does
// not define becomes Auto): it is shown, then the marker drawn, and applied
// with the change.
void DRMDemodGUI::applySanitizedSettings(QStringList keys)
{
    const DRMDemodSettings requested = m_settings;
    m_settings.sanitize();
    const QStringList dependent = m_settings.sanitizedKeys(requested);
    for (const QString& key : dependent)
    {
        if (!keys.contains(key)) {
            keys.append(key);
        }
    }

    if (!dependent.isEmpty()) {
        displaySettings();
    }

    updateChannelMarker(m_drmDemod->getStatus());
    applySettings(keys);
}

void DRMDemodGUI::displaySettings()
{
    blockApplySettings(true);
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.clearStreamIndexes();
    m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    updateIndexLabel();
    setTitle(m_settings.m_title);
    setWindowTitle(m_settings.m_title);
    setTitleColor(m_settings.m_rgbColor);
    {
        const QSignalBlocker blocker(ui->frequencyOffset);
        ui->frequencyOffset->setValue(m_settings.m_inputFrequencyOffset);
    }

    ui->searchRange->setValue(m_settings.m_acquisitionSearchRangeHz);
    ui->forcedMode->setCurrentIndex(m_settings.m_forcedMode + 1);
    ui->forcedOccupancy->setCurrentIndex(m_settings.m_forcedOccupancy + 1);
    {
        const QSignalBlocker blocker(ui->volume);
        ui->volume->setValue(static_cast<int>(std::lround(m_settings.m_volume * 10.0)));
    }

    ui->volumeText->setText(QString::number(m_settings.m_volume, 'f', 1));
    ui->audioMute->setChecked(m_settings.m_audioMute);
    for (int index = 0; index < ui->service->count(); ++index)
    {
        if (ui->service->itemData(index).toLongLong() == m_settings.m_selectedServiceIdentifier)
        {
            ui->service->setCurrentIndex(index);
            break;
        }
    }

    getRollupContents()->restoreState(m_rollupState);
    const auto status = m_drmDemod->getStatus();
    updateServices(status);
    updateAbsoluteCenterFrequency();
    updateChannelMarker(status);
    blockApplySettings(false);
}

void DRMDemodGUI::updateServices(const DRMDemodStatus& status)
{
    QStringList labels;
    for (const auto identifier : status.serviceIdentifiers)
    {
        const auto service = std::find_if(status.services.begin(), status.services.end(),
            [identifier](const DRMServiceStatus& item) { return item.serviceIdentifier == identifier; });
        labels.append(service == status.services.end() ? QString() : service->label);
    }

    if (status.serviceIdentifiers == m_displayedServices && labels == m_displayedServiceLabels
        && m_displayedSelection == m_settings.m_selectedServiceIdentifier) {
        return;
    }

    // Rebuilding the list closes an open popup; defer until it is closed.
    if (ui->service->view() && ui->service->view()->isVisible()) {
        return;
    }

    m_displayedServices = status.serviceIdentifiers;
    m_displayedServiceLabels = labels;
    m_displayedSelection = m_settings.m_selectedServiceIdentifier;
    const qint64 selected = m_settings.m_selectedServiceIdentifier;
    ui->service->blockSignals(true);
    ui->service->clear();
    ui->service->addItem(tr("Automatic"), -1LL);
    for (std::uint32_t identifier : status.serviceIdentifiers)
    {
        QString label;
        const auto service = std::find_if(
            status.services.begin(), status.services.end(),
            [identifier](const DRMServiceStatus& item) {
                return item.serviceIdentifier == identifier;
            }
        );
        if (service != status.services.end()) {
            label = service->label;
        }

        const QString id = QStringLiteral("%1").arg(identifier, 6, 16, QLatin1Char('0')).toUpper();
        ui->service->addItem(label.isEmpty() ? id : label + " (" + id + ")",
            static_cast<qint64>(identifier));
    }

    int selectedIndex = 0;
    for (int index = 0; index < ui->service->count(); ++index)
    {
        if (ui->service->itemData(index).toLongLong() == selected) {
            selectedIndex = index;
        }
    }

    if (selected >= 0 && selectedIndex == 0)
    {
        const QString id = QStringLiteral("%1").arg(selected, 6, 16, QLatin1Char('0')).toUpper();
        ui->service->addItem(tr("Unavailable (%1)").arg(id), selected);
        selectedIndex = ui->service->count() - 1;
    }

    ui->service->setCurrentIndex(selectedIndex);
    ui->service->blockSignals(false);
}

void DRMDemodGUI::setupStatusTable()
{
    // Name and value columns, rows alternating in colour.
    static constexpr const char* names[StatusRowCount] = {
        QT_TR_NOOP("Sync"), QT_TR_NOOP("Mode"), QT_TR_NOOP("Occupancy"), QT_TR_NOOP("Spectrum"), QT_TR_NOOP("Carrier offset"),
        QT_TR_NOOP("FAC"), QT_TR_NOOP("SDC"), QT_TR_NOOP("MSC"), QT_TR_NOOP("Audio"),
        QT_TR_NOOP("Audio format"), QT_TR_NOOP("Broadcast time"), QT_TR_NOOP("Sample clock"),
        QT_TR_NOOP("MSC MER"), QT_TR_NOOP("MSC frames"), QT_TR_NOOP("Audio superframes")
    };
    QTableWidget* table = ui->status;
    table->setColumnCount(2);
    table->setRowCount(StatusRowCount);
    const QFontMetrics metrics(table->font());
    const int rowHeight = metrics.height() + 4;
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(rowHeight);
    table->setColumnWidth(0, metrics.horizontalAdvance(tr("Audio superframes")) + 12);
    table->horizontalHeader()->setStretchLastSection(true);
    for (int row = 0; row < StatusRowCount; ++row)
    {
        table->setItem(row, 0, new QTableWidgetItem(tr(names[row])));
        table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("-")));
    }

    setStatus(StatusSync, tr("stopped"));
    setStatus(StatusFAC, tr("waiting"));
    setStatus(StatusSDC, tr("waiting"));
    setStatus(StatusMSC, tr("waiting"));
    setStatus(StatusAudio, tr("inactive"));
    setStatus(StatusMSCFrames, tr("0 valid / 0 invalid"));
    setStatus(StatusAudioFrames, tr("0 valid / 0 invalid"));
    setStatusToolTip(StatusAudioFormat, tr("Selected service's coding from the SDC, and the decoder output rate and channels."));
    setStatusToolTip(StatusMSCMER, tr("Decision-directed modulation error ratio and cell count for the latest bounded MSC decode."));
    // Every row shown without scrolling.
    table->setMinimumHeight(StatusRowCount * rowHeight + 2 * table->frameWidth());
}

void DRMDemodGUI::setStatus(StatusRow row, const QString& value)
{
    QTableWidgetItem* item = ui->status->item(row, 1);
    if (item->text() != value) {
        item->setText(value);
    }
}

void DRMDemodGUI::setStatusToolTip(StatusRow row, const QString& toolTip)
{
    for (int column = 0; column < 2; ++column)
    {
        QTableWidgetItem* item = ui->status->item(row, column);
        if (item->toolTip() != toolTip) {
            item->setToolTip(toolTip);
        }
    }
}

void DRMDemodGUI::displayStatus(const DRMDemodStatus& status)
{
    updateChannelMarker(status);
    setStatus(StatusSync, status.syncState);
    setStatus(StatusMode, status.mode == '?' ? QStringLiteral("-") : QString(QChar(status.mode)));
    setStatus(StatusOccupancy, status.occupancy < 0 ? QStringLiteral("-") : QString::number(status.occupancy));
    setStatus(StatusSpectrum, status.spectrumInverted < 0 ? QStringLiteral("-") : status.spectrumInverted ? tr("mirrored (I/Q swapped)") : tr("normal"));
    setStatus(StatusCarrierOffset, status.carrierOffsetAvailable ? QString::number(status.carrierOffsetHz, 'f', 1) + " Hz" : QStringLiteral("-"));
    setStatus(StatusSampleClock, status.sampleClockErrorAvailable ? QString::number(status.sampleClockErrorPPM, 'f', 2) + " ppm" : QStringLiteral("-"));
    setStatus(StatusMSCMER, status.mscMERAvailable
        ? QString::number(status.mscMERDB, 'f', 1) + " dB / "
            + QString::number(status.mscMERCellCount) + tr(" cells")
        : QStringLiteral("-"));
    setStatus(StatusFAC, status.facValid ? tr("valid") : tr("waiting"));
    setStatus(StatusSDC, status.sdcValid ? tr("valid") : tr("waiting"));
    setStatusToolTip(StatusFAC, tr("%1 valid / %2 invalid FAC slots in this RF epoch\nLatest observation age: %3 ms")
        .arg(status.validFACBlocks).arg(status.invalidFACBlocks).arg(status.facAgeMs));
    setStatusToolTip(StatusSDC, tr("%1 valid / %2 invalid / %3 skipped SDC blocks in this RF epoch\nLatest attempt age: %4 ms")
        .arg(status.validSDCBlocks).arg(status.invalidSDCBlocks)
        .arg(status.skippedSDCBlocks).arg(status.sdcAgeMs));
    setStatus(StatusMSC, status.mscReady ? tr("ready") : tr("waiting"));
    setStatus(StatusAudio, status.audioActive ? tr("playing") : tr("inactive"));
    const bool playing = status.audioActive && !m_settings.m_audioMute;
    if (playing != m_audioPlaying)
    {
        ui->audioMute->setStyleSheet(playing
            ? "QToolButton { background-color : green; }"
            : "QToolButton { background:rgb(79,79,79); }");
        m_audioPlaying = playing;
    }

    // The text box shows the reason only when there is no text message, so
    // the audio state carries it too.
    QString audioTip = tr("Latest decoded PCM age: %1 ms (-1 before decoding)").arg(status.audioAgeMs);
    if (!status.audioActive && !status.error.isEmpty()) {
        audioTip += tr("\nNo audio: %1").arg(status.error);
    }

    setStatusToolTip(StatusAudio, audioTip);
    setStatus(StatusMSCFrames, tr("%1 valid / %2 invalid")
        .arg(status.decodedMultiplexFrames)
        .arg(status.invalidMultiplexFrames));
    setStatus(StatusAudioFrames, tr("%1 valid / %2 invalid")
        .arg(status.parsedAudioSuperframes)
        .arg(status.invalidAudioSuperframes));
    setStatusToolTip(StatusAudioFrames, tr("PCM frames: %1 decoded, %2 concealed\nAudio samples dropped: %3\nInput samples dropped: %4")
        .arg(status.decodedPCMFrames).arg(status.concealedPCMFrames).arg(status.audioDroppedSamples)
        .arg(status.inputDroppedSamples));
    setStatus(StatusBroadcastTime, status.broadcastTimeUTC.isEmpty()
        ? QStringLiteral("-") : status.broadcastTimeUTC);
    QString audioFormat = QStringLiteral("-");
    const auto selected = std::find_if(status.services.begin(), status.services.end(),
        [&status](const DRMServiceStatus& service) {
            return static_cast<qint64>(service.serviceIdentifier) == status.selectedServiceIdentifier;
        });
    if (selected != status.services.end())
    {
        audioFormat = audioFormatText(*selected);
        if (status.decoderAudioSampleRate > 0) {
            audioFormat += tr(" (decoded %1 Hz, %2 ch)").arg(status.decoderAudioSampleRate).arg(status.audioChannels);
        }
    }

    setStatus(StatusAudioFormat, audioFormat);
    // Rewriting unchanged text would reset the selection and scroll position.
    const QString text = status.textMessagePresent ? status.textMessage : status.error;
    if (text != m_displayedText)
    {
        m_displayedText = text;
        ui->textMessage->setPlainText(text);
    }

    updateAlternativeFrequencies(status);
    updateServices(status);
    updateSlideshow(status);
    updateJournaline(status);
    updateWebsite(status);
}

void DRMDemodGUI::updateWebsite(const DRMDemodStatus& status)
{
    if (status.website && !status.website->files.empty()) {
        ui->websiteStatus->setText(tr("%1 of %2 files").arg(status.website->files.size()).arg(status.website->directoryObjects));
    } else if (status.websitePresent) {
        ui->websiteStatus->setText(tr("Waiting for the website..."));
    } else {
        ui->websiteStatus->setText(tr("No Broadcast Website"));
    }

    if (status.websiteUpdates == m_displayedWebsiteUpdates) {
        return;
    }

    m_displayedWebsiteUpdates = status.websiteUpdates;
    ui->website->setWebsite(status.website);
}

void DRMDemodGUI::updateJournaline(const DRMDemodStatus& status)
{
    // The selector is named by service labels, which can arrive after the
    // objects: a label change updates it as an object change does
    const auto labelOf = [this, &status](int shortId) {
        const auto found = std::find_if(status.services.begin(), status.services.end(),
            [shortId](const DRMServiceStatus& item) { return item.shortIdentifier == shortId; });
        return found != status.services.end() && !found->label.isEmpty() ? found->label : tr("Service %1").arg(shortId);
    };
    QStringList serviceLabels;

    for (const auto& service : status.journaline) {
        serviceLabels << labelOf(service.first);
    }

    if ((status.journalineUpdates == m_displayedJournalineUpdates) && (serviceLabels == m_journalineLabels)) {
        return;
    }

    m_displayedJournalineUpdates = status.journalineUpdates;
    m_journalineLabels = serviceLabels;
    m_journaline = status.journaline;

    // One selector entry per component, named by the services that carry it.
    std::vector<int> services;
    QStringList names;
    std::vector<const DRMDemodStatus::JournalineObjects*> seen;

    for (const auto& service : m_journaline)
    {
        if (!service.second || std::find(seen.begin(), seen.end(), service.second.get()) != seen.end()) {
            continue;
        }

        seen.push_back(service.second.get());
        QStringList labels;

        for (const auto& other : m_journaline)
        {
            if (other.second != service.second) {
                continue;
            }

            labels << labelOf(other.first);
        }

        services.push_back(service.first);
        names << labels.join(QStringLiteral(" / "));
    }

    // Keep the component shown; otherwise the selected service's, else the first.
    int shortId = m_journalineShortId;

    if (m_journaline.count(shortId) == 0)
    {
        const auto selected = std::find_if(status.services.begin(), status.services.end(),
            [&status](const DRMServiceStatus& item) { return static_cast<qint64>(item.serviceIdentifier) == status.selectedServiceIdentifier; });
        shortId = selected != status.services.end() && m_journaline.count(selected->shortIdentifier)
            ? selected->shortIdentifier : (services.empty() ? -1 : services.front());

        if (shortId != m_journalineShortId)
        {
            m_journalineObject = 0;
            m_journalineHistory.clear();
        }
    }

    m_journalineShortId = shortId;

    {
        const QSignalBlocker blocker(ui->journalineService);

        // Labels can arrive after the objects: rebuild when the names change too.
        if ((services != m_journalineServices) || (names != m_journalineNames))
        {
            ui->journalineService->clear();
            ui->journalineService->addItems(names);
            m_journalineServices = services;
            m_journalineNames = names;
        }

        // The entry of the component the shown short Id belongs to.
        for (std::size_t index = 0; index < m_journalineServices.size(); ++index)
        {
            const auto entry = m_journaline.find(m_journalineServices[index]);
            const auto shown = m_journaline.find(m_journalineShortId);

            if (entry != m_journaline.end() && shown != m_journaline.end() && entry->second == shown->second) {
                ui->journalineService->setCurrentIndex(static_cast<int>(index));
            }
        }

        ui->journalineService->setEnabled(m_journalineServices.size() > 1U);
    }

    showJournaline();
}

QString DRMDemodGUI::journalineHtml(const DRMDemodStatus::JournalineObjects& objects, int objectId)
{
    const auto found = objects.find(objectId);

    if (found == objects.end()) {
        return tr("<p><i>Waiting for this page...</i></p>");
    }

    const modemdrm::JournalineObject& object = found->second;
    const auto text = [](const std::string& utf8) {
        return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size())).toHtmlEscaped().replace('\n', QStringLiteral("<br>"));
    };
    QString html = QStringLiteral("<h3>%1</h3>").arg(text(object.title));

    if (object.type == modemdrm::JournalineObject::Menu)
    {
        html += QStringLiteral("<ul>");

        for (const auto& item : object.items)
        {
            // A link once the object it opens has arrived.
            if (objects.count(item.linkId)) {
                html += QStringLiteral("<li><a href=\"jml:%1\">%2</a></li>").arg(QString::number(item.linkId), text(item.text));
            } else {
                html += QStringLiteral("<li><font color=\"gray\">%1</font></li>").arg(text(item.text));
            }
        }

        html += QStringLiteral("</ul>");
    }
    else if (object.type == modemdrm::JournalineObject::List)
    {
        html += QStringLiteral("<table cellpadding=\"2\">");
        bool open = false;

        for (const auto& item : object.items)
        {
            if (item.newRow || !open)
            {
                html += open ? QStringLiteral("</tr><tr>") : QStringLiteral("<tr>");
                open = true;
            }

            html += QStringLiteral("<td>%1</td>").arg(text(item.text));
        }

        html += open ? QStringLiteral("</tr></table>") : QStringLiteral("</table>");
    }
    else
    {
        for (const auto& item : object.items) {
            html += QStringLiteral("<p>%1</p>").arg(text(item.text));
        }
    }

    return html;
}

void DRMDemodGUI::showJournaline()
{
    const auto service = m_journaline.find(m_journalineShortId);
    const QString html = (service == m_journaline.end() || !service->second)
        ? tr("<p><i>No Journaline service</i></p>") : journalineHtml(*service->second, m_journalineObject);

    // Updates arrive every few seconds: redraw only a page that changed,
    // keeping the reader's place (and selection)
    if (html != m_journalineShownHtml)
    {
        const int scroll = ui->journaline->verticalScrollBar()->value();
        ui->journaline->setHtml(html);
        ui->journaline->verticalScrollBar()->setValue(scroll);
        m_journalineShownHtml = html;
    }

    ui->journalineBack->setEnabled(!m_journalineHistory.empty());
    ui->journalineHome->setEnabled(m_journalineObject != 0);
}

void DRMDemodGUI::journalineLinkClicked(const QUrl& link)
{
    bool ok = false;
    const int objectId = link.path().toInt(&ok);

    if (link.scheme() != QLatin1String("jml") || !ok) {
        return;
    }

    m_journalineHistory.push_back(m_journalineObject);
    m_journalineObject = objectId;
    showJournaline();
    ui->journaline->verticalScrollBar()->setValue(0);
}

void DRMDemodGUI::journalineHome()
{
    if (m_journalineObject != 0)
    {
        m_journalineHistory.push_back(m_journalineObject);
        m_journalineObject = 0;
        showJournaline();
    }
}

void DRMDemodGUI::journalineBack()
{
    if (!m_journalineHistory.empty())
    {
        m_journalineObject = m_journalineHistory.back();
        m_journalineHistory.pop_back();
        showJournaline();
    }
}

void DRMDemodGUI::journalineServiceChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(m_journalineServices.size())) {
        return;
    }

    m_journalineShortId = m_journalineServices[static_cast<std::size_t>(index)];
    m_journalineObject = 0;
    m_journalineHistory.clear();
    showJournaline();
}

void DRMDemodGUI::updateSlideshow(const DRMDemodStatus& status)
{
    // Decode a slide once, when it arrives or the service changes
    if (status.slideshowUpdates != m_displayedSlideshowUpdates || status.slideshowPresent != m_displayedSlideshowPresent)
    {
        m_displayedSlideshowUpdates = status.slideshowUpdates;
        m_displayedSlideshowPresent = status.slideshowPresent;
        m_slide = QPixmap();

        if (status.slideshowImages > 0 && !m_slide.loadFromData(status.slideshowImage))
        {
            m_slide = QPixmap();
            ui->slideshow->setText(tr("Slide %1 could not be decoded").arg(status.slideshowName));
        }
        else if (status.slideshowImages == 0) {
            ui->slideshow->setText(status.slideshowPresent ? tr("Waiting for a slide") : tr("No slideshow"));
        }

        ui->slideshowName->setText(status.slideshowName);
        showSlide();
    }
}

void DRMDemodGUI::showSlide()
{
    if (m_slide.isNull()) {
        return;
    }

    ui->slideshow->setPixmap(m_slide.scaled(ui->slideshow->contentsRect().size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void DRMDemodGUI::resizeEvent(QResizeEvent* event)
{
    ChannelGUI::resizeEvent(event);
    showSlide();
}

void DRMDemodGUI::updateAlternativeFrequencies(const DRMDemodStatus& status)
{
    QStringList rows;
    for (const DRMAFSStatus& afs : status.alternativeFrequencies) {
        rows << afsTypeName(afs.type) + '	' + afsFrequencies(afs) + '	' + afsDetails(afs);
    }

    if (rows == m_displayedAlternativeFrequencies) {
        return;
    }

    m_displayedAlternativeFrequencies = rows;
    ui->afsTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row)
    {
        const QStringList columns = rows[row].split('	');
        for (int column = 0; column < columns.size() && column < 3; ++column) {
            ui->afsTable->setItem(row, column, new QTableWidgetItem(columns[column]));
        }
    }

    ui->afsTable->resizeColumnsToContents();
}

void DRMDemodGUI::tick()
{
    double average, peak;
    int samples;
    m_drmDemod->getMagSqLevels(average, peak, samples);
    const double averageDb = CalcDb::dbPower(average);
    const double peakDb = CalcDb::dbPower(peak);
    ui->channelPower->setText(QString::number(averageDb, 'f', 1));
    ui->channelPowerMeter->levelChanged(
        (100.0 + averageDb) / 100.0, (100.0 + peakDb) / 100.0, samples);
    // The status (services, up to 512 AFS entries) is copied and its text
    // rebuilt, so refresh it every few ticks rather than at the meter's rate.
    if (++m_statusTicks >= StatusRefreshTicks)
    {
        m_statusTicks = 0;
        displayStatus(m_drmDemod->getStatus());
    }
}

void DRMDemodGUI::channelMarkerChangedByCursor()
{
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    {
        const QSignalBlocker blocker(ui->frequencyOffset);
        ui->frequencyOffset->setValue(m_settings.m_inputFrequencyOffset);
    }

    updateAbsoluteCenterFrequency();
    applySettings({"inputFrequencyOffset"});
}

void DRMDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void DRMDemodGUI::on_frequencyOffset_changed(qint64 value)
{
    m_settings.m_inputFrequencyOffset = value;
    m_channelMarker.setCenterFrequency(value);
    updateAbsoluteCenterFrequency();
    applySettings({"inputFrequencyOffset"});
}

void DRMDemodGUI::on_searchRange_valueChanged(int value)
{
    // Valid ranges are Auto (0) or 100..5000 Hz; a typed 1..99 means 100.
    if (value > 0 && value < 100)
    {
        value = 100;
        const QSignalBlocker blocker(ui->searchRange);
        ui->searchRange->setValue(value);
    }

    m_settings.m_acquisitionSearchRangeHz = value;
    applySettings({"acquisitionSearchRangeHz"});
}

void DRMDemodGUI::on_forcedMode_currentIndexChanged(int index)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_forcedMode = index - 1;
    applySanitizedSettings({"forcedMode"});
}

void DRMDemodGUI::on_forcedOccupancy_currentIndexChanged(int index)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_forcedOccupancy = index - 1;
    applySanitizedSettings({"forcedOccupancy"});
}

void DRMDemodGUI::on_service_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) {
        return;
    }

    m_settings.m_selectedServiceIdentifier = ui->service->itemData(index).toLongLong();
    applySettings({"selectedServiceIdentifier"});
}

void DRMDemodGUI::on_volume_valueChanged(int value)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_volume = value / 10.0f;
    ui->volumeText->setText(QString::number(m_settings.m_volume, 'f', 1));
    applySettings({"volume"});
}

void DRMDemodGUI::on_audioMute_toggled(bool checked)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_audioMute = checked;
    displayStatus(m_drmDemod->getStatus());
    applySettings({"audioMute"});
}

void DRMDemodGUI::audioSelect(const QPoint& point)
{
    AudioSelectDialog dialog(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    dialog.move(point);
    new DialogPositioner(&dialog, false);
    dialog.exec();
    if (dialog.m_selected)
    {
        m_settings.m_audioDeviceName = dialog.m_audioDeviceName;
        applySettings({"audioDeviceName"});
    }
}

void DRMDemodGUI::on_reacquire_clicked()
{
    m_drmDemod->requestReacquire();
}

void DRMDemodGUI::onWidgetRolled(QWidget*, bool)
{
    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void DRMDemodGUI::onMenuDialogCalled(const QPoint& point)
{
    if (m_contextMenuType != ContextMenuType::ContextMenuChannelSettings)
    {
        resetContextMenuType();
        return;
    }

    BasicChannelSettingsDialog dialog(&m_channelMarker, this);
    dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
    dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
    dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
    dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
    dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
    dialog.setDefaultTitle(m_displayedName);
    if (m_deviceUISet->m_deviceMIMOEngine)
    {
        dialog.setNumberOfStreams(m_drmDemod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
    }

    dialog.move(point);
    new DialogPositioner(&dialog, false);
    dialog.exec();
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    m_settings.m_title = m_channelMarker.getTitle();
    m_settings.m_useReverseAPI = dialog.useReverseAPI();
    m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
    m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
    m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
    m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();
    if (m_deviceUISet->m_deviceMIMOEngine)
    {
        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        updateIndexLabel();
    }

    displaySettings();
    // Presentation and reverse-API changes must not re-channelize or drop lock.
    applySettings({"title", "rgbColor", "useReverseAPI", "reverseAPIAddress",
        "reverseAPIPort", "reverseAPIDeviceIndex", "reverseAPIChannelIndex",
        "streamIndex"});
    resetContextMenuType();
}

void DRMDemodGUI::makeUIConnections()
{
    // setupUi targets the RollupContents, so connect controls to this GUI explicitly.
    connect(ui->frequencyOffset, &ValueDialZ::changed, this, &DRMDemodGUI::on_frequencyOffset_changed);
    connect(ui->searchRange, QOverload<int>::of(&QSpinBox::valueChanged), this, &DRMDemodGUI::on_searchRange_valueChanged);
    connect(ui->forcedMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DRMDemodGUI::on_forcedMode_currentIndexChanged);
    connect(ui->forcedOccupancy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DRMDemodGUI::on_forcedOccupancy_currentIndexChanged);
    connect(ui->service, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DRMDemodGUI::on_service_currentIndexChanged);
    connect(ui->volume, &QDial::valueChanged, this, &DRMDemodGUI::on_volume_valueChanged);
    connect(ui->audioMute, &QToolButton::toggled, this, &DRMDemodGUI::on_audioMute_toggled);
    connect(ui->reacquire, &QToolButton::clicked, this, &DRMDemodGUI::on_reacquire_clicked);
    // Slide names come from the broadcast: never rich text, which could load files.
    // The slide is scaled to the label, which must still be allowed to shrink.
    ui->slideshowName->setTextFormat(Qt::PlainText);
    ui->slideshow->setTextFormat(Qt::PlainText);
    ui->slideshow->setMinimumSize(1, 1);
    connect(ui->journaline, &QTextBrowser::anchorClicked, this, &DRMDemodGUI::journalineLinkClicked);
    connect(ui->journalineHome, &QToolButton::clicked, this, &DRMDemodGUI::journalineHome);
    connect(ui->websiteHome, &QToolButton::clicked, ui->website, &DRMWebsiteBrowser::showStartPage);
    connect(ui->websiteBack, &QToolButton::clicked, ui->website, &DRMWebsiteBrowser::backward);
    connect(ui->website, &DRMWebsiteBrowser::backwardAvailable, ui->websiteBack, &QToolButton::setEnabled);
    ui->websiteBack->setEnabled(false);
    connect(ui->journalineBack, &QToolButton::clicked, this, &DRMDemodGUI::journalineBack);
    connect(ui->journalineService, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DRMDemodGUI::journalineServiceChanged);
}

void DRMDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}

void DRMDemodGUI::updateChannelMarker(const DRMDemodStatus& status)
{
    int mode = m_settings.m_forcedMode;
    int occupancy = m_settings.m_forcedOccupancy;
    const int detectedMode = status.mode - 'A';
    if (detectedMode >= 0 && detectedMode <= 4 && status.occupancy >= 0
        && (mode < 0 || mode == detectedMode)
        && (occupancy < 0 || occupancy == status.occupancy))
    {
        if (mode < 0) {
            mode = detectedMode;
        }

        if (occupancy < 0) {
            occupancy = status.occupancy;
        }
    }

    // Before detection, show the union of the configured hypotheses. Once FAC
    // identifies a spectrum, use its carrier edges, not the decoder sample rate.
    double lower = std::numeric_limits<double>::infinity();
    double upper = -std::numeric_limits<double>::infinity();
    for (const auto& spectrum : modemdrm::Parameters::spectra())
    {
        if ((mode >= 0 && static_cast<int>(spectrum.mode) != mode)
            || (occupancy >= 0 && spectrum.occupancy != occupancy)) {
            continue;
        }

        // The sink decodes mode E only from a channel at its 192 kHz internal rate.
        if (mode < 0 && spectrum.mode == modemdrm::Mode::E
            && m_basebandSampleRate < modemdrm::Parameters::forMode(modemdrm::Mode::E).internalSampleRate) {
            continue;
        }

        const double spacing = modemdrm::Parameters::forMode(spectrum.mode).carrierSpacingHz();
        const double minimum = (spectrum.minimumCarrier - 0.5) * spacing;
        const double maximum = (spectrum.maximumCarrier + 0.5) * spacing;
        // The receiver detects a mirrored spectrum; until it locks, show the
        // usual orientation.
        const bool mirrored = status.spectrumInverted == 1;
        lower = std::min(lower, mirrored ? -maximum : minimum);
        upper = std::max(upper, mirrored ? -minimum : maximum);
    }

    const int shift = std::isfinite(lower) ? static_cast<int>(std::lround((lower + upper) / 2.0)) : 0;
    const int bandwidth = std::isfinite(lower)
        ? 2 * static_cast<int>(std::ceil(std::max(shift - lower, upper - shift))) : 0;
    if (m_channelMarker.getBandwidth() == bandwidth && m_channelMarker.getShift() == shift) {
        return;
    }

    {
        const QSignalBlocker blocker(&m_channelMarker);
        m_channelMarker.setSidebands(ChannelMarker::dsb);
        m_channelMarker.setBandwidth(bandwidth);
        // Keep the draggable reference frequency at fR for asymmetric occupancies.
        m_channelMarker.setShift(shift);
    }

    m_channelMarker.emitChangedByAPI();
}
