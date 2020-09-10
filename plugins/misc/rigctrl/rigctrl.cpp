///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "rigctrl.h"

#include <QDebug>
#include <QThread>
#include <QtNetwork>

// Length of buffers
const unsigned int RigCtrl::m_CmdLength = 1024;
const unsigned int RigCtrl::m_UrlLength = 1024;
const unsigned int RigCtrl::m_ResponseLength = 1024;
const unsigned int RigCtrl::m_DataLength = 1024;

// Hamlib rigctrl error codes
enum rig_errcode_e {
    RIG_OK = 0,           /*!< No error, operation completed successfully */
    RIG_EINVAL = -1,      /*!< invalid parameter */
    RIG_ECONF = -2,       /*!< invalid configuration (serial,..) */
    RIG_ENOMEM = -3,      /*!< memory shortage */
    RIG_ENIMPL = -4,      /*!< function not implemented, but will be */
    RIG_ETIMEOUT = -5,    /*!< communication timed out */
    RIG_EIO = -6,         /*!< IO error, including open failed */
    RIG_EINTERNAL = -7,   /*!< Internal Hamlib error, huh! */
    RIG_EPROTO = -8,      /*!< Protocol error */
    RIG_ERJCTED = -9,     /*!< Command rejected by the rig */
    RIG_ETRUNC = -10,     /*!< Command performed, but arg truncated */
    RIG_ENAVAIL = -11,    /*!< function not available */
    RIG_ENTARGET = -12,   /*!< VFO not targetable */
    RIG_BUSERROR = -13,   /*!< Error talking on the bus */
    RIG_BUSBUSY = -14,    /*!< Collision on the bus */
    RIG_EARG = -15,       /*!< NULL RIG handle or any invalid pointer parameter in get arg */
    RIG_EVFO = -16,       /*!< Invalid VFO */
    RIG_EDOM = -17        /*!< Argument out of domain of func */
};

// Map rigctrl mode names to SDRangel modem names
const static struct mode_demod {
    const char *mode;
    const char *modem;
} mode_map[] = {
    {"FM", "NFMDemod"},
    {"WFM", "WFMDemod"},
    {"AM", "AMDemod"},
    {"LSB", "SSBDemod"},
    {"USB", "SSBDemod"},
    {nullptr, nullptr}
};

// Get double value from within nested JSON object
static double getSubObjectDouble(QJsonObject &json, const QString &key)
{
    double value = -1.0;

    for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++) {
        QJsonValue jsonValue = it.value();
        if (jsonValue.isObject()) {
            QJsonObject settings = jsonValue.toObject();
            if (settings.contains(key)) {
                value = settings[key].toDouble();
                return value;
            }
        }
    }

    return value;
}

// Set double value withing nested JSON object
static bool setSubObjectDouble(QJsonObject &json, const QString &key, double value)
{
    for (QJsonObject::iterator  it = json.begin(); it != json.end(); it++) {
        QJsonValue jsonValue = it.value();
        if (jsonValue.isObject()) {
            QJsonObject settings = jsonValue.toObject();
            if (settings.contains(key)) {
                settings[key] = value;
                it.value() = settings;
                return true;
            }
        }
    }
    return false;
}

RigCtrl::RigCtrl() :
    m_settings(),
    m_state(idle),
    m_tcpServer(nullptr),
    m_clientConnection(nullptr)
{    
    m_netman = new QNetworkAccessManager(this);
    connect(m_netman, &QNetworkAccessManager::finished, this, &RigCtrl::processAPIResponse);

    setSettings(&m_settings);
}

RigCtrl::~RigCtrl()
{
    if (m_clientConnection != nullptr) {
        m_clientConnection->close();
        delete m_clientConnection;
    }
    if (m_tcpServer != nullptr) {
        m_tcpServer->close();
        delete m_tcpServer;
    }
    delete m_netman;
}

void RigCtrl::getSettings(RigCtrlSettings *settings)
{
    *settings = m_settings;
}

// Is there a potential for this to be called while in getCommand or processAPIResponse?
void RigCtrl::setSettings(RigCtrlSettings *settings)
{
    bool disabled = (!settings->m_enabled) && m_settings.m_enabled;
    bool enabled = settings->m_enabled && !m_settings.m_enabled;
    bool portChanged = settings->m_rigCtrlPort != m_settings.m_rigCtrlPort;

    if (disabled || portChanged) {
        if (m_clientConnection != nullptr) {
            m_clientConnection->close();
            delete m_clientConnection;
            m_clientConnection = nullptr;
        }
        if (m_tcpServer != nullptr) {
            m_tcpServer->close();
            delete m_tcpServer;
            m_tcpServer = nullptr;
        }
    }

    if (enabled || portChanged) {
        qDebug() << "RigCtrl enabled on port " << settings->m_rigCtrlPort;
        m_tcpServer = new QTcpServer(this);
        if(!m_tcpServer->listen(QHostAddress::Any, settings->m_rigCtrlPort)) {
            qDebug() << "RigCtrl failed to listen on port " << settings->m_rigCtrlPort << ". Check it is not already in use.";
        } else {
            connect(m_tcpServer, &QTcpServer::newConnection, this, &RigCtrl::acceptConnection);
        }
    }

    m_settings = *settings;
}

QByteArray RigCtrl::serialize() const
{
    return m_settings.serialize();
}

bool RigCtrl::deserialize(const QByteArray& data)
{
    bool success = true;
    RigCtrlSettings settings;

    if (!settings.deserialize(data)) {
        success = false;
    } else {
        setSettings(&settings);
    }

    return success;
}

// Accept connection from rigctrl client
void RigCtrl::acceptConnection()
{
    m_clientConnection = m_tcpServer->nextPendingConnection();
    if (!m_clientConnection) {
        return;
    }
    connect(m_clientConnection, &QIODevice::readyRead, this, &RigCtrl::getCommand);
    connect(m_clientConnection, &QAbstractSocket::disconnected, m_clientConnection, &QObject::deleteLater);
}

// Get rigctrl command and start processing it
void RigCtrl::getCommand()
{
    char cmd[m_CmdLength];
    char url[m_UrlLength];
    char response[m_ResponseLength];
    qint64 len;
    QNetworkRequest request;
    char *p;
    int i, l;

    // Get rigctrld command from client
    len = m_clientConnection->readLine(cmd, sizeof(cmd));
    if (len != -1) {
        //qDebug() << "RigCtrl::getCommand - " << cmd;

        if (!strncmp(cmd, "F ", 2) || !strncmp(cmd, "set_freq ", 9)) {
            // Set frequency
            m_targetFrequency = atof(cmd[0] == 'F' ? &cmd[2] : &cmd[9]);
            // Get current centre frequency
            sprintf(url, "%s/deviceset/%d/device/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            m_netman->get(request);
            m_state = set_freq;
        } else if (!strncmp(cmd, "f", 1) || !strncmp(cmd, "get_freq", 8)) {
            // Get frequency - need to add centerFrequency and inputFrequencyOffset
            sprintf(url, "%s/deviceset/%d/device/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            m_netman->get(request);
            m_state = get_freq_center;
        } else if (!strncmp(cmd, "M ?", 3) || !(strncmp(cmd, "set_mode ?", 10))) {
            // Return list of modes supported
            p = response;
            for (i = 0; mode_map[i].mode != nullptr; i++) {
                p += sprintf(p, "%s ", mode_map[i].mode);
            }
            p += sprintf(p, "\n");
            m_clientConnection->write(response, strlen(response));
        } else if (!strncmp(cmd, "M ", 2) || !(strncmp(cmd, "set_mode ", 9))) {
            // Set mode
            // Map rigctrl mode name to SDRangel modem name
            m_targetModem = nullptr;
            m_targetBW = -1;
            p = cmd[0] == 'M' ? &cmd[2] : &cmd[9];
            for (i = 0; mode_map[i].mode != nullptr; i++) {
                l = strlen(mode_map[i].mode);
                if (!strncmp(p, mode_map[i].mode, l)) {
                    m_targetModem = mode_map[i].modem;
                    p += l;
                    break;
                }
            }
            // Save bandwidth, if given
            while(isspace(*p)) {
                p++;
            }
            if (*p == ',') {
                p++;
                m_targetBW = atoi(p);
            }
            if (mode_map[i].modem != nullptr) {
                // Delete current modem
                sprintf(url, "%s/deviceset/%d/channel/%d", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex, m_settings.m_channelIndex);
                request.setUrl(QUrl(url));
                m_netman->sendCustomRequest(request, "DELETE");
                m_state = set_mode_mod;
            } else {
                sprintf(response, "RPRT %d\n", RIG_EINVAL);
                m_clientConnection->write(response, strlen(response));
            }
        } else if (!strncmp(cmd, "set_powerstat 0", 15)) {
            // Power off radio
            sprintf(url, "%s/deviceset/%d/device/run", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            m_netman->sendCustomRequest(request, "DELETE");
            m_state = set_power_off;
        } else if (!strncmp(cmd, "set_powerstat 1", 15)) {
            // Power on radio
            sprintf(url, "%s/deviceset/%d/device/run", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            m_netman->post(request, "");
            m_state = set_power_on;
        } else if (!strncmp(cmd, "get_powerstat", 13)) {
            // Return if powered on or off
            sprintf(url, "%s/deviceset/%d/device/run", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            m_netman->get(request);
            m_state = get_power;
        } else {
            // Unimplemented command
            sprintf(response, "RPRT %d\n", RIG_ENIMPL);
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
        }
    }
}

// Process reply from SDRangel API server
void RigCtrl::processAPIResponse(QNetworkReply *reply)
{
    double freq;
    char response[m_ResponseLength];
    char url[m_UrlLength];
    char data[m_DataLength];
    QNetworkRequest request;

    if (reply->error() == QNetworkReply::NoError) {
        QString answer = reply->readAll();
        //qDebug("RigCtrl::processAPIResponse - '%s'", qUtf8Printable(answer));

        QJsonDocument jsonResponse = QJsonDocument::fromJson(answer.toUtf8());
        QJsonObject jsonObj = jsonResponse.object();

        switch (m_state) {

        case get_freq_center:
            // Reply with current center frequency
            freq = getSubObjectDouble(jsonObj, "centerFrequency");
            if (freq >= 0.0) {
                m_targetFrequency = freq;
                sprintf(url, "%s/deviceset/%d/channel/%d/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex, m_settings.m_channelIndex);
                request.setUrl(QUrl(url));
                m_netman->get(request);
            } else {
                sprintf(response, "RPRT %d\n", RIG_ENIMPL); // File source doesn't have centre frequency
            }
            m_state = get_freq_offset;
            break;

        case get_freq_offset:
            freq = m_targetFrequency + getSubObjectDouble(jsonObj, "inputFrequencyOffset");
            sprintf(response, "%u\n", (unsigned)freq);
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
            break;

        case set_freq:
            // Check if target requency is within max offset from current center frequency
            freq = getSubObjectDouble(jsonObj, "centerFrequency");
            if (fabs(freq - m_targetFrequency) > m_settings.m_maxFrequencyOffset) {
                // Update centerFrequency
                setSubObjectDouble(jsonObj, "centerFrequency", m_targetFrequency);
                sprintf(url, "%s/deviceset/%d/device/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
                request.setUrl(QUrl(url));
                m_netman->sendCustomRequest(request, "PATCH", QJsonDocument(jsonObj).toJson());
                m_state = set_freq_center;
            } else {
                // In range, so update inputFrequencyOffset
                m_targetOffset = m_targetFrequency - freq;
                // Get settings containg inputFrequencyOffset, so we can patch them
                sprintf(url, "%s/deviceset/%d/channel/%d/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex, m_settings.m_channelIndex);
                request.setUrl(QUrl(url));
                m_netman->get(request);
                m_state = set_freq_set_offset;
            }
            break;

        case set_freq_no_offset:
            // Update centerFrequency, without trying to set offset
            setSubObjectDouble(jsonObj, "centerFrequency", m_targetFrequency);
            sprintf(url, "%s/deviceset/%d/device/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            m_netman->sendCustomRequest(request, "PATCH", QJsonDocument(jsonObj).toJson());
            m_state = set_freq_center_no_offset;
            break;

        case set_freq_center:
            // Check whether frequency was set as expected
            freq = getSubObjectDouble(jsonObj, "centerFrequency");
            if (freq == m_targetFrequency) {
                // Set inputFrequencyOffset to 0
                m_targetOffset = 0;
                sprintf(url, "%s/deviceset/%d/channel/%d/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex, m_settings.m_channelIndex);
                request.setUrl(QUrl(url));
                m_netman->get(request);
                m_state = set_freq_set_offset;
            } else {
                sprintf(response, "RPRT %d\n", RIG_EINVAL);
                m_clientConnection->write(response, strlen(response));
                m_state = idle;
            }
            break;

       case set_freq_center_no_offset:
            // Check whether frequency was set as expected
            freq = getSubObjectDouble(jsonObj, "centerFrequency");
            if (freq == m_targetFrequency) {
                sprintf(response, "RPRT 0\n");
            } else {
                sprintf(response, "RPRT %d\n", RIG_EINVAL);
            }
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
            break;

        case set_freq_set_offset:
            // Patch inputFrequencyOffset
            if (setSubObjectDouble(jsonObj, "inputFrequencyOffset", m_targetOffset)) {
                sprintf(url, "%s/deviceset/%d/channel/%d/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex, m_settings.m_channelIndex);
                request.setUrl(QUrl(url));
                m_netman->sendCustomRequest(request, "PATCH", QJsonDocument(jsonObj).toJson());
                m_state = set_freq_offset;
            } else {
                // No inputFrequencyOffset
                sprintf(response, "RPRT %d\n", RIG_EINVAL);
                m_clientConnection->write(response, strlen(response));
                m_state = idle;
            }
            break;

        case set_freq_offset:
            freq = getSubObjectDouble(jsonObj, "inputFrequencyOffset");
            if (freq == m_targetOffset) {
                sprintf(response, "RPRT 0\n");
            } else {
                sprintf(response, "RPRT %d\n", RIG_EINVAL);
            }
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
            break;

        case set_mode_mod:
            // Create new modem
            sprintf(url, "%s/deviceset/%d/channel", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            sprintf(data, "{ \"channelType\": \"%s\",  \"direction\": 0,  \"originatorDeviceSetIndex\": %d}\n", m_targetModem, m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
            m_netman->post(request, data);
            if (m_targetBW >= 0) {
                m_state = set_mode_settings;
            } else {
                m_state = set_mode_reply;
            }
            break;

        case set_mode_settings:
            // Set modem bandwidth
            sprintf(url, "%s/deviceset/%d/channel/%d/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex, m_settings.m_channelIndex);
            sprintf(data, "{ \"channelType\": \"%s\", \"%sSettings\": {\"rfBandwidth\":%d}}\n", m_targetModem, m_targetModem, m_targetBW);
            request.setUrl(QUrl(url));
            request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
            m_netman->sendCustomRequest(request, "PATCH", data);
            m_state = set_mode_reply;
            break;

        case set_mode_reply:
            sprintf(response, "RPRT 0\n");
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
            break;

        case get_power:
            if (!jsonObj["state"].toString().compare("running")) {
                sprintf(response, "1\n");
            } else {
                sprintf(response, "0\n");
            }
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
            break;

        case set_power_on:
        case set_power_off:
            // Reply contains previous state, not current state, so assume it worked
            sprintf(response, "RPRT 0\n");
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
            break;
        }

    } else {
        //qDebug("RigCtrl::processAPIResponse - got error: '%s'", qUtf8Printable(reply->errorString()));
        //qDebug() << reply->readAll();

        switch (m_state) {
        case get_freq_offset:
            // Probably no modem enabled on the specified channel
            sprintf(response, "%u\n", (unsigned)m_targetFrequency);
            m_clientConnection->write(response, strlen(response));
            m_state = idle;
            break;

        case set_freq_set_offset:
            // Probably no demodulator enabled on the specified channel
            // Just set as center frequency
            sprintf(url, "%s/deviceset/%d/device/settings", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            m_netman->get(request);
            m_state = set_freq_no_offset;
            break;

       case set_mode_mod:
            // Probably no modem on channel to delete, so continue to try to create one
            sprintf(url, "%s/deviceset/%d/channel", qUtf8Printable(m_APIBaseURI), m_settings.m_deviceIndex);
            sprintf(data, "{ \"channelType\": \"%s\",  \"direction\": 0,  \"originatorDeviceSetIndex\": %d}\n", m_targetModem, m_settings.m_deviceIndex);
            request.setUrl(QUrl(url));
            request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
            m_netman->post(request, data);
            if (m_targetBW >= 0) {
                m_state = set_mode_settings;
            } else {
                m_state = set_mode_reply;
            }
            break;

        default:
            sprintf(response, "RPRT %d\n", RIG_EIO);
            m_clientConnection->write(response, strlen(response));
            break;
        }
    }

    reply->deleteLater();
}
