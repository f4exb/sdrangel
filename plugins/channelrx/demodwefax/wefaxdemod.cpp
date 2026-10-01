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

#include "wefaxdemod.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "util/db.h"
#include "SWGChannelActions.h"
#include "SWGChannelReport.h"
#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGWefaxDemodActions.h"
#include "SWGWefaxDemodReport.h"
#include "SWGWefaxDemodSettings.h"
#include "wefaxdemodimageworker.h"

MESSAGE_CLASS_DEFINITION(WefaxDemod::MsgConfigureWefaxDemod, Message)
MESSAGE_CLASS_DEFINITION(WefaxDemod::MsgDecoderAction, Message)
MESSAGE_CLASS_DEFINITION(WefaxDemod::MsgImage, Message)

const char * const WefaxDemod::m_channelIdURI = "sdrangel.channel.wefaxdemod";
const char * const WefaxDemod::m_channelId = "WefaxDemod";

WefaxDemod::WefaxDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_basebandSink(new WefaxDemodBaseband()),
    m_imageWorker(new WefaxDemodImageWorker()),
    m_basebandSampleRate(0),
    m_centerFrequency(0),
    m_imageRows(0),
    m_decoderState(WefaxDecoder::State::Idle),
    m_phasingLineCount(0),
    m_samplesPerLine(0.0),
    m_clockCorrectionPpm(0.0),
    m_appliedClockCorrectionPpm(0.0),
    m_confidence(0.0),
    m_imageQueueOverflows(0),
    m_imageId(1),
    m_completionReason("none"),
    m_tuningErrorHz(0.0),
    m_slantCorrectionPpm(0.0),
    m_alignmentPx(0),
    m_effectiveLinesPerMinute(0.0),
    m_timingSource("nominal/manual"),
    m_timingStatus("none"),
    m_captureFrequencyHz(0),
    m_captureIOC(576),
    m_captureLinesPerMinute(120),
    m_networkManager(new QNetworkAccessManager())
{
    setObjectName(m_channelId);
    m_thread.setObjectName("WefaxDemodBB");
    m_imageThread.setObjectName("WefaxDemodImage");
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->moveToThread(&m_thread);
    m_imageWorker->moveToThread(&m_imageThread);
    connect(m_imageWorker, &WefaxDemodImageWorker::imageUpdated,
        this, &WefaxDemod::handleImageUpdated);
    connect(m_imageWorker, &WefaxDemodImageWorker::saveCompleted,
        this, [this](quint64 imageId, bool success, const QString& fileName, const QString& error) {
            if (!success) {
                qWarning() << "WefaxDemod: could not save image to" << fileName << error;
            }
            if (!success) {
                // A capture may already have advanced to a new image ID by
                // the time an automatic save completes. Its error still needs
                // to be visible to the operator and Web API.
                m_lastSaveError = error;
            } else if (imageId == m_imageId) {
                m_lastSaveError.clear();
            }
            sendCurrentImageReport(success);
        });
    connect(m_imageWorker, &WefaxDemodImageWorker::queueOverloaded,
        this, [this](quint64 overflowCount) {
            m_imageQueueOverflows = overflowCount;
            qWarning() << "WefaxDemod: image queue overloaded" << overflowCount;
        });
    m_imageThread.start();
    m_imageWorker->enqueueClear(m_imageId);
    connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &WefaxDemod::networkManagerFinished);
    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
}

WefaxDemod::~WefaxDemod()
{
    disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &WefaxDemod::networkManagerFinished);
    delete m_networkManager;
    if (m_basebandSink->isRunning()) {
        stopBasebandSink();
    }

    // This object's event loop will not run again, so apply the queued
    // decoder reports, including the final one carrying the sink's held rows,
    // before the image thread stops. That publishes and auto-saves the capture.
    m_basebandSink->finishCapture(WefaxDemodSink::CompletionReason::Shutdown);
    setMessageQueueToGUI(nullptr);
    Message *message;
    while ((message = m_inputMessageQueue.pop()))
    {
        if (WefaxDemodBaseband::MsgDecoderReport::match(*message)) {
            handleMessage(*message);
        }
        delete message;
    }

    if (m_imageThread.isRunning())
    {
        QMetaObject::invokeMethod(m_imageWorker, []() {}, Qt::BlockingQueuedConnection);
        m_imageThread.quit();
        m_imageThread.wait();
    }
    delete m_imageWorker;

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    delete m_basebandSink;
}

void WefaxDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI == m_deviceAPI) {
        return;
    }

    // In every state: it ends a capture or phasing in progress, and clears
    // the AFC correction measured on the previous signal.
    m_basebandSink->getInputMessageQueue()->push(
        WefaxDemodBaseband::MsgDecoderAction::create(
            WefaxDemodBaseband::MsgDecoderAction::SourceChanged));

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, false);
    m_deviceAPI = deviceAPI;
    m_deviceAPI->addChannelSink(this, m_settings.m_streamIndex);
    m_deviceAPI->addChannelSinkAPI(this);
}

uint32_t WefaxDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void WefaxDemod::feed(
    const SampleVector::const_iterator& begin,
    const SampleVector::const_iterator& end,
    bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void WefaxDemod::start()
{
    startBasebandSink();
}

void WefaxDemod::startBasebandSink()
{
    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();
    m_basebandSink->getInputMessageQueue()->push(
        new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency));
    m_basebandSink->getInputMessageQueue()->push(
        WefaxDemodBaseband::MsgConfigureWefaxDemodBaseband::create(
            QStringList(), m_settings, true));
}

void WefaxDemod::stop()
{
    stopBasebandSink();
    // The final report follows any reports already queued to this channel,
    // so the sink's held rows are appended in order before the capture is
    // published and auto-saved.
    m_basebandSink->finishCapture(WefaxDemodSink::CompletionReason::Shutdown);
}

void WefaxDemod::stopBasebandSink()
{
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool WefaxDemod::handleMessage(const Message& message)
{
    if (MsgConfigureWefaxDemod::match(message))
    {
        const auto& configure = static_cast<const MsgConfigureWefaxDemod&>(message);
        applySettings(configure.getSettingsKeys(), configure.getSettings(), configure.getForce());
        return true;
    }

    if (DSPSignalNotification::match(message))
    {
        const auto& notification = static_cast<const DSPSignalNotification&>(message);
        const bool sourceChanged = m_basebandSampleRate > 0
            && ((m_basebandSampleRate != notification.getSampleRate())
                || (m_centerFrequency != notification.getCenterFrequency()));
        // In every state: it ends a capture or phasing in progress, and
        // clears the AFC correction measured on the previous signal.
        if (sourceChanged)
        {
            m_basebandSink->getInputMessageQueue()->push(
                WefaxDemodBaseband::MsgDecoderAction::create(
                    WefaxDemodBaseband::MsgDecoderAction::SourceChanged));
        }
        m_basebandSampleRate = notification.getSampleRate();
        m_centerFrequency = notification.getCenterFrequency();
        m_basebandSink->getInputMessageQueue()->push(new DSPSignalNotification(notification));

        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notification));
        }
        return true;
    }

    if (MsgDecoderAction::match(message))
    {
        const auto action = static_cast<const MsgDecoderAction&>(message).getAction();

        if (action == MsgDecoderAction::Clear)
        {
            clearImage();
            sendCurrentImageReport();
            return true;
        }
        if (action == MsgDecoderAction::Save) {
            saveImage();
            return true;
        }

        WefaxDemodBaseband::MsgDecoderAction::Action basebandAction;
        switch (action)
        {
        case MsgDecoderAction::StartPhasing:
            basebandAction = WefaxDemodBaseband::MsgDecoderAction::StartPhasing;
            break;
        case MsgDecoderAction::FinishPhasing:
            basebandAction = WefaxDemodBaseband::MsgDecoderAction::FinishPhasing;
            break;
        case MsgDecoderAction::StartReceiving:
            basebandAction = WefaxDemodBaseband::MsgDecoderAction::StartReceiving;
            break;
        case MsgDecoderAction::Stop:
            basebandAction = WefaxDemodBaseband::MsgDecoderAction::Stop;
            break;
        default:
            return false;
        }

        m_basebandSink->getInputMessageQueue()->push(
            WefaxDemodBaseband::MsgDecoderAction::create(basebandAction));
        return true;
    }

    if (WefaxDemodBaseband::MsgDecoderReport::match(message))
    {
        const auto& report = static_cast<const WefaxDemodBaseband::MsgDecoderReport&>(message);

        if (m_settings.m_autoMode
            && (report.getState() != WefaxDecoder::State::Idle)
            && ((m_settings.m_ioc != report.getIOC())
                || (m_settings.m_linesPerMinute != report.getLinesPerMinute())))
        {
            m_settings.m_ioc = report.getIOC();
            m_settings.m_linesPerMinute = report.getLinesPerMinute();
            m_imageWorker->enqueueConfigure(m_settings);
            if (m_guiMessageQueue) {
                m_guiMessageQueue->push(MsgConfigureWefaxDemod::create(
                    QStringList({"ioc", "linesPerMinute"}), m_settings, false));
            }
        }

        if ((report.getState() == WefaxDecoder::State::Receiving)
            && (m_decoderState != WefaxDecoder::State::Receiving)) {
            // A transition that ended the previous capture may already have
            // cleared and advanced the generation. Do not advance it twice.
            if (!m_image.isNull() || (m_imageRows > 0)) {
                clearImage();
            }
            m_captureStartTime = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            m_captureFrequencyHz = m_centerFrequency + m_settings.m_inputFrequencyOffset;
            m_captureIOC = report.getIOC();
            m_captureLinesPerMinute = report.getLinesPerMinute();
            m_imageWorker->enqueueAlignmentHint(m_imageId, report.getTimingSource() == "phasing");
        }

        appendLines(report.getLines());
        const bool captureEnded = (m_decoderState == WefaxDecoder::State::Receiving)
            && (report.getState() != WefaxDecoder::State::Receiving);
        m_decoderState = report.getState();
        m_phasingLineCount = report.getPhasingLineCount();
        m_samplesPerLine = report.getSamplesPerLine();
        m_clockCorrectionPpm = report.getClockCorrectionPpm();
        m_appliedClockCorrectionPpm = report.getAppliedClockCorrectionPpm();
        m_confidence = report.getConfidence();
        m_completionReason = WefaxDemodSink::completionReasonText(report.getCompletionReason());
        m_tuningErrorHz = report.getTuningErrorHz();
        m_effectiveLinesPerMinute = report.getEffectiveLinesPerMinute();
        m_timingSource = report.getTimingSource();
        m_timingStatus = report.getTimingStatus();
        // State and timing reports are independent of the throttled raster
        // snapshots, so controls and Web API state remain current.
        forwardImageReport(report);

        if (captureEnded)
        {
            m_imageWorker->enqueuePublish(m_imageId);
            if (m_settings.m_autoSave) {
                saveImage();
            }

            const auto reason = report.getCompletionReason();
            const bool beginNewCapture = reason == WefaxDemodSink::CompletionReason::Rephase
                || reason == WefaxDemodSink::CompletionReason::ModeChanged
                || reason == WefaxDemodSink::CompletionReason::NewStart
                || reason == WefaxDemodSink::CompletionReason::SourceChanged;
            if (beginNewCapture)
            {
                clearImage();
                sendCurrentImageReport();
            }
        }
        return true;
    }

    return false;
}

void WefaxDemod::setCenterFrequency(qint64 frequency)
{
    WefaxDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList({"inputFrequencyOffset"}), settings, false);

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureWefaxDemod::create(
            QStringList({"inputFrequencyOffset"}), settings, false));
    }
}

bool WefaxDemod::deserialize(const QByteArray& data)
{
    const bool success = m_settings.deserialize(data);

    if (!success) {
        m_settings.resetToDefaults();
    }

    m_inputMessageQueue.push(MsgConfigureWefaxDemod::create(
        QStringList(), m_settings, true));
    return success;
}

qint64 WefaxDemod::getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
{
    (void) streamIndex;
    (void) sinkElseSource;
    return m_settings.m_inputFrequencyOffset;
}

int WefaxDemod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setWefaxDemodSettings(new SWGSDRangel::SWGWefaxDemodSettings());
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int WefaxDemod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    WefaxDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);
    settings.validate();
    m_inputMessageQueue.push(MsgConfigureWefaxDemod::create(channelSettingsKeys, settings, force));
    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureWefaxDemod::create(channelSettingsKeys, settings, force));
    }
    webapiFormatChannelSettings(response, settings);
    return 200;
}

int WefaxDemod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setWefaxDemodReport(new SWGSDRangel::SWGWefaxDemodReport());
    auto *report = response.getWefaxDemodReport();
    report->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    const char *state = m_decoderState == WefaxDecoder::State::Phasing ? "phasing"
        : m_decoderState == WefaxDecoder::State::Receiving ? "receiving" : "idle";
    report->setState(new QString(state));
    report->setPhasingLineCount(m_phasingLineCount);
    report->setSamplesPerLine(static_cast<float>(m_samplesPerLine));
    report->setClockCorrectionPpm(static_cast<float>(m_clockCorrectionPpm));
    report->setAppliedClockCorrectionPpm(static_cast<float>(m_appliedClockCorrectionPpm));
    report->setConfidence(static_cast<float>(m_confidence));
    report->setIoc(m_settings.m_ioc);
    report->setLinesPerMinute(m_settings.m_linesPerMinute);
    report->setImageWidth(m_imageRows > 0 ? m_image.width() : WefaxDecoder::rasterWidth(m_settings.m_ioc));
    report->setImageHeight(m_imageRows);
    report->setImageQueueOverflows(static_cast<qint32>(std::min<quint64>(
        m_imageQueueOverflows, static_cast<quint64>(std::numeric_limits<qint32>::max()))));
    report->setImageId(static_cast<qint64>(m_imageId));
    report->setCompletionReason(new QString(m_completionReason));
    report->setLastSaveError(new QString(m_lastSaveError));
    report->setTuningErrorHz(static_cast<float>(m_tuningErrorHz));
    report->setSlantCorrectionPpm(static_cast<float>(m_slantCorrectionPpm));
    report->setEffectiveLinesPerMinute(static_cast<float>(m_effectiveLinesPerMinute));
    report->setTimingSource(new QString(m_timingSource));
    report->setTimingStatus(new QString(m_timingStatus));
    report->setTimingAccepted(m_timingStatus == "accepted" ? 1 : 0);
    report->setRequiredBandwidthHz(static_cast<float>(requiredBandwidthHz()));
    report->setBandwidthSufficient(bandwidthSufficient() ? 1 : 0);
    report->setComplete(m_completionReason == "stop-tone" ? 1 : 0);
    report->setCaptureStartTime(new QString(m_captureStartTime));
    report->setCaptureFrequencyHz(m_captureFrequencyHz);
    return 200;
}

int WefaxDemod::webapiActionsPost(
    const QStringList& channelActionsKeys,
    SWGSDRangel::SWGChannelActions& query,
    QString& errorMessage)
{
    const auto *actions = query.getWefaxDemodActions();
    if (!actions)
    {
        errorMessage = "Missing WefaxDemodActions in query";
        return 400;
    }

    bool handled = false;
    const auto post = [this, &handled](bool requested, MsgDecoderAction::Action action) {
        if (requested) {
            m_inputMessageQueue.push(MsgDecoderAction::create(action));
            handled = true;
        }
    };
    post(channelActionsKeys.contains("startPhasing") && actions->getStartPhasing() != 0,
        MsgDecoderAction::StartPhasing);
    post(channelActionsKeys.contains("finishPhasing") && actions->getFinishPhasing() != 0,
        MsgDecoderAction::FinishPhasing);
    post(channelActionsKeys.contains("startReceiving") && actions->getStartReceiving() != 0,
        MsgDecoderAction::StartReceiving);
    post(channelActionsKeys.contains("stop") && actions->getStop() != 0, MsgDecoderAction::Stop);
    post(channelActionsKeys.contains("clear") && actions->getClear() != 0, MsgDecoderAction::Clear);
    post(channelActionsKeys.contains("save") && actions->getSave() != 0, MsgDecoderAction::Save);

    if (!handled) {
        errorMessage = "No WEFAX action was requested";
        return 400;
    }
    return 202;
}

int WefaxDemod::webapiWorkspaceGet(
    SWGSDRangel::SWGWorkspaceInfo& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

void WefaxDemod::webapiUpdateChannelSettings(
    WefaxDemodSettings& settings,
    const QStringList& keys,
    SWGSDRangel::SWGChannelSettings& response)
{
    const auto *api = response.getWefaxDemodSettings();
    if (!api) {
        return;
    }
    if (keys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = api->getInputFrequencyOffset();
    }
    if (keys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = api->getRfBandwidth();
    }
    if (keys.contains("fmDeviation")) {
        settings.m_fmDeviation = api->getFmDeviation();
    }
    if (keys.contains("ioc")) {
        settings.m_ioc = api->getIoc();
    }
    if (keys.contains("linesPerMinute")) {
        settings.m_linesPerMinute = api->getLinesPerMinute();
    }
    if (keys.contains("autoMode")) {
        settings.m_autoMode = api->getAutoMode();
    }
    if (keys.contains("inverted")) {
        settings.m_inverted = api->getInverted();
    }
    if (keys.contains("minimumPhasingLines")) {
        settings.m_minimumPhasingLines = api->getMinimumPhasingLines();
    }
    if (keys.contains("startConfirmSeconds")) {
        settings.m_startConfirmSeconds = api->getStartConfirmSeconds();
    }
    if (keys.contains("stopConfirmSeconds")) {
        settings.m_stopConfirmSeconds = api->getStopConfirmSeconds();
    }
    if (keys.contains("manualClockCorrectionPpm")) {
        settings.m_manualClockCorrectionPpm = api->getManualClockCorrectionPpm();
    }
    if (keys.contains("maxRows")) {
        settings.m_maxRows = api->getMaxRows();
    }
    if (keys.contains("autoSave")) {
        settings.m_autoSave = api->getAutoSave();
    }
    if (keys.contains("displayInverted")) {
        settings.m_displayInverted = api->getDisplayInverted();
    }
    if (keys.contains("displayContrast")) {
        settings.m_displayContrast = api->getDisplayContrast();
    }
    if (keys.contains("displayThreshold")) {
        settings.m_displayThreshold = api->getDisplayThreshold();
    }
    if (keys.contains("horizontalAlignment")) {
        settings.m_horizontalAlignment = api->getHorizontalAlignment();
    }
    if (keys.contains("displaySlantCorrectionPpm")) {
        settings.m_displaySlantCorrectionPpm = api->getDisplaySlantCorrectionPpm();
    }
    if (keys.contains("displayRotation")) {
        settings.m_displayRotation = api->getDisplayRotation();
    }
    if (keys.contains("displayZoomPercent")) {
        settings.m_displayZoomPercent = api->getDisplayZoomPercent();
    }
    if (keys.contains("autoScroll")) {
        settings.m_autoScroll = api->getAutoScroll();
    }
    if (keys.contains("autoSlant")) {
        settings.m_autoSlant = api->getAutoSlant();
    }
    if (keys.contains("rgbColor")) {
        settings.m_rgbColor = api->getRgbColor();
    }
    if (keys.contains("streamIndex")) {
        settings.m_streamIndex = api->getStreamIndex();
    }
    if (keys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = api->getUseReverseApi();
    }
    if (keys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = api->getReverseApiPort();
    }
    if (keys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = api->getReverseApiDeviceIndex();
    }
    if (keys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = api->getReverseApiChannelIndex();
    }
    if (keys.contains("autoSavePath") && api->getAutoSavePath()) {
        settings.m_autoSavePath = *api->getAutoSavePath();
    }
    if (keys.contains("title") && api->getTitle()) {
        settings.m_title = *api->getTitle();
    }
    if (keys.contains("reverseAPIAddress") && api->getReverseApiAddress()) {
        settings.m_reverseAPIAddress = *api->getReverseApiAddress();
    }
}

void WefaxDemod::webapiFormatChannelSettings(
    SWGSDRangel::SWGChannelSettings& response,
    const WefaxDemodSettings& settings)
{
    auto *api = response.getWefaxDemodSettings();
    if (!api)
    {
        response.setWefaxDemodSettings(new SWGSDRangel::SWGWefaxDemodSettings());
        api = response.getWefaxDemodSettings();
    }
    api->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    api->setRfBandwidth(settings.m_rfBandwidth);
    api->setFmDeviation(settings.m_fmDeviation);
    api->setIoc(settings.m_ioc);
    api->setLinesPerMinute(settings.m_linesPerMinute);
    api->setAutoMode(settings.m_autoMode ? 1 : 0);
    api->setInverted(settings.m_inverted ? 1 : 0);
    api->setMinimumPhasingLines(settings.m_minimumPhasingLines);
    api->setStartConfirmSeconds(static_cast<float>(settings.m_startConfirmSeconds));
    api->setStopConfirmSeconds(static_cast<float>(settings.m_stopConfirmSeconds));
    api->setManualClockCorrectionPpm(static_cast<float>(settings.m_manualClockCorrectionPpm));
    api->setMaxRows(settings.m_maxRows);
    api->setAutoSave(settings.m_autoSave ? 1 : 0);
    api->setAutoSavePath(new QString(settings.m_autoSavePath));
    api->setDisplayInverted(settings.m_displayInverted ? 1 : 0);
    api->setDisplayContrast(settings.m_displayContrast);
    api->setDisplayThreshold(settings.m_displayThreshold);
    api->setHorizontalAlignment(settings.m_horizontalAlignment);
    api->setDisplaySlantCorrectionPpm(static_cast<float>(settings.m_displaySlantCorrectionPpm));
    api->setDisplayRotation(settings.m_displayRotation);
    api->setDisplayZoomPercent(settings.m_displayZoomPercent);
    api->setAutoScroll(settings.m_autoScroll ? 1 : 0);
    api->setAutoSlant(settings.m_autoSlant ? 1 : 0);
    api->setRgbColor(settings.m_rgbColor);
    api->setTitle(new QString(settings.m_title));
    api->setStreamIndex(settings.m_streamIndex);
    api->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    api->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    api->setReverseApiPort(settings.m_reverseAPIPort);
    api->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    api->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void WefaxDemod::webapiFormatReverseSettings(
    SWGSDRangel::SWGChannelSettings& response,
    const WefaxDemodSettings& settings,
    const QStringList& keys,
    bool force)
{
    if (!response.getWefaxDemodSettings()) {
        response.setWefaxDemodSettings(new SWGSDRangel::SWGWefaxDemodSettings());
    }
    auto *api = response.getWefaxDemodSettings();

#define FORMAT_REVERSE_VALUE(key, setter, value) if (force || keys.contains(key)) api->setter(value)
    FORMAT_REVERSE_VALUE("inputFrequencyOffset", setInputFrequencyOffset, settings.m_inputFrequencyOffset);
    FORMAT_REVERSE_VALUE("rfBandwidth", setRfBandwidth, settings.m_rfBandwidth);
    FORMAT_REVERSE_VALUE("fmDeviation", setFmDeviation, settings.m_fmDeviation);
    FORMAT_REVERSE_VALUE("ioc", setIoc, settings.m_ioc);
    FORMAT_REVERSE_VALUE("linesPerMinute", setLinesPerMinute, settings.m_linesPerMinute);
    FORMAT_REVERSE_VALUE("autoMode", setAutoMode, settings.m_autoMode ? 1 : 0);
    FORMAT_REVERSE_VALUE("inverted", setInverted, settings.m_inverted ? 1 : 0);
    FORMAT_REVERSE_VALUE("minimumPhasingLines", setMinimumPhasingLines, settings.m_minimumPhasingLines);
    FORMAT_REVERSE_VALUE("startConfirmSeconds", setStartConfirmSeconds, static_cast<float>(settings.m_startConfirmSeconds));
    FORMAT_REVERSE_VALUE("stopConfirmSeconds", setStopConfirmSeconds, static_cast<float>(settings.m_stopConfirmSeconds));
    FORMAT_REVERSE_VALUE("manualClockCorrectionPpm", setManualClockCorrectionPpm,
        static_cast<float>(settings.m_manualClockCorrectionPpm));
    FORMAT_REVERSE_VALUE("maxRows", setMaxRows, settings.m_maxRows);
    FORMAT_REVERSE_VALUE("autoSave", setAutoSave, settings.m_autoSave ? 1 : 0);
    FORMAT_REVERSE_VALUE("displayInverted", setDisplayInverted, settings.m_displayInverted ? 1 : 0);
    FORMAT_REVERSE_VALUE("displayContrast", setDisplayContrast, settings.m_displayContrast);
    FORMAT_REVERSE_VALUE("displayThreshold", setDisplayThreshold, settings.m_displayThreshold);
    FORMAT_REVERSE_VALUE("horizontalAlignment", setHorizontalAlignment, settings.m_horizontalAlignment);
    FORMAT_REVERSE_VALUE("displaySlantCorrectionPpm", setDisplaySlantCorrectionPpm,
        static_cast<float>(settings.m_displaySlantCorrectionPpm));
    FORMAT_REVERSE_VALUE("displayRotation", setDisplayRotation, settings.m_displayRotation);
    FORMAT_REVERSE_VALUE("displayZoomPercent", setDisplayZoomPercent, settings.m_displayZoomPercent);
    FORMAT_REVERSE_VALUE("autoScroll", setAutoScroll, settings.m_autoScroll ? 1 : 0);
    FORMAT_REVERSE_VALUE("autoSlant", setAutoSlant, settings.m_autoSlant ? 1 : 0);
    FORMAT_REVERSE_VALUE("rgbColor", setRgbColor, settings.m_rgbColor);
    FORMAT_REVERSE_VALUE("streamIndex", setStreamIndex, settings.m_streamIndex);
#undef FORMAT_REVERSE_VALUE
    if (force || keys.contains("autoSavePath")) {
        api->setAutoSavePath(new QString(settings.m_autoSavePath));
    }
    if (force || keys.contains("title")) {
        api->setTitle(new QString(settings.m_title));
    }
}

void WefaxDemod::applySettings(
    const QStringList& settingsKeys,
    const WefaxDemodSettings& settings,
    bool force)
{
    // Besides capture changes, the offset and bandwidth feed the reported
    // bandwidth check.
    const bool reportChanged = force
        || WefaxDemodSettings::endsCapture(settingsKeys)
        || settingsKeys.contains("inputFrequencyOffset")
        || settingsKeys.contains("rfBandwidth");

    if (settingsKeys.contains("streamIndex")
        && (settings.m_streamIndex != m_settings.m_streamIndex)
        && m_deviceAPI->getSampleMIMO())
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
        m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
        m_deviceAPI->addChannelSinkAPI(this);
        emit streamIndexChanged(settings.m_streamIndex);
    }

    m_basebandSink->getInputMessageQueue()->push(
        WefaxDemodBaseband::MsgConfigureWefaxDemodBaseband::create(
            settingsKeys, settings, force));

    if (settings.m_useReverseAPI)
    {
        const bool endpointChanged =
            (settingsKeys.contains("reverseAPIAddress")
                && (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress))
            || (settingsKeys.contains("reverseAPIPort")
                && (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort))
            || (settingsKeys.contains("reverseAPIDeviceIndex")
                && (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex))
            || (settingsKeys.contains("reverseAPIChannelIndex")
                && (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex));
        const bool enabled = settingsKeys.contains("useReverseAPI")
            && !m_settings.m_useReverseAPI;
        webapiReverseSendSettings(settingsKeys, settings, force || endpointChanged || enabled);
    }

    if (force) {
        m_settings = settings;
        m_settings.validate();
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
    m_imageWorker->enqueueConfigure(m_settings);

    if (reportChanged) {
        sendCurrentImageReport();
    }
}

void WefaxDemod::webapiReverseSendSettings(
    const QStringList& channelSettingsKeys,
    const WefaxDemodSettings& settings,
    bool force)
{
    SWGSDRangel::SWGChannelSettings channelSettings;
    webapiFormatReverseChannelSettings(channelSettingsKeys, channelSettings, settings, force);

    const QString url = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
        .arg(settings.m_reverseAPIAddress)
        .arg(settings.m_reverseAPIPort)
        .arg(settings.m_reverseAPIDeviceIndex)
        .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(url));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *buffer = new QBuffer();
    buffer->open(QBuffer::ReadWrite);
    buffer->write(channelSettings.asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply = m_networkManager->sendCustomRequest(
        m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);
}

void WefaxDemod::webapiFormatReverseChannelSettings(
    const QStringList& keys,
    SWGSDRangel::SWGChannelSettings& response,
    const WefaxDemodSettings& settings,
    bool force) const
{
    response.setDirection(0);
    response.setOriginatorChannelIndex(getIndexInDeviceSet());
    response.setOriginatorDeviceSetIndex(getDeviceSetIndex());
    response.setChannelType(new QString(m_channelId));
    webapiFormatReverseSettings(response, settings, keys, force);
}

void WefaxDemod::networkManagerFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qWarning() << "WefaxDemod::networkManagerFinished:"
            << static_cast<int>(reply->error()) << reply->errorString();
    }
    else
    {
        QByteArray answer = reply->readAll();
        if (answer.endsWith('\n')) {
            answer.chop(1);
        }
        qDebug() << "WefaxDemod::networkManagerFinished:" << answer;
    }
    reply->deleteLater();
}

void WefaxDemod::clearImage(bool newGeneration)
{
    if (newGeneration) {
        ++m_imageId;
    }
    m_image = QImage();
    m_imageRows = 0;
    m_slantCorrectionPpm = 0.0;
    m_alignmentPx = 0;
    m_imageWorker->enqueueClear(m_imageId);
}

bool WefaxDemod::appendLines(const WefaxDemodBaseband::MsgDecoderReport::Lines& lines)
{
    if (lines.empty()) {
        return false;
    }

    auto queuedLines = lines;
    return m_imageWorker->enqueueLines(m_imageId, std::move(queuedLines));
}

void WefaxDemod::handleImageUpdated(quint64 imageId, const QImage& image, int rowCount, double slantPpm, int alignmentPx)
{
    if (imageId != m_imageId) {
        return;
    }
    m_image = image;
    m_imageRows = rowCount;
    m_slantCorrectionPpm = slantPpm;
    m_alignmentPx = alignmentPx;
    sendCurrentImageReport();
}

QImage WefaxDemod::image() const
{
    if (m_imageRows <= 0) {
        return QImage();
    }
    // Worker updates already contain exactly the completed rows. QImage's
    // implicit sharing makes this return constant time.
    return m_image;
}

bool WefaxDemod::saveImage(const QString& fileName)
{
    m_lastSaveError.clear();
    m_imageWorker->enqueueSave(
        m_imageId,
        m_captureFrequencyHz,
        m_captureStartTime,
        m_captureIOC,
        m_captureLinesPerMinute,
        fileName);
    return true;
}

void WefaxDemod::sendCurrentImageReport(bool actionSucceeded)
{
    if (!m_guiMessageQueue) {
        return;
    }
    MsgImage *message = MsgImage::create(
        image(), m_decoderState, m_phasingLineCount, m_samplesPerLine,
        m_clockCorrectionPpm, m_appliedClockCorrectionPpm, m_confidence, m_settings.m_ioc,
        m_settings.m_linesPerMinute, m_imageId, m_completionReason,
        m_lastSaveError, m_tuningErrorHz, m_effectiveLinesPerMinute,
        m_timingSource, m_timingStatus, CalcDb::dbPower(getMagSq()),
        requiredBandwidthHz(), bandwidthSufficient(), actionSucceeded);
    message->setSlantCorrectionPpm(m_slantCorrectionPpm);
    message->setAlignmentPx(m_alignmentPx);
    m_guiMessageQueue->push(message);
}

void WefaxDemod::forwardImageReport(const WefaxDemodBaseband::MsgDecoderReport& report)
{
    if (!m_guiMessageQueue) {
        return;
    }

    MsgImage *message = MsgImage::create(
        image(),
        report.getState(),
        report.getPhasingLineCount(),
        report.getSamplesPerLine(),
        report.getClockCorrectionPpm(),
        report.getAppliedClockCorrectionPpm(),
        report.getConfidence(),
        report.getIOC(),
        report.getLinesPerMinute(),
        m_imageId,
        m_completionReason,
        m_lastSaveError,
        m_tuningErrorHz,
        m_effectiveLinesPerMinute,
        m_timingSource,
        m_timingStatus,
        CalcDb::dbPower(getMagSq()),
        requiredBandwidthHz(),
        bandwidthSufficient(),
        report.getActionSucceeded());
    message->setSlantCorrectionPpm(m_slantCorrectionPpm);
    message->setAlignmentPx(m_alignmentPx);
    m_guiMessageQueue->push(message);
}

double WefaxDemod::requiredBandwidthHz() const
{
    // Charts carry little energy near the pixel rate, so the full Carson
    // bandwidth is not needed. On a weak 120/576 recording, text stayed legible
    // down to about the shift plus a sixth of the pixel rate (1.4 kHz), while
    // anything wider only admitted more noise.
    const double pixelRate = WefaxDecoder::rasterWidth(m_settings.m_ioc)
        * static_cast<double>(m_settings.m_linesPerMinute) / 60.0;
    return m_settings.m_fmDeviation + pixelRate / 6.0;
}

bool WefaxDemod::bandwidthSufficient() const
{
    const double required = requiredBandwidthHz();
    if (m_settings.m_rfBandwidth + 1.0 < required) {
        return false;
    }
    return m_basebandSampleRate <= 0
        || (std::abs(static_cast<double>(m_settings.m_inputFrequencyOffset)) + required / 2.0
            <= m_basebandSampleRate / 2.0);
}
