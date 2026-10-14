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
#include <limits>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "settings/serializable.h"
#include "maincore.h"
#include "SWGChannelActions.h"
#include "SWGChannelReport.h"
#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGDRMAFS.h"
#include "SWGDRMAFSFrequency.h"
#include "SWGDRMDemodActions.h"
#include "SWGDRMDemodReport.h"
#include "SWGDRMDemodSettings.h"
#include "SWGDRMJournaline.h"
#include "SWGDRMJournalineItem.h"
#include "SWGDRMJournalineObject.h"
#include "SWGDRMJournalineService.h"
#include "SWGDRMService.h"

#include "drmdemod.h"

MESSAGE_CLASS_DEFINITION(DRMDemod::MsgConfigureDRMDemod, Message)

const char * const DRMDemod::m_channelIdURI = "sdrangel.channel.drmdemod";
const char * const DRMDemod::m_channelId = "DRMDemod";

DRMDemod::DRMDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_basebandSink(new DRMDemodBaseband()),
    m_basebandSampleRate(0),
    m_centerFrequency(0),
    m_networkManager(new QNetworkAccessManager())
{
    setObjectName(m_channelId);
    m_thread.setObjectName("DRMDemodBB");
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);
    connect(
        this, &ChannelAPI::indexInDeviceSetChanged,
        this, &DRMDemod::handleIndexInDeviceSetChanged
    );
    connect(
        m_networkManager, &QNetworkAccessManager::finished,
        this, &DRMDemod::networkManagerFinished
    );
    applySettings(m_settings, QStringList(), true);
    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
}

DRMDemod::~DRMDemod()
{
    if (m_basebandSink->isRunning()) {
        stop();
    }

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true, m_registeredStreamIndex);
    delete m_networkManager;
    delete m_basebandSink;
}

void DRMDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI == m_deviceAPI) {
        return;
    }

    if (m_basebandSink->isRunning()) {
        stop();
    }

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true, m_registeredStreamIndex);
    m_deviceAPI = deviceAPI;
    m_settings.m_streamIndex = std::clamp(m_settings.m_streamIndex, 0,
        std::max(0, static_cast<int>(getNumberOfDeviceStreams()) - 1));
    m_deviceAPI->addChannelSink(this, m_settings.m_streamIndex);
    m_registeredStreamIndex = m_settings.m_streamIndex;
    m_deviceAPI->addChannelSinkAPI(this);
}

uint32_t DRMDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void DRMDemod::requestReacquire()
{
    m_basebandSink->getInputMessageQueue()->push(
        DRMDemodBaseband::MsgResetDRMDemodBaseband::create()
    );
}

void DRMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    // BasebandSampleSink's flag describes real versus complex input. It is
    // not a discontinuity marker; resetting on it would reset every real-input
    // block. Epoch changes are controlled by DSP/settings/reset messages.
    (void) positiveOnly;
    m_basebandSink->feed(begin, end);
}

void DRMDemod::start()
{
    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();
    m_basebandSink->getInputMessageQueue()->push(
        new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency)
    );
    m_basebandSink->getInputMessageQueue()->push(
        DRMDemodBaseband::MsgConfigureDRMDemodBaseband::create(
            m_settings, QStringList(), true
        )
    );
}

void DRMDemod::stop()
{
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool DRMDemod::handleMessage(const Message& message)
{
    if (MsgConfigureDRMDemod::match(message))
    {
        const auto& configure = static_cast<const MsgConfigureDRMDemod&>(message);
        applySettings(
            configure.getSettings(), configure.getSettingsKeys(), configure.getForce()
        );
        return true;
    }

    if (DSPSignalNotification::match(message))
    {
        const auto& notification = static_cast<const DSPSignalNotification&>(message);
        m_basebandSampleRate = notification.getSampleRate();
        m_centerFrequency = notification.getCenterFrequency();
        m_basebandSink->getInputMessageQueue()->push(
            new DSPSignalNotification(notification)
        );
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notification));
        }

        return true;
    }

    if (MainCore::MsgChannelDemodQuery::match(message))
    {
        // Demod Analyzer and the MCP audio capture ask for the rate of the
        // audio fed to the "demod" data pipe.
        sendSampleRateToDemodAnalyzer();
        return true;
    }

    return false;
}

void DRMDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        if (messageQueue) {
            messageQueue->push(MainCore::MsgChannelDemodReport::create(this, getAudioSampleRate()));
        }
    }
}

void DRMDemod::applySettings(const DRMDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "DRMDemod::applySettings:" << settings.getDebugString(settingsKeys, force)
        << "force:" << force;
    // The settings in force after this, and the keys of the values that
    // changed with those asked for
    DRMDemodSettings sanitized = m_settings;
    QStringList keys = settingsKeys;
    if (force) {
        sanitized = settings;
    } else {
        sanitized.applySettings(settingsKeys, settings);
    }

    const DRMDemodSettings requested = sanitized;
    sanitized.sanitize();
    if (!force)
    {
        for (const QString& key : sanitized.sanitizedKeys(requested))
        {
            if (!keys.contains(key)) {
                keys.append(key);
            }
        }
    }

    sanitized.m_streamIndex = std::clamp(sanitized.m_streamIndex, 0,
        std::max(0, static_cast<int>(getNumberOfDeviceStreams()) - 1));
    const bool streamChanged = (force || keys.contains("streamIndex"))
        && sanitized.m_streamIndex != m_registeredStreamIndex;

    if (streamChanged && m_deviceAPI->getSampleMIMO())
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false, m_registeredStreamIndex);
        m_deviceAPI->addChannelSink(this, sanitized.m_streamIndex);
        m_deviceAPI->addChannelSinkAPI(this);
        m_registeredStreamIndex = sanitized.m_streamIndex;
        // Listeners read getStreamIndex() when notified.
        m_settings.m_streamIndex = sanitized.m_streamIndex;
        emit streamIndexChanged(sanitized.m_streamIndex);
    }

    m_basebandSink->getInputMessageQueue()->push(
        DRMDemodBaseband::MsgConfigureDRMDemodBaseband::create(
            sanitized, keys, force
        )
    );
    if (sanitized.m_useReverseAPI)
    {
        const bool fullUpdate = force
            || (keys.contains("useReverseAPI")
                && sanitized.m_useReverseAPI != m_settings.m_useReverseAPI)
            || (keys.contains("reverseAPIAddress")
                && sanitized.m_reverseAPIAddress != m_settings.m_reverseAPIAddress)
            || (keys.contains("reverseAPIPort")
                && sanitized.m_reverseAPIPort != m_settings.m_reverseAPIPort)
            || (keys.contains("reverseAPIDeviceIndex")
                && sanitized.m_reverseAPIDeviceIndex
                    != m_settings.m_reverseAPIDeviceIndex)
            || (keys.contains("reverseAPIChannelIndex")
                && sanitized.m_reverseAPIChannelIndex
                    != m_settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(keys, sanitized, fullUpdate);
    }

    m_settings = sanitized;
}

void DRMDemod::setCenterFrequency(qint64 frequency)
{
    DRMDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = clampFrequencyOffset(frequency);
    applySettings(settings, {"inputFrequencyOffset"}, false);
    if (m_guiMessageQueue)
    {
        m_guiMessageQueue->push(MsgConfigureDRMDemod::create(
            settings, {"inputFrequencyOffset"}, false
        ));
    }
}

bool DRMDemod::deserialize(const QByteArray& data)
{
    const bool valid = m_settings.deserialize(data);
    if (!valid) {
        m_settings.resetToDefaults();
    }

    m_inputMessageQueue.push(MsgConfigureDRMDemod::create(
        m_settings, QStringList(), true
    ));
    return valid;
}

int DRMDemod::webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setDrmDemodSettings(new SWGSDRangel::SWGDRMDemodSettings());
    response.getDrmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DRMDemod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage
)
{
    (void) errorMessage;
    DRMDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);
    const DRMDemodSettings requested = settings;
    settings.sanitize();
    // A value that changed with those asked for goes with them (the GUI
    // applies only the keys)
    QStringList keys = channelSettingsKeys;
    for (const QString& key : settings.sanitizedKeys(requested))
    {
        if (!keys.contains(key)) {
            keys.append(key);
        }
    }

    // Report the stream the channel will actually use.
    settings.m_streamIndex = std::clamp(settings.m_streamIndex, 0,
        std::max(0, static_cast<int>(getNumberOfDeviceStreams()) - 1));
    m_inputMessageQueue.push(MsgConfigureDRMDemod::create(
        settings, keys, force
    ));
    if (m_guiMessageQueue)
    {
        m_guiMessageQueue->push(MsgConfigureDRMDemod::create(
            settings, keys, force
        ));
    }

    webapiFormatChannelSettings(response, settings);
    return 200;
}

qint32 DRMDemod::clampFrequencyOffset(qint64 frequency)
{
    return static_cast<qint32>(std::clamp<qint64>(frequency, std::numeric_limits<qint32>::min(), std::numeric_limits<qint32>::max()));
}

int DRMDemod::webapiActionsPost(const QStringList& channelActionsKeys, SWGSDRangel::SWGChannelActions& query, QString& errorMessage)
{
    SWGSDRangel::SWGDRMDemodActions *actions = query.getDrmDemodActions();
    if (!actions)
    {
        errorMessage = "Missing DRMDemodActions in query";
        return 400;
    }

    // Every action asked for is done: 202 if all were only queued, 200 once
    // one has been done, else the first failure
    int result = 202;
    const auto fail = [&result, &errorMessage](int code, const QString& message) {
        if (result < 400)
        {
            result = code;
            errorMessage = message;
        }
    };

    if (channelActionsKeys.contains("reacquire") && actions->getReacquire() != 0) {
        requestReacquire();
    }

    if (channelActionsKeys.contains("saveSlideshowImage") && actions->getSaveSlideshowImage())
    {
        const QString path = *actions->getSaveSlideshowImage();
        const DRMDemodStatus status = getStatus();
        QFile file(path);

        if (path.isEmpty()) {
            fail(400, "No file path for the slideshow image");
        } else if (status.slideshowImage.isEmpty()) {
            fail(404, "No slideshow image has been received");
        } else if (!file.open(QIODevice::WriteOnly) || file.write(status.slideshowImage) != status.slideshowImage.size()) {
            fail(500, QString("Cannot write the slideshow image to %1").arg(path));
        } else if (result < 400) {
            result = 200;
        }
    }

    if (channelActionsKeys.contains("saveWebsite") && actions->getSaveWebsite())
    {
        QString message;
        const int code = saveWebsiteFiles(getStatus().website.get(), *actions->getSaveWebsite(), message);

        if (code >= 400) {
            fail(code, message);
        } else if (result < 400) {
            result = 200;
        }
    }

    return result;
}

int DRMDemod::saveWebsiteFiles(const modemdrm::MOTWebsite* website, const QString& directory, QString& message)
{
    if (directory.isEmpty())
    {
        message = "No directory for the Broadcast Website";
        return 400;
    }

    if (!website || website->files.empty())
    {
        message = "No Broadcast Website files have been received";
        return 404;
    }

    // A file that cannot be written (its name, or a file and a directory of
    // the same name) is skipped rather than ending the save
    int written = 0;
    QStringList skipped;

    for (const auto& file : website->files)
    {
        const QString name = websiteFilePath(file.first);

        if (name.isEmpty())
        {
            skipped.append(QString::fromUtf8(file.first.data(), static_cast<int>(file.first.size())));
            continue; // a name that would leave the directory, or that the file system refuses
        }

        const QString path = QDir(directory).filePath(name);
        QFile output(path);

        if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !output.open(QIODevice::WriteOnly)
            || (output.write(reinterpret_cast<const char*>(file.second.data->data()), static_cast<qint64>(file.second.data->size()))
                != static_cast<qint64>(file.second.data->size())))
        {
            skipped.append(name);
            continue;
        }

        ++written;
    }

    if (!skipped.isEmpty()) {
        qWarning("DRMDemod::saveWebsiteFiles: Broadcast Website files not saved: %s", qPrintable(skipped.join(", ")));
    }

    if (written == 0)
    {
        message = QString("No Broadcast Website file could be written to %1").arg(directory);
        return 500;
    }

    return 200;
}

QString DRMDemod::websiteFilePath(const std::string& contentName)
{
    // A relative path within the directory: no root, drive, backslash or
    // "..", and parts that any file system accepts (no Windows device names,
    // reserved characters or trailing dots and spaces)
    static const QRegularExpression device(
        QStringLiteral("^(con|prn|aux|nul|conin\\$|conout\\$|com[0-9\\x{00b9}\\x{00b2}\\x{00b3}]|lpt[0-9\\x{00b9}\\x{00b2}\\x{00b3}]) *(\\..*)?$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reserved(QStringLiteral("[<>:\"|?*\\\\\\x00-\\x1f]"));
    const QString name = QString::fromUtf8(contentName.data(), static_cast<int>(contentName.size()));

    if (name.isEmpty() || name.startsWith('/') || name.contains(reserved)) {
        return QString();
    }

    for (const QString& part : name.split('/'))
    {
        if (part.isEmpty() || (part == QLatin1String(".")) || (part == QLatin1String("..")) || part.endsWith('.')
            || part.endsWith(' ') || device.match(part).hasMatch()) {
            return QString();
        }
    }

    return name;
}

int DRMDemod::webapiWorkspaceGet(SWGSDRangel::SWGWorkspaceInfo& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DRMDemod::webapiReportGet(SWGSDRangel::SWGChannelReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setDrmDemodReport(new SWGSDRangel::SWGDRMDemodReport());
    response.getDrmDemodReport()->init();
    SWGSDRangel::SWGDRMDemodReport *report = response.getDrmDemodReport();
    const DRMDemodStatus status = getStatus();
    *report->getAudioDecoderBackend() = status.audioDecoderBackend;
    for (const auto& capability : status.capabilities) {
        report->getCapabilities()->append(new QString(capability));
    }

    *report->getSyncState() = status.syncState;
    report->setEpoch(static_cast<qint64>(status.epoch));
    report->setFacValid(status.facValid ? 1 : 0);
    report->setSdcValid(status.sdcValid ? 1 : 0);
    report->setMscReady(status.mscReady ? 1 : 0);
    report->setAudioActive(status.audioActive ? 1 : 0);
    *report->getMode() = status.mode == '?' ? QString() : QString(status.mode);
    report->setOccupancy(status.occupancy);
    report->setSpectrumInverted(status.spectrumInverted);
    report->setEffectiveSearchRangeHz(status.effectiveSearchRangeHz);
    report->setInternalSampleRate(status.internalSampleRate);
    report->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    // Left out (unset) until measured.
    double channelPowerDB = 0.0;
    if (m_basebandSink->getChannelPowerDB(channelPowerDB)) {
        report->setChannelPowerDb(static_cast<float>(channelPowerDB));
    }

    report->setInputFilterLowHz(static_cast<float>(status.inputFilterLowHz));
    report->setInputFilterHighHz(static_cast<float>(status.inputFilterHighHz));
    report->setCarrierOffsetHz(static_cast<float>(status.carrierOffsetHz));
    report->setCarrierOffsetAvailable(status.carrierOffsetAvailable ? 1 : 0);
    report->setSampleClockErrorPpm(static_cast<float>(status.sampleClockErrorPPM));
    report->setSampleClockErrorAvailable(status.sampleClockErrorAvailable ? 1 : 0);
    report->setMscMerAvailable(status.mscMERAvailable ? 1 : 0);
    report->setMscMerdb(static_cast<float>(status.mscMERDB));
    report->setMscMerCellCount(static_cast<qint64>(status.mscMERCellCount));
    for (const DRMServiceStatus& source : status.services)
    {
        auto *target = new SWGSDRangel::SWGDRMService();
        target->init();
        target->setServiceIdentifier(source.serviceIdentifier);
        target->setShortIdentifier(source.shortIdentifier);
        target->setAudioService(source.audioService ? 1 : 0);
        target->setConditionalAccess(source.conditionalAccess ? 1 : 0);
        target->setFacLanguageCode(source.facLanguageCode);
        target->setFacServiceDescriptor(source.facServiceDescriptor);
        *target->getLabel() = source.label;
        *target->getLanguage() = source.language;
        *target->getCountry() = source.country;
        target->setStreamId(source.streamId);
        target->setAudioCoding(source.audioCoding);
        target->setAudioMode(source.audioMode);
        target->setSamplingRateCode(source.samplingRateCode);
        target->setSbr(source.sbr ? 1 : 0);
        target->setTextFlag(source.textFlag ? 1 : 0);
        report->getServices()->append(target);
    }

    report->setSelectedServiceIdentifier(status.selectedServiceIdentifier);
    report->setCaptureWindows(static_cast<qint64>(status.captureWindows));
    report->setAudioDroppedSamples(static_cast<qint64>(status.audioDroppedSamples));
    report->setFacAgeMs(static_cast<qint64>(status.facAgeMs));
    report->setSdcAgeMs(static_cast<qint64>(status.sdcAgeMs));
    report->setAudioAgeMs(static_cast<qint64>(status.audioAgeMs));
    report->setBroadcastTimeAgeMs(static_cast<qint64>(status.broadcastTimeAgeMs));
    report->setValidFacBlocks(static_cast<qint64>(status.validFACBlocks));
    report->setInvalidFacBlocks(static_cast<qint64>(status.invalidFACBlocks));
    report->setValidSdcBlocks(static_cast<qint64>(status.validSDCBlocks));
    report->setInvalidSdcBlocks(static_cast<qint64>(status.invalidSDCBlocks));
    report->setSkippedSdcBlocks(static_cast<qint64>(status.skippedSDCBlocks));
    report->setInputDroppedSamples(static_cast<qint64>(status.inputDroppedSamples));
    report->setProcessedSuperframes(status.processedSuperframes);
    report->setRetainedSamples(status.retainedSamples);
    report->setDecodedMultiplexFrames(status.decodedMultiplexFrames);
    report->setInvalidMultiplexFrames(status.invalidMultiplexFrames);
    report->setParsedAudioSuperframes(status.parsedAudioSuperframes);
    report->setInvalidAudioSuperframes(status.invalidAudioSuperframes);
    report->setDecodedPcmFrames(status.decodedPCMFrames);
    report->setConcealedPcmFrames(status.concealedPCMFrames);
    report->setDecoderAudioSampleRate(status.decoderAudioSampleRate);
    report->setOutputAudioSampleRate(status.outputAudioSampleRate);
    report->setAudioChannels(status.audioChannels);
    report->setTextMessagePresent(status.textMessagePresent ? 1 : 0);
    *report->getTextMessage() = status.textMessage;
    report->setInvalidTextSegments(status.invalidTextSegments);
    report->setSlideshowPresent(status.slideshowPresent ? 1 : 0);
    report->setSlideshowImages(static_cast<qint32>(status.slideshowImages));
    *report->getSlideshowName() = status.slideshowName;
    *report->getSlideshowMimeType() = status.slideshowMimeType;
    report->setSlideshowSize(status.slideshowImage.size());
    report->setSlideshowAgeMs(status.slideshowAgeMs);
    report->setSlideshowValidPackets(static_cast<qint32>(status.slideshowValidPackets));
    report->setSlideshowInvalidPackets(static_cast<qint32>(status.slideshowInvalidPackets));
    std::vector<const DRMDemodStatus::JournalineObjects*> journalineServices;
    int journalineObjects = 0;

    for (const auto& service : status.journaline)
    {
        if (!service.second || std::find(journalineServices.begin(), journalineServices.end(), service.second.get()) != journalineServices.end()) {
            continue;
        }

        journalineServices.push_back(service.second.get());
        journalineObjects += static_cast<int>(service.second->size());
        report->getJournaline()->append(webapiFormatJournaline(status, *service.second));
    }

    report->setWebsitePresent(status.websitePresent ? 1 : 0);
    report->setWebsiteFiles(status.website ? static_cast<qint32>(status.website->files.size()) : 0);
    report->setWebsiteObjects(status.website ? status.website->directoryObjects : 0);
    *report->getWebsiteStartPage() = status.website
        ? QString::fromUtf8(status.website->startPage.data(), static_cast<int>(status.website->startPage.size())) : QString();
    report->setWebsiteValidPackets(static_cast<qint32>(status.websiteValidPackets));
    report->setWebsiteInvalidPackets(static_cast<qint32>(status.websiteInvalidPackets));
    report->setDataService(status.dataService ? 1 : 0);
    report->setJournalineServices(static_cast<qint32>(journalineServices.size()));
    report->setJournalineObjects(journalineObjects);
    *report->getBroadcastTimeUtc() = status.broadcastTimeUTC;
    report->setLocalTimeOffsetPresent(status.localTimeOffsetPresent ? 1 : 0);
    report->setLocalTimeOffsetMinutes(status.localTimeOffsetMinutes);
    report->setAfsIndex(status.afsIndex);
    for (const DRMAFSStatus& source : status.alternativeFrequencies)
    {
        auto *target = new SWGSDRangel::SWGDRMAFS();
        target->init();
        target->setEntityType(source.type);
        target->setListVersion(source.listVersion ? 1 : 0);
        target->setExtension(source.extension);
        target->setSynchronousMultiplex(source.synchronousMultiplex ? 1 : 0);
        target->setBaseLayer(source.baseLayer ? 1 : 0);
        target->setAppliesToAllServices(source.appliesToAllServices ? 1 : 0);
        target->setRegionOrScheduleRestricted(
            source.regionOrScheduleRestricted ? 1 : 0
        );
        target->setServiceRestrictionField(source.serviceRestrictionField);
        target->setRegionScheduleField(source.regionScheduleField);
        for (std::uint16_t code : source.frequencyCodes)
        {
            auto *frequency = new SWGSDRangel::SWGDRMAFSFrequency();
            frequency->init();
            frequency->setCode(code);
            // Entity 3: msb selects 10 kHz (mode E) instead of 1 kHz units.
            frequency->setFrequencyKHz(static_cast<qint32>(code & 0x7fffU) * ((code & 0x8000U) ? 10 : 1));
            target->getFrequencies()->append(frequency);
        }

        report->getAlternativeFrequencies()->append(target);
    }

    *report->getError() = status.error;
    return 200;
}

SWGSDRangel::SWGDRMJournaline *DRMDemod::webapiFormatJournaline(const DRMDemodStatus& status,
    const DRMDemodStatus::JournalineObjects& objects)
{
    auto *journaline = new SWGSDRangel::SWGDRMJournaline();
    journaline->setServices(new QList<SWGSDRangel::SWGDRMJournalineService*>());
    journaline->setObjects(new QList<SWGSDRangel::SWGDRMJournalineObject*>());

    // Services that signal the same component share one set of objects.
    for (const auto& service : status.journaline)
    {
        if (service.second.get() != &objects) {
            continue;
        }

        const auto found = std::find_if(status.services.begin(), status.services.end(),
            [&service](const DRMServiceStatus& item) { return item.shortIdentifier == service.first; });
        auto *target = new SWGSDRangel::SWGDRMJournalineService();
        target->setShortIdentifier(service.first);
        target->setServiceIdentifier(found != status.services.end() ? static_cast<qint64>(found->serviceIdentifier) : -1);
        target->setLabel(new QString(found != status.services.end() ? found->label : QString()));
        journaline->getServices()->append(target);
    }

    for (const auto& entry : objects)
    {
        const modemdrm::JournalineObject& object = entry.second;
        auto *target = new SWGSDRangel::SWGDRMJournalineObject();
        target->setObjectId(object.objectId);
        target->setType(new QString(journalineTypeName(object.type)));
        target->setRevision(object.revision);
        target->setStaticObject(object.staticObject ? 1 : 0);
        target->setTitle(new QString(QString::fromUtf8(object.title.data(), static_cast<int>(object.title.size()))));
        target->setItems(new QList<SWGSDRangel::SWGDRMJournalineItem*>());

        for (const auto& item : object.items)
        {
            auto *targetItem = new SWGSDRangel::SWGDRMJournalineItem();
            targetItem->setText(new QString(QString::fromUtf8(item.text.data(), static_cast<int>(item.text.size()))));

            if (object.type == modemdrm::JournalineObject::Menu) {
                targetItem->setLinkId(item.linkId);
            } else if (object.type == modemdrm::JournalineObject::List) {
                targetItem->setNewRow(item.newRow ? 1 : 0);
            }

            target->getItems()->append(targetItem);
        }

        journaline->getObjects()->append(target);
    }

    return journaline;
}

QString DRMDemod::journalineTypeName(int type)
{
    switch (type)
    {
    case modemdrm::JournalineObject::Menu:
        return QStringLiteral("menu");
    case modemdrm::JournalineObject::PlainText:
        return QStringLiteral("text");
    case modemdrm::JournalineObject::TitleOnly:
        return QStringLiteral("title");
    default:
        return QStringLiteral("list");
    }
}

void DRMDemod::webapiUpdateChannelSettings(
    DRMDemodSettings& settings,
    const QStringList& keys,
    SWGSDRangel::SWGChannelSettings& response
)
{
    SWGSDRangel::SWGDRMDemodSettings *api = response.getDrmDemodSettings();
    if (!api) {
        return;
    }

    // Range-check before narrowing: the API types are wider than the settings.
    if (keys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = clampFrequencyOffset(api->getInputFrequencyOffset());
    }
    if (keys.contains("acquisitionSearchRangeHz")) {
        settings.m_acquisitionSearchRangeHz = api->getAcquisitionSearchRangeHz();
    }
    if (keys.contains("forcedMode")) {
        settings.m_forcedMode = api->getForcedMode();
    }
    if (keys.contains("forcedOccupancy")) {
        settings.m_forcedOccupancy = api->getForcedOccupancy();
    }
    if (keys.contains("selectedServiceIdentifier")) {
        settings.m_selectedServiceIdentifier = api->getSelectedServiceIdentifier();
    }
    if (keys.contains("volume")) {
        settings.m_volume = api->getVolume();
    }
    if (keys.contains("audioMute")) {
        settings.m_audioMute = api->getAudioMute() != 0;
    }
    if (keys.contains("audioDeviceName") && api->getAudioDeviceName()) {
        settings.m_audioDeviceName = *api->getAudioDeviceName();
    }
    if (keys.contains("rgbColor")) {
        settings.m_rgbColor = api->getRgbColor();
    }
    if (keys.contains("title") && api->getTitle()) {
        settings.m_title = *api->getTitle();
    }
    if (keys.contains("streamIndex")) {
        settings.m_streamIndex = std::max(api->getStreamIndex(), 0);
    }
    if (keys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = api->getUseReverseApi() != 0;
    }
    if (keys.contains("reverseAPIAddress") && api->getReverseApiAddress()) {
        settings.m_reverseAPIAddress = *api->getReverseApiAddress();
    }
    if (keys.contains("reverseAPIPort"))
    {
        const qint32 port = api->getReverseApiPort();
        if (port > 1023 && port <= 65535) {
            settings.m_reverseAPIPort = static_cast<uint16_t>(port);
        }
    }
    if (keys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = static_cast<uint16_t>(std::clamp(api->getReverseApiDeviceIndex(), 0, 99));
    }
    if (keys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = static_cast<uint16_t>(std::clamp(api->getReverseApiChannelIndex(), 0, 99));
    }
    if (settings.m_channelMarker && keys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(keys, api->getChannelMarker());
    }
    if (settings.m_rollupState && keys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(keys, api->getRollupState());
    }
}

void DRMDemod::webapiFormatChannelSettings(
    SWGSDRangel::SWGChannelSettings& response,
    const DRMDemodSettings& settings
)
{
    SWGSDRangel::SWGDRMDemodSettings *api = response.getDrmDemodSettings();
    if (!api) {
        return;
    }

    api->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    api->setAcquisitionSearchRangeHz(settings.m_acquisitionSearchRangeHz);
    api->setForcedMode(settings.m_forcedMode);
    api->setForcedOccupancy(settings.m_forcedOccupancy);
    api->setSelectedServiceIdentifier(settings.m_selectedServiceIdentifier);
    api->setVolume(settings.m_volume);
    api->setAudioMute(settings.m_audioMute ? 1 : 0);
    *api->getAudioDeviceName() = settings.m_audioDeviceName;
    api->setRgbColor(settings.m_rgbColor);
    *api->getTitle() = settings.m_title;
    api->setStreamIndex(settings.m_streamIndex);
    api->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    *api->getReverseApiAddress() = settings.m_reverseAPIAddress;
    api->setReverseApiPort(settings.m_reverseAPIPort);
    api->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    api->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    if (settings.m_channelMarker) {
        settings.m_channelMarker->formatTo(api->getChannelMarker());
    }

    if (settings.m_rollupState) {
        settings.m_rollupState->formatTo(api->getRollupState());
    }
}

void DRMDemod::webapiReverseSendSettings(const QStringList& channelSettingsKeys, const DRMDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings*channelSettings = new SWGSDRangel::SWGChannelSettings();
    channelSettings->init();
    webapiFormatReverseSettings(
        channelSettingsKeys, channelSettings, settings, force
    );
    const QString url = QStringLiteral(
        "http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings"
    ).arg(settings.m_reverseAPIAddress)
     .arg(settings.m_reverseAPIPort)
     .arg(settings.m_reverseAPIDeviceIndex)
     .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(url));
    m_networkRequest.setHeader(
        QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json")
    );
    QBuffer *buffer = new QBuffer();
    buffer->open(QIODevice::ReadWrite);
    buffer->write(channelSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply = m_networkManager->sendCustomRequest(
        m_networkRequest, "PATCH", buffer
    );
    buffer->setParent(reply);
    delete channelSettings;
}

void DRMDemod::webapiFormatReverseSettings(const QStringList& keys, SWGSDRangel::SWGChannelSettings *response, const DRMDemodSettings& settings, bool force)
{
    response->setDirection(0);
    response->setOriginatorChannelIndex(getIndexInDeviceSet());
    response->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    if (response->getChannelType()) {
        *response->getChannelType() = m_channelId;
    } else {
        response->setChannelType(new QString(m_channelId));
    }

    response->setDrmDemodSettings(new SWGSDRangel::SWGDRMDemodSettings());
    response->getDrmDemodSettings()->init();
    SWGSDRangel::SWGDRMDemodSettings *api = response->getDrmDemodSettings();

    if (force || keys.contains("inputFrequencyOffset")) {
        api->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (force || keys.contains("acquisitionSearchRangeHz")) {
        api->setAcquisitionSearchRangeHz(settings.m_acquisitionSearchRangeHz);
    }
    if (force || keys.contains("forcedMode")) {
        api->setForcedMode(settings.m_forcedMode);
    }
    if (force || keys.contains("forcedOccupancy")) {
        api->setForcedOccupancy(settings.m_forcedOccupancy);
    }
    if (force || keys.contains("selectedServiceIdentifier")) {
        api->setSelectedServiceIdentifier(settings.m_selectedServiceIdentifier);
    }
    if (force || keys.contains("volume")) {
        api->setVolume(settings.m_volume);
    }
    if (force || keys.contains("audioMute")) {
        api->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (force || keys.contains("audioDeviceName")) {
        *api->getAudioDeviceName() = settings.m_audioDeviceName;
    }
    if (force || keys.contains("rgbColor")) {
        api->setRgbColor(settings.m_rgbColor);
    }
    if (force || keys.contains("title")) {
        *api->getTitle() = settings.m_title;
    }
    if (force || keys.contains("streamIndex")) {
        api->setStreamIndex(settings.m_streamIndex);
    }
    if (settings.m_channelMarker && (force || keys.contains("channelMarker"))) {
        settings.m_channelMarker->formatTo(api->getChannelMarker());
    }
    if (settings.m_rollupState && (force || keys.contains("rollupState"))) {
        settings.m_rollupState->formatTo(api->getRollupState());
    }
}

void DRMDemod::networkManagerFinished(QNetworkReply *reply)
{
    if (reply->error())
    {
        qWarning() << "DRMDemod reverse API error" << reply->error()
            << reply->errorString();
    }
    else
    {
        QByteArray answer = reply->readAll();
        if (answer.endsWith('\n')) {
            answer.chop(1);
        }

        qDebug() << "DRMDemod reverse API reply" << answer;
    }

    reply->deleteLater();
}

qint64 DRMDemod::getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
{
    return sinkElseSource && streamIndex == m_settings.m_streamIndex
        ? m_settings.m_inputFrequencyOffset : 0;
}

void DRMDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    const QString fifoLabel = QStringLiteral("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}
