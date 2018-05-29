///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <QDebug>


#ifdef _WIN32
#include <nn.h>
#include <pair.h>
#else
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#endif

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSDRdaemonSourceReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include <device/devicesourceapi.h>
#include <dsp/filerecord.h>

#include "sdrdaemonsourceinput.h"
#include "sdrdaemonsourceudphandler.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgConfigureSDRdaemonSource, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgConfigureSDRdaemonStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgReportSDRdaemonAcquisition, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSourceInput::MsgStartStop, Message)

SDRdaemonSourceInput::SDRdaemonSourceInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
	m_SDRdaemonUDPHandler(0),
	m_deviceDescription(),
	m_startingTimeStamp(0)
{
    m_sender = nn_socket(AF_SP, NN_PAIR);
    assert(m_sender != -1);
    int millis = 500;
    int rc __attribute__((unused)) = nn_setsockopt (m_sender, NN_SOL_SOCKET, NN_SNDTIMEO, &millis, sizeof (millis));
    assert (rc == 0);

	m_sampleFifo.setSize(96000 * 4);
	m_SDRdaemonUDPHandler = new SDRdaemonSourceUDPHandler(&m_sampleFifo, m_deviceAPI);

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

SDRdaemonSourceInput::~SDRdaemonSourceInput()
{
	stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
	delete m_SDRdaemonUDPHandler;
}

void SDRdaemonSourceInput::destroy()
{
    delete this;
}

void SDRdaemonSourceInput::init()
{
    applySettings(m_settings, true);
}

bool SDRdaemonSourceInput::start()
{
	qDebug() << "SDRdaemonSourceInput::start";
    m_SDRdaemonUDPHandler->start();
	return true;
}

void SDRdaemonSourceInput::stop()
{
	qDebug() << "SDRdaemonSourceInput::stop";
    m_SDRdaemonUDPHandler->stop();
}

QByteArray SDRdaemonSourceInput::serialize() const
{
    return m_settings.serialize();
}

bool SDRdaemonSourceInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSDRdaemonSource* message = MsgConfigureSDRdaemonSource::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRdaemonSource* messageToGUI = MsgConfigureSDRdaemonSource::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void SDRdaemonSourceInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
    m_SDRdaemonUDPHandler->setMessageQueueToGUI(queue);
}

const QString& SDRdaemonSourceInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SDRdaemonSourceInput::getSampleRate() const
{
    if (m_SDRdaemonUDPHandler->getSampleRate()) {
        return m_SDRdaemonUDPHandler->getSampleRate();
    } else {
        return m_settings.m_sampleRate / (1<<m_settings.m_log2Decim);
    }
}

quint64 SDRdaemonSourceInput::getCenterFrequency() const
{
    if (m_SDRdaemonUDPHandler->getCenterFrequency()) {
        return m_SDRdaemonUDPHandler->getCenterFrequency();
    } else {
        return m_settings.m_centerFrequency;
    }
}

void SDRdaemonSourceInput::setCenterFrequency(qint64 centerFrequency)
{
    SDRdaemonSourceSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureSDRdaemonSource* message = MsgConfigureSDRdaemonSource::create(m_settings, false);
    m_inputMessageQueue.push(message);
}

std::time_t SDRdaemonSourceInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool SDRdaemonSourceInput::isStreaming() const
{
    return m_SDRdaemonUDPHandler->isStreaming();
}

bool SDRdaemonSourceInput::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        return m_fileSink->handleMessage(notif); // forward to file sink
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "SDRdaemonSourceInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SDRdaemonSourceInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
        }

        return true;
    }
    else if (MsgConfigureSDRdaemonSource::match(message))
    {
        qDebug() << "SDRdaemonSourceInput::handleMessage:" << message.getIdentifier();
        MsgConfigureSDRdaemonSource& conf = (MsgConfigureSDRdaemonSource&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else
	{
		return false;
	}
}

void SDRdaemonSourceInput::applySettings(const SDRdaemonSourceSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    bool changeTxDelay = false;
    std::ostringstream os;
    int nbArgs = 0;
    QString remoteAddress;
    m_SDRdaemonUDPHandler->getRemoteAddress(remoteAddress);

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
        qDebug("SDRdaemonSourceInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                m_settings.m_dcBlock ? "true" : "false",
                m_settings.m_iqCorrection ? "true" : "false");
    }

    if (force || (m_settings.m_address != settings.m_address) || (m_settings.m_dataPort != settings.m_dataPort))
    {
        m_SDRdaemonUDPHandler->configureUDPLink(settings.m_address, settings.m_dataPort);
        m_SDRdaemonUDPHandler->getRemoteAddress(remoteAddress);
    }

    if (force || (remoteAddress != m_remoteAddress) || (m_settings.m_controlPort != settings.m_controlPort))
    {
        int rc = nn_shutdown(m_sender, 0);

        if (rc < 0) {
            qDebug() << "SDRdaemonSourceInput::applySettings: nn disconnection failed";
        } else {
            qDebug() << "SDRdaemonSourceInput::applySettings: nn disconnection successful";
        }

        std::ostringstream os;
        os << "tcp://" << remoteAddress.toStdString() << ":" << m_settings.m_controlPort;
        std::string addrstrng = os.str();
        rc = nn_connect(m_sender, addrstrng.c_str());

        if (rc < 0) {
            qDebug() << "SDRdaemonSourceInput::applySettings: nn connexion to " << addrstrng.c_str() << " failed";
        } else {
            qDebug() << "SDRdaemonSourceInput::applySettings: nn connexion to " << addrstrng.c_str() << " successful";
        }
    }

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency))
    {
        os << "freq=" << settings.m_centerFrequency;
        nbArgs++;
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate) || (m_settings.m_log2Decim != settings.m_log2Decim))
    {
        if (nbArgs > 0) os << ",";
        os << "srate=" << m_settings.m_sampleRate;
        nbArgs++;
        changeTxDelay = m_settings.m_sampleRate != settings.m_sampleRate;
    }

    if (force || (m_settings.m_log2Decim != settings.m_log2Decim))
    {
        if (nbArgs > 0) os << ",";
        os << "decim=" << settings.m_log2Decim;
        nbArgs++;
    }

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        if (nbArgs > 0) os << ",";
        os << "fcpos=" << m_settings.m_fcPos;
        nbArgs++;
    }

    if (force || (m_settings.m_nbFECBlocks != settings.m_nbFECBlocks))
    {
        if (nbArgs > 0) os << ",";
        os << "fecblk=" << m_settings.m_nbFECBlocks;
        nbArgs++;
        changeTxDelay = true;
    }

    if (force || (m_settings.m_txDelay != settings.m_txDelay))
    {
        changeTxDelay = true;
    }

    if (changeTxDelay)
    {
        double delay = ((127*127*settings.m_txDelay) / settings.m_sampleRate)/(128 + settings.m_nbFECBlocks);
        qDebug("SDRdaemonSourceInput::applySettings: Tx delay: %f us", delay*1e6);

        if (delay != 0.0)
        {
            if (nbArgs > 0) os << ",";
            os << "txdelay=" << (int) (delay*1e6);
            nbArgs++;
        }
    }

    if ((m_settings.m_specificParameters != settings.m_specificParameters) || force)
    {
        if (settings.m_specificParameters.size() > 0)
        {
            if (nbArgs > 0) os << ",";
            os << settings.m_specificParameters.toStdString();
            nbArgs++;
        }
    }

    if (nbArgs > 0)
    {
        int config_size = os.str().size();
        int rc = nn_send(m_sender, (void *) os.str().c_str(), config_size, 0);

        if (rc != config_size)
        {
            qDebug() << "SDRdaemonSourceInput::applySettings: Cannot nn send to "
                << " remoteAddress: " << remoteAddress
                << " remotePort: " << settings.m_controlPort
                << " message: " << os.str().c_str();
        }
        else
        {
            qDebug() << "SDRdaemonSourceInput::applySettings: nn send to "
                << "remoteAddress:" << remoteAddress
                << "remotePort:" << settings.m_controlPort
                << "message:" << os.str().c_str();
        }
    }

    mutexLocker.unlock();
    m_settings = settings;
    m_remoteAddress = remoteAddress;

    qDebug() << "SDRdaemonSourceInput::applySettings: "
            << " m_address: " << m_settings.m_address
            << " m_remoteAddress: " << m_remoteAddress
            << " m_dataPort: " << m_settings.m_dataPort
            << " m_controlPort: " << m_settings.m_controlPort
            << " m_centerFrequency: " << m_settings.m_centerFrequency
            << " m_sampleRate: " << m_settings.m_sampleRate
            << " m_log2Decim: " << m_settings.m_log2Decim
            << " m_fcPos: " << m_settings.m_fcPos
            << " m_txDelay: " << m_settings.m_txDelay
            << " m_nbFECBlocks: " << m_settings.m_nbFECBlocks
            << " m_specificParameters: " << m_settings.m_specificParameters;
}

int SDRdaemonSourceInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SDRdaemonSourceInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

int SDRdaemonSourceInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonSourceSettings(new SWGSDRangel::SWGSDRdaemonSourceSettings());
    response.getSdrDaemonSourceSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SDRdaemonSourceInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    SDRdaemonSourceSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getSdrDaemonSourceSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getSdrDaemonSourceSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getSdrDaemonSourceSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("txDelay")) {
        settings.m_txDelay = response.getSdrDaemonSourceSettings()->getTxDelay();
    }
    if (deviceSettingsKeys.contains("nbFECBlocks")) {
        settings.m_txDelay = response.getSdrDaemonSourceSettings()->getNbFecBlocks();
    }
    if (deviceSettingsKeys.contains("address")) {
        settings.m_address = *response.getSdrDaemonSourceSettings()->getAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getSdrDaemonSourceSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("controlPort")) {
        settings.m_controlPort = response.getSdrDaemonSourceSettings()->getControlPort();
    }
    if (deviceSettingsKeys.contains("specificParameters")) {
        settings.m_specificParameters = *response.getSdrDaemonSourceSettings()->getSpecificParameters();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getSdrDaemonSourceSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getSdrDaemonSourceSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        int fcPos = response.getSdrDaemonSourceSettings()->getFcPos();
        settings.m_fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
    }
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getSdrDaemonSourceSettings()->getFileRecordName();
    }

    MsgConfigureSDRdaemonSource *msg = MsgConfigureSDRdaemonSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSDRdaemonSource *msgToGUI = MsgConfigureSDRdaemonSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void SDRdaemonSourceInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRdaemonSourceSettings& settings)
{
    response.getSdrDaemonSourceSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getSdrDaemonSourceSettings()->setSampleRate(settings.m_sampleRate);
    response.getSdrDaemonSourceSettings()->setLog2Decim(settings.m_log2Decim);
    response.getSdrDaemonSourceSettings()->setTxDelay(settings.m_txDelay);
    response.getSdrDaemonSourceSettings()->setNbFecBlocks(settings.m_nbFECBlocks);
    response.getSdrDaemonSourceSettings()->setAddress(new QString(settings.m_address));
    response.getSdrDaemonSourceSettings()->setDataPort(settings.m_dataPort);
    response.getSdrDaemonSourceSettings()->setControlPort(settings.m_controlPort);
    response.getSdrDaemonSourceSettings()->setSpecificParameters(new QString(settings.m_specificParameters));
    response.getSdrDaemonSourceSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getSdrDaemonSourceSettings()->setIqCorrection(settings.m_iqCorrection);
    response.getSdrDaemonSourceSettings()->setFcPos((int) settings.m_fcPos);

    if (response.getSdrDaemonSourceSettings()->getFileRecordName()) {
        *response.getSdrDaemonSourceSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getSdrDaemonSourceSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}

int SDRdaemonSourceInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonSourceReport(new SWGSDRangel::SWGSDRdaemonSourceReport());
    response.getSdrDaemonSourceReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void SDRdaemonSourceInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getSdrDaemonSourceReport()->setCenterFrequency(m_SDRdaemonUDPHandler->getCenterFrequency());
    response.getSdrDaemonSourceReport()->setSampleRate(m_SDRdaemonUDPHandler->getSampleRate());
    response.getSdrDaemonSourceReport()->setBufferRwBalance(m_SDRdaemonUDPHandler->getBufferGauge());

    quint64 startingTimeStampMsec = ((quint64) m_SDRdaemonUDPHandler->getTVSec() * 1000LL) + ((quint64) m_SDRdaemonUDPHandler->getTVuSec() / 1000LL);
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    response.getSdrDaemonSourceReport()->setDaemonTimestamp(new QString(dt.toString("yyyy-MM-dd  HH:mm:ss.zzz")));

    response.getSdrDaemonSourceReport()->setMinNbBlocks(m_SDRdaemonUDPHandler->getMinNbBlocks());
    response.getSdrDaemonSourceReport()->setMaxNbRecovery(m_SDRdaemonUDPHandler->getMaxNbRecovery());
}
