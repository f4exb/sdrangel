///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#include <hamlib/rig.h>

#include <QtGlobal>
#include "util/simpleserializer.h"
#include "audiocatsisosettings.h"

MESSAGE_CLASS_DEFINITION(AudioCATSISOSettings::MsgPTT, Message)
MESSAGE_CLASS_DEFINITION(AudioCATSISOSettings::MsgCATConnect, Message)
MESSAGE_CLASS_DEFINITION(AudioCATSISOSettings::MsgCATReportStatus, Message)

const int AudioCATSISOSettings::m_catSpeeds[] = {
    1200,
    2400,
    4800,
    9600,
    19200,
    38400,
    57600,
    115200
};

const int AudioCATSISOSettings::m_catDataBits[]
{
    7,
    8
};

const int AudioCATSISOSettings::m_catStopBits[]
{
    1,
    2
};

const int AudioCATSISOSettings::m_catHandshakes[]
{
    RIG_HANDSHAKE_NONE,
    RIG_HANDSHAKE_XONXOFF,
    RIG_HANDSHAKE_HARDWARE
};

const int AudioCATSISOSettings::m_catPTTMethods[]
{
    RIG_PTT_RIG,
    RIG_PTT_SERIAL_DTR,
    RIG_PTT_SERIAL_RTS
};

AudioCATSISOSettings::AudioCATSISOSettings()
{
    resetToDefaults();
}

void AudioCATSISOSettings::resetToDefaults()
{
    m_txEnable = false;
    m_pttSpectrumLink = true;
    m_rxCenterFrequency = 14200000;
    m_txCenterFrequency = 14200000;
    m_rxDeviceName = "";
    m_rxVolume = 1.0f;
    m_log2Decim = 0;
    m_rxIQMapping = LR;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_fcPosRx = FC_POS_CENTER;
    m_txDeviceName = "";
    m_txVolume = -10;
    m_txIQMapping = LR;
    m_txEnable = false;
    m_catDevicePath = "";
    m_hamlibModel = 1; // Hamlib dummy model
    m_catSpeedIndex = 4; // 19200
    m_catDataBitsIndex = 1; // 8
    m_catStopBitsIndex = 0; // 1
    m_catHandshakeIndex = 0; // None
    m_catPTTMethodIndex = 0; // PTT
    m_catDTRHigh = true; // High
    m_catRTSHigh = true; // High
    m_catPollingMs = 5000;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

AudioCATSISOSettings::AudioCATSISOSettings(const AudioCATSISOSettings& other)
{
    m_txEnable = other.m_txEnable;
    m_pttSpectrumLink = other.m_pttSpectrumLink;
    m_rxCenterFrequency = other.m_rxCenterFrequency;
    m_txCenterFrequency = other.m_txCenterFrequency;
    m_rxDeviceName = other.m_rxDeviceName;
    m_rxVolume = other.m_rxVolume;
    m_log2Decim = other.m_log2Decim;
    m_rxIQMapping = other.m_rxIQMapping;
    m_dcBlock = other.m_dcBlock;
    m_iqCorrection = other.m_iqCorrection;
    m_fcPosRx = other.m_fcPosRx;
    m_txDeviceName = other.m_txDeviceName;
    m_txVolume = other.m_txVolume;
    m_txIQMapping = other.m_txIQMapping;
    m_catDevicePath = other.m_catDevicePath;
    m_hamlibModel = other.m_hamlibModel;
    m_catSpeedIndex = other.m_catSpeedIndex;
    m_catDataBitsIndex = other.m_catDataBitsIndex;
    m_catStopBitsIndex = other.m_catStopBitsIndex;
    m_catHandshakeIndex = other.m_catHandshakeIndex;
    m_catPTTMethodIndex = other.m_catPTTMethodIndex;
    m_catDTRHigh = other.m_catDTRHigh;
    m_catRTSHigh = other.m_catRTSHigh;
    m_catPollingMs = other.m_catPollingMs;
    m_useReverseAPI = other.m_useReverseAPI;
    m_reverseAPIAddress = other.m_reverseAPIAddress;
    m_reverseAPIPort = other.m_reverseAPIPort;
    m_reverseAPIDeviceIndex = other.m_reverseAPIDeviceIndex;
}

QByteArray AudioCATSISOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_rxDeviceName);
    s.writeU64(2, m_rxCenterFrequency);
    s.writeFloat(3, m_rxVolume);
    s.writeU32(4, m_log2Decim);
    s.writeS32(5, (int)m_rxIQMapping);
    s.writeBool(6, m_dcBlock);
    s.writeBool(7, m_iqCorrection);
    s.writeS32(8, (int) m_fcPosRx);

    s.writeString(21, m_txDeviceName);
    s.writeU64(22, m_txCenterFrequency);
    s.writeS32(23, m_txVolume);
    s.writeS32(24, (int)m_txIQMapping);

    s.writeString(31, m_catDevicePath);
    s.writeU32(32, m_hamlibModel);
    s.writeS32(33, m_catSpeedIndex);
    s.writeS32(34, m_catDataBitsIndex);
    s.writeS32(35, m_catStopBitsIndex);
    s.writeS32(36, m_catHandshakeIndex);
    s.writeS32(37, m_catPTTMethodIndex);
    s.writeBool(38, m_catDTRHigh);
    s.writeBool(39, m_catRTSHigh);
    s.writeU32(40, m_catPollingMs);

    s.writeBool(51, m_useReverseAPI);
    s.writeString(52, m_reverseAPIAddress);
    s.writeU32(53, m_reverseAPIPort);
    s.writeU32(54, m_reverseAPIDeviceIndex);
    s.writeBool(56, m_pttSpectrumLink);
    s.writeBool(57, m_txEnable);

    return s.final();
}

bool AudioCATSISOSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        uint32_t uintval;
        int intval;

        d.readString(1, &m_rxDeviceName, "");
        d.readU64(2, &m_rxCenterFrequency, 14200000);
        d.readFloat(3, &m_rxVolume, 1.0f);
        d.readU32(4, &m_log2Decim, 0);
        d.readS32(5, (int *)&m_rxIQMapping, IQMapping::L);
        d.readBool(6, &m_dcBlock, false);
        d.readBool(7, &m_iqCorrection, false);
        d.readS32(8, &intval, 2);
        m_fcPosRx = (fcPos_t) intval;

        d.readString(21, &m_txDeviceName, "");
        d.readU64(22, &m_txCenterFrequency, 14200000);
        d.readS32(23, &m_txVolume, -10);
        d.readS32(24,(int *)&m_txIQMapping, IQMapping::LR);

        d.readString(31, &m_catDevicePath, "");
        d.readU32(32, &m_hamlibModel, 1);
        d.readS32(33, &m_catSpeedIndex, 4);
        d.readS32(34, &m_catDataBitsIndex, 1);
        d.readS32(35, &m_catStopBitsIndex, 0);
        d.readS32(36, &m_catHandshakeIndex, 0);
        d.readS32(37, &m_catPTTMethodIndex, 0);
        d.readBool(38, &m_catDTRHigh, true);
        d.readBool(39, &m_catRTSHigh, true);
        d.readU32(40, &m_catPollingMs, 500);

        d.readBool(51, &m_useReverseAPI, false);
        d.readString(52, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(53, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(54, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        d.readBool(56, &m_pttSpectrumLink, true);
        d.readBool(57, &m_txEnable, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AudioCATSISOSettings::applySettings(const QStringList& settingsKeys, const AudioCATSISOSettings& settings)
{
    if (settingsKeys.contains("rxDeviceName")) {
        m_rxDeviceName = settings.m_rxDeviceName;
    }
    if (settingsKeys.contains("rxCenterFrequency")) {
        m_rxCenterFrequency = settings.m_rxCenterFrequency;
    }
    if (settingsKeys.contains("rxVolume")) {
        m_rxVolume = settings.m_rxVolume;
    }
    if (settingsKeys.contains("log2Decim")) {
        m_log2Decim = settings.m_log2Decim;
    }
    if (settingsKeys.contains("rxIQMapping")) {
        m_rxIQMapping = settings.m_rxIQMapping;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
    }
    if (settingsKeys.contains("fcPosRx")) {
        m_fcPosRx = settings.m_fcPosRx;
    }

    if (settingsKeys.contains("txDeviceName")) {
        m_txDeviceName = settings.m_txDeviceName;
    }
    if (settingsKeys.contains("txCenterFrequency")) {
        m_txCenterFrequency = settings.m_txCenterFrequency;
    }
    if (settingsKeys.contains("txVolume")) {
        m_txVolume = settings.m_txVolume;
    }
    if (settingsKeys.contains("txIQMapping")) {
        m_txIQMapping = settings.m_txIQMapping;
    }

    if (settingsKeys.contains("txEnable")) {
        m_txEnable = settings.m_txEnable;
    }
    if (settingsKeys.contains("pttSpectrumLink")) {
        m_pttSpectrumLink = settings.m_pttSpectrumLink;
    }
    if (settingsKeys.contains("catDevicePath")) {
        m_catDevicePath = settings.m_catDevicePath;
    }
    if (settingsKeys.contains("hamlibModel")) {
        m_hamlibModel = settings.m_hamlibModel;
    }
    if (settingsKeys.contains("catSpeedIndex")) {
        m_catSpeedIndex = settings.m_catSpeedIndex;
    }
    if (settingsKeys.contains("catHandshakeIndex")) {
        m_catHandshakeIndex = settings.m_catHandshakeIndex;
    }
    if (settingsKeys.contains("catDataBitsIndex")) {
        m_catDataBitsIndex = settings.m_catDataBitsIndex;
    }
    if (settingsKeys.contains("catStopBitsIndex")) {
        m_catStopBitsIndex = settings.m_catStopBitsIndex;
    }
    if (settingsKeys.contains("catPTTMethodIndex")) {
        m_catPTTMethodIndex = settings.m_catPTTMethodIndex;
    }
    if (settingsKeys.contains("catDTRHigh")) {
        m_catDTRHigh = settings.m_catDTRHigh;
    }
    if (settingsKeys.contains("catRTSHigh")) {
        m_catRTSHigh = settings.m_catRTSHigh;
    }
    if (settingsKeys.contains("catPollingMs")) {
        m_catPollingMs = settings.m_catPollingMs;
    }

    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
}

QString AudioCATSISOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("rxDeviceName") || force) {
        ostr << " m_rxDeviceName: " << m_rxDeviceName.toStdString();
    }
    if (settingsKeys.contains("rxCenterFrequency") || force) {
        ostr << " m_rxCenterFrequency: " << m_rxCenterFrequency;
    }
    if (settingsKeys.contains("rxVolume") || force) {
        ostr << " m_rxVolume: " << m_rxVolume;
    }
    if (settingsKeys.contains("log2Decim") || force) {
        ostr << " m_log2Decim: " << m_log2Decim;
    }
    if (settingsKeys.contains("rxIQMapping") || force) {
        ostr << " m_rxIQMapping: " << m_rxIQMapping;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
    }
    if (settingsKeys.contains("fcPosRx") || force) {
        ostr << " m_fcPosRx: " << m_fcPosRx;
    }

    if (settingsKeys.contains("txDeviceName") || force) {
        ostr << " m_txDeviceName: " << m_txDeviceName.toStdString();
    }
    if (settingsKeys.contains("txCenterFrequency") || force) {
        ostr << " m_txCenterFrequency: " << m_txCenterFrequency;
    }
    if (settingsKeys.contains("txVolume") || force) {
        ostr << " m_txVolume: " << m_txVolume;
    }
    if (settingsKeys.contains("txIQMapping") || force) {
        ostr << " m_txIQMapping: " << m_txIQMapping;
    }

    if (settingsKeys.contains("txEnable") || force) {
        ostr << " m_txEnable: " << m_txEnable;
    }
    if (settingsKeys.contains("pttSpectrumLink") || force) {
        ostr << " m_pttSpectrumLink: " << m_pttSpectrumLink;
    }
    if (settingsKeys.contains("catDevicePath") || force) {
        ostr << " m_catDevicePath: " << m_catDevicePath.toStdString();
    }
    if (settingsKeys.contains("hamlibModel") || force) {
        ostr << " m_hamlibModel: " << m_hamlibModel;
    }
    if (settingsKeys.contains("catSpeedIndex") || force) {
        ostr << " m_catSpeedIndex: " << m_catSpeedIndex;
    }
    if (settingsKeys.contains("catHandshakeIndex") || force) {
        ostr << " m_catHandshakeIndex: " << m_catHandshakeIndex;
    }
    if (settingsKeys.contains("catStopBits") || force) {
        ostr << " m_catStopBits: " << m_catStopBits;
    }
    if (settingsKeys.contains("catDataBits") || force) {
        ostr << " m_catDataBits: " << m_catDataBits;
    }
    if (settingsKeys.contains("catPTTMethodIndex") || force) {
        ostr << " m_catPTTMethodIndex: " << m_catPTTMethodIndex;
    }
    if (settingsKeys.contains("catDTRHigh") || force) {
        ostr << " m_catDTRHigh: " << m_catDTRHigh;
    }
    if (settingsKeys.contains("catRTSHigh") || force) {
        ostr << " m_catRTSHigh: " << m_catRTSHigh;
    }
    if (settingsKeys.contains("catPollingMs") || force) {
        ostr << " m_catPollingMs: " << m_catPollingMs;
    }

    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }

    return QString(ostr.str().c_str());
}
