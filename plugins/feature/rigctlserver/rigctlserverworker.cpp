///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTimer>

#include <cmath>

#include "SWGDeviceState.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGDeviceSettings.h"
#include "SWGChannelSettings.h"
#include "SWGDeviceSet.h"

#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"

#include "rigctlserverworker.h"

MESSAGE_CLASS_DEFINITION(RigCtlServerWorker::MsgConfigureRigCtlServerWorker, Message)

const unsigned int RigCtlServerWorker::m_CmdLength = 1024;
const unsigned int RigCtlServerWorker::m_ResponseLength = 1024;
const struct RigCtlServerWorker::ModeDemod RigCtlServerWorker::m_modeMap[] = {
    {"FM", "NFMDemod"},
    {"WFM", "WFMDemod"},
    {"AM", "AMDemod"},
    {"LSB", "SSBDemod"},
    {"USB", "SSBDemod"},
    {nullptr, nullptr}
};

RigCtlServerWorker::RigCtlServerWorker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_state(idle),
    m_tcpServer(nullptr),
    m_clientConnection(nullptr),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToFeature(nullptr),
    m_running(false)
{
    qDebug("RigCtlServerWorker::RigCtlServerWorker");
}

RigCtlServerWorker::~RigCtlServerWorker()
{
    m_inputMessageQueue.clear();
}

void RigCtlServerWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool RigCtlServerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
    return m_running;
}

void RigCtlServerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    restartServer(false, 0);
    m_running = false;
}

void RigCtlServerWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool RigCtlServerWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureRigCtlServerWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureRigCtlServerWorker& cfg = (MsgConfigureRigCtlServerWorker&) cmd;
        qDebug() << "RigCtlServerWorker::handleMessage: MsgConfigureRigCtlServerWorker";

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

void RigCtlServerWorker::applySettings(const RigCtlServerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "RigCtlServerWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("rigCtlPort") ||
        settingsKeys.contains("enabled") ||  force)
    {
        restartServer(settings.m_enabled, settings.m_rigCtlPort);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void RigCtlServerWorker::restartServer(bool enabled, uint32_t port)
{
    if (m_tcpServer)
    {
        if (m_clientConnection)
        {
            m_clientConnection->close();
            delete m_clientConnection;
            m_clientConnection = nullptr;
        }

        disconnect(m_tcpServer, &QTcpServer::newConnection, this, &RigCtlServerWorker::acceptConnection);
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }

    if (enabled)
    {
        qDebug() << "RigCtlServerWorker::restartServer: server enabled on port " << port;
        m_tcpServer = new QTcpServer(this);

        if (!m_tcpServer->listen(QHostAddress::Any, port)) {
            qWarning("RigCtrl failed to listen on port %u. Check it is not already in use.", port);
        } else {
            connect(m_tcpServer, &QTcpServer::newConnection, this, &RigCtlServerWorker::acceptConnection);
        }
    }
    else
    {
        qDebug() << "RigCtlServerWorker::restartServer: server disabled";
    }
}

void RigCtlServerWorker::acceptConnection()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_clientConnection = m_tcpServer->nextPendingConnection();

    if (!m_clientConnection) {
        return;
    }

    connect(m_clientConnection, &QIODevice::readyRead, this, &RigCtlServerWorker::getCommand);
    connect(m_clientConnection, &QAbstractSocket::disconnected, m_clientConnection, &QObject::deleteLater);
}

// Get rigctl command and start processing it
void RigCtlServerWorker::getCommand()
{
    QMutexLocker mutexLocker(&m_mutex);
    char cmd[m_CmdLength];
    qint64 len;
    rig_errcode_e rigCtlRC;
    char response[m_ResponseLength];
    std::fill(response, response + m_ResponseLength, '\0');

    // Get rigctld command from client
    len = m_clientConnection->readLine(cmd, sizeof(cmd));

    if (len != -1)
    {
        //qDebug() << "RigCtrl::getCommand - " << cmd;
        if (!strncmp(cmd, "F ", 2) || !strncmp(cmd, "set_freq ", 9))
        {
            // Set frequency
            double targetFrequency = atof(cmd[0] == 'F' ? &cmd[2] : &cmd[9]);
            setFrequency(targetFrequency, rigCtlRC);
            sprintf(response, "RPRT %d\n", rigCtlRC);
        }
        else if (!strncmp(cmd, "f", 1) || !strncmp(cmd, "get_freq", 8))
        {
            // Get frequency - need to add centerFrequency and inputFrequencyOffset
            double frequency;

            if (getFrequency(frequency, rigCtlRC)) {
                sprintf(response, "%u\n", (unsigned int) frequency);
            } else {
                sprintf(response, "RPRT %d\n", rigCtlRC);
            }
        }
        else if (!strncmp(cmd, "M ?", 3) || !(strncmp(cmd, "set_mode ?", 10)))
        {
            // Return list of modes supported
            char *p = response;

            for (int i = 0; m_modeMap[i].mode != nullptr; i++) {
                p += sprintf(p, "%s ", m_modeMap[i].mode);
            }

            p += sprintf(p, "\n");
        }
        else if (!strncmp(cmd, "M ", 2) || !(strncmp(cmd, "set_mode ", 9)))
        {
            // Set mode
            // Map rigctrl mode name to SDRangel modem name
            const char *targetModem = nullptr;
            const char *targetMode = nullptr;
            int targetBW = -1;
            char *p = (cmd[0] == 'M' ? &cmd[2] : &cmd[9]);
            int i, l;

            for (i = 0; m_modeMap[i].mode != nullptr; i++)
            {
                l = strlen(m_modeMap[i].mode);

                if (!strncmp(p, m_modeMap[i].mode, l))
                {
                    targetMode = m_modeMap[i].mode;
                    targetModem = m_modeMap[i].modem;
                    p += l;
                    break;
                }
            }

            // Save bandwidth, if given
            while(isspace(*p)) {
                p++;
            }

            if (isdigit(*p))
            {
                targetBW = atoi(p);
            }

            if (m_modeMap[i].modem != nullptr)
            {
                changeModem(targetMode, targetModem, targetBW, rigCtlRC);
                sprintf(response, "RPRT %d\n", rigCtlRC);
            }
            else
            {
                sprintf(response, "RPRT %d\n", RIG_EINVAL);
                m_clientConnection->write(response, strlen(response));
            }
        }
        else if (!strncmp(cmd, "m", 1) || !(strncmp(cmd, "get_mode", 8)))
        {
            // Get mode
            const char *mode;
            double passband;

            if (getMode(&mode, passband, rigCtlRC))
            {
                if (passband < 0) {
                    sprintf(response, "%s\n", mode);
                } else {
                    sprintf(response, "%s %u\n", mode, (unsigned int) passband);
                }
            }
            else
            {
                sprintf(response, "RPRT %d\n", rigCtlRC);
            }
        }
        else if (!strncmp(cmd, "set_powerstat 0", 15))
        {
            // Power off radio
            setPowerOff(rigCtlRC);
            sprintf(response, "RPRT %d\n", rigCtlRC);
        }
        else if (!strncmp(cmd, "set_powerstat 1", 15))
        {
            // Power on radio
            setPowerOn(rigCtlRC);
            sprintf(response, "RPRT %d\n", rigCtlRC);
        }
        else if (!strncmp(cmd, "get_powerstat", 13))
        {
            // Return if powered on or off
            bool power;
            if (getPower(power, rigCtlRC))
            {
                if (power) {
                    sprintf(response, "1\n");
                } else {
                    sprintf(response, "0\n");
                }
            }
            else
            {
                sprintf(response, "RPRT %d\n", rigCtlRC);
            }
        }
        else
        {
            // Unimplemented command
            sprintf(response, "RPRT %d\n", RIG_ENIMPL);
            m_clientConnection->write(response, strlen(response));
        }
    }

    m_clientConnection->write(response, strlen(response));
}

bool RigCtlServerWorker::setFrequency(double targetFrequency, rig_errcode_e& rigCtlRC)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;

    // Get current device center frequency
    httpRC = m_webAPIAdapterInterface->devicesetDeviceSettingsGet(
        m_settings.m_deviceIndex,
        deviceSettingsResponse,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("RigCtlServerWorker::setFrequency: get device frequency error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
    double freq;

    if (WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", freq))
    {
        bool outOfRange = std::abs(freq - targetFrequency) > m_settings.m_maxFrequencyOffset;

        if (outOfRange)
        {
            // Update centerFrequency
            WebAPIUtils::setSubObjectDouble(*jsonObj, "centerFrequency", targetFrequency);
            QStringList deviceSettingsKeys;
            deviceSettingsKeys.append("centerFrequency");
            deviceSettingsResponse.init();
            deviceSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;

            httpRC = m_webAPIAdapterInterface->devicesetDeviceSettingsPutPatch(
                m_settings.m_deviceIndex,
                false, // PATCH
                deviceSettingsKeys,
                deviceSettingsResponse,
                errorResponse2
            );

            if (httpRC/100 == 2)
            {
                qDebug("RigCtlServerWorker::setFrequency: set device frequency %f OK", targetFrequency);
            }
            else
            {
                qWarning("RigCtlServerWorker::setFrequency: set device frequency error %d: %s",
                    httpRC, qPrintable(*errorResponse2.getMessage()));
                rigCtlRC = RIG_EINVAL;
                return false;
            }
        }

        // Update inputFrequencyOffset (offet if in range else zero)
        float targetOffset = outOfRange ?  0 : targetFrequency - freq;
        SWGSDRangel::SWGChannelSettings channelSettingsResponse;
        // Get channel settings containing inputFrequencyOffset, so we can patch it
        httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsGet(
            m_settings.m_deviceIndex,
            m_settings.m_channelIndex,
            channelSettingsResponse,
            errorResponse
        );

        if (httpRC/100 != 2)
        {
            qWarning("RigCtlServerWorker::setFrequency: get channel offset frequency error %d: %s",
                httpRC, qPrintable(*errorResponse.getMessage()));
            rigCtlRC = RIG_EINVAL;
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();

        if (WebAPIUtils::setSubObjectDouble(*jsonObj, "inputFrequencyOffset", targetOffset))
        {
            QStringList channelSettingsKeys;
            channelSettingsKeys.append("inputFrequencyOffset");
            channelSettingsResponse.init();
            channelSettingsResponse.fromJsonObject(*jsonObj);

            httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsPutPatch(
                m_settings.m_deviceIndex,
                m_settings.m_channelIndex,
                false, // PATCH
                channelSettingsKeys,
                channelSettingsResponse,
                errorResponse
            );

            if (httpRC/100 == 2)
            {
                qDebug("RigCtlServerWorker::setFrequency: set channel offset frequency %f OK", targetOffset);
            }
            else
            {
                qWarning("RigCtlServerWorker::setFrequency: set channel frequency offset error %d: %s",
                    httpRC, qPrintable(*errorResponse.getMessage()));
                rigCtlRC = RIG_EINVAL;
                return false;
            }
        }
        else
        {
            qWarning("RigCtlServerWorker::setFrequency: No inputFrequencyOffset key in channel settings");
            rigCtlRC = RIG_ENIMPL;
            return false;
        }
    }
    else
    {
        qWarning("RigCtlServerWorker::setFrequency: no centerFrequency key in device settings");
        rigCtlRC = RIG_ENIMPL;
        return false;
    }

    rigCtlRC = RIG_OK; // return OK
    return true;
}

bool RigCtlServerWorker::getFrequency(double& frequency, rig_errcode_e& rigCtlRC)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;

    // Get current device center frequency
    httpRC = m_webAPIAdapterInterface->devicesetDeviceSettingsGet(
        m_settings.m_deviceIndex,
        deviceSettingsResponse,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("RigCtlServerWorker::getFrequency: get device frequency error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
    double deviceFreq;

    if (WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", deviceFreq))
    {
        SWGSDRangel::SWGChannelSettings channelSettingsResponse;
        // Get channel settings containg inputFrequencyOffset, so we can patch them
        httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsGet(
            m_settings.m_deviceIndex,
            m_settings.m_channelIndex,
            channelSettingsResponse,
            errorResponse
        );

        if (httpRC/100 != 2)
        {
            qWarning("RigCtlServerWorker::setFrequency: get channel offset frequency error %d: %s",
                httpRC, qPrintable(*errorResponse.getMessage()));
            rigCtlRC = RIG_EINVAL;
            return false;
        }

        QJsonObject *jsonObj2 = channelSettingsResponse.asJsonObject();
        double channelOffset;

        if (WebAPIUtils::getSubObjectDouble(*jsonObj2, "inputFrequencyOffset", channelOffset))
        {
            frequency = deviceFreq + channelOffset;
        }
        else
        {
            qWarning("RigCtlServerWorker::setFrequency: No inputFrequencyOffset key in channel settings");
            rigCtlRC = RIG_ENIMPL;
            return false;
        }
    }
    else
    {
        qWarning("RigCtlServerWorker::setFrequency: no centerFrequency key in device settings");
        rigCtlRC = RIG_ENIMPL;
        return false;
    }

    rigCtlRC = RIG_OK;
    return true;
}

bool RigCtlServerWorker::changeModem(const char *newMode, const char *newModemId, int newModemBw, rig_errcode_e& rigCtlRC)
{
    SWGSDRangel::SWGDeviceSet deviceSetResponse;
    SWGSDRangel::SWGSuccessResponse successResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;
    int nbChannels;
    int currentOffset;

    // get deviceset details to get the number of channel modems
    httpRC = m_webAPIAdapterInterface->devicesetGet(
        m_settings.m_deviceIndex,
        deviceSetResponse,
        errorResponse
    );

    if (httpRC/100 != 2) // error
    {
        qWarning("RigCtlServerWorker::changeModem: deevice set get information error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    if (!WebAPIUtils::getObjectInt(*deviceSetResponse.asJsonObject(), "channelcount", nbChannels))
    {
        qWarning("RigCtlServerWorker::changeModem: no channelcount key in device set information");
        rigCtlRC = RIG_ENIMPL;
        return false;
    }

    QList<QJsonObject> channelObjects;

    if (!WebAPIUtils::getObjectObjects(*deviceSetResponse.asJsonObject(), "channels", channelObjects))
    {
        qWarning("RigCtlServerWorker::changeModem: no channels key in device set information");
        rigCtlRC = RIG_ENIMPL;
        return false;
    }

    if (m_settings.m_channelIndex < channelObjects.size())
    {
        const QJsonObject& channelInfo = channelObjects.at(m_settings.m_channelIndex);

        if (!WebAPIUtils::getObjectInt(channelInfo, "deltaFrequency", currentOffset))
        {
            qWarning("RigCtlServerWorker::changeModem: no deltaFrequency key in device set channel information");
            rigCtlRC = RIG_ENIMPL;
            return false;
        }
    }
    else
    {
        qWarning("RigCtlServerWorker::changeModem: channel not found in device set channels information");
        rigCtlRC = RIG_ENIMPL;
        return false;
    }

    // delete current modem
    httpRC = m_webAPIAdapterInterface->devicesetChannelDelete(
        m_settings.m_deviceIndex,
        m_settings.m_channelIndex,
        successResponse,
        errorResponse
    );

    if (httpRC/100 != 2) // error
    {
        qWarning("RigCtlServerWorker::changeModem: delete channel error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    // create new modem
    SWGSDRangel::SWGChannelSettings query;
    QString newModemIdStr(newModemId);
    bool lsb = !strncmp(newMode, "LSB", 3);
    query.init();
    query.setChannelType(new QString(newModemIdStr));
    query.setDirection(0);

    httpRC = m_webAPIAdapterInterface->devicesetChannelPost(
        m_settings.m_deviceIndex,
        query,
        successResponse,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("RigCtlServerWorker::changeModem: create channel error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    // wait for channel switchover
    QEventLoop loop;
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer->start(200);
    loop.exec();
    delete timer;

    // when a new channel is created it is put last in the list
    qDebug("RigCtlServerWorker::changeModem: created %s at %d", newModemId, nbChannels-1);

    if (m_msgQueueToFeature)
    {
        RigCtlServerSettings::MsgChannelIndexChange *msg = RigCtlServerSettings::MsgChannelIndexChange::create(nbChannels-1);
        m_msgQueueToFeature->push(msg);
    }

    SWGSDRangel::SWGChannelSettings swgChannelSettings;
    QStringList channelSettingsKeys;
    channelSettingsKeys.append("inputFrequencyOffset");
    QString jsonSettingsStr = tr("\"inputFrequencyOffset\":%1").arg(currentOffset);

    if (lsb || (newModemBw >= 0))
    {
        int bw = lsb ? (newModemBw < 0 ? -3000 : -newModemBw) : newModemBw;
        channelSettingsKeys.append("rfBandwidth");
        jsonSettingsStr.append(tr(",\"rfBandwidth\":%2").arg(bw));
    }

    QString jsonStr = tr("{ \"channelType\": \"%1\", \"%2Settings\": {%3}}")
        .arg(QString(newModemId))
        .arg(QString(newModemId))
        .arg(jsonSettingsStr);
    swgChannelSettings.fromJson(jsonStr);

    httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsPutPatch(
        m_settings.m_deviceIndex,
        nbChannels-1, // new index
        false, // PATCH
        channelSettingsKeys,
        swgChannelSettings,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("RigCtlServerWorker::changeModem: set channel settings error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    rigCtlRC = RIG_OK;
    return true;
}

bool RigCtlServerWorker::getMode(const char **mode, double& passband, rig_errcode_e& rigCtlRC)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;

    // Get channel settings containg inputFrequencyOffset, so we can patch them
    httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsGet(
        m_settings.m_deviceIndex,
        m_settings.m_channelIndex,
        channelSettingsResponse,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("RigCtlServerWorker::getModem: get channel settings error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    QJsonObject *jsonObj = channelSettingsResponse.asJsonObject();
    QString channelType;
    int i;

    if (!WebAPIUtils::getObjectString(*jsonObj, "channelType", channelType))
    {
        qWarning("RigCtlServerWorker::getModem: no channelType key in channel settings");
        rigCtlRC = RIG_ENIMPL;
        return false;
    }

    for (i = 0; m_modeMap[i].mode != nullptr; i++)
    {
        if (!channelType.compare(m_modeMap[i].modem))
        {
            *mode = m_modeMap[i].mode;
            break;
        }
    }

    if (!m_modeMap[i].mode)
    {
        qWarning("RigCtlServerWorker::getModem: channel type not implemented: %s", qPrintable(channelType));
        rigCtlRC = RIG_ENIMPL;
        return false;
    }


    if (WebAPIUtils::getSubObjectDouble(*jsonObj, "rfBandwidth", passband))
    {
        if (!channelType.compare("SSBDemod"))
        {
            if (passband < 0) { // LSB
                passband = -passband;
            } else { // USB
                *mode = m_modeMap[i+1].mode;
            }
        }
    }
    else
    {
        passband = -1;
    }

    rigCtlRC = RIG_OK;
    return true;
}

bool RigCtlServerWorker::setPowerOn(rig_errcode_e& rigCtlRC)
{
    SWGSDRangel::SWGDeviceState response;
    SWGSDRangel::SWGErrorResponse errorResponse;

    int httpRC = m_webAPIAdapterInterface->devicesetDeviceRunPost(
        m_settings.m_deviceIndex,
        response,
        errorResponse
    );

    if (httpRC/100 == 2)
    {
        qDebug("RigCtlServerWorker::setPowerOn: set device start OK");
        rigCtlRC = RIG_OK;
        return true;
    }
    else
    {
        qWarning("RigCtlServerWorker::setPowerOn: set device start error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }
}

bool RigCtlServerWorker::setPowerOff(rig_errcode_e& rigCtlRC)
{
    SWGSDRangel::SWGDeviceState response;
    SWGSDRangel::SWGErrorResponse errorResponse;

    int httpRC = m_webAPIAdapterInterface->devicesetDeviceRunDelete(
        m_settings.m_deviceIndex,
        response,
        errorResponse
    );

    if (httpRC/100 == 2)
    {
        qDebug("RigCtlServerWorker::setPowerOff: set device stop OK");
        rigCtlRC = RIG_OK;
        return true;
    }
    else
    {
        qWarning("RigCtlServerWorker::setPowerOff: set device stop error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }
}

bool RigCtlServerWorker::getPower(bool& power, rig_errcode_e& rigCtlRC)
{
    SWGSDRangel::SWGDeviceState response;
    SWGSDRangel::SWGErrorResponse errorResponse;

    int httpRC = m_webAPIAdapterInterface->devicesetDeviceRunGet(
        m_settings.m_deviceIndex,
        response,
        errorResponse
    );

    if (httpRC/100 != 2)
    {
        qWarning("RigCtlServerWorker::getPower: get device run state error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    QJsonObject *jsonObj = response.asJsonObject();
    QString state;

    if (WebAPIUtils::getObjectString(*jsonObj, "state", state))
    {
        qDebug("RigCtlServerWorker::getPower: run state is %s", qPrintable(state));

        if (state.compare("running")) {
            power = false; // not equal
        } else {
            power = true;  // equal
        }
    }
    else
    {
        qWarning("RigCtlServerWorker::getPower: get device run state error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        rigCtlRC = RIG_EINVAL;
        return false;
    }

    return true;
}
