///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2018, 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "chirpchatdemodsettings.h"

const int ChirpChatDemodSettings::bandwidths[] = {
    325,    // 384k / 1024
    488,    // 500k / 1024
    750,    // 384k / 512
    1500,   // 384k / 256
    2604,   // 333k / 128
    3125,   // 400k / 128
    3906,   // 500k / 128
    5208,   // 333k / 64
    6250,   // 400k / 64
    7813,   // 500k / 64
    10417,  // 333k / 32
    12500,  // 400k / 32
    15625,  // 500k / 32
    20833,  // 333k / 16
    25000,  // 400k / 16
    31250,  // 500k / 16
    41667,  // 333k / 8
    50000,  // 400k / 8
    62500,  // 500k / 8
    83333,  // 333k / 4
    100000, // 400k / 4
    125000, // 500k / 4
    166667, // 333k / 2
    200000, // 400k / 2
    250000, // 500k / 2
    333333, // 333k / 1
    400000, // 400k / 1
    500000  // 500k / 1
};
const int ChirpChatDemodSettings::nbBandwidths = 3*8 + 4;
const int ChirpChatDemodSettings::oversampling = 2;

ChirpChatDemodSettings::ChirpChatDemodSettings() :
    m_inputFrequencyOffset(0),
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void ChirpChatDemodSettings::resetToDefaults()
{
    m_bandwidthIndex = 5;
    m_spreadFactor = 7;
    m_deBits = 0;
    m_codingScheme = CodingLoRa;
    m_decodeActive = true;
    m_fftWindow = FFTWindow::Rectangle;
    m_eomSquelchTenths = 60;
    m_nbSymbolsMax = 255;
    m_preambleChirps = 8;
    m_packetLength = 32;
    m_nbParityBits = 1;
    m_hasCRC = true;
    m_hasHeader = true;
    m_sendViaUDP = false;
    m_invertRamps = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "ChirpChat Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray ChirpChatDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_bandwidthIndex);
    s.writeS32(3, m_spreadFactor);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(5, m_channelMarker->serialize());
    }

    s.writeString(6, m_title);
    s.writeS32(7, m_deBits);
    s.writeS32(8, m_codingScheme);
    s.writeBool(9, m_decodeActive);
    s.writeS32(10, m_eomSquelchTenths);
    s.writeU32(11, m_nbSymbolsMax);
    s.writeS32(12, m_packetLength);
    s.writeS32(13, m_nbParityBits);
    s.writeBool(14, m_hasCRC);
    s.writeBool(15, m_hasHeader);
    s.writeU32(17, m_preambleChirps);
    s.writeS32(18, (int) m_fftWindow);
    s.writeBool(19, m_invertRamps);
    s.writeBool(20, m_useReverseAPI);
    s.writeString(21, m_reverseAPIAddress);
    s.writeU32(22, m_reverseAPIPort);
    s.writeU32(23, m_reverseAPIDeviceIndex);
    s.writeU32(24, m_reverseAPIChannelIndex);
    s.writeS32(25, m_streamIndex);
    s.writeBool(26, m_sendViaUDP);
    s.writeString(27, m_udpAddress);
    s.writeU32(28, m_udpPort);

    if (m_rollupState) {
        s.writeBlob(29, m_rollupState->serialize());
    }

    s.writeS32(30, m_workspaceIndex);
    s.writeBlob(31, m_geometryBytes);
    s.writeBool(32, m_hidden);

    return s.final();
}

bool ChirpChatDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        int tmp;
        unsigned int utmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_bandwidthIndex, 0);
        d.readS32(3, &m_spreadFactor, 0);

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(5, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(6, &m_title, "ChirpChat Demodulator");
        d.readS32(7, &m_deBits, 0);
        d.readS32(8, &tmp);
        m_codingScheme = (CodingScheme) tmp;
        d.readBool(9, &m_decodeActive, true);
        d.readS32(10, &m_eomSquelchTenths, 60);
        d.readU32(11, &m_nbSymbolsMax, 255);
        d.readS32(12, &m_packetLength, 32);
        d.readS32(13, &m_nbParityBits, 1);
        d.readBool(14, &m_hasCRC, true);
        d.readBool(15, &m_hasHeader, true);
        d.readU32(17, &m_preambleChirps, 8);
        d.readS32(18, &tmp, (int) FFTWindow::Rectangle);
        m_fftWindow = (FFTWindow::Function) tmp;
        d.readBool(19, &m_invertRamps, false);
        d.readBool(20, &m_useReverseAPI, false);
        d.readString(21, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(22, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(23, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(24, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(25, &m_streamIndex, 0);
        d.readBool(26, &m_sendViaUDP, false);
        d.readString(27, &m_udpAddress, "127.0.0.1");
        d.readU32(28, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        if (m_rollupState)
        {
            d.readBlob(29, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(30, &m_workspaceIndex, 0);
        d.readBlob(31, &m_geometryBytes);
        d.readBool(32, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void ChirpChatDemodSettings::applySettings(const QStringList& settingsKeys, const ChirpChatDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset"))
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    if (settingsKeys.contains("bandwidthIndex"))
        m_bandwidthIndex = settings.m_bandwidthIndex;
    if (settingsKeys.contains("spreadFactor"))
        m_spreadFactor = settings.m_spreadFactor;
    if (settingsKeys.contains("deBits"))
        m_deBits = settings.m_deBits;
    if (settingsKeys.contains("fftWindow"))
        m_fftWindow = settings.m_fftWindow;
    if (settingsKeys.contains("codingScheme"))
        m_codingScheme = settings.m_codingScheme;
    if (settingsKeys.contains("decodeActive"))
        m_decodeActive = settings.m_decodeActive;
    if (settingsKeys.contains("eomSquelchTenths"))
        m_eomSquelchTenths = settings.m_eomSquelchTenths;
    if (settingsKeys.contains("nbSymbolsMax"))
        m_nbSymbolsMax = settings.m_nbSymbolsMax;
    if (settingsKeys.contains("preambleChirps"))
        m_preambleChirps = settings.m_preambleChirps;
    if (settingsKeys.contains("nbParityBits"))
        m_nbParityBits = settings.m_nbParityBits;
    if (settingsKeys.contains("packetLength"))
        m_packetLength = settings.m_packetLength;
    if (settingsKeys.contains("hasCRC"))
        m_hasCRC = settings.m_hasCRC;
    if (settingsKeys.contains("hasHeader"))
        m_hasHeader = settings.m_hasHeader;
    if (settingsKeys.contains("sendViaUDP"))
        m_sendViaUDP = settings.m_sendViaUDP;
    if (settingsKeys.contains("invertRamps"))
        m_invertRamps = settings.m_invertRamps;
    if (settingsKeys.contains("udpAddress"))
        m_udpAddress = settings.m_udpAddress;
    if (settingsKeys.contains("udpPort"))
        m_udpPort = settings.m_udpPort;
    if (settingsKeys.contains("useReverseAPI"))
        m_useReverseAPI = settings.m_useReverseAPI;
    if (settingsKeys.contains("reverseAPIAddress"))
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    if (settingsKeys.contains("reverseAPIPort"))
        m_reverseAPIPort = settings.m_reverseAPIPort;
    if (settingsKeys.contains("reverseAPIDeviceIndex"))
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    if (settingsKeys.contains("reverseAPIChannelIndex"))
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    if (settingsKeys.contains("streamIndex"))
        m_streamIndex = settings.m_streamIndex;
}

QString ChirpChatDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    QString debug;

    if (force || settingsKeys.contains("inputFrequencyOffset"))
        debug += QString("InputFrequencyOffset: %1 ").arg(m_inputFrequencyOffset);
    if (force || settingsKeys.contains("bandwidthIndex"))
        debug += QString("BandwidthIndex: %1 ").arg(m_bandwidthIndex);
    if (force || settingsKeys.contains("spreadFactor"))
        debug += QString("SpreadFactor: %1 ").arg(m_spreadFactor);
    if (force || settingsKeys.contains("deBits"))
        debug += QString("DEBits: %1 ").arg(m_deBits);
    if (force || settingsKeys.contains("fftWindow"))
        debug += QString("FFTWindow: %1 ").arg((int) m_fftWindow);
    if (force || settingsKeys.contains("codingScheme"))
        debug += QString("CodingScheme: %1 ").arg((int) m_codingScheme);
    if (force || settingsKeys.contains("decodeActive"))
        debug += QString("DecodeActive: %1 ").arg(m_decodeActive);
    if (force || settingsKeys.contains("eomSquelchTenths"))
        debug += QString("EOMSquelchTenths: %1 ").arg(m_eomSquelchTenths);
    if (force || settingsKeys.contains("nbSymbolsMax"))
        debug += QString("NbSymbolsMax: %1 ").arg(m_nbSymbolsMax);
    if (force || settingsKeys.contains("preambleChirps"))
        debug += QString("PreambleChirps: %1 ").arg(m_preambleChirps);
    if (force || settingsKeys.contains("nbParityBits"))
        debug += QString("NbParityBits: %1 ").arg(m_nbParityBits);
    if (force || settingsKeys.contains("packetLength"))
        debug += QString("PacketLength: %1 ").arg(m_packetLength);
    if (force || settingsKeys.contains("hasCRC"))
        debug += QString("HasCRC: %1 ").arg(m_hasCRC);
    if (force || settingsKeys.contains("hasHeader"))
        debug += QString("HasHeader: %1 ").arg(m_hasHeader);
    if (force || settingsKeys.contains("sendViaUDP"))
        debug += QString("SendViaUDP: %1 ").arg(m_sendViaUDP);
    if (force || settingsKeys.contains("invertRamps"))
        debug += QString("InvertRamps: %1 ").arg(m_invertRamps);
    if (force || settingsKeys.contains("udpAddress"))
        debug += QString("UDPAddress: %1 ").arg(m_udpAddress);
    if (force || settingsKeys.contains("udpPort"))
        debug += QString("UDPPort: %1 ").arg(m_udpPort);
    if (force || settingsKeys.contains("useReverseAPI"))
        debug += QString("UseReverseAPI: %1 ").arg(m_useReverseAPI);
    if (force || settingsKeys.contains("reverseAPIAddress"))
        debug += QString("ReverseAPIAddress: %1 ").arg(m_reverseAPIAddress);
    if (force || settingsKeys.contains("reverseAPIPort"))
        debug += QString("ReverseAPIPort: %1 ").arg(m_reverseAPIPort);
    if (force || settingsKeys.contains("reverseAPIDeviceIndex"))
        debug += QString("ReverseAPIDeviceIndex: %1 ").arg(m_reverseAPIDeviceIndex);
    if (force || settingsKeys.contains("reverseAPIChannelIndex"))
        debug += QString("ReverseAPIChannelIndex: %1 ").arg(m_reverseAPIChannelIndex);
    if (force || settingsKeys.contains("streamIndex"))
        debug += QString("StreamIndex: %1 ").arg(m_streamIndex);
    return debug;
}

unsigned int ChirpChatDemodSettings::getNbSFDFourths() const
{
    switch (m_codingScheme)
    {
    case CodingLoRa:
        return 9;
    default:
        return 8;
    }
}

bool ChirpChatDemodSettings::hasSyncWord() const
{
    return m_codingScheme == CodingLoRa;
}
