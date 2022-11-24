///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "SWGDeviceState.h"
#include "SWGErrorResponse.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"

#include "util/simpleserializer.h"
#include "util/serialutil.h"
#include "settings/serializable.h"
#include "webapi/webapiadapterinterface.h"

#include "limerfe.h"

MESSAGE_CLASS_DEFINITION(LimeRFE::MsgConfigureLimeRFE, Message)
MESSAGE_CLASS_DEFINITION(LimeRFE::MsgReportSetRx, Message)
MESSAGE_CLASS_DEFINITION(LimeRFE::MsgReportSetTx, Message)

const char* const LimeRFE::m_featureIdURI = "sdrangel.feature.limerfe";
const char* const LimeRFE::m_featureId = "LimeRFE";

const std::map<int, std::string> LimeRFE::m_errorCodesMap = {
    { 0, "OK"},
    {-4, "Error synchronizing communication"},
    {-3, "Non-configurable GPIO pin specified. Only pins 4 and 5 are configurable."},
    {-2, "Problem with .ini configuration file"},
    {-1, "Communication error"},
    { 1, "Wrong TX connector - not possible to route TX of the selecrted channel to the specified port"},
    { 2, "Wrong RX connector - not possible to route RX of the selecrted channel to the specified port"},
    { 3, "Mode TXRX not allowed - when the same port is selected for RX and TX, it is not allowed to use mode RX & TX"},
    { 4, "Wrong mode for cellular channel - Cellular FDD bands (1, 2, 3, and 7) are only allowed mode RX & TX, while TDD band 38 is allowed only RX or TX mode"},
    { 5, "Cellular channels must be the same both for RX and TX"},
    { 6, "Requested channel code is wrong"}
};

LimeRFE::LimeRFE(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_rfeDevice(nullptr)
{
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "LimeRFE error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeRFE::networkManagerFinished
    );
    listComPorts();
}

LimeRFE::~LimeRFE()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeRFE::networkManagerFinished
    );
    delete m_networkManager;
    closeDevice();
}

void LimeRFE::start()
{
    qDebug("LimeRFE::start");
    m_state = StRunning;
}

void LimeRFE::stop()
{
    qDebug("LimeRFE::stop");
    m_state = StIdle;
}

void LimeRFE::listComPorts()
{
    m_comPorts.clear();
    std::vector<std::string> comPorts;
    SerialUtil::getComPorts(comPorts, "ttyUSB[0-9]+"); // regex is for Linux only

    for (std::vector<std::string>::const_iterator it = comPorts.begin(); it != comPorts.end(); ++it) {
        m_comPorts.push_back(QString(it->c_str()));
    }
}

void LimeRFE::applySettings(const LimeRFESettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "LimeRFE::applySettings:" << settings.getDebugString(settingsKeys, force) << " force:" << force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

}

bool LimeRFE::handleMessage(const Message& cmd)
{
	if (MsgConfigureLimeRFE::match(cmd))
	{
        MsgConfigureLimeRFE& cfg = (MsgConfigureLimeRFE&) cmd;
        qDebug() << "LimeRFE::handleMessage: MsgConfigureLimeRFE";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}

    return false;
}

QByteArray LimeRFE::serialize() const
{
    SimpleSerializer s(1);
    s.writeBlob(1, m_settings.serialize());
    return s.final();
}

bool LimeRFE::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        m_settings.resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        d.readBlob(1, &bytetmp);

        if (m_settings.deserialize(bytetmp))
        {
            MsgConfigureLimeRFE *msg = MsgConfigureLimeRFE::create(m_settings, QList<QString>(), true);
            m_inputMessageQueue.push(msg);
            return true;
        }
        else
        {
            m_settings.resetToDefaults();
            MsgConfigureLimeRFE *msg = MsgConfigureLimeRFE::create(m_settings, QList<QString>(), true);
            m_inputMessageQueue.push(msg);
            return false;
        }
    }
    else
    {
        return false;
    }
}

int LimeRFE::openDevice(const std::string& serialDeviceName)
{
    closeDevice();

    rfe_dev_t *rfeDevice = RFE_Open(serialDeviceName.c_str(), nullptr);

    if (rfeDevice != (void *) -1)
    {
        m_rfeDevice = rfeDevice;
        return 0;
    }
    else
    {
        return -1;
    }
}

void LimeRFE::closeDevice()
{
    if (m_rfeDevice)
    {
        RFE_Close(m_rfeDevice);
        m_rfeDevice = nullptr;
    }
}

int LimeRFE::configure()
{
    if (!m_rfeDevice) {
        return -1;
    }

    qDebug() << "LimeRFE::configure: "
        << "attValue: " << (int) m_rfeBoardState.attValue
        << "channelIDRX: " << (int) m_rfeBoardState.channelIDRX
        << "channelIDTX: " << (int) m_rfeBoardState.channelIDTX
        << "mode: " << (int) m_rfeBoardState.mode
        << "notchOnOff: " << (int) m_rfeBoardState.notchOnOff
        << "selPortRX: " << (int) m_rfeBoardState.selPortRX
        << "selPortTX: " << (int) m_rfeBoardState.selPortTX
        << "enableSWR: " << (int) m_rfeBoardState.enableSWR
        << "sourceSWR: " << (int) m_rfeBoardState.sourceSWR;

    int rc = RFE_ConfigureState(m_rfeDevice, m_rfeBoardState);

    if (rc != 0) {
        qInfo("LimeRFE::configure: %s", getError(rc).c_str());
    } else {
        qDebug() << "LimeRFE::configure: done";
    }

    return rc;
}

int LimeRFE::getState()
{
    if (!m_rfeDevice) {
        return -1;
    }

    int rc = RFE_GetState(m_rfeDevice, &m_rfeBoardState);

    qDebug() << "LimeRFE::getState: "
        << "attValue: " << (int) m_rfeBoardState.attValue
        << "channelIDRX: " << (int) m_rfeBoardState.channelIDRX
        << "channelIDTX: " << (int) m_rfeBoardState.channelIDTX
        << "mode: " << (int) m_rfeBoardState.mode
        << "notchOnOff: " << (int) m_rfeBoardState.notchOnOff
        << "selPortRX: " << (int) m_rfeBoardState.selPortRX
        << "selPortTX: " << (int) m_rfeBoardState.selPortTX
        << "enableSWR: " << (int) m_rfeBoardState.enableSWR
        << "sourceSWR: " << (int) m_rfeBoardState.sourceSWR;

    if (rc != 0) {
        qInfo("LimeRFE::getState: %s", getError(rc).c_str());
    }

    if (m_rfeBoardState.mode == RFE_MODE_RX)
    {
        m_rxOn = true;
        m_txOn = false;
    }
    else if (m_rfeBoardState.mode == RFE_MODE_TX)
    {
        m_rxOn = false;
        m_txOn = true;
    }
    else if (m_rfeBoardState.mode == RFE_MODE_NONE)
    {
        m_rxOn = false;
        m_txOn = false;
    }
    else if (m_rfeBoardState.mode == RFE_MODE_TXRX)
    {
        m_rxOn = true;
        m_txOn = true;
    }

    return rc;
}

std::string LimeRFE::getError(int errorCode)
{
    std::map<int, std::string>::const_iterator it = m_errorCodesMap.find(errorCode);

    if (it == m_errorCodesMap.end()) {
        return "Unknown error";
    } else {
        return it->second;
    }
}

int LimeRFE::setRx(bool rxOn)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int mode = RFE_MODE_NONE;

    if (rxOn)
    {
        if (m_txOn) {
            mode = RFE_MODE_TXRX;
        } else {
            mode = RFE_MODE_RX;
        }
    }
    else
    {
        if (m_txOn) {
            mode = RFE_MODE_TX;
        }
    }

    int rc = RFE_Mode(m_rfeDevice, mode);

    if (rc == 0)
    {
        m_rxOn = rxOn;
        qDebug("LimeRFE::setRx: switch %s mode: %d", rxOn ? "on" : "off", mode);
    }
    else
    {
        qInfo("LimeRFE::setRx %s: %s", rxOn ? "on" : "off", getError(rc).c_str());
    }

    return rc;
}

int LimeRFE::setTx(bool txOn)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int mode = RFE_MODE_NONE;

    if (txOn)
    {
        if (m_rxOn) {
            mode = RFE_MODE_TXRX;
        } else {
            mode = RFE_MODE_TX;
        }
    }
    else
    {
        if (m_rxOn) {
            mode = RFE_MODE_RX;
        }
    }

    int rc = RFE_Mode(m_rfeDevice, mode);

    if (rc == 0)
    {
        m_txOn = txOn;
        qDebug("LimeRFE::setTx: switch %s mode: %d", txOn ? "on" : "off", mode);
    }
    else
    {
        qInfo("LimeRFE::setTx %s: %s", txOn ? "on" : "off", getError(rc).c_str());
    }

    return rc;
}

bool LimeRFE::turnDevice(int deviceSetIndex, bool on)
{
    qDebug("LimeRFE::turnDevice %d: %s", deviceSetIndex, on ? "on" : "off");
    SWGSDRangel::SWGDeviceState response;
    SWGSDRangel::SWGErrorResponse error;
    int httpCode;

    if (on) {
        httpCode = m_webAPIAdapterInterface->devicesetDeviceRunPost(deviceSetIndex, response, error);
    } else {
        httpCode = m_webAPIAdapterInterface->devicesetDeviceRunDelete(deviceSetIndex, response, error);
    }

    if (httpCode/100 == 2)
    {
        qDebug("LimeRFE::turnDevice: %s success", on ? "on" : "off");
        return true;
    }
    else
    {
        qWarning("LimeRFE::turnDevice: error: %s", qPrintable(*error.getMessage()));
        return false;
    }
}

int LimeRFE::getFwdPower(int& powerDB)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int power;
    int rc = RFE_ReadADC(m_rfeDevice, RFE_ADC1, &power);

    if (rc == 0) {
        powerDB = power;
    }

    return rc;
}

int LimeRFE::getRefPower(int& powerDB)
{
    if (!m_rfeDevice) {
        return -1;
    }

    int power;
    int rc = RFE_ReadADC(m_rfeDevice, RFE_ADC2, &power);

    if (rc == 0) {
        powerDB = power;
    }

    return rc;
}

void LimeRFE::settingsToState(const LimeRFESettings& settings)
{
    if (settings.m_rxChannels == LimeRFESettings::ChannelGroups::ChannelsCellular)
    {
        if (settings.m_rxCellularChannel == LimeRFESettings::CellularChannel::CellularBand1)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND01;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }
        else if (settings.m_rxCellularChannel == LimeRFESettings::CellularChannel::CellularBand2)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND02;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }
        else if (settings.m_rxCellularChannel == LimeRFESettings::CellularChannel::CellularBand3)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND03;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }
        else if (settings.m_rxCellularChannel == LimeRFESettings::CellularChannel::CellularBand38)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND38;
        }
        else if (settings.m_rxCellularChannel == LimeRFESettings::CellularChannel::CellularBand7)
        {
            m_rfeBoardState.channelIDRX = RFE_CID_CELL_BAND07;
            m_rfeBoardState.mode = RFE_MODE_TXRX;
        }

        m_rfeBoardState.selPortRX = RFE_PORT_1;
        m_rfeBoardState.selPortTX = RFE_PORT_1;
        m_rfeBoardState.channelIDTX = m_rfeBoardState.channelIDRX;
    }
    else
    {
        if (settings.m_rxChannels == LimeRFESettings::ChannelGroups::ChannelsWideband)
        {
            if (settings.m_rxWidebandChannel == LimeRFESettings::WidebandChannel::WidebandLow) {
                m_rfeBoardState.channelIDRX = RFE_CID_WB_1000;
            } else if (settings.m_rxWidebandChannel == LimeRFESettings::WidebandChannel::WidebandHigh) {
                m_rfeBoardState.channelIDRX = RFE_CID_WB_4000;
            }
        }
        else if (settings.m_rxChannels == LimeRFESettings::ChannelGroups::ChannelsHAM)
        {
            if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_30M) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0030;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_50_70MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0070;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_144_146MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0145;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_220_225MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0220;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_430_440MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0435;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_902_928MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_0920;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_1240_1325MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_1280;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_2300_2450MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_2400;
            } else if (settings.m_rxHAMChannel == LimeRFESettings::HAMChannel::HAM_3300_3500MHz) {
                m_rfeBoardState.channelIDRX = RFE_CID_HAM_3500;
            }
        }

        if (settings.m_rxPort == LimeRFESettings::RxPort::RxPortJ3) {
            m_rfeBoardState.selPortRX = RFE_PORT_1;
        } else if (settings.m_rxPort == LimeRFESettings::RxPort::RxPortJ5) {
            m_rfeBoardState.selPortRX = RFE_PORT_3;
        }

        if (settings.m_txRxDriven)
        {
            m_rfeBoardState.channelIDTX = m_rfeBoardState.channelIDRX;
        }
        else
        {
            if (settings.m_txChannels == LimeRFESettings::ChannelGroups::ChannelsWideband)
            {
                if (settings.m_txWidebandChannel == LimeRFESettings::WidebandChannel::WidebandLow) {
                    m_rfeBoardState.channelIDTX = RFE_CID_WB_1000;
                } else if (settings.m_txWidebandChannel == LimeRFESettings::WidebandChannel::WidebandHigh) {
                    m_rfeBoardState.channelIDTX = RFE_CID_WB_4000;
                }
            }
            else if (settings.m_txChannels == LimeRFESettings::ChannelGroups::ChannelsHAM)
            {
                if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_30M) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0030;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_50_70MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0070;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_144_146MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0145;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_220_225MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0220;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_430_440MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0435;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_902_928MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_0920;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_1240_1325MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_1280;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_2300_2450MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_2400;
                } else if (settings.m_txHAMChannel == LimeRFESettings::HAMChannel::HAM_3300_3500MHz) {
                    m_rfeBoardState.channelIDTX = RFE_CID_HAM_3500;
                }
            }
        }

        if (settings.m_txPort == LimeRFESettings::TxPort::TxPortJ3) {
            m_rfeBoardState.selPortTX = RFE_PORT_1;
        } else if (settings.m_txPort == LimeRFESettings::TxPort::TxPortJ4) {
            m_rfeBoardState.selPortTX = RFE_PORT_2;
        } else if (settings.m_txPort == LimeRFESettings::TxPort::TxPortJ5) {
            m_rfeBoardState.selPortTX = RFE_PORT_3;
        }
    }

    m_rfeBoardState.attValue = settings.m_attenuationFactor > 7 ? 7 : settings.m_attenuationFactor;
    m_rfeBoardState.notchOnOff = settings.m_amfmNotch;
    m_rfeBoardState.enableSWR = settings.m_swrEnable ? RFE_SWR_ENABLE : RFE_SWR_DISABLE;

    if (settings.m_swrSource == LimeRFESettings::SWRSource::SWRExternal) {
        m_rfeBoardState.sourceSWR = RFE_SWR_SRC_EXT;
    } else if (settings.m_swrSource == LimeRFESettings::SWRSource::SWRCellular) {
        m_rfeBoardState.sourceSWR = RFE_SWR_SRC_CELL;
    }
}

void LimeRFE::stateToSettings(LimeRFESettings& settings, QList<QString>& settingsKeys)
{
    if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND01)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_rxCellularChannel = LimeRFESettings::CellularChannel::CellularBand1;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxCellularChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND02)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_rxCellularChannel = LimeRFESettings::CellularChannel::CellularBand2;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxCellularChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND03)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_rxCellularChannel = LimeRFESettings::CellularChannel::CellularBand3;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxCellularChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND07)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_rxCellularChannel = LimeRFESettings::CellularChannel::CellularBand7;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxCellularChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_CELL_BAND38)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_rxCellularChannel = LimeRFESettings::CellularChannel::CellularBand38;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxCellularChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_WB_1000)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsWideband;
        settings.m_rxWidebandChannel = LimeRFESettings::WidebandChannel::WidebandLow;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxWidebandChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_WB_4000)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsWideband;
        settings.m_rxWidebandChannel = LimeRFESettings::WidebandChannel::WidebandHigh;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxWidebandChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0030)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_30M;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0070)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_50_70MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0145)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_144_146MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0220)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_220_225MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0435)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_430_440MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_0920)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_902_928MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_1280)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_1240_1325MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_2400)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_2300_2450MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }
    else if (m_rfeBoardState.channelIDRX == RFE_CID_HAM_3500)
    {
        settings.m_rxChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_rxHAMChannel = LimeRFESettings::HAMChannel::HAM_3300_3500MHz;
        settingsKeys.append("rxChannels");
        settingsKeys.append("rxHAMChannel");
    }

    if (m_rfeBoardState.selPortRX == RFE_PORT_1) {
        settings.m_rxPort = LimeRFESettings::RxPort::RxPortJ3;
    } else if (m_rfeBoardState.selPortRX == RFE_PORT_3) {
        settings.m_rxPort = LimeRFESettings::RxPort::RxPortJ5;
    }

    settingsKeys.append("rxPort");

    if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND01)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_txCellularChannel = LimeRFESettings::CellularChannel::CellularBand1;
        settingsKeys.append("txChannels");
        settingsKeys.append("txCellularChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND02)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_txCellularChannel = LimeRFESettings::CellularChannel::CellularBand2;
        settingsKeys.append("txChannels");
        settingsKeys.append("txCellularChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND03)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_txCellularChannel = LimeRFESettings::CellularChannel::CellularBand3;
        settingsKeys.append("txChannels");
        settingsKeys.append("txCellularChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND07)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_txCellularChannel = LimeRFESettings::CellularChannel::CellularBand7;
        settingsKeys.append("txChannels");
        settingsKeys.append("txCellularChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_CELL_BAND38)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsCellular;
        settings.m_txCellularChannel = LimeRFESettings::CellularChannel::CellularBand38;
        settingsKeys.append("txChannels");
        settingsKeys.append("txCellularChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_WB_1000)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsWideband;
        settings.m_txWidebandChannel = LimeRFESettings::WidebandChannel::WidebandLow;
        settingsKeys.append("txChannels");
        settingsKeys.append("txWidebandChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_WB_4000)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsWideband;
        settings.m_txWidebandChannel = LimeRFESettings::WidebandChannel::WidebandHigh;
        settingsKeys.append("txChannels");
        settingsKeys.append("txWidebandChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0030)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_30M;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0070)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_50_70MHz;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0145)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_144_146MHz;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0220)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_220_225MHz;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0435)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_430_440MHz;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_0920)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_902_928MHz;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_1280)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_1240_1325MHz;
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_2400)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_2300_2450MHz;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }
    else if (m_rfeBoardState.channelIDTX == RFE_CID_HAM_3500)
    {
        settings.m_txChannels = LimeRFESettings::ChannelGroups::ChannelsHAM;
        settings.m_txHAMChannel = LimeRFESettings::HAMChannel::HAM_3300_3500MHz;
        settingsKeys.append("txChannels");
        settingsKeys.append("txHAMChannel");
    }

    if (m_rfeBoardState.selPortTX == RFE_PORT_1) {
        settings.m_txPort = LimeRFESettings::TxPort::TxPortJ3;
    } else if (m_rfeBoardState.selPortTX == RFE_PORT_2) {
        settings.m_txPort = LimeRFESettings::TxPort::TxPortJ4;
    } else if (m_rfeBoardState.selPortTX == RFE_PORT_3) {
        settings.m_txPort = LimeRFESettings::TxPort::TxPortJ5;
    }

    settingsKeys.append("txPort");
    settingsKeys.append("attenuationFactor");
    settingsKeys.append("amfmNotch");
    settingsKeys.append("swrEnable");
    settingsKeys.append("swrSource");

    settings.m_attenuationFactor = m_rfeBoardState.attValue;
    settings.m_amfmNotch =  m_rfeBoardState.notchOnOff == RFE_NOTCH_ON;
    settings.m_swrEnable = m_rfeBoardState.enableSWR == RFE_SWR_ENABLE;
    settings.m_swrSource = m_rfeBoardState.sourceSWR == RFE_SWR_SRC_CELL ?
        LimeRFESettings::SWRSource::SWRCellular :
        LimeRFESettings::SWRSource::SWRExternal;
}

void LimeRFE::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LimeRFE::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LimeRFE::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

int LimeRFE::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setLimeRfeSettings(new SWGSDRangel::SWGLimeRFESettings());
    response.getLimeRfeSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int LimeRFE::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    LimeRFESettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureLimeRFE *msg = MsgConfigureLimeRFE::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("LimeRFE::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLimeRFE *msgToGUI = MsgConfigureLimeRFE::create(settings, featureSettingsKeys, true);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int LimeRFE::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    response.setLimeRfeReport(new SWGSDRangel::SWGLimeRFEReport());
    response.getLimeRfeReport()->init();
    return webapiFormatFeatureReport(response, errorMessage);
}

int LimeRFE::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGLimeRFEActions *swgLimeRFEActions = query.getLimeRfeActions();

    if (swgLimeRFEActions)
    {
        bool unknownAction = true;
        int channel = -1;
        int deviceSetIndex = -1;

        if (featureActionsKeys.contains("selectChannel"))
        {
            channel = swgLimeRFEActions->getSelectChannel();
            unknownAction = false;
        }

        if (featureActionsKeys.contains("deviceSetIndex"))
        {
            deviceSetIndex = swgLimeRFEActions->getDeviceSetIndex();
            unknownAction = false;
        }

        if (featureActionsKeys.contains("openCloseDevice") && (swgLimeRFEActions->getOpenCloseDevice() != 0))
        {
            int rc = openDevice(m_settings.m_devicePath.toStdString());
            unknownAction = false;

            if (rc != 0)
            {
                errorMessage = QString("Open %1: %2").arg(m_settings.m_devicePath).arg(getError(rc).c_str());
                return 500;
            }
        }

        if (featureActionsKeys.contains("getState") && (swgLimeRFEActions->getGetState() != 0))
        {
            int rc = getState();
            unknownAction = false;

            if (rc != 0)
            {
                errorMessage = QString("Get state %1: %2").arg(m_settings.m_devicePath).arg(getError(rc).c_str());
                return 500;
            }
        }

        if (featureActionsKeys.contains("fromToSettings") && (swgLimeRFEActions->getFromToSettings() != 0))
        {
            settingsToState(m_settings);
            unknownAction = false;
        }

        if ((channel >= 0) && featureActionsKeys.contains("switchChannel"))
        {
            if (channel == 0)
            {
                bool on = swgLimeRFEActions->getSwitchChannel() != 0;
                int rc = setRx(on);

                if (rc != 0)
                {
                    errorMessage = QString("Set Rx %1 %2: %3").arg(m_settings.m_devicePath).arg(on).arg(getError(rc).c_str());
                    return 500;
                }

                if (getMessageQueueToGUI())
                {
                    MsgReportSetRx *msg = MsgReportSetRx::create(on);
                    getMessageQueueToGUI()->push(msg);
                }
            }
            else
            {
                bool on = swgLimeRFEActions->getSwitchChannel() != 0;
                int rc = setTx(on);

                if (rc != 0)
                {
                    errorMessage = QString("Set Tx %1 %2: %3").arg(m_settings.m_devicePath).arg(on).arg(getError(rc).c_str());
                    return 500;
                }

                if (getMessageQueueToGUI())
                {
                    MsgReportSetTx *msg = MsgReportSetTx::create(on);
                    getMessageQueueToGUI()->push(msg);
                }
            }

            if (deviceSetIndex >= 0) {
                turnDevice(deviceSetIndex, swgLimeRFEActions->getSwitchChannel() != 0);
            }

            unknownAction = false;
        }

        if (featureActionsKeys.contains("fromToSettings") && (swgLimeRFEActions->getFromToSettings() == 0))
        {
            QList<QString> settingsKeys;
            stateToSettings(m_settings, settingsKeys);
            unknownAction = false;

            if (getMessageQueueToGUI())
            {
                MsgConfigureLimeRFE *msg = MsgConfigureLimeRFE::create(m_settings, settingsKeys, false);
                getMessageQueueToGUI()->push(msg);
            }
        }

        if (featureActionsKeys.contains("openCloseDevice") && (swgLimeRFEActions->getOpenCloseDevice() == 0))
        {
            closeDevice();
            unknownAction = false;
        }

        if (featureActionsKeys.contains("setRx"))
        {
            int rc = setRx(swgLimeRFEActions->getSetRx() != 0);
            unknownAction = false;

            if (rc == 0)
            {
                if (getMessageQueueToGUI())
                {
                    MsgReportSetRx *msg = MsgReportSetRx::create(swgLimeRFEActions->getSetRx() != 0);
                    getMessageQueueToGUI()->push(msg);
                }
            }
            else
            {
                errorMessage = QString("Cannot switch Rx");
                return 500;
            }
        }

        if (featureActionsKeys.contains("setTx"))
        {
            int rc = setTx(swgLimeRFEActions->getSetTx() != 0);
            unknownAction = false;

            if (rc == 0)
            {
                if (getMessageQueueToGUI())
                {
                    MsgReportSetTx *msg = MsgReportSetTx::create(swgLimeRFEActions->getSetTx() != 0);
                    getMessageQueueToGUI()->push(msg);
                }
            }
            else
            {
                errorMessage = QString("Cannot switch Tx");
                return 500;
            }
        }

        if (unknownAction)
        {
            errorMessage = "Unknown action";
            return 400;
        }
        else
        {
            return 202;
        }
    }
    else
    {
        errorMessage = "Missing SimplePTTActions in query";
        return 400;
    }
}

void LimeRFE::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const LimeRFESettings& settings)
{
    if (response.getLimeRfeSettings()->getTitle()) {
        *response.getLimeRfeSettings()->getTitle() = settings.m_title;
    } else {
        response.getLimeRfeSettings()->setTitle(new QString(settings.m_title));
    }

    response.getLimeRfeSettings()->setRgbColor(settings.m_rgbColor);
    response.getLimeRfeSettings()->setDevicePath(new QString(settings.m_devicePath));
    response.getLimeRfeSettings()->setRxChannels((int) settings.m_rxChannels);
    response.getLimeRfeSettings()->setRxWidebandChannel((int) settings.m_rxWidebandChannel);
    response.getLimeRfeSettings()->setRxHamChannel((int) settings.m_rxHAMChannel);
    response.getLimeRfeSettings()->setRxCellularChannel((int) settings.m_rxCellularChannel);
    response.getLimeRfeSettings()->setRxPort((int) settings.m_rxPort);
    response.getLimeRfeSettings()->setAmfmNotch(settings.m_amfmNotch ? 1 : 0);
    response.getLimeRfeSettings()->setAttenuationFactor(settings.m_attenuationFactor);
    response.getLimeRfeSettings()->setTxChannels((int) settings.m_txChannels);
    response.getLimeRfeSettings()->setTxWidebandChannel((int) settings.m_txWidebandChannel);
    response.getLimeRfeSettings()->setTxHamChannel((int) settings.m_txHAMChannel);
    response.getLimeRfeSettings()->setTxCellularChannel((int) settings.m_txCellularChannel);
    response.getLimeRfeSettings()->setTxPort((int) settings.m_txPort);
    response.getLimeRfeSettings()->setSwrEnable(settings.m_swrEnable ? 1 : 0);
    response.getLimeRfeSettings()->setSwrSource((int) settings.m_swrSource);
    response.getLimeRfeSettings()->setTxRxDriven(settings.m_txRxDriven ? 1 : 0);

    response.getLimeRfeSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLimeRfeSettings()->getReverseApiAddress()) {
        *response.getLimeRfeSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLimeRfeSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLimeRfeSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLimeRfeSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getLimeRfeSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getLimeRfeSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getLimeRfeSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getLimeRfeSettings()->setRollupState(swgRollupState);
        }
    }
}

void LimeRFE::webapiUpdateFeatureSettings(
    LimeRFESettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getLimeRfeSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getLimeRfeSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("devicePath")) {
        settings.m_devicePath = *response.getLimeRfeSettings()->getDevicePath();
    }
    if (featureSettingsKeys.contains("rxChannels")) {
        settings.m_rxChannels = (LimeRFESettings::ChannelGroups) response.getLimeRfeSettings()->getRxChannels();
    }
    if (featureSettingsKeys.contains("rxWidebandChannel")) {
        settings.m_rxWidebandChannel = (LimeRFESettings::WidebandChannel) response.getLimeRfeSettings()->getRxWidebandChannel();
    }
    if (featureSettingsKeys.contains("rxHAMChannel")) {
        settings.m_rxHAMChannel = (LimeRFESettings::HAMChannel) response.getLimeRfeSettings()->getRxHamChannel();
    }
    if (featureSettingsKeys.contains("rxCellularChannel")) {
        settings.m_rxCellularChannel = (LimeRFESettings::CellularChannel) response.getLimeRfeSettings()->getRxCellularChannel();
    }
    if (featureSettingsKeys.contains("rxPort")) {
        settings.m_rxPort = (LimeRFESettings::RxPort) response.getLimeRfeSettings()->getRxPort();
    }
    if (featureSettingsKeys.contains("amfmNotch")) {
        settings.m_amfmNotch = response.getLimeRfeSettings()->getAmfmNotch() != 0;
    }
    if (featureSettingsKeys.contains("attenuationFactor")) {
        settings.m_attenuationFactor = response.getLimeRfeSettings()->getAttenuationFactor();
    }
    if (featureSettingsKeys.contains("txChannels")) {
        settings.m_txChannels = (LimeRFESettings::ChannelGroups) response.getLimeRfeSettings()->getTxChannels();
    }
    if (featureSettingsKeys.contains("txWidebandChannel")) {
        settings.m_txWidebandChannel = (LimeRFESettings::WidebandChannel) response.getLimeRfeSettings()->getTxWidebandChannel();
    }
    if (featureSettingsKeys.contains("txHAMChannel")) {
        settings.m_txHAMChannel = (LimeRFESettings::HAMChannel) response.getLimeRfeSettings()->getTxHamChannel();
    }
    if (featureSettingsKeys.contains("txCellularChannel")) {
        settings.m_txCellularChannel = (LimeRFESettings::CellularChannel) response.getLimeRfeSettings()->getTxCellularChannel();
    }
    if (featureSettingsKeys.contains("txPort")) {
        settings.m_txPort = (LimeRFESettings::TxPort) response.getLimeRfeSettings()->getTxPort();
    }
    if (featureSettingsKeys.contains("swrEnable")) {
        settings.m_swrEnable = response.getLimeRfeSettings()->getSwrEnable() != 0;
    }
    if (featureSettingsKeys.contains("swrSource")) {
        settings.m_swrSource = (LimeRFESettings::SWRSource) response.getLimeRfeSettings()->getSwrSource();
    }
    if (featureSettingsKeys.contains("txRxDriven")) {
        settings.m_txRxDriven = response.getLimeRfeSettings()->getTxRxDriven() != 0;
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLimeRfeSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLimeRfeSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLimeRfeSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getLimeRfeSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getLimeRfeSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getLimeRfeSettings()->getRollupState());
    }
}

int LimeRFE::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response, QString& errorMessage)
{
    response.getLimeRfeReport()->setRxOn(m_rxOn ? 1 : 0);
    response.getLimeRfeReport()->setTxOn(m_txOn ? 1 : 0);

    int fwdPower;
    int rc = getFwdPower(fwdPower);

    if (rc != 0)
    {
        errorMessage = QString("Error getting forward power from LimeRFE device %1: %2")
            .arg(m_settings.m_devicePath).arg(getError(rc).c_str());
        return 500;
    }

    int refPower;
    rc = getRefPower(refPower);

    if (rc != 0)
    {
        errorMessage = QString("Error getting reflected power from LimeRFE device %1: %2")
            .arg(m_settings.m_devicePath).arg(getError(rc).c_str());
        return 500;
    }

    response.getLimeRfeReport()->setForwardPower(fwdPower);
    response.getLimeRfeReport()->setReflectedPower(refPower);
    return 200;
}
