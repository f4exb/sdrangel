///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QStringList>
#include "util/simpleserializer.h"
#include "spectranmisosettings.h"

MESSAGE_CLASS_DEFINITION(MsgConfigureSpectranMISO, Message)

const QMap<SpectranMISOMode, QString> SpectranMISOSettings::m_modeDisplayNames = {
    { SPECTRANMISO_MODE_RX_IQ, "RX IQ" },
    { SPECTRANMISO_MODE_TX_IQ, "TX IQ" },
    { SPECTRANMISO_MODE_RXTX_IQ, "RX/TX IQ" },
    { SPECTRANMISO_MODE_RX_RAW, "RX RAW" },
    { SPECTRANMISO_MODE_2RX_RAW_INTL, "2x RX RAW Intl" },
    { SPECTRANMISO_MODE_2RX_RAW, "2x RX RAW" },
};

SpectranMISOSettings::SpectranMISOSettings() :
    m_mode(SPECTRANMISO_MODE_RX_IQ), // Default mode, can be set later
    m_rxChannel(SPECTRAN_RX_CHANNEL_1), // Default RX channel
    m_rxCenterFrequency(144300000), // Default RX center frequency, can be set later
    m_txCenterFrequency(144300000), // Default TX center frequency, can be set later
    m_sampleRate(1000000), // Default sample rate, can be set later
    m_clockRate(SPECTRANMISO_CLOCK_46M), // Default clock rate
    m_logDecimation(0), // Default log decimation factor
    m_streamIndex(0),
    m_spectrumStreamIndex(0),
    m_streamLock(false),
    m_useReverseAPI(false),
    m_reverseAPIAddress(""),
    m_reverseAPIPort(0),
    m_reverseAPIDeviceIndex(0)
{
}

void SpectranMISOSettings::resetToDefaults()
{
    m_mode = SPECTRANMISO_MODE_RX_IQ; // Reset to default mode
    m_rxChannel = SPECTRAN_RX_CHANNEL_1; // Reset to default RX channel
    m_rxCenterFrequency = 144300000;
    m_txCenterFrequency = 144300000;
    m_sampleRate = 1000000; // Reset sample rate to default
    m_clockRate = SPECTRANMISO_CLOCK_46M; // Reset to default clock rate
    m_logDecimation = 0; // Reset log decimation factor
    m_streamIndex = 0;
    m_spectrumStreamIndex = 0;
    m_streamLock = false;
    m_useReverseAPI = false;
    m_reverseAPIAddress.clear();
    m_reverseAPIPort = 0;
    m_reverseAPIDeviceIndex = 0;
}
QByteArray SpectranMISOSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(0, (int) m_mode);
    s.writeS32(1, (int) m_rxChannel);
    s.writeU64(2, m_rxCenterFrequency);
    s.writeU64(3, m_txCenterFrequency);
    s.writeS32(4, (qint32) m_sampleRate); // Serialize sample rate
    s.writeS32(5, (int) m_clockRate);
    s.writeS32(6, m_logDecimation);
    s.writeU32(7, (quint32) m_streamIndex);
    s.writeU32(8, (quint32) m_spectrumStreamIndex);
    s.writeBool(9, m_streamLock);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIDeviceIndex);

    return s.final();
}

bool SpectranMISOSettings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		int intval;
		uint32_t uintval;
        d.readS32(0, &intval, (int) SPECTRANMISO_MODE_RX_IQ); // Deserialize mode
        if ((intval < 0) || (intval >= SPECTRANMISO_MODE_END)) {
            m_mode = SPECTRANMISO_MODE_RX_IQ; // Default mode
        } else {
            m_mode = static_cast<SpectranMISOMode>(intval);
        }
        d.readS32(1, &intval, (int) SPECTRAN_RX_CHANNEL_1); // Deserialize RX channel
        if ((intval < 0) || (intval >= SPECTRAN_RX_CHANNEL_END)) {
            m_rxChannel = SPECTRAN_RX_CHANNEL_1; // Default RX channel
        } else {
            m_rxChannel = static_cast<SpectranRxChannel>(intval);
        }
        d.readU64(2, &m_rxCenterFrequency, 0);
        d.readU64(3, &m_txCenterFrequency, 0);
        d.readS32(4, &m_sampleRate, 0); // Deserialize sample rate
        d.readS32(5, &intval, 0);
        if ((intval < 0) || (intval >= SPECTRANMISO_CLOCK_END)) {
            m_clockRate = SPECTRANMISO_CLOCK_46M; // Default clock rate
        } else {
            m_clockRate = static_cast<SpectranMISOClockRate>(intval);
        }
        d.readS32(6, &m_logDecimation, 0);
        d.readU32(7, &uintval, 0);
        if (uintval > 255) {
            m_streamIndex = 0;
        } else {
            m_streamIndex = static_cast<int>(uintval);
        }
        d.readU32(8, &uintval, 0);
        if (uintval > 255) {
            m_spectrumStreamIndex = 0;
        } else {
            m_spectrumStreamIndex = static_cast<int>(uintval);
        }
        d.readBool(9, &m_streamLock, false);
        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, QString());
        d.readU32(12, &uintval, 0);
        if (uintval > 65535) {
            m_reverseAPIPort = 0;
        } else {
            m_reverseAPIPort = static_cast<uint16_t>(uintval);
        }
        d.readU32(13, &uintval, 0);
        if (uintval > 255) {
            m_reverseAPIDeviceIndex = 0;
        } else {
            m_reverseAPIDeviceIndex = static_cast<uint16_t>(uintval);
        }
        return true;
    }

    return false; // Unsupported version
}

void SpectranMISOSettings::applySettings(const QStringList& settingsKeys, const SpectranMISOSettings& settings)
{
    if (settingsKeys.contains("mode")) {
        m_mode = settings.m_mode;
    }
    if (settingsKeys.contains("rxChannel")) {
        m_rxChannel = settings.m_rxChannel; // Apply RX channel
    }
    if (settingsKeys.contains("rxCenterFrequency")) {
        m_rxCenterFrequency = settings.m_rxCenterFrequency;
    }
    if (settingsKeys.contains("txCenterFrequency")) {
        m_txCenterFrequency = settings.m_txCenterFrequency;
    }
    if (settingsKeys.contains("sampleRate")) {
        m_sampleRate = settings.m_sampleRate; // Apply sample rate
    }
    if (settingsKeys.contains("clockRate")) {
        m_clockRate = settings.m_clockRate;
    }
    if (settingsKeys.contains("logDecimation")) {
        m_logDecimation = settings.m_logDecimation;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("spectrumStreamIndex")) {
        m_spectrumStreamIndex = settings.m_spectrumStreamIndex;
    }
    if (settingsKeys.contains("streamLock")) {
        m_streamLock = settings.m_streamLock;
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

QString SpectranMISOSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    QString debugString;
    debugString += "Spectran MISO settings:\n";
    debugString += "---------------------\n";
    debugString += QString("Settings keys: %1\n").arg(settingsKeys.join(", "));
    debugString += QString("Force all settings: %1\n").arg(force ? "yes" : "no");
    if (force || settingsKeys.contains("mode")) {
        debugString += QString("Mode: %1\n").arg(m_modeDisplayNames.value(m_mode, "Unknown"));
    }
    if (force || settingsKeys.contains("rxChannel")) {
        debugString += QString("RX Channel: %1\n").arg(m_rxChannel);
    }
    if (force || settingsKeys.contains("rxCenterFrequency")) {
        debugString += QString("Rx Center Frequency: %1 Hz\n").arg(m_rxCenterFrequency);
    }
    if (force || settingsKeys.contains("txCenterFrequency")) {
        debugString += QString("Tx Center Frequency: %1 Hz\n").arg(m_txCenterFrequency);
    }
    if (force || settingsKeys.contains("sampleRate")) {
        debugString += QString("Sample Rate: %1 Hz\n").arg(m_sampleRate);
    }
    if (force || settingsKeys.contains("clockRate")) {
        debugString += QString("Clock Rate: %1\n").arg(m_clockRate);
    }
    if (force || settingsKeys.contains("logDecimation")) {
        debugString += QString("Log Decimation: %1\n").arg(m_logDecimation);
    }
    if (force || settingsKeys.contains("streamIndex")) {
        debugString += QString("Stream Index: %1\n").arg(m_streamIndex);
    }
    if (force || settingsKeys.contains("spectrumStreamIndex")) {
        debugString += QString("Spectrum Stream Index: %1\n").arg(m_spectrumStreamIndex);
    }
    if (force || settingsKeys.contains("streamLock")) {
        debugString += QString("Stream Lock: %1\n").arg(m_streamLock ? "Enabled" : "Disabled");
    }
    if (force || settingsKeys.contains("useReverseAPI")) {
        debugString += QString("Use Reverse API: %1\n").arg(m_useReverseAPI ? "Yes" : "No");
    }
    if (force || settingsKeys.contains("reverseAPIAddress")) {
        debugString += QString("Reverse API Address: %1\n").arg(m_reverseAPIAddress);
    }
    if (force || settingsKeys.contains("reverseAPIPort")) {
        debugString += QString("Reverse API Port: %1\n").arg(m_reverseAPIPort);
    }
    if (force || settingsKeys.contains("reverseAPIDeviceIndex")) {
        debugString += QString("Reverse API Device Index: %1\n").arg(m_reverseAPIDeviceIndex);
    }
    return debugString;
}

QDataStream &operator<<(QDataStream &stream, const SpectranMISOSettings &settings)
{
    stream << settings.m_mode
           << settings.m_rxChannel
           << settings.m_rxCenterFrequency
           << settings.m_txCenterFrequency
           << settings.m_sampleRate // Serialize sample rate
           << settings.m_clockRate
           << settings.m_logDecimation
           << settings.m_streamIndex
           << settings.m_spectrumStreamIndex
           << settings.m_streamLock
           << settings.m_useReverseAPI
           << settings.m_reverseAPIAddress
           << settings.m_reverseAPIPort
           << settings.m_reverseAPIDeviceIndex;
    return stream;
}

QDataStream &operator>>(QDataStream &stream, SpectranMISOSettings &settings)
{
    stream >> settings.m_mode
           >> settings.m_rxChannel
           >> settings.m_rxCenterFrequency
           >> settings.m_txCenterFrequency
           >> settings.m_sampleRate // Deserialize sample rate
           >> settings.m_clockRate
           >> settings.m_logDecimation
           >> settings.m_streamIndex
           >> settings.m_spectrumStreamIndex
           >> settings.m_streamLock
           >> settings.m_useReverseAPI
           >> settings.m_reverseAPIAddress
           >> settings.m_reverseAPIPort
           >> settings.m_reverseAPIDeviceIndex;
    return stream;
}

bool SpectranMISOSettings::isRawMode(const SpectranMISOMode& mode)
{
    return (mode == SpectranMISOMode::SPECTRANMISO_MODE_RX_RAW)
    || (mode == SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW_INTL)
    || (mode == SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW);
}

bool SpectranMISOSettings::isDualRx(const SpectranMISOMode &mode)
{
    return (mode == SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW_INTL);
}

bool SpectranMISOSettings::isRxModeSingle(const SpectranMISOMode &mode)
{
    return (mode == SpectranMISOMode::SPECTRANMISO_MODE_RX_IQ) || (mode == SpectranMISOMode::SPECTRANMISO_MODE_RX_RAW);
}

bool SpectranMISOSettings::isTxMode(const SpectranMISOMode &mode)
{
    return (mode == SpectranMISOMode::SPECTRANMISO_MODE_TX_IQ) || (mode == SpectranMISOMode::SPECTRANMISO_MODE_RXTX_IQ);
}

bool SpectranMISOSettings::isDecimationEnabled(const SpectranMISOMode &mode)
{
    return (mode == SpectranMISOMode::SPECTRANMISO_MODE_RX_RAW) || (mode == SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW);
}
