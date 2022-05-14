///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "filesource.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGFileSourceReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesink.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/filerecord.h"
#include "feature/feature.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "filesourcebaseband.h"

MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSource, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSourceWork, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgReportFileSourceAcquisition, Message)

const char* const FileSource::m_channelIdURI = "sdrangel.channeltx.filesource";
const char* const FileSource::m_channelId ="FileSource";

FileSource::FileSource(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
	m_settingsMutex(QMutex::Recursive),
	m_frequencyOffset(0),
	m_basebandSampleRate(0),
    m_linearGain(0.0)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new FileSourceBaseband();
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FileSource::networkManagerFinished
    );
}

FileSource::~FileSource()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FileSource::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void FileSource::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void FileSource::start()
{
	qDebug("FileSource::start");
    m_basebandSource->reset();
    m_thread->start();
}

void FileSource::stop()
{
    qDebug("FileSource::stop");
	m_thread->exit();
	m_thread->wait();
}

void FileSource::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool FileSource::handleMessage(const Message& cmd)
{
    if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "FileSource::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        m_basebandSampleRate = notif.getSampleRate();
        calculateFrequencyOffset(); // This is when device sample rate changes
        setCenterFrequency(notif.getCenterFrequency());

        // Notify source of input sample rate change
        qDebug() << "FileSource::handleMessage: DSPSignalNotification: push to source";
        DSPSignalNotification *sig = new DSPSignalNotification(notif);
        m_basebandSource->getInputMessageQueue()->push(sig);

        if (m_guiMessageQueue)
        {
            qDebug() << "FileSource::handleMessage: DSPSignalNotification: push to GUI";
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgConfigureFileSource::match(cmd))
    {
        MsgConfigureFileSource& cfg = (MsgConfigureFileSource&) cmd;
        qDebug() << "FileSource::handleMessage: MsgConfigureFileSource";
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
	else if (MsgConfigureFileSourceWork::match(cmd))
	{
		MsgConfigureFileSourceWork& conf = (MsgConfigureFileSourceWork&) cmd;
        FileSourceBaseband::MsgConfigureFileSourceWork *msg = FileSourceBaseband::MsgConfigureFileSourceWork::create(conf.isWorking());
        m_basebandSource->getInputMessageQueue()->push(msg);

		return true;
	}
	else if (MsgConfigureFileSourceSeek::match(cmd))
	{
		MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        FileSourceBaseband::MsgConfigureFileSourceSeek *msg = FileSourceBaseband::MsgConfigureFileSourceSeek::create(conf.getMillis());
        m_basebandSource->getInputMessageQueue()->push(msg);

		return true;
	}
	else if (MsgConfigureFileSourceStreamTiming::match(cmd))
	{
        if (getMessageQueueToGUI())
        {
            FileSourceReport::MsgReportFileSourceStreamTiming *report =
                FileSourceReport::MsgReportFileSourceStreamTiming::create(m_basebandSource->getSamplesCount());
            getMessageQueueToGUI()->push(report);
        }

		return true;
	}
    else
    {
        return false;
    }
}

QByteArray FileSource::serialize() const
{
    return m_settings.serialize();
}

bool FileSource::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureFileSource *msg = MsgConfigureFileSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureFileSource *msg = MsgConfigureFileSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void FileSource::applySettings(const FileSourceSettings& settings, bool force)
{
    qDebug() << "FileSource::applySettings:"
        << "m_fileName:" << settings.m_fileName
        << "m_loop:" << settings.m_loop
        << "m_gainDB:" << settings.m_gainDB
        << "m_log2Interp:" << settings.m_log2Interp
        << "m_filterChainHash:" << settings.m_filterChainHash
        << "m_useReverseAPI:" << settings.m_useReverseAPI
        << "m_reverseAPIAddress:" << settings.m_reverseAPIAddress
        << "m_reverseAPIChannelIndex:" << settings.m_reverseAPIChannelIndex
        << "m_reverseAPIDeviceIndex:" << settings.m_reverseAPIDeviceIndex
        << "m_reverseAPIPort:" << settings.m_reverseAPIPort
        << "m_rgbColor:" << settings.m_rgbColor
        << "m_title:" << settings.m_title
        << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_fileName != settings.m_fileName) || force)
    {
        reverseAPIKeys.append("fileName");
        FileSourceBaseband::MsgConfigureFileSourceName *msg = FileSourceBaseband::MsgConfigureFileSourceName::create(settings.m_fileName);
        m_basebandSource->getInputMessageQueue()->push(msg);
    }

    if ((m_settings.m_loop != settings.m_loop) || force) {
        reverseAPIKeys.append("loop");
    }
    if ((m_settings.m_log2Interp != settings.m_log2Interp) || force) {
        reverseAPIKeys.append("log2Interp");
    }
    if ((m_settings.m_filterChainHash != settings.m_filterChainHash) || force) {
        reverseAPIKeys.append("filterChainHash");
    }
    if ((m_settings.m_gainDB != settings.m_gainDB) || force) {
        reverseAPIKeys.append("gainDB");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    FileSourceBaseband::MsgConfigureFileSourceBaseband *msg = FileSourceBaseband::MsgConfigureFileSourceBaseband::create(settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

void FileSource::validateFilterChainHash(FileSourceSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Interp; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void FileSource::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Interp, m_settings.m_filterChainHash);
    m_frequencyOffset = m_basebandSampleRate * shiftFactor;
}

int FileSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
    response.getFileSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int FileSource::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int FileSource::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    FileSourceSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureFileSource *msg = MsgConfigureFileSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("FileSource::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFileSource *msgToGUI = MsgConfigureFileSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void FileSource::webapiUpdateChannelSettings(
        FileSourceSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("fileName")) {
        settings.m_fileName = *response.getFileSourceSettings()->getFileName();
    }
    if (channelSettingsKeys.contains("loop")) {
        settings.m_loop = response.getFileSourceSettings()->getLoop() != 0;
    }
    if (channelSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getFileSourceSettings()->getLog2Interp();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getFileSourceSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("gainDB")) {
        settings.m_gainDB = response.getFileSourceSettings()->getGainDb();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFileSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFileSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getFileSourceSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFileSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFileSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFileSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFileSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getFileSourceSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getFileSourceSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getFileSourceSettings()->getRollupState());
    }
}

int FileSource::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSourceReport(new SWGSDRangel::SWGFileSourceReport());
    response.getFileSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int FileSource::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGFileSourceActions *swgFileSourceActions = query.getFileSourceActions();

    if (swgFileSourceActions)
    {
        if (channelActionsKeys.contains("play"))
        {
            bool play = swgFileSourceActions->getPlay() != 0;
            FileSourceBaseband::MsgConfigureFileSourceWork *msg = FileSourceBaseband::MsgConfigureFileSourceWork::create(play);
            m_basebandSource->getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
                MsgConfigureFileSourceWork *msgToGUI = MsgConfigureFileSourceWork::create(play);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }

        if (channelActionsKeys.contains("seekMillis"))
        {
            int seekMillis = swgFileSourceActions->getSeekMillis();
            seekMillis = seekMillis < 0 ? 0 : seekMillis > 1000 ? 1000 : seekMillis;
            FileSourceBaseband::MsgConfigureFileSourceSeek *msg
                = FileSourceBaseband::MsgConfigureFileSourceSeek::create(seekMillis);
            m_basebandSource->getInputMessageQueue()->push(msg);

            if (getMessageQueueToGUI())
            {
        		MsgConfigureFileSourceSeek *msgToGUI = MsgConfigureFileSourceSeek::create(seekMillis);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }

        return 202;
    }
    else
    {
        errorMessage = "Missing FileSourceActions in query";
        return 400;
    }
}

void FileSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FileSourceSettings& settings)
{
    if (response.getFileSourceSettings()->getFileName()) {
        *response.getFileSourceSettings()->getFileName() = settings.m_fileName;
    } else {
        response.getFileSourceSettings()->setFileName(new QString(settings.m_fileName));
    }

    response.getFileSourceSettings()->setLoop(settings.m_loop ? 1 : 0);
    response.getFileSourceSettings()->setLog2Interp(settings.m_log2Interp);
    response.getFileSourceSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getFileSourceSettings()->setGainDb(settings.m_gainDB);
    response.getFileSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getFileSourceSettings()->getTitle()) {
        *response.getFileSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getFileSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getFileSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFileSourceSettings()->getReverseApiAddress()) {
        *response.getFileSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFileSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFileSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFileSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getFileSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getFileSourceSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getFileSourceSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getFileSourceSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getFileSourceSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getFileSourceSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getFileSourceSettings()->setRollupState(swgRollupState);
        }
    }
}

void FileSource::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    qint64 t_sec = 0;
    qint64 t_msec = 0;
    quint64 samplesCount = m_basebandSource->getSamplesCount();
    uint32_t fileSampleRate = m_basebandSource->getFileSampleRate();
    quint64 startingTimeStamp = m_basebandSource->getStartingTimeStamp();
    quint64 fileRecordLength = m_basebandSource->getRecordLengthMuSec() / 1000000UL;
    quint32 fileSampleSize = m_basebandSource->getFileSampleSize();

    if (fileSampleRate > 0)
    {
        t_sec = samplesCount / fileSampleRate;
        t_msec = (samplesCount - (t_sec * fileSampleRate)) * 1000 / fileSampleRate;
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    response.getFileSourceReport()->setElapsedTime(new QString(t.toString("HH:mm:ss.zzz")));

    qint64 startingTimeStampMsec = startingTimeStamp * 1000LL;
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    dt = dt.addSecs(t_sec);
    dt = dt.addMSecs(t_msec);
    response.getFileSourceReport()->setAbsoluteTime(new QString(dt.toString("yyyy-MM-dd HH:mm:ss.zzz")));

    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(fileRecordLength);
    response.getFileSourceReport()->setDurationTime(new QString(recordLength.toString("HH:mm:ss")));

    response.getFileSourceReport()->setFileName(new QString(m_settings.m_fileName));
    response.getFileSourceReport()->setFileSampleRate(fileSampleRate);
    response.getFileSourceReport()->setFileSampleSize(fileSampleSize);
    response.getFileSourceReport()->setSampleRate(m_basebandSampleRate);
    response.getFileSourceReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
}

void FileSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FileSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgChannelSettings;
}

void FileSource::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const FileSourceSettings& settings,
    bool force)
{
    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue)
        {
            SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
            webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);
            MainCore::MsgChannelSettings *msg = MainCore::MsgChannelSettings::create(
                this,
                channelSettingsKeys,
                swgChannelSettings,
                force
            );
            messageQueue->push(msg);
        }
    }
}

void FileSource::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const FileSourceSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
    SWGSDRangel::SWGFileSourceSettings *swgFileSourceSettings = swgChannelSettings->getFileSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("log2Interp") || force) {
        swgFileSourceSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgFileSourceSettings->setFilterChainHash(settings.m_filterChainHash);
    }
    if (channelSettingsKeys.contains("gainDB") || force) {
        swgFileSourceSettings->setGainDb(settings.m_gainDB);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgFileSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgFileSourceSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgFileSourceSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgFileSourceSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgFileSourceSettings->setRollupState(swgRollupState);
    }
}

void FileSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FileSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FileSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void FileSource::getMagSqLevels(double& avg, double& peak, int& nbSamples) const
{
    m_basebandSource->getMagSqLevels(avg, peak, nbSamples);
}

void FileSource::setMessageQueueToGUI(MessageQueue* queue) {
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSource->setMessageQueueToGUI(queue);
}

double FileSource::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

uint32_t FileSource::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}
